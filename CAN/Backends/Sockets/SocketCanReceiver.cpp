/*
 * SocketCanReceiver.cpp
 *
 *  Created on: May 10, 2018
 *      Author: fernado
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/net_tstamp.h>


#include <Backends/Sockets/SocketCanReceiver.h>

using namespace Utils;


namespace Can {
namespace Sockets {

SocketCanReceiver::SocketCanReceiver(int sock, bool timeStamp) : mSock(sock), mTimeStamp(timeStamp) {


	iov.iov_base = &frame;
	msg.msg_name = &addr;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &ctrlmsg;

}

SocketCanReceiver::~SocketCanReceiver() {
}


bool SocketCanReceiver::setFilters(std::set<CanFilter> filters) {

	bool retVal;

	can_filter *rfilters, *filterPtr;

	if(mSock == -1)			return false;								//Socket is not open... nothing todo

	if(filters.empty())		return false;								//No filters specified

	filterPtr = rfilters = new can_filter[filters.size()]; 				//Allocate filters according to the number of them

	for(auto iter = filters.begin(); iter != filters.end(); ++iter) {

		filterPtr->can_id = iter->getId() & CAN_EFF_MASK;
		filterPtr->can_mask = iter->getMask() & CAN_EFF_MASK;

		if(iter->filterStdFrame() == iter->filterExtFrame()) {			//If none of them are filtered or both are filtered, remove the extended frame flag from the mask

			filterPtr->can_mask &= ~CAN_EFF_FLAG;

		} else {

			filterPtr->can_mask |= CAN_EFF_FLAG;						//Set the flag in the mask to check if it is a standard frame or extended frame

			if(iter->filterExtFrame()) {

				filterPtr->can_id |= CAN_EFF_FLAG;						//If it is extended, we set the EFF flag
			} else {
				filterPtr->can_mask &= CAN_SFF_MASK;					//If it is standard, we set to 0 the unnecessary bits from the id (only 11 bits)
			}

		}

		++filterPtr;

	}

	retVal = (setsockopt(mSock, SOL_CAN_RAW, CAN_RAW_FILTER,
			rfilters, filters.size() * sizeof(can_filter)) == 0);

	delete[] rfilters;		//Deallocate filters

	return retVal;

}

bool SocketCanReceiver::receive(CanFrame& canFrame, TimeStamp& timestamp) {

	cmsghdr *cmsg;
	int nbytes;

	iov.iov_len = sizeof(frame);
	msg.msg_namelen = sizeof(addr);
	msg.msg_controllen = sizeof(ctrlmsg);
	msg.msg_flags = 0;


	nbytes = recvmsg(mSock, &msg, 0);

	if(nbytes >= 0) {

		if(mTimeStamp) {			//Timestamp option is enabled

			//Extract timestamp
			for (cmsg = CMSG_FIRSTHDR(&msg);
				 cmsg && (cmsg->cmsg_level == SOL_SOCKET);
				 cmsg = CMSG_NXTHDR(&msg,cmsg)) {
				if (cmsg->cmsg_type == SO_TIMESTAMP) {

					timeval *stamp = (timeval*)(CMSG_DATA(cmsg));

					timestamp.setMicroSec(stamp->tv_usec);
					timestamp.setSeconds(stamp->tv_sec);

				} else if (cmsg->cmsg_type == SO_TIMESTAMPING) {

					timespec *stamp = (struct timespec *)CMSG_DATA(cmsg);

					//Take timestamp from software

					timestamp.setSeconds(stamp[0].tv_sec);
					timestamp.setMicroSec(stamp[0].tv_nsec/1000);

				}
			}

		}


		//Copy Frame
		canFrame.setExtendedFormat(frame.can_id & CAN_EFF_FLAG);
		canFrame.setId(frame.can_id & ~CAN_EFF_FLAG);

		std::string data;
		data.append((char*)(frame.data), frame.len);


		canFrame.setData(data);


	}

	return true;

}


int SocketCanReceiver::getFD() {

	return mSock;

}

} /* namespace Sockets */
} /* namespace Can */
