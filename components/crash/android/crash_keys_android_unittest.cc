// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/crash_keys_android.h"

#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using crash_reporter::GetCrashKeyValue;

class CrashKeysAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeys();
  }

  void TearDown() override { crash_reporter::ResetCrashKeysForTesting(); }
};

TEST_F(CrashKeysAndroidTest, SetAndClear) {
  EXPECT_TRUE(GetCrashKeyValue("installed_modules").empty());

  SetAndroidCrashKey(CrashKeyIndex::INSTALLED_MODULES, "foobar");
  EXPECT_TRUE(GetCrashKeyValue("installed_modules").empty());

  ClearAndroidCrashKey(CrashKeyIndex::APPLICATION_STATUS);
  FlushAndroidCrashKeys();
  EXPECT_EQ(GetCrashKeyValue("installed_modules"), "foobar");
  EXPECT_TRUE(GetCrashKeyValue("application_status").empty());

  ClearAndroidCrashKey(CrashKeyIndex::INSTALLED_MODULES);
  EXPECT_TRUE(GetCrashKeyValue("installed_modules").empty());
  EXPECT_TRUE(GetCrashKeyValue("application_status").empty());
}
