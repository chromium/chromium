// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/human_presence/human_presence_configuration.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"

namespace hps {
namespace {

// Default minimum amount of time for which a positive snooper presence will be
// reported.
constexpr base::TimeDelta kSnoopingProtectionPositiveWindowDefault =
    base::Seconds(3);

// Default quick dim delay to configure power_manager.
constexpr base::TimeDelta kQuickDimDelayDefault = base::Seconds(10);

// Default quick lock delay to configure power_manager.
constexpr base::TimeDelta kQuickLockDelayDefault = base::Seconds(130);

// Default value determines whether send feedback to configure power_manager.
constexpr int kShouldSendFeedbackIfUndimmed = false;

// Returns either the integer parameter with the given name, or nullopt if the
// parameter can't be found or parsed.
std::optional<int> GetIntParam(const base::FieldTrialParams& params,
                               const std::string& feature_name,
                               const std::string& param_name) {
  const std::string full_param_name =
      base::StrCat({feature_name, "_", param_name});
  const auto it = params.find(full_param_name);
  if (it == params.end())
    return std::nullopt;

  int result;
  if (!base::StringToInt(it->second, &result))
    return std::nullopt;

  return result;
}

// The config to use when no parameters are specified. We need to ensure that
// that empty params induce a working config (as required for, e.g., a
// chrome:://flags entry).
hps::FeatureConfig GetDefaultSnoopingProtectionConfig() {
  hps::FeatureConfig config;

  // Three consecutive positive/negative/unknown frames are required to change
  // the second person presence state to reduce noise.
  auto& filter_config = *config.mutable_consecutive_results_filter_config();
  filter_config.set_positive_count_threshold(3);
  filter_config.set_negative_count_threshold(3);
  filter_config.set_uncertain_count_threshold(3);
  filter_config.set_positive_score_threshold(40);
  filter_config.set_negative_score_threshold(40);

  return config;
}

hps::FeatureConfig GetDefaultLockOnLeaveConfig() {
  hps::FeatureConfig config;
  auto& filter_config = *config.mutable_consecutive_results_filter_config();
  // Any positive result will undim the screen.
  filter_config.set_positive_count_threshold(1);
  // Only dim if at least two consecutive negative result.
  filter_config.set_negative_count_threshold(2);
  // UNKNOWN will not undim, but it will block quick dim, so it should also be
  // fired with confidence.
  filter_config.set_uncertain_count_threshold(2);
  filter_config.set_positive_score_threshold(0);
  filter_config.set_negative_score_threshold(0);
  return config;
}

// This function constructs a FeatureConfig proto from feature parameters.
// The FeatureConfig contains one type of FilterConfig that will be used for
// enabling a human presence feature.
//
// If empty parameters are provided, a reasonable default is used.
//
// More details can be found at:
// src/platform2/hps/daemon/filters/filter_factory.h
std::optional<hps::FeatureConfig> ConstructFilterConfigFromFeatureParams(
    const base::Feature& feature,
    const hps::FeatureConfig& default_value) {
  // Load current params map for the feature.
  base::FieldTrialParams params;
  base::GetFieldTrialParamsByFeature(feature, &params);

  // Returns default if no params is set.
  if (params.empty()) {
    return default_value;
  }

  const std::string& feature_name = feature.name;

  const std::optional<int> filter_config_case =
      GetIntParam(params, feature_name, "filter_config_case");
  if (!filter_config_case.has_value()) {
    LOG(ERROR) << "Filter config error: missing param filter_config_case for "
               << feature_name;
    return std::nullopt;
  }

  switch (*filter_config_case) {
    case hps::FeatureConfig::kBasicFilterConfig: {
      hps::FeatureConfig config;
      config.mutable_basic_filter_config();
      return config;
    }

    case hps::FeatureConfig::kConsecutiveResultsFilterConfig: {
      const std::optional<int> positive_count_threshold =
          GetIntParam(params, feature_name, "positive_count_threshold");
      const std::optional<int> negative_count_threshold =
          GetIntParam(params, feature_name, "negative_count_threshold");
      const std::optional<int> uncertain_count_threshold =
          GetIntParam(params, feature_name, "uncertain_count_threshold");
      const std::optional<int> positive_score_threshold =
          GetIntParam(params, feature_name, "positive_score_threshold");
      const std::optional<int> negative_score_threshold =
          GetIntParam(params, feature_name, "negative_score_threshold");

      if (!positive_count_threshold.has_value() ||
          !negative_count_threshold.has_value() ||
          !uncertain_count_threshold.has_value() ||
          !positive_score_threshold.has_value() ||
          !negative_score_threshold.has_value()) {
        LOG(ERROR) << "Filter config error: missing params for "
                      "ConsecutiveResultsFilterConfig for "
                   << feature_name;
        return std::nullopt;
      }

      hps::FeatureConfig config;
      auto& filter_config = *config.mutable_consecutive_results_filter_config();
      filter_config.set_positive_count_threshold(*positive_count_threshold);
      filter_config.set_negative_count_threshold(*negative_count_threshold);
      filter_config.set_uncertain_count_threshold(*uncertain_count_threshold);
      filter_config.set_positive_score_threshold(*positive_score_threshold);
      filter_config.set_negative_score_threshold(*negative_score_threshold);
      return config;
    }

    case hps::FeatureConfig::kAverageFilterConfig: {
      const std::optional<int> average_window_size =
          GetIntParam(params, feature_name, "average_window_size");
      const std::optional<int> positive_score_threshold =
          GetIntParam(params, feature_name, "positive_score_threshold");
      const std::optional<int> negative_score_threshold =
          GetIntParam(params, feature_name, "negative_score_threshold");
      const std::optional<int> default_uncertain_score =
          GetIntParam(params, feature_name, "default_uncertain_score");

      if (!average_window_size.has_value() ||
          !positive_score_threshold.has_value() ||
          !negative_score_threshold.has_value() ||
          !default_uncertain_score.has_value()) {
        LOG(ERROR) << "Filter config error: missing params for "
                      "AverageFilterConfig for "
                   << feature_name;
        return std::nullopt;
      }

      hps::FeatureConfig config;
      auto& filter_config = *config.mutable_average_filter_config();
      filter_config.set_average_window_size(*average_window_size);
      filter_config.set_positive_score_threshold(*positive_score_threshold);
      filter_config.set_negative_score_threshold(*negative_score_threshold);
      filter_config.set_default_uncertain_score(*default_uncertain_score);
      return config;
    }

    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<hps::FeatureConfig> GetEnableLockOnLeaveConfig() {
  return ConstructFilterConfigFromFeatureParams(ash::features::kQuickDim,
                                                GetDefaultLockOnLeaveConfig());
}

std::optional<hps::FeatureConfig> GetEnableSnoopingProtectionConfig() {
  return ConstructFilterConfigFromFeatureParams(
      ash::features::kSnoopingProtection, GetDefaultSnoopingProtectionConfig());
}

base::TimeDelta GetQuickDimDelay() {
  const int quick_dim_ms = base::GetFieldTrialParamByFeatureAsInt(
      ash::features::kQuickDim, "QuickDim_quick_dim_ms",
      kQuickDimDelayDefault.InMilliseconds());
  return base::Milliseconds(quick_dim_ms);
}

base::TimeDelta GetQuickLockDelay() {
  const int quick_lock_ms = base::GetFieldTrialParamByFeatureAsInt(
      ash::features::kQuickDim, "QuickDim_quick_lock_ms",
      kQuickLockDelayDefault.InMilliseconds());
  return base::Milliseconds(quick_lock_ms);
}

bool GetQuickDimFeedbackEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kQuickDim, "QuickDim_send_feedback_if_undimmed",
      kShouldSendFeedbackIfUndimmed);
}

base::TimeDelta GetSnoopingProtectionPositiveWindow() {
  const int pos_window_ms = base::GetFieldTrialParamByFeatureAsInt(
      ash::features::kSnoopingProtection, "SnoopingProtection_pos_window_ms",
      kSnoopingProtectionPositiveWindowDefault.InMilliseconds());
  return base::Milliseconds(pos_window_ms);
}

}  // namespace hps
