// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include <stdint.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/testing/test_server/test_server.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace {

const int kTestIterations = 10;
const BOOL kUseExternalUrl = NO;
const int kDownloadSize = 19307439;  // used for internal server only
const char* kExternalUrl = "https://www.gstatic.com/chat/hangouts/bg/davec.jpg";

struct PerfResult {
  NSTimeInterval total;
  NSTimeInterval mean;
  NSTimeInterval max;
  int64_t total_bytes_downloaded;
  int failed_requests;
  int total_requests;
};

struct TestConfig {
  BOOL quic;
  BOOL http2;
  BOOL akd4;
  BOOL cronet;
};

bool operator<(TestConfig a, TestConfig b) {
  return std::tie(a.quic, a.http2, a.akd4, a.cronet) <
         std::tie(b.quic, b.http2, b.akd4, b.cronet);
}

const TestConfig test_combinations[] = {
    //  QUIC   HTTP2  AKD4   Cronet
    { false, false, false, false, },
    { false, false, false, true, },
    { false, true, false, true, },
    { true, false, false, true, },
    { true, false, true, true, },
};

}  // namespace

namespace cronet {

class PerfTest : public CronetTestBase,
                 public ::testing::WithParamInterface<TestConfig> {
 public:
  static void TearDownTestCase() {
    NSMutableString* perf_data_acc = [NSMutableString stringWithCapacity:0];

    LOG(INFO) << "Performance Data:";
    for (auto const& entry : perf_test_results) {
      NSString* formatted_entry = [NSString
          stringWithFormat:
              @"Quic %i\tHttp2 %i\tAKD4 %i\tCronet %i: Mean: %fs "
              @"(%fmbps)\tMax: "
              @"%fs with %i fails out of %i total requests.",
              entry.first.quic, entry.first.http2, entry.first.akd4,
              entry.first.cronet, entry.second.mean,
              entry.second.total ? 8 * entry.second.total_bytes_downloaded /
                                       entry.second.total / 1e6
                                 : 0,
              entry.second.max, entry.second.failed_requests,
              entry.second.total_requests];

      [perf_data_acc appendFormat:@"%@\n", formatted_entry];

      LOG(INFO) << base::SysNSStringToUTF8(formatted_entry);
    }

    NSString* filename = [NSString
        stringWithFormat:@"performance_metrics-%@.txt",
                         [[NSDate date]
                             descriptionWithLocale:[NSLocale currentLocale]]];
    NSString* path =
        [[[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                  inDomains:NSUserDomainMask]
            lastObject] URLByAppendingPathComponent:filename] path];

    NSData* filedata = [perf_data_acc dataUsingEncoding:NSUTF8StringEncoding];
    [[NSFileManager defaultManager] createFileAtPath:path
                                            contents:filedata
                                          attributes:nil];
  }

 protected:
  static std::map<TestConfig, PerfResult> perf_test_results;

  PerfTest() {}
  ~PerfTest() override {}

  void SetUp() override {
    CronetTestBase::SetUp();
    TestServer::Start();

    // These are normally called by StartCronet(), but because of the test
    // parameterization we need to call them inline, and not use StartCronet()
    [Cronet setUserAgent:@"CronetTest/1.0.0.0" partial:NO];
    [Cronet setQuicEnabled:GetParam().quic];
    [Cronet setHttp2Enabled:GetParam().http2];
    [Cronet setAcceptLanguages:@"en-US,en"];
    if (kUseExternalUrl) {
      NSString* external_host = [[NSURL
          URLWithString:[NSString stringWithUTF8String:kExternalUrl]] host];
      [Cronet addQuicHint:external_host port:443 altPort:443];
    } else {
      [Cronet addQuicHint:@"test.example.com" port:443 altPort:443];
    }
    [Cronet enableTestCertVerifierForTesting];
    [Cronet setHttpCacheType:CRNHttpCacheTypeDisabled];
    if (GetParam().akd4) {
      [Cronet setExperimentalOptions:
                  @"{\"QUIC\":{\"connection_options\":\"AKD4\"}}"];
    }

    [Cronet start];

    NSString* rules = base::SysUTF8ToNSString(
        base::StringPrintf("MAP test.example.com 127.0.0.1:%d,"
                           "MAP notfound.example.com ^NOTFOUND",
                           net::QuicSimpleTestServer::GetPort()));
    [Cronet setHostResolverRulesForTesting:rules];
    // This is the end of the behavior normally performed by StartCronet()

    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    if (GetParam().cronet) {
      [Cronet registerHttpProtocolHandler];
      [Cronet installIntoSessionConfiguration:config];
    } else {
      [Cronet unregisterHttpProtocolHandler];
    }
    session_ = [NSURLSession sessionWithConfiguration:config
                                             delegate:delegate_
                                        delegateQueue:nil];
  }

  void TearDown() override {
    TestServer::Shutdown();

    [Cronet shutdownForTesting];
    CronetTestBase::TearDown();
  }

  NSURLSession* session_;
};

// static
std::map<TestConfig, PerfResult> PerfTest::perf_test_results;

TEST_P(PerfTest, NSURLSessionReceivesImageLoop) {
  int iterations = kTestIterations;
  int failed_iterations = 0;
  int64_t total_bytes_received = 0;
  NSTimeInterval elapsed_total = 0;
  NSTimeInterval elapsed_max = 0;

  int first_log = false;

  LOG(INFO) << "Running with parameters: "
            << "QUIC: " << GetParam().quic << "\t"
            << "HTTP2: " << GetParam().http2 << "\t"
            << "AKD4: " << GetParam().akd4 << "\t"
            << "Cronet: " << GetParam().cronet << "\t";

  NSURL* url;
  if (kUseExternalUrl) {
    url = net::NSURLWithGURL(GURL(kExternalUrl));
  } else {
    LOG(INFO) << "Downloading " << kDownloadSize << " bytes per iteration";
    url =
        net::NSURLWithGURL(GURL(TestServer::PrepareBigDataURL(kDownloadSize)));
  }

  for (int i = 0; i < iterations; ++i) {
    __block BOOL block_used = NO;
    NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      block_used = YES;
      EXPECT_EQ([request URL], url);
      return YES;
    }];

    NSDate* start = [NSDate date];
    BOOL success = StartDataTaskAndWaitForCompletion(task);

    if (!success) {
      [task cancel];
    }

    success = success && IsResponseSuccessful(task);

    NSTimeInterval elapsed = -[start timeIntervalSinceNow];

    // Do not tolerate failures on internal server.
    if (!kUseExternalUrl) {
      CHECK(success);
    }

    if (kUseExternalUrl && success && !first_log) {
      LOG(INFO) << "Downloaded "
                << [[delegate_ totalBytesReceivedPerTask][task] intValue]
                << " bytes on first iteration.";
      first_log = true;
    }

    if (!success) {
      if ([delegate_ errorPerTask][task]) {
        LOG(WARNING) << "Request failed during performance testing: "
                     << base::SysNSStringToUTF8([[delegate_ errorPerTask][task]
                            localizedDescription]);
      } else {
        LOG(WARNING) << "Request timed out during performance testing.";
      }
      ++failed_iterations;
    } else {
      // Checking that the correct amount of data was downloaded only makes
      // sense if the request succeeded.
      EXPECT_EQ([[delegate_ expectedContentLengthPerTask][task] intValue],
                [[delegate_ totalBytesReceivedPerTask][task] intValue]);

      elapsed_total += elapsed;
      elapsed_max = MAX(elapsed, elapsed_max);

      total_bytes_received +=
          [[delegate_ totalBytesReceivedPerTask][task] intValue];
    }

    EXPECT_EQ(block_used, GetParam().cronet);
  }

  LOG(INFO) << "Elapsed Total:" << elapsed_total * 1000 << "ms";

  // Reject performance data from too many failures.
  if (kUseExternalUrl) {
    CHECK_LE(failed_iterations, iterations / 2);
  }

  perf_test_results[GetParam()] = {
      elapsed_total,        elapsed_total / iterations, elapsed_max,
      total_bytes_received, failed_iterations,          iterations};

  if (!kUseExternalUrl) {
    TestServer::ReleaseBigDataURL();
  }
}

INSTANTIATE_TEST_SUITE_P(Loops,
                         PerfTest,
                         ::testing::ValuesIn(test_combinations));
}  // namespace cronet
