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

TEST_F(CrashKeysAndroidTest, Default) {
  EXPECT_TRUE(GetCrashKeyValue("loaded_dynamic_module").empty());
  EXPECT_TRUE(GetCrashKeyValue("active_dynamic_module").empty());
}

TEST_F(CrashKeysAndroidTest, SetAndClear) {
  SetAndroidCrashKey(CrashKeyIndex::LOADED_DYNAMIC_MODULE, "foobar");
  SetAndroidCrashKey(CrashKeyIndex::ACTIVE_DYNAMIC_MODULE, "blurp");
  EXPECT_TRUE(GetCrashKeyValue("loaded_dynamic_module").empty());
  EXPECT_TRUE(GetCrashKeyValue("active_dynamic_module").empty());

  ClearAndroidCrashKey(CrashKeyIndex::ACTIVE_DYNAMIC_MODULE);
  FlushAndroidCrashKeys();
  EXPECT_EQ(GetCrashKeyValue("loaded_dynamic_module"), "foobar");
  EXPECT_TRUE(GetCrashKeyValue("active_dynamic_module").empty());

  ClearAndroidCrashKey(CrashKeyIndex::LOADED_DYNAMIC_MODULE);
  EXPECT_TRUE(GetCrashKeyValue("loaded_dynamic_module").empty());
  EXPECT_TRUE(GetCrashKeyValue("active_dynamic_module").empty());
}
