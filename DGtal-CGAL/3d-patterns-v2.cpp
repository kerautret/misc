#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <QtGui/qapplication.h>

#include <DGtal/base/Common.h>
#include <DGtal/helpers/StdDefs.h>
#include "DGtal/images/ImageSelector.h"
#include "DGtal/images/imagesSetsUtils/SetFromImage.h"
#include <DGtal/shapes/Shapes.h>
#include <DGtal/shapes/ShapeFactory.h>
#include "DGtal/shapes/implicit/ImplicitPolynomial3Shape.h"
#include <DGtal/shapes/GaussDigitizer.h>
#include <DGtal/topology/helpers/Surfaces.h>
#include "DGtal/io/viewers/Viewer3D.h"
#include "DGtal/io/DrawWithDisplay3DModifier.h"
#include "DGtal/io/readers/MPolynomialReader.h"
#include "DGtal/io/readers/VolReader.h"

#include <CGAL/Delaunay_triangulation_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_3.h>
#include <CGAL/Cartesian.h>
#include <CGAL/CORE/Expr.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
//typedef CGAL::Cartesian<CORE::Expr> K;
typedef CGAL::Delaunay_triangulation_3<K> Delaunay;
//typedef Delaunay::Vertex_circulator Vertex_circulator;
//typedef Delaunay::Edge_iterator  Edge_iterator;
typedef Delaunay::Cell_iterator     Cell_iterator;
typedef Delaunay::Facet             Facet;
typedef Delaunay::Facet_iterator    Facet_iterator;
typedef Delaunay::Edge              Edge;
typedef Delaunay::Edge_iterator     Edge_iterator;
typedef Delaunay::Vertex_iterator   Vertex_iterator;
typedef Delaunay::Vertex_handle     Vertex_handle;
typedef Delaunay::Point             CGALPoint;
typedef Delaunay::Cell_handle       Cell_handle;
typedef Delaunay::Triangle          Triangle;
typedef DGtal::SpaceND<3, DGtal::int64_t> Z3;
typedef Z3::Point Point;
typedef DGtal::HyperRectDomain<Z3> Domain;
typedef Domain::ConstIterator DomainConstIterator;

Point toDGtal(const CGALPoint &p)
{
  return Point ( p.x(),
		 p.y(),
		 p.z() );
}
// Point toDGtal(const CGALPoint &p)
// {
//   return Point ( p.x().longValue(),
// 		 p.y().longValue(),
// 		 p.z().longValue() );
// }
CGALPoint toCGAL(const Point &p)
{
  return CGALPoint( p[0], p[1], p[2] );
}

inline
Point operator^( const Point & a, const Point & b )
{
  BOOST_STATIC_ASSERT( Point::dimension == 3 );
  return Point( a[ 1 ] * b[ 2 ] - a[ 2 ] * b[ 1 ],
		a[ 2 ] * b[ 0 ] - a[ 0 ] * b[ 2 ],
		a[ 0 ] * b[ 1 ] - a[ 1 ] * b[ 0 ] );
}


/**
   Handy structure to hold a CGAL edge of a Triangulaion_3. It is
   unambiguous (meaning an edge has only one representant) and permits
   comparison (for use as index in std::map).
*/
struct VEdge {
public:
  Vertex_handle first;
  Vertex_handle second;
  inline VEdge( const Edge & e )
  {
    first = (e.first)->vertex( e.second );
    second = (e.first)->vertex( e.third );
    if ( second < first ) std::swap( first, second );
  }
  inline VEdge( Vertex_handle v1, Vertex_handle v2 )
  {
    if ( v1 < v2 ) { first = v1; second = v2; }
    else           { first = v2; second = v1; }
  }
  bool operator<( const VEdge & other ) const
  {
    return ( first < other.first )
      || ( ( first == other.first )
           && ( second < other.second ) );
  }
};

/**
   Handy structure to hold a CGAL facet of a Triangulaion_3. It is
   unambiguous (meaning a facet has only one representant) and permits
   comparison (for use as index in std::map).
*/
struct VFacet {
public:
  Vertex_handle first;
  Vertex_handle second;
  Vertex_handle third;
  inline VFacet( const Facet & f )
  {
    int i = f.second;
    first = (f.first)->vertex( (i+1)%4 );
    second = (f.first)->vertex( (i+2)%4 );
    third = (f.first)->vertex( (i+3)%4 );
    sort();
  }
  inline VFacet( Vertex_handle v1, Vertex_handle v2, Vertex_handle v3 )
    : first( v1 ), second( v2 ), third( v3 )
  {
    sort();
  }
  
  inline void sort()
  {
    if ( second < first ) std::swap( first, second );
    if ( third < first )  std::swap( first, third );
    if ( third < second ) std::swap( second, third );
  }
  bool operator<( const VFacet & other ) const
  {
    return ( first < other.first )
      || ( ( first == other.first )
	   && ( ( second < other.second ) 
		|| ( ( second == other.second )  
		     && ( third < other.third ) ) ) );
  }
};

struct OFacet {
  Facet facet;
  DGtal::int64_t crossNormL1;

  inline OFacet() : crossNormL1( 0 ) {}

  inline OFacet( const Facet & f )
    : facet( f ) 
  {
    computeCrossNormL1();
  }
  inline OFacet( const Facet & f, DGtal::int64_t nl1 )
    : facet( f ), crossNormL1( nl1 )
  {}

  inline OFacet( const OFacet & other )
    : facet( other.facet ), crossNormL1( other.crossNormL1 )
  {}

  inline
  OFacet & operator=( const OFacet & other )
  {
    if ( this != &other )
      {
	facet = other.facet;
	crossNormL1 = other.crossNormL1;
      }
    return *this;
  }

  void computeCrossNormL1()
  {
    VFacet vf( facet );
    Point a( toDGtal( vf.first->point() ) );
    Point b( toDGtal( vf.second->point() ) );
    Point c( toDGtal( vf.third->point() ) );
    Point n( (b-a)^(c-a) );
    crossNormL1 = n.norm( Point::L_1 );
  }
};

struct OFacetLessComparator{
  inline
  bool operator()( const OFacet & f1, const OFacet & f2 ) const 
  {
    return ( f1.crossNormL1 < f2.crossNormL1 )
      || ( ( f1.crossNormL1 == f2.crossNormL1 )
	   && ( f1.facet < f2.facet ) );
  }
};

typedef std::map< Cell_iterator, DGtal::int64_t > MapCell2Int;
typedef std::map< VFacet, DGtal::int64_t > MapFacet2Int;
typedef std::map< VEdge, DGtal::int64_t > MapEdge2Int;
typedef std::map< Vertex_iterator, DGtal::int64_t > MapVertex2Int;
typedef std::set< Facet > FacetSet;
typedef std::set< OFacet, OFacetLessComparator > OFacetSet;

DGtal::uint64_t 
markBasicEdges( MapEdge2Int & basicMap, const Delaunay & t )
{
  DGtal::uint64_t nb = 0;
  for( Edge_iterator it = t.edges_begin(), itend = t.edges_end();
       it != itend; ++it)
    {
      VEdge edge( *it );
      if ( ! t.is_infinite( *it ) )
        {
          Point a( toDGtal( edge.first->point() ) );
          Point b( toDGtal( edge.second->point() ) );
          if ( (b-a).norm( Point::L_infty ) == 1 )
            {
              basicMap[ edge ] = 1;
              ++nb;
            }
          else
            {
              basicMap[ edge ] = 0;
            }
        }
      else
        {
          basicMap[ edge ] = -1;
        }
    }
  return nb;
}

DGtal::uint64_t 
markBasicFacets( FacetSet & bFacets, OFacetSet & qFacets,
		 const Delaunay & t, MapEdge2Int & basicEdgeMap )
{
  DGtal::uint64_t nb = 0;
  for( Facet_iterator it = t.facets_begin(), itend = t.facets_end();
       it != itend; ++it)
    {
      VFacet f( *it ); 
      if ( ! t.is_infinite( *it ) )
        {
          Cell_iterator itCell = it->first; int i = it->second;
          VEdge e1( f.first, f.second );
          VEdge e2( f.second, f.third );
          VEdge e3( f.third, f.first );
          unsigned int n = 0;
          n += basicEdgeMap[ e1 ] == 1 ? 1 : 0;
          n += basicEdgeMap[ e2 ] == 1 ? 1 : 0;
          n += basicEdgeMap[ e3 ] == 1 ? 1 : 0;
          if ( n == 3 )
            {
	      OFacet f1( *it );
	      OFacet f2( t.mirror_facet( f1.facet ), f1.crossNormL1 );
	      bFacets.insert( f1.facet );
	      bFacets.insert( f2.facet );
	      qFacets.insert( f1 );
	      qFacets.insert( f2 );
              ++nb;
            }
        }
    }
  return nb;
}

DGtal::int64_t
countLatticePointsInTetrahedra( const Point & a, const Point & b, const Point & c, const Point & d )
{
  DGtal::int64_t nb = 0;
  Point ab = b - a;
  Point bc = c - b;
  Point cd = d - c;  
  Point da = a - d;
  Point abc = ab^bc;
  Point bcd = bc^cd;
  Point cda = cd^da;
  Point dab = da^ab;
  Point::Component abc_shift = abc.dot( a );
  Point::Component bcd_shift = bcd.dot( b );
  Point::Component cda_shift = cda.dot( c );
  Point::Component dab_shift = dab.dot( d );
  if ( abc.dot( d ) < abc_shift )  { abc = -abc; abc_shift = -abc_shift; }
  if ( bcd.dot( a ) < bcd_shift )  { bcd = -bcd; bcd_shift = -bcd_shift; }
  if ( cda.dot( b ) < cda_shift )  { cda = -cda; cda_shift = -cda_shift; }
  if ( dab.dot( c ) < dab_shift )  { dab = -dab; dab_shift = -dab_shift; }
  Point inf = a.inf( b ).inf( c ).inf( d );
  Point sup = a.sup( b ).sup( c ).sup( d );
  Domain domain( inf, sup );
  for ( DomainConstIterator it = domain.begin(), itE = domain.end();
	it != itE; ++it )
    {
      Point p = *it;
      if ( ( abc.dot( p ) >= abc_shift )
	   && ( bcd.dot( p ) >= bcd_shift )
	   && ( cda.dot( p ) >= cda_shift )
	   && ( dab.dot( p ) >= dab_shift ) )
	++nb;
    }
  return nb;
}

void 
getFacets( std::vector<Facet> & facets, const Cell_iterator & it )
{
  for ( int i = 0; i < 4; ++i )
    facets.push_back( Facet( it, i ) );
}

double
computeDihedralAngle( const Delaunay & t, const Facet & f1, const Facet & f2 )
{
  typedef CGAL::Vector_3<K> Vector;
  ASSERT( f1.first == f2.first );
  Vector n1 = t.triangle(f1).supporting_plane().orthogonal_vector();
  n1 = n1 / sqrt( n1.squared_length() );
  Vector n2 = t.triangle(f2).supporting_plane().orthogonal_vector();
  n2 = n2 / sqrt( n2.squared_length() );
  return acos( (double) (n1*n2) );
}

///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;

/**
   Main function.

   @param argc the number of parameters given on the line command.

   @param argv an array of C-string, such that argv[0] is the name of
   the program, argv[1] the first parameter, etc.

     "29*z+47*y+23*x-5"
  "10*z-x^2+y^2-100"
  "z*x*y+x^4-5*x^2+2*y^2*z-z^2-1000"
  "(15*z-x^2+y^2-100)*(x^2+y^2+z^2-1000)" nice
  "(x^2+y^2+(z+5)^2-100)^2+10*(x^2+y^2+(z-3)^2)-2000" bowl
  "x^2+y^2+2*z^2-x*y*z+z^3-100" dragonfly
  "(x^2+y^2+(z+5)^2)^2-x^2*(z^2-x^2-y^2)-100" joli coeur
  "0.5*(z^2-4*4)^2+(x^2-7*7)^2+(y^2-7*7)^2-7.2*7.2*7.2*7.2" convexites et concavites

*/
int main (int argc, char** argv )
{
  using namespace DGtal;

  typedef KhalimskySpaceND<3,DGtal::int64_t> K3;
  typedef Z3::Vector Vector;
  typedef Z3::RealPoint RealPoint;
  typedef K3::SCell SCell;
  typedef K3::SCellSet SCellSet;
  typedef SCellSet::const_iterator SCellSetConstIterator;
  // typedef ImplicitRoundedHyperCube<Z3> Shape;
  typedef RealPoint::Coordinate Ring;

  QApplication application(argc,argv); // remove Qt arguments.

  po::options_description general_opt("Specific allowed options (for Qt options, see Qt official site) are: ");
  general_opt.add_options()
    ("help,h", "display this message")
    ("vol,v", po::value<std::string>(), "specifies the shape as some subset of a .vol file [arg]" )
    ("min,m", po::value<int>()->default_value( 1 ), "the minimum threshold in the .vol file: voxel x in shape iff m < I(x) <= M" )
    ("max,M", po::value<int>()->default_value( 255 ), "the maximum threshold in the .vol file: voxel x in shape iff m < I(x) <= M" )
    ("poly,p", po::value<std::string>(), "specifies the shape as the zero-level of the multivariate polynomial [arg]" )
    ("gridstep,g", po::value<double>()->default_value( 1.0 ), "specifies the digitization grid step ([arg] is a double) when the shape is given as a polynomial." )
    ("bounds,b", po::value<int>()->default_value( 20 ), "specifies the diagonal integral bounds [-arg,-arg,-arg]x[arg,arg,arg] for the digitization of the polynomial surface." )
    ("prune,P", po::value<double>(), "prunes the resulting the complex of the tetrahedra were the length of the dubious edge is [arg] times the length of the opposite edge." );
    ; 
  
  // parse command line ----------------------------------------------
  bool parseOK=true;
  po::variables_map vm;
  try {
    po::command_line_parser clp( argc, argv );
    clp.options( general_opt );
    po::store( clp.run(), vm );
  } catch( const std::exception& ex ) {
    parseOK = false;
    trace.info() << "Error checking program options: "<< ex.what() << endl;
  }
  po::notify( vm );    
  if( !parseOK || vm.count("help")||argc<=1
      || !( vm.count("poly") || vm.count("vol") ) )
    {
      std::cout << "Usage: " << argv[0] << " [options] {--vol <vol-file> || --poly <polynomial-string>}\n"
		<< "Computes the linear reconstruction of the given digital surface, specified either as a thrsholded .vol file or a zero-level of a multivariate polynomial.\n"
		<< general_opt << "\n\n";
      std::cout << "Example:\n"
		<< argv[0] << " -p \"x^2+y^2+2*z^2-x*y*z+z^3-100\" -g " << (double) 0.5 << std::endl;
      return 0;
    }
  
  
  // process command line ----------------------------------------------
  trace.beginBlock("Construction of the shape");
  K3 ks;
  SCellSet boundary;
  SurfelAdjacency<3> sAdj( true );
  
  if ( vm.count( "vol" ) )
    { // .vol file
      typedef ImageSelector < Domain, int>::Type Image;
      typedef DigitalSetSelector< Domain, BIG_DS+HIGH_BEL_DS >::Type DigitalSet;
      std::string input = vm["vol"].as<std::string>();
      int minThreshold = vm["min"].as<int>();
      int maxThreshold = vm["max"].as<int>();
      trace.beginBlock( "Reading vol file into an image." );
      Image image = VolReader<Image>::importVol( input );
      DigitalSet set3d ( image.domain() );
      SetFromImage<DigitalSet>::append<Image>( set3d, image,
                                               minThreshold, maxThreshold );
      trace.endBlock();
      trace.beginBlock( "Creating space." );
      bool space_ok = ks.init( image.domain().lowerBound(),
                               image.domain().upperBound(), true );
      if (!space_ok)
        {
          trace.error() << "Error in the Khamisky space construction." << std::endl;
          return 2;
        }

      trace.endBlock();
      trace.beginBlock( "Extracting boundary by scanning the space. " );
      Surfaces<K3>::sMakeBoundary( boundary,
                                   ks, set3d,
                                   image.domain().lowerBound(),
                                   image.domain().upperBound() );
      trace.info() << "Digital surface has " << boundary.size() << " surfels."
                   << std::endl;
      trace.endBlock();
     }
  else if ( vm.count( "poly" ) )
    {
      typedef MPolynomial<3, Ring> Polynomial3;
      typedef MPolynomialReader<3, Ring> Polynomial3Reader;
      typedef ImplicitPolynomial3Shape<Z3> Shape;
      typedef GaussDigitizer<Z3,Shape> Digitizer;

      std::string poly_str = vm[ "poly" ].as<std::string>();
      double h = vm[ "gridstep" ].as<double>();
      int b = vm[ "bounds" ].as<int>();

      trace.beginBlock( "Reading polynomial." );
      Polynomial3 P;
      Polynomial3Reader reader;
      std::string::const_iterator iter 
        = reader.read( P, poly_str.begin(), poly_str.end() );
      if ( iter != poly_str.end() )
        {
          std::cerr << "ERROR: I read only <" 
                    << poly_str.substr( 0, iter - poly_str.begin() )
                    << ">, and I built P=" << P << std::endl;
          return 3;
        }
      trace.info() << "P( X_0, X_1, X_2 ) = " << P << std::endl;
      trace.endBlock();

      trace.beginBlock( "Extract polynomial by tracking." );
      Shape shape( P );
      Digitizer dig;
      dig.attach( shape );
      dig.init( Vector::diagonal( -b ),
                Vector::diagonal(  b ), h ); 
      ks.init( dig.getLowerBound(), dig.getUpperBound(), true );
      SCell bel = Surfaces<K3>::findABel( ks, dig, 100000 );
      trace.info() << "initial bel (Khalimsky coordinates): " << ks.sKCoords( bel ) << std::endl;
      Surfaces<K3>::trackBoundary( boundary, ks, sAdj, dig, bel );
      trace.info() << "Digital surface has " << boundary.size() << " surfels."
                   << std::endl;
      trace.endBlock();
    }
  else return 4; // should not happen

  trace.beginBlock("Delaunay tetrahedrization");
  trace.beginBlock("Getting coordinates.");
  std::set<SCell> inner_points;
  for( SCellSetConstIterator it = boundary.begin(), itE = boundary.end(); it != itE; ++it )
    // Get inner point.
    inner_points.insert( ks.sDirectIncident( *it, ks.sOrthDir( *it ) ) );
  trace.endBlock();
  trace.beginBlock("Shuffle points.");
  std::vector<SCell> shuffle_points;
  for ( std::set<SCell>::const_iterator it = inner_points.begin(), itE = inner_points.end();
	it != itE; ++it )
    shuffle_points.push_back( *it );
  random_shuffle( shuffle_points.begin(), shuffle_points.end() );
  trace.endBlock();


  trace.beginBlock("Creating the Delaunay complex.");
  Delaunay t;
  double setsize = (double) inner_points.size()-1;
  trace.info() << "Vertices to process: " << setsize << std::endl;
  double step = 0.0;
  for ( std::vector<SCell>::const_iterator it = shuffle_points.begin(), itE = shuffle_points.end();
	it != itE; ++it, ++step )
    {
      trace.progressBar( step, setsize );
      t.insert( toCGAL( ks.sCoords( *it ) ) );
    }
  // for ( std::set<SCell>::const_iterator it = inner_points.begin(), itE = inner_points.end();
  // 	it != itE; ++it, ++step )
  //   {
  //     trace.progressBar( step, setsize );
  //     t.insert( toCGAL( ks.sCoords( *it ) ) );
  //   }
  trace.endBlock();
  trace.endBlock();

  // start viewer
  Viewer3D viewer1, viewer2, viewer3;
  viewer1.show();
  viewer2.show();
  viewer3.show();
  // for ( SCellSetConstIterator it=boundary.begin(), itend=boundary.end(); it != itend; ++it )
  //    viewer1 << *it;
  
  trace.beginBlock("Counting lattice points.");
  MapCell2Int nbLatticePoints;
  for(Cell_iterator it = t.cells_begin(), itend=t.cells_end();
      it != itend; ++it)
    {
      if ( t.is_infinite( it ) ) 
        {
          nbLatticePoints[ it ] = -1;
        }
      else
        {
          Point a( toDGtal(it->vertex(0)->point())),
            b(toDGtal(it->vertex(1)->point())),
            c(toDGtal(it->vertex(2)->point())),
            d(toDGtal(it->vertex(3)->point()));
          nbLatticePoints[ it ] = countLatticePointsInTetrahedra( a, b, c, d );
        }
    }
  trace.endBlock();

  trace.beginBlock("Computing basic edges");
  MapEdge2Int bEdge;
  uint64_t nbBE = markBasicEdges( bEdge, t );
  trace.info() << "Nb basic edges :" << nbBE << std::endl;

  Color colBasicEdge( 0, 0, 255, 255 );
  for( Edge_iterator it = t.edges_begin(), itend = t.edges_end();
       it != itend; ++it)
    {
      VEdge e( *it );
      if ( bEdge[ e ] == 1 )
        {
          Cell_handle itC = it->first; 
          int i = it->second;
          int j = it->third;
          Point a( toDGtal(itC->vertex( i )->point()));
          Point b( toDGtal(itC->vertex( j )->point()));
	  viewer1.addLine( a[ 0 ], a[ 1 ], a[ 2 ],
                          b[ 0 ], b[ 1 ], b[ 2 ],
                          colBasicEdge, 1.0 );
        }
    }
  trace.endBlock();

  trace.beginBlock("Extracting Min-Polyhedron");
  std::set<Cell_handle> markedCells;
  FacetSet basicFacet;
  FacetSet markedFacet;
  OFacetSet priorityQ;
  FacetSet elementQ;
  std::set<Cell_handle> weirdCells;
  uint64_t nbBF = markBasicFacets( basicFacet, priorityQ, t, bEdge );
  trace.info() << "Nb basic facets:" << nbBF << std::endl;
  // At the beginning, the queue is equal to marked facets.
  markedFacet = basicFacet;
  elementQ = markedFacet;
  bool isMarked[ 4 ];
  while ( ! priorityQ.empty() )
    {
      OFacetSet::iterator it = priorityQ.begin();
      OFacet ofacet = *it;
      priorityQ.erase( it );
      elementQ.erase( ofacet.facet );
      // infinite cells are not processed.
      if ( t.is_infinite( ofacet.facet.first ) ) continue;
      std::vector<Facet> facets;
      getFacets( facets, ofacet.facet.first );
      bool found_neighbors_in_queue = false;
      for ( unsigned int i = 0; i < facets.size(); ++i )
        {
	  if ( elementQ.find( facets[ i ] ) != elementQ.end() )
	    {
	      found_neighbors_in_queue = true;
	      break;
	    }
	}
      // a later facet will take care of the possible fusion.
      if ( found_neighbors_in_queue ) continue;
      if ( nbLatticePoints[ ofacet.facet.first ] == 4 )
	{ // potential fusion
	  std::vector<unsigned int> f_marked;
	  std::vector<unsigned int> f_unmarked;
	  for ( unsigned int i = 0; i < facets.size(); ++i )
	    {
	      isMarked[ i ] = ( markedFacet.find( facets[ i ] ) != markedFacet.end() );
	      if ( isMarked[ i ] )  f_marked.push_back( i );
	      else                  f_unmarked.push_back( i );
	    }
	  unsigned int n = f_marked.size();
	  // At least 2 are marked, by convexity, we can close the gap
	  // to the further faces of the cell.
	  // std::cout << " " << n;
	  bool propagate = n >= 2;
	  if ( n == 2 )
	    {
	      // We must check that further tetrahedra do not contain integer points.
	      // Facet h0 = t.mirror_facet( facets[ f_marked[ 0 ] ] );
	      // Facet h1 = t.mirror_facet( facets[ f_marked[ 1 ] ] );
	      // std::cout << "(" << nbLatticePoints[ h0.first ] 
	      // 		<< "," << nbLatticePoints[ h1.first ] << ")";
	      weirdCells.insert( ofacet.facet.first );
	      // if ( ( nbLatticePoints[ h0.first ] != 4 )
	      // 	   && ( nbLatticePoints[ h1.first ] != 4 ) )
	      // 	propagate = false;
	    }
	  if ( propagate ) 
	    {
	      // marked cell as interior
	      // std::cout << "+";
	      markedCells.insert( ofacet.facet.first );
	      for ( unsigned int i = 0; i < facets.size(); ++i )
		{
		  if ( ! isMarked[ i ] )
		    {
		      OFacet nfacet1( facets[ i ] );
		      OFacet nfacet2( t.mirror_facet( nfacet1.facet ), nfacet1.crossNormL1 );
		      // markedFacet.insert( nfacet1.facet );
		      // put everything into queue.
		      priorityQ.insert( nfacet2 );
		      markedFacet.insert( nfacet2.facet );
		      elementQ.insert( nfacet2.facet );
		    }
		}
	    }
	}
    }
  std::cout << std::endl;
  trace.endBlock();

  trace.beginBlock("Prune weird cells");
  trace.info() << "weird cells: " << weirdCells.size() << std::endl;
  std::set<Cell_handle> removedWeirdCells;
  bool prune = vm.count( "prune" );
  if ( prune )
    {
      double factor = vm[ "prune" ].as<double>();
      double changed = true;
      while ( changed ) {
          changed = false;
          for ( std::set<Cell_handle>::const_iterator it = weirdCells.begin(), itend = weirdCells.end();
                it != itend; ++it )
            {
              if ( removedWeirdCells.find( *it ) == removedWeirdCells.end() ) 
                { 
                  std::vector<Facet> facets;
                  getFacets( facets, *it );
                  std::vector<unsigned int> f_exterior;
                  std::vector<unsigned int> f_interior;
                  std::vector<unsigned int> f_basic;
                  for ( unsigned int i = 0; i < facets.size(); ++i )
                    { // count exterior facets
                      Facet m = t.mirror_facet( facets[ i ] );
                      if ( ( markedFacet.find( m ) != markedFacet.end() )
                           && ( markedCells.find( m.first ) == markedCells.end() ) )
                        f_exterior.push_back( i );
                      else if ( markedFacet.find( facets[ i ] ) != markedFacet.end() )
                        f_interior.push_back( i );
                      if ( basicFacet.find( m ) != basicFacet.end() )
                        f_basic.push_back( i );
                    }
                  if ( ( f_exterior.size() >= 3 ) ) //&& ( f_interior.size() == 2 ) )
                    {
                      //if ( f_basic.size() == 0 )
                        std::cerr << "Weird !"
                                  << " ext=" << f_exterior.size()
                                  << " int=" << f_interior.size()
                                  << " bas=" << f_basic.size()
                                  << std::endl;
                      // while ( f_basic.empty() )
                      //   {
                      //     f_exterior.erase
                      //   }
                    }
                  if ( ( f_exterior.size() == 2 ) && ( f_interior.size() == 2 ) )
                    {
                      // (i,j) is the edge common to the two other faces.
                      int i = facets[ f_exterior[ 0 ] ].second; 
                      int j = facets[ f_exterior[ 1 ] ].second;
                      // (k,l) is the edge common to the two exterior faces.
                      int k = ( (i+1)%4 == j ) ? (i+2)%4 : (i+1)%4;
                      int l = (k+1)%4;
                      for ( ; (l == i ) || ( l == j ); l = (l+1)%4 ) ;
                      Point A( toDGtal( (*it )->vertex( i )->point() ) );
                      Point B( toDGtal( (*it )->vertex( j )->point() ) );
                      Point C( toDGtal( (*it )->vertex( k )->point() ) );
                      Point D( toDGtal( (*it )->vertex( l )->point() ) );

                      double angle = fabs( M_PI - computeDihedralAngle( t, facets[ f_exterior[ 0 ] ], 
                                                                        facets[ f_exterior[ 1 ] ] ) );
                      if ( angle < factor )
                        // if ( (C-D).norm( Point::L_1 ) > factor*(A-B).norm( Point::L_1 ) )
                        { // preference to shorter edge.
                          // std::cout << "(" << f_exterior.size() << "," << f_interior.size() << std::endl;
                          markedFacet.erase( t.mirror_facet( facets[ f_exterior[ 0 ] ] ) );
                          markedFacet.erase( t.mirror_facet( facets[ f_exterior[ 1 ] ] ) );
                          markedCells.erase( *it );
                          removedWeirdCells.insert( *it );
                          markedFacet.insert( facets[ f_interior[ 0 ] ] );
                          markedFacet.insert( facets[ f_interior[ 1 ] ] );
                          changed = true;
                        }
                    }
                }
            }
      } //  while ( changed ) {

    }
  trace.info() << "weird cells removed : " << removedWeirdCells.size() << std::endl;
  trace.endBlock();

  Color colBasicFacet2( 0, 255, 255, 255 );
  Color colBasicFacet1( 0, 255, 0, 255 );
  for ( FacetSet::const_iterator it = markedFacet.begin(), itend = markedFacet.end();
  	it != itend; ++it )
    {
      Cell_handle cell = it->first;
      if ( markedCells.find( cell ) == markedCells.end() )
  	{ // we display it.
	  Triangle triangle = t.triangle( *it );
	  Point a( toDGtal( triangle.vertex( 0 ) ) );
	  Point b( toDGtal( triangle.vertex( 1 ) ) );
	  Point c( toDGtal( triangle.vertex( 2 ) ) );
  	  // int i = it->second;
  	  // Point a( toDGtal( cell->vertex( (i+1)%4 )->point() ) );
  	  // Point b( toDGtal( cell->vertex( (i+2)%4 )->point() ) );
  	  // Point c( toDGtal( cell->vertex( (i+3)%4 )->point() ) );
	  Facet f2 = t.mirror_facet( *it );
	  if ( ( markedFacet.find( f2 ) != markedFacet.end() )
	       && ( markedCells.find( f2.first ) == markedCells.end() ) )
	    { // the mirror facet is also in the triangulation. 
	      // We need to move vertices a little bit when two triangles are at the same position.
	      Point n = (b-a)^(c-a);
	      double norm = n.norm(Point::L_2);
	      double dx[ 3 ];
	      for ( unsigned int j = 0; j < 3; ++j )
		dx[ j ] = 0.001*((double) n[j])/norm;
	      viewer2.addTriangle( (double) a[ 0 ] + dx[ 0 ], (double) a[ 1 ] +  dx[ 1 ], (double) a[ 2 ] + dx[ 2 ],
				   (double) c[ 0 ] + dx[ 0 ], (double) c[ 1 ] +  dx[ 1 ], (double) c[ 2 ] + dx[ 2 ],
				   (double) b[ 0 ] + dx[ 0 ], (double) b[ 1 ] +  dx[ 1 ], (double) b[ 2 ] + dx[ 2 ],
				   colBasicFacet2 );
	    }
	  else
	    viewer2.addTriangle( a[ 0 ], a[ 1 ], a[ 2 ],
				 c[ 0 ], c[ 1 ], c[ 2 ],
				 b[ 0 ], b[ 1 ], b[ 2 ],
				 colBasicFacet1 );
  	}
    }

  //////////////////////////////////////////////////////////////////////
  for ( Cell_iterator it = t.cells_begin(), itend = t.cells_end();
        it != itend; ++it )
    {
      bool draw = false;
      Color col;
      if ( weirdCells.find( it ) != weirdCells.end() )
        {
          col = Color( 255, 100, 255, 100 );
          draw = true;
        }
      if ( prune && ( removedWeirdCells.find( it ) != removedWeirdCells.end() ) )
        {
          col = Color( 255, 255, 0, 100 );
          draw = true;
        }
      else
        if ( ( ! t.is_infinite( it ) )
		&& ( nbLatticePoints[ it ] == 4 )
		&& ( markedCells.find( it ) == markedCells.end() ) )
	{
	  draw = true;
	  col = Color( 255, 0, 0, 100 );
	}
        //if ( n > 4 ) trace.info() << " " << n;
      // if ( n <= 3 ) trace.error() << " Not enough lattice points" << std::endl;
      if ( draw )
        {
          Point a( toDGtal(it->vertex(0)->point())),
            b(toDGtal(it->vertex(1)->point())),
            c(toDGtal(it->vertex(2)->point())),
            d(toDGtal(it->vertex(3)->point()));
          viewer1.addTriangle( a[ 0 ], a[ 1 ], a[ 2 ],
        		      b[ 0 ], b[ 1 ], b[ 2 ],
        		      c[ 0 ], c[ 1 ], c[ 2 ],
        		      col );
          viewer1.addTriangle( c[ 0 ], c[ 1 ], c[ 2 ],
        		      b[ 0 ], b[ 1 ], b[ 2 ],
        		      d[ 0 ], d[ 1 ], d[ 2 ], 
        		      col );
          viewer1.addTriangle( c[ 0 ], c[ 1 ], c[ 2 ],
        		      d[ 0 ], d[ 1 ], d[ 2 ], 
        		      a[ 0 ], a[ 1 ], a[ 2 ],
        		      col );
          viewer1.addTriangle( d[ 0 ], d[ 1 ], d[ 2 ], 
        		      b[ 0 ], b[ 1 ], b[ 2 ],
        		      a[ 0 ], a[ 1 ], a[ 2 ],
        		      col );
        }
    }

  std::cout << "number of vertices :  " ;
  std::cout << t.number_of_vertices() << std::endl;
  std::cout << "number of edges :  " ;
  std::cout << t.number_of_edges() << std::endl;
  std::cout << "number of facets :  " ;
  std::cout << t.number_of_facets() << std::endl;
  std::cout << "number of cells :  " ;
  std::cout << t.number_of_cells() << std::endl;

  for ( SCellSetConstIterator it=boundary.begin(), itend=boundary.end(); it != itend; ++it )
    viewer3 << ks.sDirectIncident( *it, ks.sOrthDir( *it ) );

  viewer1 << Viewer3D::updateDisplay;
  viewer2 << Viewer3D::updateDisplay;
  viewer3 << Viewer3D::updateDisplay;
  application.exec();
  
  return 0;
}
