// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "net/test/quic_simple_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

class NetLogTest : public ::testing::Test {
 protected:
  NetLogTest() {}
  ~NetLogTest() override {}

  void SetUp() override { StartCronet(net::QuicSimpleTestServer::GetPort()); }

  void TearDown() override {
    [Cronet stopNetLog];
    [Cronet shutdownForTesting];
  }
};

TEST_F(NetLogTest, OpenFile) {
  bool netlog_started =
      [Cronet startNetLogToFile:@"cronet_netlog.json" logBytes:YES];

  EXPECT_TRUE(netlog_started);
}

TEST_F(NetLogTest, CreateFile) {
  NSString* filename = [[[NSProcessInfo processInfo] globallyUniqueString]
      stringByAppendingString:@"_netlog.json"];
  bool netlog_started = [Cronet startNetLogToFile:filename logBytes:YES];
  [Cronet stopNetLog];

  bool file_created = [[NSFileManager defaultManager]
      fileExistsAtPath:[Cronet getNetLogPathForFile:filename]];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet getNetLogPathForFile:filename]
                 error:nil];

  EXPECT_TRUE(netlog_started);
  EXPECT_TRUE(file_created);
}

TEST_F(NetLogTest, NonExistantDir) {
  NSString* notdir = [[[NSProcessInfo processInfo] globallyUniqueString]
      stringByAppendingString:@"/netlog.json"];
  bool netlog_started = [Cronet startNetLogToFile:notdir logBytes:NO];

  EXPECT_FALSE(netlog_started);
}

TEST_F(NetLogTest, ExistantDir) {
  NSString* dir = [[NSProcessInfo processInfo] globallyUniqueString];

  bool dir_created = [[NSFileManager defaultManager]
            createDirectoryAtPath:[Cronet getNetLogPathForFile:dir]
      withIntermediateDirectories:NO
                       attributes:nil
                            error:nil];

  bool netlog_started =
      [Cronet startNetLogToFile:[dir stringByAppendingString:@"/netlog.json"]
                       logBytes:NO];

  [Cronet stopNetLog];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet
                           getNetLogPathForFile:
                               [dir stringByAppendingString:@"/netlog.json"]]
                 error:nil];

  [[NSFileManager defaultManager]
      removeItemAtPath:[Cronet getNetLogPathForFile:dir]
                 error:nil];

  EXPECT_TRUE(dir_created);
  EXPECT_TRUE(netlog_started);
}

TEST_F(NetLogTest, EmptyFilename) {
  bool netlog_started = [Cronet startNetLogToFile:@"" logBytes:NO];

  EXPECT_FALSE(netlog_started);
}

TEST_F(NetLogTest, AbsoluteFilename) {
  bool netlog_started =
      [Cronet startNetLogToFile:@"/home/netlog.json" logBytes:NO];

  EXPECT_FALSE(netlog_started);
}

TEST_F(NetLogTest, ExperimentalOptions) {
  [Cronet shutdownForTesting];
  NSString* netlog_file = @"cronet_netlog.json";
  NSString* netlog_path = [Cronet getNetLogPathForFile:netlog_file];

  // Remove old netlog if any.
  [[NSFileManager defaultManager] removeItemAtPath:netlog_path error:nil];

  // Set experimental options and start the netlog.
  [Cronet
      setExperimentalOptions:
          @"{ \"QUIC\" : {\"max_server_configs_stored_in_properties\" : 8} }"];

  StartCronet(net::QuicSimpleTestServer::GetPort());
  bool netlog_started =
      [Cronet startNetLogToFile:@"cronet_netlog.json" logBytes:NO];
  ASSERT_TRUE(netlog_started);

  // Stop the netlog and check that it contains the experimental options.
  [Cronet stopNetLog];

  NSError* error = nil;
  NSString* netlog_content =
      [NSString stringWithContentsOfFile:netlog_path
                                encoding:NSASCIIStringEncoding
                                   error:&error];
  ASSERT_FALSE(error) << error.localizedDescription.UTF8String;
  ASSERT_TRUE(netlog_content);
  ASSERT_TRUE([netlog_content
      containsString:@"\"cronetExperimentalParams\":{\"QUIC\":{\"max_server_"
                     @"configs_stored_in_properties\":8}}"])
      << "Netlog doesn't contain 'cronetExperimentalParams'.";
}
}
