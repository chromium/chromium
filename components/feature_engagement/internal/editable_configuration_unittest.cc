// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/editable_configuration.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kEditableTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEditableTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEditableTestGroupFeatureFoo,
             "test_group_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEditableTestGroupFeatureBar,
             "test_group_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);

class EditableConfigurationTest : public ::testing::Test {
 public:
  FeatureConfig CreateFeatureConfig(const std::string& feature_used_event,
                                    bool valid) {
    FeatureConfig feature_config;
    feature_config.valid = valid;
    feature_config.used.name = feature_used_event;
    return feature_config;
  }

  GroupConfig CreateGroupConfig(bool valid) {
    GroupConfig group_config;
    group_config.valid = valid;
    return group_config;
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

TEST_F(EditableConfigurationTest, SingleGropuConfigAddAndGet) {
  GroupConfig foo_config = CreateGroupConfig(true);
  configuration_.SetConfiguration(&kEditableTestGroupFeatureFoo, foo_config);
  const GroupConfig& foo_config_result =
      configuration_.GetGroupConfig(kEditableTestGroupFeatureFoo);

  EXPECT_EQ(foo_config, foo_config_result);
}

TEST_F(EditableConfigurationTest, TwoGroupConfigAddAndGet) {
  GroupConfig foo_config = CreateGroupConfig(true);
  configuration_.SetConfiguration(&kEditableTestGroupFeatureFoo, foo_config);
  GroupConfig bar_config = CreateGroupConfig(false);
  configuration_.SetConfiguration(&kEditableTestGroupFeatureBar, bar_config);

  const GroupConfig& foo_config_result =
      configuration_.GetGroupConfig(kEditableTestGroupFeatureFoo);
  const GroupConfig& bar_config_result =
      configuration_.GetGroupConfig(kEditableTestGroupFeatureBar);

  EXPECT_EQ(foo_config, foo_config_result);
  EXPECT_EQ(bar_config, bar_config_result);
}

TEST_F(EditableConfigurationTest, GroupConfigShouldBeEditable) {
  GroupConfig valid_foo_config = CreateGroupConfig(true);
  configuration_.SetConfiguration(&kEditableTestGroupFeatureFoo,
                                  valid_foo_config);

  const GroupConfig& valid_foo_config_result =
      configuration_.GetGroupConfig(kEditableTestGroupFeatureFoo);
  EXPECT_EQ(valid_foo_config, valid_foo_config_result);

  GroupConfig invalid_foo_config = CreateGroupConfig(false);
  configuration_.SetConfiguration(&kEditableTestGroupFeatureFoo,
                                  invalid_foo_config);
  const GroupConfig& invalid_foo_config_result =
      configuration_.GetGroupConfig(kEditableTestGroupFeatureFoo);
  EXPECT_EQ(invalid_foo_config, invalid_foo_config_result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(EditableConfigurationTest, SinglePrefixAddAndGet) {
  const std::string foo_prefix = "FooEventPrefix";
  configuration_.AddAllowedEventPrefix(foo_prefix);
  const auto& foo_prefix_result =
      configuration_.GetRegisteredAllowedEventPrefixes();
  EXPECT_EQ(1u, foo_prefix_result.size());
  EXPECT_TRUE(foo_prefix_result.contains(foo_prefix));
}
#endif

}  // namespace feature_engagement
