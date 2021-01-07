// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_unittest.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

namespace {

void ClearUserDefaults() {
  @autoreleasepool {
    NSUserDefaults* userDefaults = [[NSUserDefaults alloc]
        initWithSuiteName:[NSString
                              stringWithUTF8String:kUserDefaultsSuiteName]];
    [userDefaults
        removeObjectForKey:[NSString stringWithUTF8String:kDevOverrideKeyUrl]];
    [userDefaults
        removeObjectForKey:[NSString
                               stringWithUTF8String:kDevOverrideKeyUseCUP]];
  }
}

}  // namespace

void DevOverrideTest::SetUp() {
  ClearUserDefaults();
}

void DevOverrideTest::TearDown() {
  ClearUserDefaults();
}

TEST_F(DevOverrideTest, TestDevOverrides) {
  std::unique_ptr<ExternalConstants> consts = CreateExternalConstants();

  @autoreleasepool {
    NSUserDefaults* userDefaults = [[NSUserDefaults alloc]
        initWithSuiteName:[NSString
                              stringWithUTF8String:kUserDefaultsSuiteName]];
    [userDefaults setURL:[NSURL URLWithString:@"http://localhost:8080"]
                  forKey:[NSString stringWithUTF8String:kDevOverrideKeyUrl]];
    [userDefaults
        setBool:NO
         forKey:[NSString stringWithUTF8String:kDevOverrideKeyUseCUP]];
  }

  EXPECT_FALSE(consts->UseCUP());
  std::vector<GURL> urls = consts->UpdateURL();
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], GURL("http://localhost:8080"));
  ASSERT_TRUE(urls[0].is_valid());
}

}  // namespace updater
