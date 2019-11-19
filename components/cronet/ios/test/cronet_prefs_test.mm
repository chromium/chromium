// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "components/cronet/test/test_server.h"
#include "net/base/mac/url_conversions.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace cronet {

class PrefsTest : public CronetTestBase {
 protected:
  void SetUp() override {
    CronetTestBase::SetUp();
    TestServer::Start();

    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      return YES;
    }];
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
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

  NSString* GetFileContentWaitUntilCreated(NSString* file,
                                           NSTimeInterval timeout,
                                           NSError** error) {
    // Wait until the file appears on disk.
    NSFileManager* file_manager = [NSFileManager defaultManager];
    NSLog(@"Waiting for file %@.", file);
    while (timeout > 0) {
      if ([file_manager fileExistsAtPath:file]) {
        NSLog(@"File %@ exists.", file);
        break;
      }
      NSLog(@"Time left: %i seconds", (int)timeout);
      NSTimeInterval sleep_interval = fmin(5.0, timeout);
      [NSThread sleepForTimeInterval:sleep_interval];
      timeout -= sleep_interval;
    }

    // Read the file on the file thread to avoid reading the changing file.
    dispatch_semaphore_t lock = dispatch_semaphore_create(0);
    __block NSString* file_content = nil;
    __block NSError* block_error = nil;
    PostBlockToFileThread(FROM_HERE, ^{
      file_content = [NSString stringWithContentsOfFile:file
                                               encoding:NSUTF8StringEncoding
                                                  error:&block_error];
      dispatch_semaphore_signal(lock);
    });

    // Wait for the file thread to finish reading the file content.
    dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);
    if (block_error) {
      *error = block_error;
    }
    return file_content;
  }

  NSURLSession* session_;
};

TEST_F(PrefsTest, HttpServerProperties) {
  base::FilePath storage_path;
  bool result = base::PathService::Get(base::DIR_CACHE, &storage_path);
  ASSERT_TRUE(result);
  storage_path =
      storage_path.Append(FILE_PATH_LITERAL("cronet/prefs/local_prefs.json"));
  NSString* prefs_file_name =
      [NSString stringWithCString:storage_path.AsUTF8Unsafe().c_str()
                         encoding:NSUTF8StringEncoding];

  // Delete the prefs file if it exists.
  [[NSFileManager defaultManager] removeItemAtPath:prefs_file_name error:nil];

  // Add "max_server_configs_stored_in_properties" experimental option.
  NSString* options =
      @"{ \"QUIC\" : {\"max_server_configs_stored_in_properties\" : 5} }";
  [Cronet setExperimentalOptions:options];

  // Start Cronet Engine
  StartCronet(net::QuicSimpleTestServer::GetPort());

  // Start the request
  NSURL* url = net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);

  // Wait 80 seconds for the prefs file to appear on the disk.
  NSError* error = nil;
  NSString* prefs_file_content =
      GetFileContentWaitUntilCreated(prefs_file_name, 80, &error);
  ASSERT_FALSE(error) << "Unable to read " << storage_path << " file. Error: "
                      << error.localizedDescription.UTF8String;

  // Check the file content
  ASSERT_TRUE(prefs_file_content);
  ASSERT_TRUE(
      [prefs_file_content containsString:@"{\"http_server_properties\":"])
      << "Unable to find 'http_server_properties' in the JSON prefs.";
  ASSERT_TRUE([prefs_file_content containsString:@"\"supports_quic\":"])
      << "Unable to find 'supports_quic' in the JSON prefs.";
  ASSERT_TRUE([prefs_file_content containsString:@"\"server_info\":"])
      << "Unable to find 'server_info' in the JSON prefs.";

  // Delete the prefs file to avoid side effects with other tests.
  [[NSFileManager defaultManager] removeItemAtPath:prefs_file_name error:nil];
}

}  // namespace cronet
