// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "net/base/mac/url_conversions.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace cronet {

class QuicTest : public CronetTestBase {
 protected:
  QuicTest() {}
  ~QuicTest() override {}

  void SetUp() override {
    CronetTestBase::SetUp();

    // Prepare Cronet
    [Cronet setUserAgent:@"CronetTest/1.0.0.0" partial:NO];
    [Cronet setHttp2Enabled:false];
    [Cronet setQuicEnabled:true];
    [Cronet setAcceptLanguages:@"en-US,en"];
    [Cronet addQuicHint:@"test.example.com" port:443 altPort:443];
    [Cronet enableTestCertVerifierForTesting];
    [Cronet setHttpCacheType:CRNHttpCacheTypeDisabled];
    [Cronet setMetricsEnabled:YES];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      return YES;
    }];

    // QUIC Server simple URL.
    simple_url_ = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());
  }

  void TearDown() override {
    [Cronet stopNetLog];
    [Cronet shutdownForTesting];
    CronetTestBase::TearDown();
  }

  void StartCronet() {
    [Cronet start];

    // Add URL mapping to test server.
    NSString* rules = base::SysUTF8ToNSString(
        base::StringPrintf("MAP test.example.com 127.0.0.1:%d,"
                           "MAP notfound.example.com ~NOTFOUND",
                           net::QuicSimpleTestServer::GetPort()));
    [Cronet setHostResolverRulesForTesting:rules];

    // Prepare a session.
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    [Cronet installIntoSessionConfiguration:config];
    session_ = [NSURLSession sessionWithConfiguration:config
                                             delegate:delegate_
                                        delegateQueue:nil];
  }

  NSURLSession* session_;
  NSURL* simple_url_;
};

TEST_F(QuicTest, InvalidQuicHost) {
  BOOL success =
      [Cronet addQuicHint:@"https://test.example.com/" port:443 altPort:443];

  EXPECT_FALSE(success);
}

TEST_F(QuicTest, ValidQuicHost) {
  BOOL success = [Cronet addQuicHint:@"test.example.com" port:443 altPort:443];

  EXPECT_TRUE(success);
}

// Tests a request with enabled "enable_socket_recv_optimization" QUIC
// experimental option.
TEST_F(QuicTest, RequestWithSocketOptimizationEnabled) {
  // Apply test specific Cronet configuration and start it.
  [Cronet setExperimentalOptions:
              @"{\"QUIC\" : {\"enable_socket_recv_optimization\" : true} }"];
  StartCronet();

  // Make request and wait for the response.
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:simple_url_];
  StartDataTaskAndWaitForCompletion(task);

  // Check that a successful response was received using QUIC.
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
            base::SysNSStringToUTF8(delegate_.responseBody));
  if (@available(iOS 10.2, *)) {
    NSURLSessionTaskTransactionMetrics* metrics =
        delegate_.taskMetrics.transactionMetrics[0];
    EXPECT_TRUE([metrics.networkProtocolName containsString:@"quic"] ||
                [metrics.networkProtocolName containsString:@"h3"])
        << base::SysNSStringToUTF8(metrics.networkProtocolName);
  }
}
}
