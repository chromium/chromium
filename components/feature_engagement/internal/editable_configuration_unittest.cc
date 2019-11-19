// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/editable_configuration.h"

#include <string>

#include "base/feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const base::Feature kEditableTestFeatureFoo{"test_foo",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEditableTestFeatureBar{"test_bar",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

class EditableConfigurationTest : public ::testing::Test {
 public:
  FeatureConfig CreateFeatureConfig(const std::string& feature_used_event,
                                    bool valid) {
    FeatureConfig feature_config;
    feature_config.valid = valid;
    feature_config.used.name = feature_used_event;
    return feature_config;
  }

 protected:
  EditableConfiguration configuration_;
};

}  // namespace

TEST_F(EditableConfigurationTest, SingleConfigAddAndGet) {
  FeatureConfig foo_config = CreateFeatureConfig("foo", true);
  configuration_.SetConfiguration(&kEditableTestFeatureFoo, foo_config);
  const FeatureConfig& foo_config_result =
      configuration_.GetFeatureConfig(kEditableTestFeatureFoo);

  EXPECT_EQ(foo_config, foo_config_result);
}

TEST_F(EditableConfigurationTest, TwoConfigAddAndGet) {
  FeatureConfig foo_config = CreateFeatureConfig("foo", true);
  configuration_.SetConfiguration(&kEditableTestFeatureFoo, foo_config);
  FeatureConfig bar_config = CreateFeatureConfig("bar", true);
  configuration_.SetConfiguration(&kEditableTestFeatureBar, bar_config);

  const FeatureConfig& foo_config_result =
      configuration_.GetFeatureConfig(kEditableTestFeatureFoo);
  const FeatureConfig& bar_config_result =
      configuration_.GetFeatureConfig(kEditableTestFeatureBar);

  EXPECT_EQ(foo_config, foo_config_result);
  EXPECT_EQ(bar_config, bar_config_result);
}

TEST_F(EditableConfigurationTest, ConfigShouldBeEditable) {
  FeatureConfig valid_foo_config = CreateFeatureConfig("foo", true);
  configuration_.SetConfiguration(&kEditableTestFeatureFoo, valid_foo_config);

  const FeatureConfig& valid_foo_config_result =
      configuration_.GetFeatureConfig(kEditableTestFeatureFoo);
  EXPECT_EQ(valid_foo_config, valid_foo_config_result);

  FeatureConfig invalid_foo_config = CreateFeatureConfig("foo2", false);
  configuration_.SetConfiguration(&kEditableTestFeatureFoo, invalid_foo_config);
  const FeatureConfig& invalid_foo_config_result =
      configuration_.GetFeatureConfig(kEditableTestFeatureFoo);
  EXPECT_EQ(invalid_foo_config, invalid_foo_config_result);
}

}  // namespace feature_engagement
