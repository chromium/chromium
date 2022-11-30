// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_METRICS_H_
#define COMPONENTS_CRONET_IOS_CRONET_METRICS_H_

#import <Foundation/Foundation.h>

#include "components/grpc_support/include/bidirectional_stream_c.h"
#import "ios/net/crn_http_protocol_handler.h"
#include "net/http/http_network_session.h"

// These are internal versions of NSURLSessionTaskTransactionMetrics and
// NSURLSessionTaskMetrics, defined primarily so that Cronet can
// initialize them and set their properties (the iOS classes are readonly).

// The correspondences are
//   CronetTransactionMetrics -> NSURLSessionTaskTransactionMetrics
//   CronetMetrics -> NSURLSessionTaskMetrics

FOUNDATION_EXPORT GRPC_SUPPORT_EXPORT NS_AVAILABLE_IOS(10.0)
@interface CronetTransactionMetrics : NSURLSessionTaskTransactionMetrics

// All of the below redefined as readwrite.

// This is set to [task currentRequest].
@property(copy, readwrite) NSURLRequest* request;
// This is set to [task response].
@property(copy, readwrite) NSURLResponse* response;

// This is set to net::LoadTimingInfo::request_start_time.
@property(copy, readwrite) NSDate* fetchStartDate;
// This is set to net::LoadTimingInfo::ConnectTiming::dns_start.
@property(copy, readwrite) NSDate* domainLookupStartDate;
// This is set to net::LoadTimingInfo::ConnectTiming::dns_end.
@property(copy, readwrite) NSDate* domainLookupEndDate;
// This is set to net::LoadTimingInfo::ConnectTiming::connect_start.
@property(copy, readwrite) NSDate* connectStartDate;
// This is set to net::LoadTimingInfo::ConnectTiming::ssl_start.
@property(copy, readwrite) NSDate* secureConnectionStartDate;
// This is set to net::LoadTimingInfo::ConnectTiming::ssl_end.
@property(copy, readwrite) NSDate* secureConnectionEndDate;
// This is set to net::LoadTimingInfo::ConnectTiming::connect_end.
@property(copy, readwrite) NSDate* connectEndDate;
// This is set to net::LoadTimingInfo::sent_start.
@property(copy, readwrite) NSDate* requestStartDate;
// This is set to net::LoadTimingInfo::send_end.
@property(copy, readwrite) NSDate* requestEndDate;
// This is set to net::LoadTimingInfo::receive_headers_end.
@property(copy, readwrite) NSDate* responseStartDate;
// This is set to net::MetricsDelegate::Metrics::response_end_time.
@property(copy, readwrite) NSDate* responseEndDate;

// This is set to net::HttpResponseInfo::connection_info.
@property(copy, readwrite) NSString* networkProtocolName;
// This is set to YES if net::HttpResponseInfo::proxy_server.is_direct()
// returns false.
@property(assign, readwrite, getter=isProxyConnection) BOOL proxyConnection;
// This is set to YES if net::LoadTimingInfo::ConnectTiming::conect_start is
// null.
@property(assign, readwrite, getter=isReusedConnection) BOOL reusedConnection;
// This is set to LocalCache if net::HttpResponseInfo::was_cached is true, set
// to ServerPush if net::LoadTimingInfo::push_start is non-null, and set to
// NetworkLoad otherwise.
@property(assign, readwrite)
    NSURLSessionTaskMetricsResourceFetchType resourceFetchType;

- (NSString*)description;

@end

// This is an internal version of NSURLSessionTaskMetrics - see comment above
// CronetTransactionMetrics.
NS_AVAILABLE_IOS(10.0) @interface CronetMetrics : NSURLSessionTaskMetrics
// Redefined as readwrite.
@property(copy, readwrite)
    NSArray<NSURLSessionTaskTransactionMetrics*>* transactionMetrics;
@end

namespace cronet {

// net::MetricsDelegate for Cronet.
class CronetMetricsDelegate : public net::MetricsDelegate {
 public:
  using Metrics = net::MetricsDelegate::Metrics;

  CronetMetricsDelegate() {}
  void OnStartNetRequest(NSURLSessionTask* task) override;
  void OnStopNetRequest(std::unique_ptr<Metrics> metrics) override;

  // Returns the metrics collected for a specific task (removing that task's
  // entry from the map in the process).
  // It is called exactly once by the swizzled delegate proxy (see below),
  // uses it to retrieve metrics data collected by net/ and pass them on to
  // the client. If there is no metrics data for the passed task, this returns
  // nullptr.
  static std::unique_ptr<Metrics> MetricsForTask(NSURLSessionTask* task);

  // Used by tests to query the size of the |gTaskMetricsMap| map.
  static size_t GetMetricsMapSize();
};

// This is the swizzling function that Cronet (in its startInternal
// method) calls to inject the proxy delegate into iOS networking API and
// intercept didFinishCollectingMetrics to replace the (empty) iOS metrics data
// with metrics data from net.
void SwizzleSessionWithConfiguration();

}  // namespace cronet

#endif // COMPONENTS_CRONET_IOS_CRONET_METRICS_H_
