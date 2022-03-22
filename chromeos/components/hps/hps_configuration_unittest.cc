// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/hps/hps_configuration.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace hps {

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST(HpsFeatureConfigTest, EmptyParamsValid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kQuickDim, ash::features::kSnoopingProtection},
      {} /* disabled_features */);

  EXPECT_EQ(GetEnableHpsSenseConfig()->filter_config_case(),
            hps::FeatureConfig::kConsecutiveResultsFilterConfig);
  EXPECT_EQ(GetEnableHpsNotifyConfig()->filter_config_case(),
            hps::FeatureConfig::kConsecutiveResultsFilterConfig);
}

TEST(HpsFeatureConfigTest, ReturnNullIfTypeIsNotRecognizableHpsSense) {
  const base::FieldTrialParams params = {{"QuickDim_filter_config_case", "0"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  EXPECT_FALSE(GetEnableHpsSenseConfig().has_value());
}

TEST(HpsFeatureConfigTest, ReturnNullIfTypeIsNotRecognizableHpsNotify) {
  const base::FieldTrialParams params = {
      {"SnoopingProtection_filter_config_case", "0"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  EXPECT_FALSE(GetEnableHpsNotifyConfig().has_value());
}

TEST(HpsFeatureConfigTest, VerifyBasicFilterConfigHpsSense) {
  const std::map<std::string, std::string> params = {
      {"QuickDim_filter_config_case", "1"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kQuickDim, params}}, {});

  hps::FeatureConfig expected_config;
  expected_config.mutable_basic_filter_config();

  EXPECT_THAT(GetEnableHpsSenseConfig().value(), ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyBasicFilterConfigHpsNotify) {
  const std::map<std::string, std::string> params = {
      {"SnoopingProtection_filter_config_case", "1"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{ash::features::kSnoopingProtection, params}}, {});

  hps::FeatureConfig expected_config;
  expected_config.mutable_basic_filter_config();

  EXPECT_THAT(GetEnableHpsNotifyConfig().value(), ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyConsecutiveResultsFilterConfigHpsSense) {
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

  const auto hps_sense_config = GetEnableHpsSenseConfig();
  EXPECT_THAT(*hps_sense_config, ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyConsecutiveResultsFilterConfigHpsNotify) {
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

  const auto hps_notify_config = GetEnableHpsNotifyConfig();
  EXPECT_THAT(*hps_notify_config, ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyAverageFilterConfigHpsSense) {
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

  const auto hps_sense_config = GetEnableHpsSenseConfig();
  EXPECT_THAT(*hps_sense_config, ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyAverageFilterConfigHpsNotify) {
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

  const auto hps_notify_config = GetEnableHpsNotifyConfig();
  EXPECT_THAT(*hps_notify_config, ProtoEquals(expected_config));
}

}  // namespace hps
