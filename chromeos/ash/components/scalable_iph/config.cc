// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/config.h"

#include "ash/constants/ash_features.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace scalable_iph {

namespace {

// This ID is from //chrome/browser/web_applications/web_app_id_constants.h. We
// cannot include the file from this directory as //chromeos should not depend
// on //chrome.
constexpr char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

std::unique_ptr<Config> GetBaseConfig() {
  std::unique_ptr<Config> config = std::make_unique<Config>();
  config->params[kCustomConditionNetworkConnectionParamName] =
      kCustomConditionNetworkConnectionOnline;
  config->params[kCustomConditionClientAgeInDaysParamName] = "6";
  return config;
}

std::unique_ptr<Config> GetUnlockedBasedConfig(const base::Feature& feature) {
  // TODO(b/308010596): Move other config.

  return nullptr;
}

std::unique_ptr<Config> GetTimerBasedConfig(const base::Feature& feature) {
  // TODO(b/308010596): Move other config.

  return nullptr;
}

std::unique_ptr<Config> GetHelpAppBasedConfig(const base::Feature& feature) {
  if (&feature == &feature_engagement::kIPHScalableIphHelpAppBasedOneFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedTwoFeature ||
      &feature ==
          &feature_engagement::kIPHScalableIphHelpAppBasedThreeFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedFourFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedFiveFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedSixFeature ||
      &feature ==
          &feature_engagement::kIPHScalableIphHelpAppBasedSevenFeature ||
      &feature ==
          &feature_engagement::kIPHScalableIphHelpAppBasedEightFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedNineFeature ||
      &feature == &feature_engagement::kIPHScalableIphHelpAppBasedTenFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    config->ui_type = UiType::kNone;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphHelpAppBasedNudgeFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    config->ui_type = UiType::kBubble;
    config->bubble_params =
        std::make_unique<ScalableIphDelegate::BubbleParams>();
    config->bubble_params->bubble_id = "scalable_iph_help_app_bubble";
    config->bubble_params->text = "Continue learning about your Chromebook";
    config->bubble_params->anchor_view_app_id = kHelpAppId;
    return config;
  }

  return nullptr;
}

}  // namespace

Config::Config() = default;
Config::~Config() = default;

std::unique_ptr<Config> GetConfig(const base::Feature& feature) {
  if (!ash::features::IsScalableIphClientConfigEnabled()) {
    return nullptr;
  }

  std::unique_ptr<Config> help_app_based = GetHelpAppBasedConfig(feature);
  if (help_app_based) {
    return help_app_based;
  }

  std::unique_ptr<Config> unlocked_based = GetUnlockedBasedConfig(feature);
  if (unlocked_based) {
    return unlocked_based;
  }

  std::unique_ptr<Config> timer_based = GetTimerBasedConfig(feature);
  if (timer_based) {
    return timer_based;
  }

  return nullptr;
}

}  // namespace scalable_iph
