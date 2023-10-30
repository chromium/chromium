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

constexpr char kWelcomeTips[] = "Welcome Tips";

std::unique_ptr<Config> GetBaseConfig() {
  std::unique_ptr<Config> config = std::make_unique<Config>();
  config->params[kCustomConditionNetworkConnectionParamName] =
      kCustomConditionNetworkConnectionOnline;
  config->params[kCustomConditionClientAgeInDaysParamName] = "6";
  return config;
}

void AddWelcomeTipsBubbleParams(Config* config) {
  config->ui_type = UiType::kBubble;

  CHECK(!config->bubble_params) << "Bubble params is already set";
  config->bubble_params = std::make_unique<ScalableIphDelegate::BubbleParams>();
  config->bubble_params->title = kWelcomeTips;
}

std::unique_ptr<Config> GetUnlockedBasedConfig(const base::Feature& feature) {
  if (&feature == &feature_engagement::kIPHScalableIphUnlockedBasedOneFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_one";
    config->bubble_params->text =
        "Connect to the world on your Chromebook with Chrome browser";
    config->bubble_params->icon = ScalableIphDelegate::BubbleIcon::kChromeIcon;
    config->bubble_params->button.text = "Open Chrome";
    config->bubble_params->button.action.action_type = ActionType::kOpenChrome;
    return config;
  }

  if (&feature == &feature_engagement::kIPHScalableIphUnlockedBasedTwoFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_two";
    config->bubble_params->text = "Search and find your apps in the Launcher ◉";
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedThreeFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_three";
    config->bubble_params->text =
        "Make your Chromebook uniquely yours with a new wallpaper";
    config->bubble_params->button.text = "Select wallpaper";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenPersonalizationApp;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedFourFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_four";
    config->bubble_params->text = "Get your favorite apps from the Play Store";
    config->bubble_params->icon =
        ScalableIphDelegate::BubbleIcon::kPlayStoreIcon;
    config->bubble_params->button.text = "Open Play Store";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenPlayStore;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedFiveFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_five";
    config->bubble_params->text =
        "Create, edit, and collaborate with Google Docs";
    config->bubble_params->icon =
        ScalableIphDelegate::BubbleIcon::kGoogleDocsIcon;
    config->bubble_params->button.text = "Open Docs";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenGoogleDocs;
    return config;
  }

  if (&feature == &feature_engagement::kIPHScalableIphUnlockedBasedSixFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_six";
    config->bubble_params->text =
        "Explore your favorite memories in Google Photos";
    config->bubble_params->icon =
        ScalableIphDelegate::BubbleIcon::kGooglePhotosIcon;
    config->bubble_params->button.text = "Open Photos";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenGooglePhotos;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedSevenFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();

    config->params[kCustomConditionHasSavedPrintersParamName] =
        kCustomConditionHasSavedPrintersValueFalse;

    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_seven";
    config->bubble_params->text = "Easily add a printer to your Chromebook";
    config->bubble_params->icon =
        ScalableIphDelegate::BubbleIcon::kPrintJobsIcon;
    config->bubble_params->button.text = "Add printer";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenSettingsPrinter;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedEightFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();

    config->params[kCustomConditionPhoneHubOnboardingEligibleParamName] =
        kCustomConditionPhoneHubOnboardingEligibleValueTrue;

    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_eight";
    config->bubble_params->text =
        "Quickly reply to your messages from your Android phone";
    config->bubble_params->button.text = "Connect phone";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenPhoneHub;
    return config;
  }

  if (&feature ==
      &feature_engagement::kIPHScalableIphUnlockedBasedNineFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_nine";
    config->bubble_params->text = "Watch your favorite content on YouTube";
    config->bubble_params->icon = ScalableIphDelegate::BubbleIcon::kYouTubeIcon;
    config->bubble_params->button.text = "Open YouTube";
    config->bubble_params->button.action.action_type = ActionType::kOpenYouTube;
    return config;
  }

  if (&feature == &feature_engagement::kIPHScalableIphUnlockedBasedTenFeature) {
    std::unique_ptr<Config> config = GetBaseConfig();
    AddWelcomeTipsBubbleParams(config.get());
    config->bubble_params->bubble_id = "scalable_iph_unlocked_based_ten";
    config->bubble_params->text = "Printing is easy with your Chromebook";
    config->bubble_params->icon =
        ScalableIphDelegate::BubbleIcon::kPrintJobsIcon;
    config->bubble_params->button.text = "Select file";
    config->bubble_params->button.action.action_type =
        ActionType::kOpenFileManager;
    return config;
  }

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
