// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/human_presence/human_presence_configuration.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace hps {

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST(HumanPresenceFeatureConfigTest, EmptyParamsValid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kQuickDim, ash::features::kSnoopingProtection},
      {} /* disabled_features */);

  EXPECT_EQ(GetEnableLockOnLeaveConfig()->filter_config_case(),
            hps::FeatureConfig::kConsecutiveResultsFilterConfig);
  EXPECT_EQ(GetEnableSnoopingProtectionConfig()->filter_config_case(),
            hps::FeatureConfig::kConsecutiveResultsFilterConfig);
}

TEST(HumanPresenceFeatureConfigTest,
     ReturnNullIfTypeIsNotRecognizableLockOnLeave) {
  const base::FieldTrialParams params = {{"QuickDim_filter_config_case", "0"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  EXPECT_FALSE(GetEnableLockOnLeaveConfig().has_value());
}

TEST(HumanPresenceFeatureConfigTest,
     ReturnNullIfTypeIsNotRecognizableSnoopingProtection) {
  const base::FieldTrialParams params = {
      {"SnoopingProtection_filter_config_case", "0"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  EXPECT_FALSE(GetEnableSnoopingProtectionConfig().has_value());
}

TEST(HumanPresenceFeatureConfigTest, VerifyBasicFilterConfigLockOnLeave) {
  const std::map<std::string, std::string> params = {
      {"QuickDim_filter_config_case", "1"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  hps::FeatureConfig expected_config;
  expected_config.mutable_basic_filter_config();

  EXPECT_THAT(GetEnableLockOnLeaveConfig().value(),
              ProtoEquals(expected_config));
}

TEST(HumanPresenceFeatureConfigTest,
     VerifyBasicFilterConfigSnoopingProtection) {
  const std::map<std::string, std::string> params = {
      {"SnoopingProtection_filter_config_case", "1"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  hps::FeatureConfig expected_config;
  expected_config.mutable_basic_filter_config();

  EXPECT_THAT(GetEnableSnoopingProtectionConfig().value(),
              ProtoEquals(expected_config));
}

TEST(HumanPresenceFeatureConfigTest,
     VerifyConsecutiveResultsFilterConfigLockOnLeave) {
  const std::map<std::string, std::string> params = {
      {"QuickDim_filter_config_case", "2"},
      {"QuickDim_positive_count_threshold", "3"},
      {"QuickDim_negative_count_threshold", "4"},
      {"QuickDim_uncertain_count_threshold", "5"},
      {"QuickDim_positive_score_threshold", "7"},
      {"QuickDim_negative_score_threshold", "6"},
  };
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  hps::FeatureConfig expected_config;
  auto& consecutive_results_filter_config =
      *expected_config.mutable_consecutive_results_filter_config();
  consecutive_results_filter_config.set_positive_count_threshold(3);
  consecutive_results_filter_config.set_negative_count_threshold(4);
  consecutive_results_filter_config.set_uncertain_count_threshold(5);
  consecutive_results_filter_config.set_positive_score_threshold(7);
  consecutive_results_filter_config.set_negative_score_threshold(6);

  const auto lock_on_leave = GetEnableLockOnLeaveConfig();
  EXPECT_THAT(*lock_on_leave, ProtoEquals(expected_config));
}

TEST(HumanPresenceFeatureConfigTest,
     VerifyConsecutiveResultsFilterConfigSnoopingProtection) {
  const std::map<std::string, std::string> params = {
      {"SnoopingProtection_filter_config_case", "2"},
      {"SnoopingProtection_positive_count_threshold", "3"},
      {"SnoopingProtection_negative_count_threshold", "4"},
      {"SnoopingProtection_uncertain_count_threshold", "5"},
      {"SnoopingProtection_positive_score_threshold", "7"},
      {"SnoopingProtection_negative_score_threshold", "6"},
  };
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  hps::FeatureConfig expected_config;
  auto& consecutive_results_filter_config =
      *expected_config.mutable_consecutive_results_filter_config();
  consecutive_results_filter_config.set_positive_count_threshold(3);
  consecutive_results_filter_config.set_negative_count_threshold(4);
  consecutive_results_filter_config.set_uncertain_count_threshold(5);
  consecutive_results_filter_config.set_positive_score_threshold(7);
  consecutive_results_filter_config.set_negative_score_threshold(6);

  const auto snooping_protection_config = GetEnableSnoopingProtectionConfig();
  EXPECT_THAT(*snooping_protection_config, ProtoEquals(expected_config));
}

TEST(HumanPresenceFeatureConfigTest, VerifyAverageFilterConfigLockOnLeave) {
  const std::map<std::string, std::string> params = {
      {"QuickDim_filter_config_case", "3"},
      {"QuickDim_average_window_size", "4"},
      {"QuickDim_positive_score_threshold", "5"},
      {"QuickDim_negative_score_threshold", "6"},
      {"QuickDim_default_uncertain_score", "7"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  hps::FeatureConfig expected_config;
  auto& average_filter_config =
      *expected_config.mutable_average_filter_config();
  average_filter_config.set_average_window_size(4);
  average_filter_config.set_positive_score_threshold(5);
  average_filter_config.set_negative_score_threshold(6);
  average_filter_config.set_default_uncertain_score(7);

  const auto lock_on_leave = GetEnableLockOnLeaveConfig();
  EXPECT_THAT(*lock_on_leave, ProtoEquals(expected_config));
}

TEST(HumanPresenceFeatureConfigTest,
     VerifyAverageFilterConfigSnoopingProtection) {
  const std::map<std::string, std::string> params = {
      {"SnoopingProtection_filter_config_case", "3"},
      {"SnoopingProtection_average_window_size", "4"},
      {"SnoopingProtection_positive_score_threshold", "5"},
      {"SnoopingProtection_negative_score_threshold", "6"},
      {"SnoopingProtection_default_uncertain_score", "7"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  hps::FeatureConfig expected_config;
  auto& average_filter_config =
      *expected_config.mutable_average_filter_config();
  average_filter_config.set_average_window_size(4);
  average_filter_config.set_positive_score_threshold(5);
  average_filter_config.set_negative_score_threshold(6);
  average_filter_config.set_default_uncertain_score(7);

  const auto snooping_protection_config = GetEnableSnoopingProtectionConfig();
  EXPECT_THAT(*snooping_protection_config, ProtoEquals(expected_config));
}

}  // namespace hps
