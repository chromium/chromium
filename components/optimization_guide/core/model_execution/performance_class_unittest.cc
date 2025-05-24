// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/performance_class.h"

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(PerformanceClassTest, IsPerformanceClassCompatible) {
  EXPECT_FALSE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kError));
  EXPECT_TRUE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kMedium));
  EXPECT_FALSE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kHigh));
  EXPECT_TRUE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kVeryHigh));
}

TEST(PerformanceClassTest, UpdatePrefs) {
  TestingPrefServiceSimple prefs_;
  model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kUnknown);

  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kMedium);

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kMedium);
}

TEST(PerformanceClassTest, ConvertFromMojom) {
  // Converting from mojom should not be lossy.
  using PerformanceClass = on_device_model::mojom::PerformanceClass;
  std::vector<PerformanceClass> inputs{
      PerformanceClass::kError,      PerformanceClass::kVeryLow,
      PerformanceClass::kLow,        PerformanceClass::kMedium,
      PerformanceClass::kHigh,       PerformanceClass::kVeryHigh,
      PerformanceClass::kGpuBlocked, PerformanceClass::kFailedToLoadLibrary,
  };
  std::set<OnDeviceModelPerformanceClass> outputs;
  for (auto mojo_val : inputs) {
    outputs.insert(ConvertToOnDeviceModelPerformanceClass(mojo_val));
  }
  EXPECT_EQ(outputs.size(), inputs.size());
}

TEST(PerformanceClassTest, GroupsAreUnique) {
  // Synthetic trial groups should be distinct.
  std::vector<OnDeviceModelPerformanceClass> inputs{
      OnDeviceModelPerformanceClass::kError,
      OnDeviceModelPerformanceClass::kVeryLow,
      OnDeviceModelPerformanceClass::kLow,
      OnDeviceModelPerformanceClass::kMedium,
      OnDeviceModelPerformanceClass::kHigh,
      OnDeviceModelPerformanceClass::kVeryHigh,
      OnDeviceModelPerformanceClass::kGpuBlocked,
      OnDeviceModelPerformanceClass::kFailedToLoadLibrary,
  };
  std::set<std::string> outputs;
  for (auto mojo_val : inputs) {
    outputs.insert(SyntheticTrialGroupForPerformanceClass(mojo_val));
  }
  EXPECT_EQ(outputs.size(), inputs.size());
}

}  // namespace

}  // namespace optimization_guide
