// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals::features {

class SignalsFeaturesTest : public testing::Test {
 protected:
  const int min_enum_value_ = static_cast<int>(NewEvFunction::kFileSystemInfo);
  const int max_enum_value_ = static_cast<int>(NewEvFunction::kHotfix);
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that IsNewFunctionEnabled will return false when the feature flag is
// disabled.
TEST_F(SignalsFeaturesTest, DisabledFeature) {
  scoped_features_.InitAndDisableFeature(kNewEvSignalsEnabled);
  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    EXPECT_FALSE(IsNewFunctionEnabled(static_cast<NewEvFunction>(i)));
  }
}

// Tests that IsNewFunctionEnabled will return true when the feature flag is
// enabled, and no specific function is disabled.
TEST_F(SignalsFeaturesTest, EnabledFeature) {
  scoped_features_.InitAndEnableFeature(kNewEvSignalsEnabled);
  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    EXPECT_TRUE(IsNewFunctionEnabled(static_cast<NewEvFunction>(i)));
  }
}

// Tests how IsNewFunctionEnabled behaves when the feature flag is enabled, but
// the FileSystemInfo function is disabled.
TEST_F(SignalsFeaturesTest, Enabled_DisableFileSystemInfo) {
  scoped_features_.InitAndEnableFeatureWithParameters(
      kNewEvSignalsEnabled, {{"DisableFileSystemInfo", "true"}});

  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    NewEvFunction current_function = static_cast<NewEvFunction>(i);
    EXPECT_EQ(IsNewFunctionEnabled(current_function),
              current_function != NewEvFunction::kFileSystemInfo);
  }
}

// Tests how IsNewFunctionEnabled behaves when the feature flag is enabled, but
// the Settings function is disabled.
TEST_F(SignalsFeaturesTest, Enabled_DisableSetting) {
  scoped_features_.InitAndEnableFeatureWithParameters(
      kNewEvSignalsEnabled, {{"DisableSettings", "true"}});

  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    NewEvFunction current_function = static_cast<NewEvFunction>(i);
    EXPECT_EQ(IsNewFunctionEnabled(current_function),
              current_function != NewEvFunction::kSettings);
  }
}

// Tests how IsNewFunctionEnabled behaves when the feature flag is enabled, but
// the Antivirus function is disabled.
TEST_F(SignalsFeaturesTest, Enabled_DisableAntiVirus) {
  scoped_features_.InitAndEnableFeatureWithParameters(
      kNewEvSignalsEnabled, {{"DisableAntiVirus", "true"}});

  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    NewEvFunction current_function = static_cast<NewEvFunction>(i);
    EXPECT_EQ(IsNewFunctionEnabled(current_function),
              current_function != NewEvFunction::kAntiVirus);
  }
}

// Tests how IsNewFunctionEnabled behaves when the feature flag is enabled, but
// the Hotfix function is disabled.
TEST_F(SignalsFeaturesTest, Enabled_DisableHotfix) {
  scoped_features_.InitAndEnableFeatureWithParameters(
      kNewEvSignalsEnabled, {{"DisableHotfix", "true"}});

  for (int i = min_enum_value_; i <= max_enum_value_; i++) {
    NewEvFunction current_function = static_cast<NewEvFunction>(i);
    EXPECT_EQ(IsNewFunctionEnabled(current_function),
              current_function != NewEvFunction::kHotfix);
  }
}

}  // namespace enterprise_signals::features
