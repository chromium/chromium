// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "base/strings/sys_string_conversions.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "components/cronet/testing/test_server/test_server.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Forward declaration of class in cronet_metrics.h for testing.
NS_AVAILABLE_IOS(10.0)
@interface CronetTransactionMetrics : NSURLSessionTaskTransactionMetrics
@end

namespace cronet {

class CronetMetricsTest : public CronetTestBase {
 protected:
  void SetUpWithMetrics(BOOL metrics_enabled) {
    TestServer::Start();

    [Cronet setMetricsEnabled:metrics_enabled];
    StartCronet(net::QuicSimpleTestServer::GetPort());

    [Cronet registerHttpProtocolHandler];
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    [Cronet installIntoSessionConfiguration:config];
    session_ = [NSURLSession sessionWithConfiguration:config
                                             delegate:delegate_
                                        delegateQueue:nil];
  }

  void TearDown() override {
    [Cronet shutdownForTesting];

    TestServer::Shutdown();
    CronetTestBase::TearDown();
  }

  NSURLSession* session_;
};

class CronetEnabledMetricsTest : public CronetMetricsTest {
 protected:
  void SetUp() override {
    CronetMetricsTest::SetUp();
    SetUpWithMetrics(YES);
  }
};

class CronetDisabledMetricsTest : public CronetMetricsTest {
 protected:
  void SetUp() override {
    CronetMetricsTest::SetUp();
    SetUpWithMetrics(NO);
  }
};

// Tests that metrics data is sane for a QUIC request.
TEST_F(CronetEnabledMetricsTest, ProtocolIsQuic) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
              base::SysNSStringToUTF8([delegate_ responseBody]));

    NSURLSessionTaskMetrics* task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* metrics =
        task_metrics.transactionMetrics.firstObject;
    EXPECT_TRUE([metrics isMemberOfClass:[CronetTransactionMetrics class]]);

    // Confirm that metrics data is the correct type.
    EXPECT_TRUE([metrics.fetchStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.domainLookupStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.domainLookupEndDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.connectStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE(
        [metrics.secureConnectionStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.secureConnectionEndDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.connectEndDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.requestStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.requestEndDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.responseStartDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.responseEndDate isKindOfClass:[NSDate class]]);
    EXPECT_TRUE([metrics.networkProtocolName isKindOfClass:[NSString class]]);

    // Confirm that the metrics values are sane.
    EXPECT_NE(NSOrderedDescending, [metrics.domainLookupStartDate
                                       compare:metrics.domainLookupEndDate]);
    EXPECT_NE(NSOrderedDescending,
              [metrics.connectStartDate compare:metrics.connectEndDate]);
    EXPECT_NE(NSOrderedDescending,
              [metrics.secureConnectionStartDate
                  compare:metrics.secureConnectionEndDate]);
    EXPECT_NE(NSOrderedDescending,
              [metrics.requestStartDate compare:metrics.requestEndDate]);
    EXPECT_NE(NSOrderedDescending,
              [metrics.responseStartDate compare:metrics.responseEndDate]);

    EXPECT_FALSE(metrics.proxyConnection);

    EXPECT_TRUE([metrics.networkProtocolName containsString:@"quic"] ||
                [metrics.networkProtocolName containsString:@"h3"])
        << base::SysNSStringToUTF8(metrics.networkProtocolName);
  }
}

// Tests that metrics data is sane for an HTTP/1.1 request.
TEST_F(CronetEnabledMetricsTest, ProtocolIsNotQuic) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetSimpleURL()));

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_STREQ("The quick brown fox jumps over the lazy dog.",
                 base::SysNSStringToUTF8([delegate_ responseBody]).c_str());

    NSURLSessionTaskMetrics* task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* metrics =
        task_metrics.transactionMetrics.firstObject;
    EXPECT_TRUE([metrics isMemberOfClass:[CronetTransactionMetrics class]]);

    EXPECT_NSEQ(metrics.networkProtocolName, @"http/1.1");
  }
}

// Tests that Cronet provides similar metrics data to iOS.
TEST_F(CronetEnabledMetricsTest, PlatformComparison) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetSimpleURL()));

    // Perform a connection using Cronet.

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_STREQ("The quick brown fox jumps over the lazy dog.",
                 base::SysNSStringToUTF8([delegate_ responseBody]).c_str());

    NSURLSessionTaskMetrics* cronet_task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(cronet_task_metrics);
    ASSERT_EQ(1lU, cronet_task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* cronet_metrics =
        cronet_task_metrics.transactionMetrics.firstObject;

    // Perform a connection using the platform stack.

    block_used = NO;
    task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return NO;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_STREQ("The quick brown fox jumps over the lazy dog.",
                 base::SysNSStringToUTF8([delegate_ responseBody]).c_str());

    NSURLSessionTaskMetrics* platform_task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(platform_task_metrics);
    ASSERT_EQ(1lU, platform_task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* platform_metrics =
        platform_task_metrics.transactionMetrics.firstObject;

    // Compare platform and Cronet metrics data.

    EXPECT_NSEQ(cronet_metrics.networkProtocolName,
                platform_metrics.networkProtocolName);
  }
}

// Tests that the metrics API behaves sanely when making a request to an
// invalid URL.
TEST_F(CronetEnabledMetricsTest, InvalidURL) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(GURL("http://notfound.example.com"));

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_TRUE([delegate_ error]);

    NSURLSessionTaskMetrics* task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* metrics =
        task_metrics.transactionMetrics.firstObject;
    EXPECT_TRUE([metrics isMemberOfClass:[CronetTransactionMetrics class]]);

    EXPECT_TRUE(metrics.fetchStartDate);
    EXPECT_FALSE(metrics.domainLookupStartDate);
    EXPECT_FALSE(metrics.domainLookupEndDate);
    EXPECT_FALSE(metrics.connectStartDate);
    EXPECT_FALSE(metrics.secureConnectionStartDate);
    EXPECT_FALSE(metrics.secureConnectionEndDate);
    EXPECT_FALSE(metrics.connectEndDate);
    EXPECT_FALSE(metrics.requestStartDate);
    EXPECT_FALSE(metrics.requestEndDate);
    EXPECT_FALSE(metrics.responseStartDate);
  }
}

// Tests that the metrics API behaves sanely when the request is canceled.
TEST_F(CronetEnabledMetricsTest, CanceledRequest) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];

    StartDataTaskAndWaitForCompletion(task, 1);
    [task cancel];

    EXPECT_TRUE(block_used);
    EXPECT_NE(nil, [delegate_ error]);
  }
}

// Tests the metrics data for a reused connection is correct.
TEST_F(CronetEnabledMetricsTest, ReusedConnection) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
              base::SysNSStringToUTF8([delegate_ responseBody]));

    NSURLSessionTaskMetrics* task_metrics = [delegate_ taskMetrics];
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* metrics =
        task_metrics.transactionMetrics.firstObject;
    EXPECT_TRUE([metrics isMemberOfClass:[CronetTransactionMetrics class]]);

    // Second connection

    block_used = NO;
    task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
              base::SysNSStringToUTF8([delegate_ responseBody]));

    task_metrics = delegate_.taskMetrics;
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    metrics = task_metrics.transactionMetrics.firstObject;

    EXPECT_TRUE(metrics.isReusedConnection);
    EXPECT_FALSE(metrics.domainLookupStartDate);
    EXPECT_FALSE(metrics.domainLookupEndDate);
    EXPECT_FALSE(metrics.connectStartDate);
    EXPECT_FALSE(metrics.secureConnectionStartDate);
    EXPECT_FALSE(metrics.secureConnectionEndDate);
    EXPECT_FALSE(metrics.connectEndDate);
  }
}

// Checks that there is no crash if the session delegate is not set when a
// NSURLSession is created. Also checks that the internal metrics map is cleaned
// and contains 0 records at the end of the request. This is a regression test
// for http://crbug/834401.
TEST_F(CronetEnabledMetricsTest, SessionWithoutDelegate) {
  if (@available(iOS 10.2, *)) {
    NSURLSessionConfiguration* default_config =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    [Cronet installIntoSessionConfiguration:default_config];
    NSURLSession* default_session =
        [NSURLSession sessionWithConfiguration:default_config];
    NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());
    NSURLRequest* request = [NSURLRequest requestWithURL:url];

    __block BOOL no_error = NO;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    NSURLSessionDataTask* task = [default_session
        dataTaskWithRequest:request
          completionHandler:^(NSData* data, NSURLResponse* response,
                              NSError* error) {
            EXPECT_TRUE(error == nil)
                << base::SysNSStringToUTF8([error description]);
            no_error = YES;
            dispatch_semaphore_signal(semaphore);
          }];
    __block BOOL block_used = NO;
    [Cronet setRequestFilterBlock:^(NSURLRequest* nsUrlRequest) {
      block_used = YES;
      EXPECT_EQ(nsUrlRequest.URL, url);
      return YES;
    }];

    [task resume];
    long wait_result = dispatch_semaphore_wait(
        semaphore, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

    // Check results
    EXPECT_EQ(0, wait_result);
    EXPECT_TRUE(block_used);
    EXPECT_TRUE(no_error);
    EXPECT_EQ(0UL, [Cronet getMetricsMapSize]);
  }
}

// Tests that the metrics disable switch works.
TEST_F(CronetDisabledMetricsTest, MetricsDisabled) {
  if (@available(iOS 10.2, *)) {
    NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ(request.URL, url);
      return YES;
    }];
    StartDataTaskAndWaitForCompletion(task);
    EXPECT_TRUE(block_used);
    EXPECT_EQ(nil, [delegate_ error]);
    EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
              base::SysNSStringToUTF8([delegate_ responseBody]));

    NSURLSessionTaskMetrics* task_metrics = [delegate_ taskMetrics];
    ASSERT_TRUE(task_metrics);
    ASSERT_EQ(1lU, task_metrics.transactionMetrics.count);
    NSURLSessionTaskTransactionMetrics* metrics =
        task_metrics.transactionMetrics.firstObject;
    EXPECT_FALSE([metrics isMemberOfClass:[CronetTransactionMetrics class]]);

    EXPECT_TRUE(metrics.fetchStartDate);
    EXPECT_FALSE(metrics.domainLookupStartDate);
    EXPECT_FALSE(metrics.domainLookupEndDate);
    EXPECT_FALSE(metrics.connectStartDate);
    EXPECT_FALSE(metrics.secureConnectionStartDate);
    EXPECT_FALSE(metrics.secureConnectionEndDate);
    EXPECT_FALSE(metrics.connectEndDate);
    EXPECT_FALSE(metrics.requestStartDate);
    EXPECT_FALSE(metrics.requestEndDate);
    EXPECT_FALSE(metrics.responseStartDate);
    EXPECT_FALSE(metrics.responseEndDate);
    EXPECT_FALSE(metrics.networkProtocolName);
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST_F(CronetEnabledMetricsTest, LegacyApi) {
  NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

  __block BOOL block_used = NO;
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    block_used = YES;
    EXPECT_EQ(request.URL, url);
    return YES;
  }];

  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  NSError* err;
  NSHTTPURLResponse* response;
  [NSURLConnection sendSynchronousRequest:request
                        returningResponse:&response
                                    error:&err];

  EXPECT_EQ(200, [response statusCode]);
  EXPECT_TRUE(block_used);
  EXPECT_FALSE(err);
}
#pragma clang diagnostic pop

}  // namespace cronet
