// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/client_lib/CRURegistration.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CRURegistrationTest, SmokeTest) {
  CRURegistration* registration = [[CRURegistration alloc]
      initWithAppId:
          @"org.chromium.ChromiumUpdater.CRURegistrationTest.SmokeTest"];
  ASSERT_TRUE(registration);
}

}  // namespace
