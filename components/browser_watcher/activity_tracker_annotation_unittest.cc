// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_tracker_annotation.h"

#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

class ActivityTrackerAnnotationTest : public testing::Test {
 public:
  void SetUp() override { crash_reporter::InitializeCrashKeysForTesting(); }
  void TearDown() override { crash_reporter::ResetCrashKeysForTesting(); }
};

TEST_F(ActivityTrackerAnnotationTest, RegistersOnFirstSet) {
  static const char* kBuffer[128];
  ActivityTrackerAnnotation* annotation =
      ActivityTrackerAnnotation::GetInstance();
  // Validate that the annotation doesn't register on construction.
  EXPECT_EQ("", crash_reporter::GetCrashKeyValue(
                    ActivityTrackerAnnotation::kAnnotationName));

  annotation->SetValue(&kBuffer, sizeof(kBuffer));
  std::string string_value = crash_reporter::GetCrashKeyValue(
      ActivityTrackerAnnotation::kAnnotationName);
  ASSERT_EQ(sizeof(ActivityTrackerAnnotation::ValueType), string_value.size());

  ActivityTrackerAnnotation::ValueType value = {};
  memcpy(&value, string_value.data(), sizeof(value));

  EXPECT_EQ(value.address, reinterpret_cast<uint64_t>(&kBuffer));
  EXPECT_EQ(value.size, sizeof(kBuffer));
}

}  // namespace browser_watcher
