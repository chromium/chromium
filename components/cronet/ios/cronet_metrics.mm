// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/cronet/ios/cronet_metrics.h"

#include <objc/runtime.h>

#include "base/lazy_instance.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CronetTransactionMetrics

@synthesize request = _request;
@synthesize response = _response;

@synthesize fetchStartDate = _fetchStartDate;
@synthesize domainLookupStartDate = _domainLookupStartDate;
@synthesize domainLookupEndDate = _domainLookupEndDate;
@synthesize connectStartDate = _connectStartDate;
@synthesize secureConnectionStartDate = _secureConnectionStartDate;
@synthesize secureConnectionEndDate = _secureConnectionEndDate;
@synthesize connectEndDate = _connectEndDate;
@synthesize requestStartDate = _requestStartDate;
@synthesize requestEndDate = _requestEndDate;
@synthesize responseStartDate = _responseStartDate;
@synthesize responseEndDate = _responseEndDate;

@synthesize networkProtocolName = _networkProtocolName;
@synthesize proxyConnection = _proxyConnection;
@synthesize reusedConnection = _reusedConnection;
@synthesize resourceFetchType = _resourceFetchType;

// The NSURLSessionTaskTransactionMetrics and NSURLSessionTaskMetrics classes
// are not supposed to be extended.  Its default init method initialized an
// internal class, and therefore needs to be overridden to explicitly
// initialize (and return) an instance of this class.
// The |self = old_self| swap is necessary because [super init] must be
// assigned to self (or returned immediately), but in this case is returning
// a value of the wrong type.

- (instancetype)init {
  id old_self = self;
  self = [super init];
  self = old_self;
  return old_self;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @""
           "fetchStartDate: %@\n"
           "domainLookupStartDate: %@\n"
           "domainLookupEndDate: %@\n"
           "connectStartDate: %@\n"
           "secureConnectionStartDate: %@\n"
           "secureConnectionEndDate: %@\n"
           "connectEndDate: %@\n"
           "requestStartDate: %@\n"
           "requestEndDate: %@\n"
           "responseStartDate: %@\n"
           "responseEndDate: %@\n"
           "networkProtocolName: %@\n"
           "proxyConnection: %i\n"
           "reusedConnection: %i\n"
           "resourceFetchType: %lu\n",
          [self fetchStartDate], [self domainLookupStartDate],
          [self domainLookupEndDate], [self connectStartDate],
          [self secureConnectionStartDate], [self secureConnectionEndDate],
          [self connectEndDate], [self requestStartDate], [self requestEndDate],
          [self responseStartDate], [self responseEndDate],
          [self networkProtocolName], [self isProxyConnection],
          [self isReusedConnection], (long)[self resourceFetchType]];
}

@end

@implementation CronetMetrics

@synthesize transactionMetrics = _transactionMetrics;

- (instancetype)init {
  id old_self = self;
  self = [super init];
  self = old_self;
  return old_self;
}

@end

namespace {

using Metrics = net::MetricsDelegate::Metrics;

// Synchronizes access to |gTaskMetricsMap|.
base::LazyInstance<base::Lock>::Leaky gTaskMetricsMapLock =
    LAZY_INSTANCE_INITIALIZER;

// A global map that contains metrics information for pending URLSessionTasks.
// The map has to be "leaky"; otherwise, it will be destroyed on the main thread
// when the client app terminates. When the client app terminates, the network
// thread may still be finishing some work that requires access to the map.
base::LazyInstance<std::map<NSURLSessionTask*, std::unique_ptr<Metrics>>>::Leaky
    gTaskMetricsMap = LAZY_INSTANCE_INITIALIZER;

// Helper method that converts the ticks data found in LoadTimingInfo to an
// NSDate value to be used in client-side data.
NSDate* TicksToDate(const net::LoadTimingInfo& reference,
                    const base::TimeTicks& ticks) {
  if (ticks.is_null())
    return nil;
  base::Time ticks_since_1970 =
      (reference.request_start_time + (ticks - reference.request_start));
  return [NSDate dateWithTimeIntervalSince1970:ticks_since_1970.ToDoubleT()];
}

// Converts Metrics metrics data into CronetTransactionMetrics (which
// importantly implements the NSURLSessionTaskTransactionMetrics API)
CronetTransactionMetrics* NativeToIOSMetrics(Metrics& metrics)
    NS_AVAILABLE_IOS(10.0) {
  NSURLSessionTask* task = metrics.task;
  const net::LoadTimingInfo& load_timing_info = metrics.load_timing_info;
  const net::HttpResponseInfo& response_info = metrics.response_info;

  CronetTransactionMetrics* transaction_metrics =
      [[CronetTransactionMetrics alloc] init];

  [transaction_metrics setRequest:[task currentRequest]];
  [transaction_metrics setResponse:[task response]];

  transaction_metrics.fetchStartDate =
      [NSDate dateWithTimeIntervalSince1970:load_timing_info.request_start_time
                                                .ToDoubleT()];

  transaction_metrics.domainLookupStartDate = TicksToDate(
      load_timing_info, load_timing_info.connect_timing.domain_lookup_start);
  transaction_metrics.domainLookupEndDate = TicksToDate(
      load_timing_info, load_timing_info.connect_timing.domain_lookup_end);

  transaction_metrics.connectStartDate = TicksToDate(
      load_timing_info, load_timing_info.connect_timing.connect_start);
  transaction_metrics.secureConnectionStartDate =
      TicksToDate(load_timing_info, load_timing_info.connect_timing.ssl_start);
  transaction_metrics.secureConnectionEndDate =
      TicksToDate(load_timing_info, load_timing_info.connect_timing.ssl_end);
  transaction_metrics.connectEndDate = TicksToDate(
      load_timing_info, load_timing_info.connect_timing.connect_end);

  transaction_metrics.requestStartDate =
      TicksToDate(load_timing_info, load_timing_info.send_start);
  transaction_metrics.requestEndDate =
      TicksToDate(load_timing_info, load_timing_info.send_end);
  transaction_metrics.responseStartDate =
      TicksToDate(load_timing_info, load_timing_info.receive_headers_end);
  transaction_metrics.responseEndDate = [NSDate
      dateWithTimeIntervalSince1970:metrics.response_end_time.ToDoubleT()];

  transaction_metrics.networkProtocolName =
      base::SysUTF8ToNSString(net::HttpResponseInfo::ConnectionInfoToString(
          response_info.connection_info));
  transaction_metrics.proxyConnection = !response_info.proxy_server.is_direct();

  // If the connect timing information is null, then there was no connection
  // establish - i.e., one was reused.
  // The corrolary to this is that, if reusedConnection is YES, then
  // domainLookupStartDate, domainLookupEndDate, connectStartDate,
  // connectEndDate, secureConnectionStartDate, and secureConnectionEndDate are
  // all meaningless.
  transaction_metrics.reusedConnection =
      load_timing_info.connect_timing.connect_start.is_null();

  // Guess the resource fetch type based on some heuristics about what data is
  // present.
  if (response_info.was_cached) {
    transaction_metrics.resourceFetchType =
        NSURLSessionTaskMetricsResourceFetchTypeLocalCache;
  } else if (!load_timing_info.push_start.is_null()) {
    transaction_metrics.resourceFetchType =
        NSURLSessionTaskMetricsResourceFetchTypeServerPush;
  } else {
    transaction_metrics.resourceFetchType =
        NSURLSessionTaskMetricsResourceFetchTypeNetworkLoad;
  }

  return transaction_metrics;
}

}  // namespace

// A blank implementation of NSURLSessionDelegate that contains no methods.
// It is used as a substitution for a session delegate when the client
// either creates a session without a delegate or passes 'nil' as its value.
@interface BlankNSURLSessionDelegate : NSObject<NSURLSessionDelegate>
@end

@implementation BlankNSURLSessionDelegate : NSObject
@end

// In order for Cronet to use the iOS metrics collection API, it needs to
// replace the normal NSURLSession mechanism for calling into the delegate
// (so it can provide metrics from net/, instead of the empty metrics that iOS
// would provide otherwise.
// To this end, Cronet's startInternal method replaces the NSURLSession's
// sessionWithConfiguration method to inject a delegateProxy in between the
// client delegate and iOS code.
// This class represrents that delegateProxy. The important function is the
// didFinishCollectingMetrics callback, which when a request is being handled
// by Cronet, replaces the metrics collected by iOS with those connected by
// Cronet.
@interface URLSessionTaskDelegateProxy : NSProxy<NSURLSessionTaskDelegate>
- (instancetype)initWithDelegate:(id<NSURLSessionDelegate>)delegate;
@end

@implementation URLSessionTaskDelegateProxy {
  id<NSURLSessionDelegate> _delegate;
  BOOL _respondsToDidFinishCollectingMetrics;
}

// As this is a proxy delegate, it needs to be initialized with a real client
// delegate, to whom all of the method invocations will eventually get passed.
- (instancetype)initWithDelegate:(id<NSURLSessionDelegate>)delegate {
  // If the client passed a real delegate, use it. Otherwise, create a blank
  // delegate that will handle method invocations that are forwarded by this
  // proxy implementation. It is incorrect to forward calls to a 'nil' object.
  if (delegate) {
    _delegate = delegate;
  } else {
    _delegate = [[BlankNSURLSessionDelegate alloc] init];
  }

  _respondsToDidFinishCollectingMetrics =
      [_delegate respondsToSelector:@selector
                 (URLSession:task:didFinishCollectingMetrics:)];
  return self;
}

// Any methods other than didFinishCollectingMetrics should be forwarded
// directly to the client delegate.
- (void)forwardInvocation:(NSInvocation*)invocation {
  [invocation setTarget:_delegate];
  [invocation invoke];
}

// And for that reason, URLSessionTaskDelegateProxy should act like it responds
// to any of the selectors that the client delegate does.
- (nullable NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  return [(id)_delegate methodSignatureForSelector:sel];
}

// didFinishCollectionMetrics ultimately calls into the corresponding method on
// the client delegate (if it exists), but first replaces the iOS-supplied
// metrics with metrics collected by Cronet (if they exist).
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics
    NS_AVAILABLE_IOS(10.0) {
  std::unique_ptr<Metrics> netMetrics =
      cronet::CronetMetricsDelegate::MetricsForTask(task);

  if (_respondsToDidFinishCollectingMetrics) {
    if (netMetrics) {
      CronetTransactionMetrics* cronetTransactionMetrics =
          NativeToIOSMetrics(*netMetrics);

      CronetMetrics* cronetMetrics = [[CronetMetrics alloc] init];
      [cronetMetrics setTransactionMetrics:@[ cronetTransactionMetrics ]];

      [(id<NSURLSessionTaskDelegate>)_delegate URLSession:session
                                                     task:task
                               didFinishCollectingMetrics:cronetMetrics];
    } else {
      // If there are no metrics is Cronet's task->metrics map, then Cronet is
      // not handling this request, so just transparently pass iOS's collected
      // metrics.
      [(id<NSURLSessionTaskDelegate>)_delegate URLSession:session
                                                     task:task
                               didFinishCollectingMetrics:metrics];
    }
  }
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  // Regardless whether the underlying session delegate handles
  // URLSession:task:didFinishCollectingMetrics: or not, always
  // return 'YES' for that selector. Otherwise, the method may
  // not be called, causing unbounded growth of |gTaskMetricsMap|.
  if (aSelector == @selector(URLSession:task:didFinishCollectingMetrics:)) {
    return YES;
  }
  return [_delegate respondsToSelector:aSelector];
}

@end

@implementation NSURLSession (Cronet)

+ (NSURLSession*)
hookSessionWithConfiguration:(NSURLSessionConfiguration*)configuration
                    delegate:(nullable id<NSURLSessionDelegate>)delegate
               delegateQueue:(nullable NSOperationQueue*)queue {
  URLSessionTaskDelegateProxy* delegate_proxy =
      [[URLSessionTaskDelegateProxy alloc] initWithDelegate:delegate];
  // Because the the method implementations are swapped, this is not a
  // recursive call, and instead just forwards the call to the original
  // sessionWithConfiguration method.
  return [self hookSessionWithConfiguration:configuration
                                   delegate:delegate_proxy
                              delegateQueue:queue];
}

@end

namespace cronet {

std::unique_ptr<Metrics> CronetMetricsDelegate::MetricsForTask(
    NSURLSessionTask* task) {
  base::AutoLock auto_lock(gTaskMetricsMapLock.Get());
  auto metrics_search = gTaskMetricsMap.Get().find(task);
  if (metrics_search == gTaskMetricsMap.Get().end()) {
    return nullptr;
  }

  std::unique_ptr<Metrics> metrics = std::move(metrics_search->second);
  // Remove the entry to free memory.
  gTaskMetricsMap.Get().erase(metrics_search);

  return metrics;
}

void CronetMetricsDelegate::OnStartNetRequest(NSURLSessionTask* task) {
  base::AutoLock auto_lock(gTaskMetricsMapLock.Get());
  if ([task state] == NSURLSessionTaskStateRunning) {
    gTaskMetricsMap.Get()[task] = nullptr;
  }
}

void CronetMetricsDelegate::OnStopNetRequest(std::unique_ptr<Metrics> metrics) {
  base::AutoLock auto_lock(gTaskMetricsMapLock.Get());
  auto metrics_search = gTaskMetricsMap.Get().find(metrics->task);
  if (metrics_search != gTaskMetricsMap.Get().end())
    metrics_search->second = std::move(metrics);
}

size_t CronetMetricsDelegate::GetMetricsMapSize() {
  base::AutoLock auto_lock(gTaskMetricsMapLock.Get());
  return gTaskMetricsMap.Get().size();
}

#pragma mark - Swizzle

void SwizzleSessionWithConfiguration() {
  Class nsurlsession_class = object_getClass([NSURLSession class]);

  SEL original_selector =
      @selector(sessionWithConfiguration:delegate:delegateQueue:);
  SEL swizzled_selector =
      @selector(hookSessionWithConfiguration:delegate:delegateQueue:);

  Method original_method =
      class_getInstanceMethod(nsurlsession_class, original_selector);
  Method swizzled_method =
      class_getInstanceMethod(nsurlsession_class, swizzled_selector);

  method_exchangeImplementations(original_method, swizzled_method);
}

}  // namespace cronet
