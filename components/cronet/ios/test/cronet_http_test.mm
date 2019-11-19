// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include <stdint.h>

#include "TargetConditionals.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/cronet/cronet_buildflags.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "components/cronet/test/test_server.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#include "url/gurl.h"

namespace {

// The buffer size of the stream for HTTPBodyStream post test.
const NSUInteger kRequestBodyBufferLength = 1024;

// The buffer size of the stream for HTTPBodyStream post test when
// testing the stream buffered data size larger than the net stack internal
// buffer size.
const NSUInteger kLargeRequestBodyBufferLength = 100 * kRequestBodyBufferLength;

// The body data write times for HTTPBodyStream post test.
const NSInteger kRequestBodyWriteTimes = 16;
}

@interface StreamBodyRequestDelegate : NSObject<NSStreamDelegate>
- (void)setOutputStream:(NSOutputStream*)outputStream;
- (NSMutableString*)requestBody;
@end
@implementation StreamBodyRequestDelegate {
  NSOutputStream* _stream;
  NSInteger _count;

  NSMutableString* _requestBody;
}

- (instancetype)init {
  _requestBody = [NSMutableString string];
  return self;
}

- (void)setOutputStream:(NSOutputStream*)outputStream {
  _stream = outputStream;
}

- (NSMutableString*)requestBody {
  return _requestBody;
}

- (void)stream:(NSStream*)stream handleEvent:(NSStreamEvent)event {
  ASSERT_EQ(stream, _stream);
  switch (event) {
    case NSStreamEventHasSpaceAvailable: {
      if (_count < kRequestBodyWriteTimes) {
        uint8_t buffer[kRequestBodyBufferLength];
        memset(buffer, 'a' + _count, kRequestBodyBufferLength);
        NSUInteger bytes_write =
            [_stream write:buffer maxLength:kRequestBodyBufferLength];
        ASSERT_EQ(kRequestBodyBufferLength, bytes_write);
        [_requestBody appendString:[[NSString alloc]
                                       initWithBytes:buffer
                                              length:kRequestBodyBufferLength
                                            encoding:NSUTF8StringEncoding]];
        ++_count;
      } else {
        [_stream close];
      }
      break;
    }
    case NSStreamEventErrorOccurred:
    case NSStreamEventEndEncountered: {
      [_stream close];
      [_stream setDelegate:nil];
      [_stream removeFromRunLoop:[NSRunLoop currentRunLoop]
                         forMode:NSDefaultRunLoopMode];
      break;
    }
    default:
      break;
  }
}
@end

namespace cronet {
const char kUserAgent[] = "CronetTest/1.0.0.0";

class HttpTest : public CronetTestBase {
 protected:
  HttpTest() {}
  ~HttpTest() override {}

  void SetUp() override {
    CronetTestBase::SetUp();
    TestServer::Start();

    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      return YES;
    }];
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
    TestServer::Shutdown();

    [Cronet stopNetLog];
    [Cronet shutdownForTesting];
    CronetTestBase::TearDown();
  }

  NSURLSession* session_;
};

TEST_F(HttpTest, CreateSslKeyLogFile) {
  // Shutdown Cronet so that it can be restarted with specific configuration
  // (SSL key log file specified in experimental options) for this one test.
  // This is necessary because SslKeyLogFile can only be set once, before any
  // SSL Client Sockets are created.

  [Cronet shutdownForTesting];

  NSString* ssl_key_log_file = [Cronet getNetLogPathForFile:@"SSLKEYLOGFILE"];

  // Ensure that the keylog file doesn't exist.
  [[NSFileManager defaultManager] removeItemAtPath:ssl_key_log_file error:nil];

  [Cronet setExperimentalOptions:
              [NSString stringWithFormat:@"{\"ssl_key_log_file\":\"%@\"}",
                                         ssl_key_log_file]];

  StartCronet(net::QuicSimpleTestServer::GetPort());

  bool ssl_file_created =
      [[NSFileManager defaultManager] fileExistsAtPath:ssl_key_log_file];

  [[NSFileManager defaultManager] removeItemAtPath:ssl_key_log_file error:nil];

  [Cronet shutdownForTesting];
  [Cronet setExperimentalOptions:@""];

  EXPECT_TRUE(ssl_file_created);
}

TEST_F(HttpTest, NSURLSessionReceivesData) {
  NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());
  __block BOOL block_used = NO;
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    block_used = YES;
    EXPECT_EQ([request URL], url);
    return YES;
  }];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_TRUE(block_used);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
            base::SysNSStringToUTF8([delegate_ responseBody]));
}

// https://crbug.com/830005 Disable histogram support to reduce binary size.
#if BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
#define MAYBE_GetGlobalMetricsDeltas DISABLED_GetGlobalMetricsDeltas
#else  // BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
#define MAYBE_GetGlobalMetricsDeltas GetGlobalMetricsDeltas
#endif  // BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
TEST_F(HttpTest, MAYBE_GetGlobalMetricsDeltas) {
  NSData* delta1 = [Cronet getGlobalMetricsDeltas];
  NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_EQ(net::QuicSimpleTestServer::GetSimpleBodyValue(),
            base::SysNSStringToUTF8([delegate_ responseBody]));

  NSData* delta2 = [Cronet getGlobalMetricsDeltas];
  EXPECT_FALSE([delta2 isEqualToData:delta1]);
}

TEST_F(HttpTest, SdchDisabledByDefault) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_FALSE([[delegate_ responseBody] containsString:@"sdch"]);
}

// Verify that explictly setting Accept-Encoding request header to 'gzip,sdch"
// is passed to the server and does not trigger any failures. This behavior may
// In the future Cronet may not allow caller to set Accept-Encoding header and
// could limit it to set of internally suported and enabled encodings, matching
// behavior of Cronet on Android.
TEST_F(HttpTest, AcceptEncodingSdchIsAllowed) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSMutableURLRequest* mutableRequest =
      [[NSURLRequest requestWithURL:url] mutableCopy];
  [mutableRequest addValue:@"gzip,sdch" forHTTPHeaderField:@"Accept-Encoding"];
  NSURLSessionDataTask* task = [session_ dataTaskWithRequest:mutableRequest];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"gzip,sdch"]);
}

// Verify that explictly setting Accept-Encoding request header to 'foo,bar"
// is passed to the server and does not trigger any failures. This behavior may
// In the future Cronet may not allow caller to set Accept-Encoding header and
// could limit it to set of internally suported and enabled encodings, matching
// behavior of Cronet on Android.
TEST_F(HttpTest, AcceptEncodingFooBarIsAllowed) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSMutableURLRequest* mutableRequest =
      [[NSURLRequest requestWithURL:url] mutableCopy];
  [mutableRequest addValue:@"foo,bar" forHTTPHeaderField:@"Accept-Encoding"];
  NSURLSessionDataTask* task = [session_ dataTaskWithRequest:mutableRequest];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"foo,bar"]);
}

TEST_F(HttpTest, NSURLSessionAcceptLanguage) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Language")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  ASSERT_STREQ("en-US,en",
               base::SysNSStringToUTF8([delegate_ responseBody]).c_str());
}

TEST_F(HttpTest, SetUserAgentIsExact) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("User-Agent")));
  [Cronet setRequestFilterBlock:nil];
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ(kUserAgent,
               base::SysNSStringToUTF8([delegate_ responseBody]).c_str());
}

TEST_F(HttpTest, SetCookie) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName =
      [NSString stringWithFormat:@"SetCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSString* cookieLine =
      [NSString stringWithFormat:@"%@=%@", cookieName, cookieValue];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* cookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  // Verify that cookie is not set in system storage.
  for (NSHTTPCookie* cookie in [systemCookieStorage cookiesForURL:cookieUrl]) {
    EXPECT_FALSE([[cookie name] isEqualToString:cookieName]);
  }

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:cookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ("Header not found. :(",
               base::SysNSStringToUTF8([delegate_ responseBody]).c_str());

  NSURL* setCookieUrl = net::NSURLWithGURL(
      GURL(TestServer::GetSetCookieURL(base::SysNSStringToUTF8(cookieLine))));
  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:setCookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieLine]);

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:cookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieLine]);

  // Verify that cookie is set in system storage.
  NSHTTPCookie* systemCookie = nil;
  for (NSHTTPCookie* cookie in [systemCookieStorage cookiesForURL:cookieUrl]) {
    if ([cookie.name isEqualToString:cookieName]) {
      systemCookie = cookie;
      break;
    }
  }
  EXPECT_TRUE([[systemCookie value] isEqualToString:cookieValue]);
  [systemCookieStorage deleteCookie:systemCookie];
}

TEST_F(HttpTest, SetSystemCookie) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName = [NSString
      stringWithFormat:@"SetSystemCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* echoCookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  NSHTTPCookie* systemCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : cookieName,
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
  }];
  [systemCookieStorage setCookie:systemCookie];

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:echoCookieUrl]);
  [systemCookieStorage deleteCookie:systemCookie];
  EXPECT_EQ(nil, [delegate_ error]);
  // Verify that cookie set in system store was sent to the serever.
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieName]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieValue]);
}

TEST_F(HttpTest, SystemCookieWithNullCreationTime) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName = [NSString
      stringWithFormat:@"SetSystemCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* echoCookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  NSHTTPCookie* nullCreationTimeCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : cookieName,
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
    @"Created" : [NSNumber numberWithDouble:0.0],
  }];
  [systemCookieStorage setCookie:nullCreationTimeCookie];
  NSHTTPCookie* normalCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : [cookieName stringByAppendingString:@"-normal"],
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
  }];
  [systemCookieStorage setCookie:normalCookie];
  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:echoCookieUrl]);
  [systemCookieStorage deleteCookie:nullCreationTimeCookie];
  [systemCookieStorage deleteCookie:normalCookie];
  EXPECT_EQ(nil, [delegate_ error]);
  // Verify that cookie set in system store was sent to the serever.
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieName]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieValue]);
}

TEST_F(HttpTest, FilterOutRequest) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("User-Agent")));
  __block BOOL block_used = NO;
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    block_used = YES;
    EXPECT_EQ([request URL], url);
    return NO;
  }];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_TRUE(block_used);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_FALSE([[delegate_ responseBody]
      containsString:base::SysUTF8ToNSString(kUserAgent)]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"CFNetwork"]);
}

TEST_F(HttpTest, FileSchemeNotSupported) {
  NSString* fileData = @"Hello, World!";
  NSString* documentsDirectory = [NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  NSString* filePath = [documentsDirectory
      stringByAppendingPathComponent:[[NSProcessInfo processInfo]
                                         globallyUniqueString]];
  [fileData writeToFile:filePath
             atomically:YES
               encoding:NSUTF8StringEncoding
                  error:nil];

  NSURL* url = [NSURL fileURLWithPath:filePath];
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
    EXPECT_TRUE(false) << "Block should not be called for unsupported requests";
    return YES;
  }];
  StartDataTaskAndWaitForCompletion(task);
  [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:fileData]);
}

TEST_F(HttpTest, DataSchemeNotSupported) {
  NSString* testString = @"Hello, World!";
  NSData* testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  NSString* dataString =
      [NSString stringWithFormat:@"data:text/plain;base64,%@",
                                 [testData base64EncodedStringWithOptions:0]];
  NSURL* url = [NSURL URLWithString:dataString];
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    EXPECT_TRUE(false) << "Block should not be called for unsupported requests";
    return YES;
  }];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:testString]);
}

TEST_F(HttpTest, BrotliAdvertisedTest) {
  [Cronet shutdownForTesting];

  [Cronet setBrotliEnabled:YES];

  StartCronet(net::QuicSimpleTestServer::GetPort());

  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"br"]);
}

TEST_F(HttpTest, BrotliNotAdvertisedTest) {
  [Cronet shutdownForTesting];

  [Cronet setBrotliEnabled:NO];

  StartCronet(net::QuicSimpleTestServer::GetPort());

  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_FALSE([[delegate_ responseBody] containsString:@"br"]);
}

TEST_F(HttpTest, BrotliHandleDecoding) {
  [Cronet shutdownForTesting];

  [Cronet setBrotliEnabled:YES];

  StartCronet(net::QuicSimpleTestServer::GetPort());

  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetUseEncodingURL("brotli")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ(base::SysNSStringToUTF8([delegate_ responseBody]).c_str(),
               "The quick brown fox jumps over the lazy dog");
}

TEST_F(HttpTest, PostRequest) {
  // Create request body.
  NSString* request_body = [NSString stringWithFormat:@"Post Data %i", rand()];
  NSData* post_data = [request_body dataUsingEncoding:NSUTF8StringEncoding];

  // Prepare the request.
  NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetEchoRequestBodyURL()));
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBody = post_data;

  // Set the request filter to check that the request was handled by the Cronet
  // stack.
  __block BOOL block_used = NO;
  [Cronet setRequestFilterBlock:^(NSURLRequest* req) {
    block_used = YES;
    EXPECT_EQ([req URL], url);
    return YES;
  }];

  // Send the request and wait for the response.
  NSURLSessionDataTask* data_task = [session_ dataTaskWithRequest:request];
  StartDataTaskAndWaitForCompletion(data_task);

  // Verify that the response from the server matches the request body.
  NSString* response_body = [delegate_ responseBody];
  ASSERT_EQ(nil, [delegate_ error]);
  ASSERT_STREQ(base::SysNSStringToUTF8(request_body).c_str(),
               base::SysNSStringToUTF8(response_body).c_str());
  ASSERT_TRUE(block_used);
}

TEST_F(HttpTest, PostRequestWithLargeBody) {
  // Create request body.
  std::string request_body(kLargeRequestBodyBufferLength, 'z');
  NSData* post_data = [NSData dataWithBytes:request_body.c_str()
                                     length:request_body.length()];

  // Prepare the request.
  NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetEchoRequestBodyURL()));
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBody = post_data;

  // Set the request filter to check that the request was handled by the Cronet
  // stack.
  __block BOOL block_used = NO;
  [Cronet setRequestFilterBlock:^(NSURLRequest* req) {
    block_used = YES;
    EXPECT_EQ([req URL], url);
    return YES;
  }];

  // Send the request and wait for the response.
  NSURLSessionDataTask* data_task = [session_ dataTaskWithRequest:request];
  StartDataTaskAndWaitForCompletion(data_task);

  // Verify that the response from the server matches the request body.
  NSString* response_body = [delegate_ responseBody];
  ASSERT_EQ(nil, [delegate_ error]);
  ASSERT_STREQ(request_body.c_str(),
               base::SysNSStringToUTF8(response_body).c_str());
  ASSERT_TRUE(block_used);
}

// Verify the chunked request body upload function.
TEST_F(HttpTest, PostRequestWithBodyStream) {
  // Create request body stream.
  CFReadStreamRef read_stream = NULL;
  CFWriteStreamRef write_stream = NULL;
  CFStreamCreateBoundPair(NULL, &read_stream, &write_stream,
                          kRequestBodyBufferLength);

  NSInputStream* input_stream = CFBridgingRelease(read_stream);
  NSOutputStream* output_stream = CFBridgingRelease(write_stream);

  StreamBodyRequestDelegate* stream_delegate =
      [[StreamBodyRequestDelegate alloc] init];
  output_stream.delegate = stream_delegate;
  [stream_delegate setOutputStream:output_stream];

  dispatch_queue_t queue =
      dispatch_queue_create("data upload queue", DISPATCH_QUEUE_SERIAL);
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  dispatch_async(queue, ^{
    [output_stream scheduleInRunLoop:[NSRunLoop currentRunLoop]
                             forMode:NSDefaultRunLoopMode];
    [output_stream open];

    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:10.0]];

    dispatch_semaphore_signal(semaphore);
  });

  // Prepare the request.
  NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetEchoRequestBodyURL()));
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBodyStream = input_stream;

  // Set the request filter to check that the request was handled by the Cronet
  // stack.
  __block BOOL block_used = NO;
  [Cronet setRequestFilterBlock:^(NSURLRequest* req) {
    block_used = YES;
    EXPECT_EQ([req URL], url);
    return YES;
  }];

  // Send the request and wait for the response.
  NSURLSessionDataTask* data_task = [session_ dataTaskWithRequest:request];
  StartDataTaskAndWaitForCompletion(data_task);

  // Verify that the response from the server matches the request body.
  ASSERT_EQ(nil, [delegate_ error]);
  NSString* response_body = [delegate_ responseBody];
  NSMutableString* request_body = [stream_delegate requestBody];
  ASSERT_STREQ(base::SysNSStringToUTF8(request_body).c_str(),
               base::SysNSStringToUTF8(response_body).c_str());
  ASSERT_TRUE(block_used);

  // Wait for the run loop of the child thread exits. Timeout is 5 seconds.
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
  ASSERT_EQ(0, dispatch_semaphore_wait(semaphore, timeout));
}

// Verify that the chunked data uploader can correctly handle the request body
// if the stream contains data length exceed the internal upload buffer.
TEST_F(HttpTest, PostRequestWithLargeBodyStream) {
  // Create request body stream.
  CFReadStreamRef read_stream = NULL;
  CFWriteStreamRef write_stream = NULL;
  // 100KB data is written in one time.
  CFStreamCreateBoundPair(NULL, &read_stream, &write_stream,
                          kLargeRequestBodyBufferLength);

  NSInputStream* input_stream = CFBridgingRelease(read_stream);
  NSOutputStream* output_stream = CFBridgingRelease(write_stream);
  [output_stream open];

  uint8_t buffer[kLargeRequestBodyBufferLength];
  memset(buffer, 'a', kLargeRequestBodyBufferLength);
  NSUInteger bytes_write =
      [output_stream write:buffer maxLength:kLargeRequestBodyBufferLength];
  ASSERT_EQ(kLargeRequestBodyBufferLength, bytes_write);
  [output_stream close];

  // Prepare the request.
  NSURL* url = net::NSURLWithGURL(GURL(TestServer::GetEchoRequestBodyURL()));
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBodyStream = input_stream;

  // Set the request filter to check that the request was handled by the Cronet
  // stack.
  __block BOOL block_used = NO;
  [Cronet setRequestFilterBlock:^(NSURLRequest* req) {
    block_used = YES;
    EXPECT_EQ([req URL], url);
    return YES;
  }];

  // Send the request and wait for the response.
  NSURLSessionDataTask* data_task = [session_ dataTaskWithRequest:request];
  StartDataTaskAndWaitForCompletion(data_task);

  // Verify that the response from the server matches the request body.
  ASSERT_EQ(nil, [delegate_ error]);
  NSString* response_body = [delegate_ responseBody];
  ASSERT_EQ(kLargeRequestBodyBufferLength, [response_body length]);
  ASSERT_TRUE(block_used);
}

// iOS Simulator doesn't support changing thread priorities.
// Therefore, run these tests only on a physical device.
#if TARGET_OS_SIMULATOR
#define MAYBE_ChangeThreadPriorityAfterStart \
  DISABLED_ChangeThreadPriorityAfterStart
#define MAYBE_ChangeThreadPriorityBeforeStart \
  DISABLED_ChangeThreadPriorityBeforeStart
#else
#define MAYBE_ChangeThreadPriorityAfterStart ChangeThreadPriorityAfterStart
#define MAYBE_ChangeThreadPriorityBeforeStart ChangeThreadPriorityBeforeStart
#endif  // TARGET_OS_SIMULATOR

// Tests that the network thread priority can be changed after
// Cronet has been started.
TEST_F(HttpTest, MAYBE_ChangeThreadPriorityAfterStart) {
  // Get current (default) priority of the network thread.
  __block double default_priority;
  PostBlockToNetworkThread(FROM_HERE, ^{
    default_priority = NSThread.threadPriority;
  });

  // Modify the network thread priority.
  const double new_priority = 1.0;
  [Cronet setNetworkThreadPriority:new_priority];

  // Get modified priority of the network thread.
  dispatch_semaphore_t lock = dispatch_semaphore_create(0);
  __block double actual_priority;
  PostBlockToNetworkThread(FROM_HERE, ^{
    actual_priority = NSThread.threadPriority;
    dispatch_semaphore_signal(lock);
  });

  // Wait until the posted tasks are completed.
  dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);

  EXPECT_EQ(0.5, default_priority);

  // Check that the priority was modified and is close to the set priority.
  EXPECT_TRUE(abs(actual_priority - new_priority) < 0.01)
      << "Unexpected thread priority. Expected " << new_priority << " but got "
      << actual_priority;
}

// Tests that the network thread priority can be changed before
// Cronet has been started.
TEST_F(HttpTest, MAYBE_ChangeThreadPriorityBeforeStart) {
  // Start a new Cronet engine modifying the network thread priority before the
  // start.
  [Cronet shutdownForTesting];
  const double new_priority = 0.8;
  [Cronet setNetworkThreadPriority:new_priority];
  [Cronet start];

  // Get modified priority of the network thread.
  dispatch_semaphore_t lock = dispatch_semaphore_create(0);
  __block double actual_priority;
  PostBlockToNetworkThread(FROM_HERE, ^{
    actual_priority = NSThread.threadPriority;
    dispatch_semaphore_signal(lock);
  });

  // Wait until the posted task is completed.
  dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);

  // Check that the priority was modified and is close to the set priority.
  EXPECT_TRUE(abs(actual_priority - new_priority) < 0.01)
      << "Unexpected thread priority. Expected " << new_priority << " but got "
      << actual_priority;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST_F(HttpTest, LegacyApi) {
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
