#include "Astar_searcher.h"

using namespace std;
using namespace Eigen;

void AstarPathFinder::initGridMap(double _resolution, Vector3d global_xyz_l, Vector3d global_xyz_u, int max_x_id, int max_y_id, int max_z_id)
{   
    gl_xl = global_xyz_l(0);
    gl_yl = global_xyz_l(1);
    gl_zl = global_xyz_l(2);

    gl_xu = global_xyz_u(0);
    gl_yu = global_xyz_u(1);
    gl_zu = global_xyz_u(2);
    
    GLX_SIZE = max_x_id;
    GLY_SIZE = max_y_id;
    GLZ_SIZE = max_z_id;
    GLYZ_SIZE  = GLY_SIZE * GLZ_SIZE;
    GLXYZ_SIZE = GLX_SIZE * GLYZ_SIZE;

    resolution = _resolution;
    inv_resolution = 1.0 / _resolution;    

    data = new uint8_t[GLXYZ_SIZE];
    memset(data, 0, GLXYZ_SIZE * sizeof(uint8_t));
    
    GridNodeMap = new GridNodePtr ** [GLX_SIZE];
    for(int i = 0; i < GLX_SIZE; i++){
        GridNodeMap[i] = new GridNodePtr * [GLY_SIZE];
        for(int j = 0; j < GLY_SIZE; j++){
            GridNodeMap[i][j] = new GridNodePtr [GLZ_SIZE];
            for( int k = 0; k < GLZ_SIZE;k++){
                Vector3i tmpIdx(i,j,k);
                Vector3d pos = gridIndex2coord(tmpIdx);
                GridNodeMap[i][j][k] = new GridNode(tmpIdx, pos);
            }
        }
    }
}

void AstarPathFinder::resetGrid(GridNodePtr ptr)
{
    ptr->id = 0;
    ptr->cameFrom = NULL;
    ptr->gScore = inf;
    ptr->fScore = inf;
}

void AstarPathFinder::resetUsedGrids()
{   
    for(int i=0; i < GLX_SIZE ; i++)
        for(int j=0; j < GLY_SIZE ; j++)
            for(int k=0; k < GLZ_SIZE ; k++)
                resetGrid(GridNodeMap[i][j][k]);
}

void AstarPathFinder::setObs(const double coord_x, const double coord_y, const double coord_z)
{
    if( coord_x < gl_xl  || coord_y < gl_yl  || coord_z <  gl_zl || 
        coord_x >= gl_xu || coord_y >= gl_yu || coord_z >= gl_zu )
        return;

    int idx_x = static_cast<int>( (coord_x - gl_xl) * inv_resolution);
    int idx_y = static_cast<int>( (coord_y - gl_yl) * inv_resolution);
    int idx_z = static_cast<int>( (coord_z - gl_zl) * inv_resolution);      

    data[idx_x * GLYZ_SIZE + idx_y * GLZ_SIZE + idx_z] = 1;
}

vector<Vector3d> AstarPathFinder::getVisitedNodes()
{   
    vector<Vector3d> visited_nodes;
    for(int i = 0; i < GLX_SIZE; i++)
        for(int j = 0; j < GLY_SIZE; j++)
            for(int k = 0; k < GLZ_SIZE; k++){   
                //if(GridNodeMap[i][j][k]->id != 0) // visualize all nodes in open and close list
                if(GridNodeMap[i][j][k]->id == -1)  // visualize nodes in close list only
                    visited_nodes.push_back(GridNodeMap[i][j][k]->coord);
            }

    ROS_WARN("visited_nodes size : %d", visited_nodes.size());
    return visited_nodes;
}

Vector3d AstarPathFinder::gridIndex2coord(const Vector3i & index) 
{
    Vector3d pt;

    pt(0) = ((double)index(0) + 0.5) * resolution + gl_xl;
    pt(1) = ((double)index(1) + 0.5) * resolution + gl_yl;
    pt(2) = ((double)index(2) + 0.5) * resolution + gl_zl;

    return pt;
}

Vector3i AstarPathFinder::coord2gridIndex(const Vector3d & pt) 
{
    Vector3i idx;
    idx <<  min( max( int( (pt(0) - gl_xl) * inv_resolution), 0), GLX_SIZE - 1),
            min( max( int( (pt(1) - gl_yl) * inv_resolution), 0), GLY_SIZE - 1),
            min( max( int( (pt(2) - gl_zl) * inv_resolution), 0), GLZ_SIZE - 1);                  
  
    return idx;
}

Eigen::Vector3d AstarPathFinder::coordRounding(const Eigen::Vector3d & coord)
{
    return gridIndex2coord(coord2gridIndex(coord));
}

inline bool AstarPathFinder::isOccupied(const Eigen::Vector3i & index) const
{
    return isOccupied(index(0), index(1), index(2));
}

inline bool AstarPathFinder::isFree(const Eigen::Vector3i & index) const
{
    return isFree(index(0), index(1), index(2));
}

inline bool AstarPathFinder::isOccupied(const int & idx_x, const int & idx_y, const int & idx_z) const 
{
    return  (idx_x >= 0 && idx_x < GLX_SIZE && idx_y >= 0 && idx_y < GLY_SIZE && idx_z >= 0 && idx_z < GLZ_SIZE && 
            (data[idx_x * GLYZ_SIZE + idx_y * GLZ_SIZE + idx_z] == 1));
}

inline bool AstarPathFinder::isFree(const int & idx_x, const int & idx_y, const int & idx_z) const 
{
    return (idx_x >= 0 && idx_x < GLX_SIZE && idx_y >= 0 && idx_y < GLY_SIZE && idx_z >= 0 && idx_z < GLZ_SIZE && 
           (data[idx_x * GLYZ_SIZE + idx_y * GLZ_SIZE + idx_z] < 1));
}

inline void AstarPathFinder::AstarGetSucc(GridNodePtr currentPtr, vector<GridNodePtr> & neighborPtrSets, vector<double> & edgeCostSets)
{   
    neighborPtrSets.clear();
    edgeCostSets.clear();
    
    // Find all neighbor points
    for(int delta_x=-1;delta_x<2;++delta_x){
		for(int delta_y=-1;delta_y<2;++delta_y){
			for(int delta_z=-1;delta_z<2;++delta_z){
				if (delta_x !=0 || delta_y !=0 || delta_z!=0){
					Vector3i neighbor_idx = currentPtr -> index;
					neighbor_idx(0) +=  delta_x;
					neighbor_idx(1) +=  delta_y;
					neighbor_idx(2) +=  delta_z;
				
					if((isFree(neighbor_idx))){
						if (GridNodeMap[neighbor_idx(0)][neighbor_idx(1)][neighbor_idx(2)] -> id != -1){
							// put the node into the neighbor set
							neighborPtrSets.push_back(GridNodeMap[neighbor_idx(0)][neighbor_idx(1)][neighbor_idx(2)]);
							// define the cost
							edgeCostSets.push_back(sqrt(delta_x*delta_x + delta_y*delta_y + delta_z*delta_z));
						}
					}
			
			    }
			}    
		}
	}
       
}


double AstarPathFinder::getHeu(GridNodePtr node1, GridNodePtr node2, const string heuOption)
{
    /* 
    choose possible heuristic function you want
    Manhattan, Euclidean, Diagonal, or 0 (Dijkstra)
    Remember tie_breaker learned in lecture, add it here ?
    */
	double h = 0;
	double dx, dy, dz;
	dx = abs(node1->index(0) - node2->index(0));
	dy = abs(node1->index(1) - node2->index(1));
	dz = abs(node1->index(2) - node2->index(2));
    
    if (heuOption == "Diagonal"){
		// Use Diagonal as the heuristic function
		double minIndex = min(dx, min(dy, dz));
		double medIndex = max(min(dx,dy), min(max(dx,dy),dz));
		h = dx + dy + dz - (3-sqrt(3))*minIndex - (2-sqrt(2))*(medIndex - minIndex);
	}
	else if (heuOption == "Manhattan"){
		// Use Manhattan as the heuristic function
		h = dx + dy + dz;
	}
    else if (heuOption == "Euclidean"){
		// Use Euclidean as the heuristic function
		h = sqrt(dx*dx + dy*dy + dz*dz);
	}
   else if (heuOption == "Dijkstra"){
		h = 0;
	}
	
   return h + h * 0.05;;
}
	
void AstarPathFinder::AstarGraphSearch(Vector3d start_pt, Vector3d end_pt, const string heuOption)
{   
    ros::Time time_1 = ros::Time::now();    

    //index of start_point and end_point
    Vector3i start_idx = coord2gridIndex(start_pt);
    Vector3i end_idx   = coord2gridIndex(end_pt);
    goalIdx = end_idx;

    //position of start_point and end_point
    start_pt = gridIndex2coord(start_idx);
    end_pt   = gridIndex2coord(end_idx);

    //Initialize the pointers of struct GridNode which represent start node and goal node
    GridNodePtr startPtr = new GridNode(start_idx, start_pt);
    GridNodePtr endPtr   = new GridNode(end_idx,   end_pt);

    //openSet is the open_list implemented through multimap in STL library
    openSet.clear();
    // currentPtr represents the node with lowest f(n) in the open_list
    GridNodePtr currentPtr  = NULL;
    GridNodePtr neighborPtr = NULL;

    //put start node in open set
    startPtr -> gScore = 0;
    startPtr -> fScore = getHeu(startPtr,endPtr,heuOption);   
    startPtr -> id = 1; 
    startPtr -> coord = start_pt;
    openSet.insert( make_pair(startPtr -> fScore, startPtr));
	GridNodeMap[start_idx(0)][start_idx(1)][start_idx(2)]->id = 1;
	
    vector<GridNodePtr> neighborPtrSets;
    vector<double> edgeCostSets;

    // this is the main loop
    while ( !openSet.empty() ){
		
		currentPtr = openSet.begin() -> second; 
		currentPtr -> id = -1;
		openSet.erase(openSet.begin());

        // if the current node is the goal, break loop
        if( currentPtr->index == goalIdx ){
            ros::Time time_2 = ros::Time::now();
            terminatePtr = currentPtr;
            ROS_WARN("[A*]{sucess}  Time in A*  is %f ms, path cost if %f m, heuristic function is %s", (time_2 - time_1).toSec() * 1000.0, currentPtr->gScore * resolution, heuOption.c_str());            
            return;
        }
        //get the succetion
        AstarGetSucc(currentPtr, neighborPtrSets, edgeCostSets);   
		//ROS_INFO("AstarPathFinder: get successor !");
        for(int i = 0; i < (int)neighborPtrSets.size(); ++i){
			
			neighborPtr = neighborPtrSets[i];
            // discover a new node, which is not in the closed set and open set           
            if(neighborPtr -> id == 0){ 
 
                neighborPtr -> gScore = currentPtr-> gScore +  edgeCostSets[i];
                neighborPtr -> fScore = neighborPtr -> gScore + getHeu(neighborPtr,endPtr,heuOption);
                neighborPtr -> cameFrom = currentPtr;
                openSet.insert(make_pair(neighborPtr -> fScore, neighborPtr));
                neighborPtr -> id = 1;
                continue; 
            }
            // this node is in open set and need to judge if it needs to update
            else if(neighborPtr ->id == 1){ 
                if (neighborPtr -> gScore > (currentPtr-> gScore +  edgeCostSets[i]))
				{
					// update costs and parent node
					neighborPtr -> gScore = (currentPtr-> gScore +  edgeCostSets[i]);
					neighborPtr -> fScore = neighborPtr -> gScore + getHeu(neighborPtr,endPtr,heuOption); 
					neighborPtr -> cameFrom = currentPtr;
					continue;
				}                 
            }
            else {
				ROS_WARN ("Neighbor node is in closed set, check the code!");
                continue;}           
        } 
    } 
    
          
    //if search fails
    ros::Time time_2 = ros::Time::now();
    if((time_2 - time_1).toSec() > 0.1)
        ROS_WARN("Time consume in Astar path finding is %f", (time_2 - time_1).toSec());
}


vector<Vector3d> AstarPathFinder::getPath() 
{   
    vector<Vector3d> path;
    vector<GridNodePtr> gridPath;
    
    GridNodePtr stopPtr = terminatePtr;
    
    while(stopPtr -> gScore != 0){
		gridPath.push_back(stopPtr);
		stopPtr = stopPtr -> cameFrom;		
		}	

    for (auto ptr: gridPath) 
        path.push_back(ptr->coord);
        
    reverse(path.begin(),path.end());

    return path;
}
