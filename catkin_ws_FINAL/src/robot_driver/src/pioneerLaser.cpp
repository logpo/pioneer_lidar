#include "ros/ros.h" 
#include "geometry_msgs/Twist.h" 
#include "sensor_msgs/LaserScan.h"
#include <iostream>
#include <string>
#include <std_msgs/String.h>

//Initialises messages to be accessable globally
geometry_msgs::Twist velocityCommand;
std_msgs::String stateName; 

using namespace std;

void laserScanCallback(const sensor_msgs::LaserScan::ConstPtr& laserScanData) 
{ 
	// Example of using some of the non-range data-types
	float rangeDataNum = 1 + (laserScanData->angle_max - laserScanData->angle_min)/laserScanData->angle_increment;
	//Give the robot something to do if nothing is detected around it 
	velocityCommand.linear.x = 0.2;
	velocityCommand.angular.z = 0.0;

	//Initialise the values to track where the pole at the end of the row is
	float startpole = 0, endpole = 0, lastval = laserScanData->ranges[(rangeDataNum / 2) - 1];	
	int j = (rangeDataNum / 2), startpolesample = 0, endpolesample = 0;
	//Loop from the centre around to the right until a pole is fully located
	while( j > 0 && endpole == 0  )
	{	
		//Checks for a major change in lidar reading indicating the start or end of a pole
		if( 0 < laserScanData->ranges[j] && laserScanData->ranges[j] < (0.9 * lastval) && startpole == 0)
		{
			//Store the location of the start of the pole
			startpole = laserScanData->ranges[j];
			startpolesample = j;
			if( j < 25 && stateName.data != "Changing Rows" )
			{
				//If the pole is in line with the robot it should begin changing rows
				stateName.data = "Changing Rows";
				velocityCommand.linear.x = 0.1;
				velocityCommand.angular.z = -0.25;
			}
			else if( j > 40 && stateName.data == "Changing Rows" )
			{
				//Ensures the robot continues changing rows if it cannot see the pole next to it
				velocityCommand.linear.x = 0.1;
				velocityCommand.angular.z = -0.25;
			}
		} 
		else if(laserScanData->ranges[j] < (0.9 * lastval) || laserScanData->ranges[j] > (1.1 * lastval))
		{
			//Store the location of the end of the pole
			endpole = laserScanData->ranges[j];
			endpolesample = j;
		}
		//Record the current lidar value being checked and increment the loop
		lastval = laserScanData->ranges[j];
		--j;
	}

	//Carries out the row change operation
	if(stateName.data == "Changing Rows")
	{
		//Looks at the lidar readings straight ahead of the robot
		j = (rangeDataNum / 3);
		while((j < ((2 * rangeDataNum) / 3)) && stateName.data != "Completed Row Change")
		{
			//Checks the reading is valid and that the robot is not in the next row
			if( 0 < laserScanData->ranges[j] && laserScanData->ranges[j] < 2.0 )
			{
				//Sets trajectory to turn into the next row
				velocityCommand.linear.x = 0.1;
				velocityCommand.angular.z = -0.25;
				break;
			}
			else
			{
				//Leaves the row change state
				stateName.data = "Completed Row Change";
				velocityCommand.angular.z = 0;
				break;
			}
			++j;
		}
	}

	// Go through the laser data only occurs when traversing a row
	j = 0; 
	while(j < rangeDataNum && stateName.data != "Changing Rows")
	{
		//Checks if the row on the left and right are within 0.4m of the robot centre
		if( 0 < laserScanData->ranges[j] && laserScanData->ranges[j] < 0.4 && j < rangeDataNum / 3)
		{
			stateName.data = "Avoiding Row Edges";
			//Avoid obstacle to the left
			velocityCommand.linear.x = 0.1;
			velocityCommand.angular.z = 0.2;
			break;
		} 
		else if( 0 < laserScanData->ranges[j] && laserScanData->ranges[j] < 0.4 && j >= ((2 * rangeDataNum) / 3))
		{
			stateName.data = "Avoiding Row Edges";
			//Avoid obstacle to the right
			velocityCommand.linear.x = 0.1;
			velocityCommand.angular.z = -0.2;
			break;
		}
		++j;
	}

	//Search through the middle third of the laser data
	j = rangeDataNum / 4;
	while(j < ((3 * rangeDataNum) / 4))
	{
		//Checks the reading is valid and if there is an object directly in front of the robot
		if(0 < laserScanData->ranges[j] && laserScanData->ranges[j] < 0.2 )
		{
			stateName.data = "Avoiding Unexpected Object";
			//Avoid obstacle crashing into obstacle
			velocityCommand.linear.x = 0;
			velocityCommand.angular.z = 0;
		}
		++j;
	}
}

int main (int argc, char **argv) 
{ 
	//Keeps track of whether the robot is currently avoiding an unexpected obstacle and the last message given to the user
	stateName.data = "Beginning Mapping";
	string lastmessage = " ";

	//Initialises variable to control how often messages are displayed to the command line
	int cmdlinemessagefreq = 0;	
	
	// command line ROS arguments
	ros::init(argc, argv, "pioneer_laser_node"); 
	ros::init(argc, argv, "displayer");

	// ROS comms access point 
	ros::NodeHandle my_handle; 

	//Tells the master which topic the vel message will be published to
	//Sets the queue length to 1 (no queue)
	ros::Publisher vel_pub_object = my_handle.advertise<geometry_msgs::Twist>("/RosAria/cmd_vel",1);

	//Tells the master that state message will be published to topic state_name
	//Sets the queue length of state messages to 2
	ros::Publisher state_pub_object = my_handle.advertise<std_msgs::String>("/RosAria/state_name",2);

	// subscribe to the scan topic and define a callback function to process the data
	ros::Subscriber laser_sub_object = my_handle.subscribe("/scan", 1, laserScanCallback);

	// loop 10 Hz 
	ros::Rate loop_rate(10); 

	// publish the velocity set in the call back
	while(ros::ok())
	{ 
		//Controls how often messages are displayed to the command line
		//Updates limited to 1 per second
		if (cmdlinemessagefreq >= 10 && stateName.data.c_str() != lastmessage)
		{
			//Outputs a message to the user and records that a message has been sent
			ROS_INFO("%s", stateName.data.c_str());
			cmdlinemessagefreq = 0;	
			lastmessage = stateName.data.c_str();
		}
		//Increments count of when the last message was outputted
		cmdlinemessagefreq++;

		ros::spinOnce();
		loop_rate.sleep(); 

		// publish
		vel_pub_object.publish(velocityCommand);
		state_pub_object.publish(stateName);
	} 

	return 0; 
}
