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

std::optional<FeatureConfig> GetBaseConfig() {
  std::optional<FeatureConfig> config = FeatureConfig();
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

void AddPreconditionLauncher(FeatureConfig* config) {
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameAppListShown, Comparator(EQUAL, 0)));
}

void AddPreconditionPersonalizationApp(FeatureConfig* config) {
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameOpenPersonalizationApp, Comparator(EQUAL, 0)));
}

void AddPreconditionPlayStore(FeatureConfig* config) {
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameShelfItemActivationGooglePlay,
                     Comparator(EQUAL, 0)));
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameAppListItemActivationGooglePlayStore,
      Comparator(EQUAL, 0)));
}

void AddPreconditionGoogleDocs(FeatureConfig* config) {
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameShelfItemActivationGoogleDocs,
                     Comparator(EQUAL, 0)));
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameAppListItemActivationGoogleDocs,
                     Comparator(EQUAL, 0)));
}

void AddPreconditionGooglePhotos(FeatureConfig* config) {
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameShelfItemActivationGooglePhotosWeb,
                     Comparator(EQUAL, 0)));
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameShelfItemActivationGooglePhotosAndroid,
      Comparator(EQUAL, 0)));
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameAppListItemActivationGooglePhotosWeb,
      Comparator(EQUAL, 0)));
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNameAppListItemActivationGooglePhotosAndroid,
      Comparator(EQUAL, 0)));
}

void AddPreconditionYouTube(FeatureConfig* config) {
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameShelfItemActivationYouTube,
                     Comparator(EQUAL, 0)));
  config->event_configs.insert(
      GetEventConfig(scalable_iph::kEventNameAppListItemActivationYouTube,
                     Comparator(EQUAL, 0)));
}

void AddPreconditionPrintJob(FeatureConfig* config) {
  config->event_configs.insert(GetEventConfig(
      scalable_iph::kEventNamePrintJobCreated, Comparator(EQUAL, 0)));
}

std::optional<FeatureConfig> GetUnlockedBasedConfig(
    const base::Feature* feature) {
  if (kIPHScalableIphUnlockedBasedOneFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedOneEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedOneTriggered",
                                     Comparator(EQUAL, 0));
    return config;
  }

  if (kIPHScalableIphUnlockedBasedTwoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(scalable_iph::kEventNameAppListShown,
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedTwoTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 1)));
    AddPreconditionLauncher(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedThreeFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedThreeEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedThreeTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 2)));
    AddPreconditionPersonalizationApp(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedFourFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedFourEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedFourTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 3)));
    AddPreconditionPlayStore(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedFiveFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedFiveEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedFiveTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 4)));
    AddPreconditionGoogleDocs(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedSixFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedSixEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedSixTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 5)));
    AddPreconditionGooglePhotos(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedSevenFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedSevenEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedSevenTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 6)));
    return config;
  }

  if (kIPHScalableIphUnlockedBasedEightFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedEightEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedEightTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 7)));
    return config;
  }

  if (kIPHScalableIphUnlockedBasedNineFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedNineEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedNineTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 8)));
    AddPreconditionYouTube(&config.value());
    return config;
  }

  if (kIPHScalableIphUnlockedBasedTenFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig("ScalableIphUnlockedBasedTenEventUsed",
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphUnlockedBasedTenTriggered",
                                     Comparator(EQUAL, 0));
    config->event_configs.insert(
        GetEventConfig(scalable_iph::kEventNameUnlocked,
                       Comparator(GREATER_THAN_OR_EQUAL, 9)));
    AddPreconditionPrintJob(&config.value());
    return config;
  }

  return std::nullopt;
}

std::optional<FeatureConfig> GetTimerBasedConfig(const base::Feature* feature) {
  // TODO(b/308010596): Move other config.
  return std::nullopt;
}

std::optional<FeatureConfig> GetHelpAppBasedConfig(
    const base::Feature* feature) {
  if (!ash::features::IsScalableIphClientConfigEnabled()) {
    return std::nullopt;
  }

  if (kIPHScalableIphHelpAppBasedOneFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenChrome,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedOneTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedTwoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(scalable_iph::kEventNameAppListShown,
                                  Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedTwoTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedThreeFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenPersonalizationApp,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedThreeTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedFourFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenPlayStore,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedFourTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedFiveFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenGoogleDocs,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedFiveTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedSixFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenGooglePhotos,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedSixTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedSevenFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        scalable_iph::kEventNameHelpAppActionTypeOpenSettingsPrinter,
        Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedSevenTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedEightFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenPhoneHub,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedEightTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedNineFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenYouTube,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig(
        "ScalableIphHelpAppBasedNineTriggerNotUsed", Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedTenFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        GetEventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenFileManager,
                       Comparator(ANY, 0));
    config->trigger = GetEventConfig("ScalableIphHelpAppBasedTenTriggerNotUsed",
                                     Comparator(EQUAL, 1));
    return config;
  }

  if (kIPHScalableIphHelpAppBasedNudgeFeature.name == feature->name) {
    std::optional<FeatureConfig> config = GetBaseConfig();
    config->used = GetEventConfig(
        "ScalableIphHelpAppBasedNudgeEventUsedNotUsed", Comparator(ANY, 0));
    config->trigger = EventConfig("ScalableIphHelpAppBasedNudgeTrigger",
                                  Comparator(EQUAL, 0), 1, 7);
    return config;
  }

  return std::nullopt;
}

}  // namespace

std::optional<FeatureConfig> GetScalableIphFeatureConfig(
    const base::Feature* feature) {
  if (std::optional<FeatureConfig> help_app_based =
          GetHelpAppBasedConfig(feature)) {
    return help_app_based;
  }

  if (std::optional<FeatureConfig> unlocked_based =
          GetUnlockedBasedConfig(feature)) {
    return unlocked_based;
  }

  if (std::optional<FeatureConfig> timer_based = GetTimerBasedConfig(feature)) {
    return timer_based;
  }

  return std::nullopt;
}

}  // namespace feature_engagement
