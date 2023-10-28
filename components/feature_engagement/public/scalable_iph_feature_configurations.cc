// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/scalable_iph_feature_configurations.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {
namespace {

bool IsTrackingOnly() {
  return ash::features::IsScalableIphTrackingOnlyEnabled();
}

absl::optional<FeatureConfig> GetBaseConfig() {
  absl::optional<FeatureConfig> config = FeatureConfig();
  config->valid = true;
  config->availability = Comparator(ANY, 0);
  config->session_rate = Comparator(ANY, 0);
  config->session_rate_impact.type = SessionRateImpact::Type::NONE;
  config->blocked_by.type = BlockedBy::Type::NONE;
  config->blocking.type = Blocking::Type::NONE;
  config->tracking_only = IsTrackingOnly();
  return config;
}

EventConfig GetEventConfig(const std::string& event_name,
                           Comparator comparator) {
  return EventConfig(event_name, comparator, 7, 8);
}

absl::optional<FeatureConfig> GetUnlockedBasedConfig(
    const base::Feature* feature) {
  // TODO(b/308010596): Move other config.
  return absl::nullopt;
}

absl::optional<FeatureConfig> GetTimerBasedConfig(
    const base::Feature* feature) {
  // TODO(b/308010596): Move other config.
  return absl::nullopt;
}

absl::optional<FeatureConfig> GetHelpAppBasedConfig(
    const base::Feature* feature) {
  if (!ash::features::IsScalableIphClientConfigEnabled()) {
    return absl::nullopt;
  }

  if (kIPHScalableIphHelpAppBasedOneFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenChrome,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedOneTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedTwoFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(scalable_iph::kEventNameAppListShown,
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedTwoTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedThreeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenPersonalizationApp,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedThreeTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedFourFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenPlayStore,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedFourTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedFiveFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenGoogleDocs,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedFiveTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedSixFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenGooglePhotos,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedSixTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedSevenFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenSettingsPrinter,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedSevenTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedEightFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenPhoneHub,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedEightTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedNineFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenYouTube,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedNineTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedTenFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenFileManager,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedTenTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedNudgeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        "ScalableIphHelpAppBasedNudgeEventUsedNotUsed", Comparator(ANY, 0));
    config->trigger = EventConfig("ScalableIphHelpAppBasedNudgeTrigger",
                                  Comparator(EQUAL, 0), 1, 7);
    return config;
  }

  return absl::nullopt;
}

}  // namespace

absl::optional<FeatureConfig> GetScalableIphFeatureConfig(
    const base::Feature* feature) {
  if (absl::optional<FeatureConfig> help_app_based =
          GetHelpAppBasedConfig(feature)) {
    return help_app_based;
  }

  if (absl::optional<FeatureConfig> unlocked_based =
          GetUnlockedBasedConfig(feature)) {
    return unlocked_based;
  }

  if (absl::optional<FeatureConfig> timer_based =
          GetTimerBasedConfig(feature)) {
    return timer_based;
  }

  return absl::nullopt;
}

}  // namespace feature_engagement
