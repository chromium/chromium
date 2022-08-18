// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/personalization_section.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/personalization_hub_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/wallpaper_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetPersonalizationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PERSONALIZATION,
       mojom::kPersonalizationSectionPath,
       mojom::SearchResultIcon::kPaintbrush,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPersonalization},
       {IDS_OS_SETTINGS_TAG_PERSONALIZATION_ALT1,
        IDS_OS_SETTINGS_TAG_PERSONALIZATION_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CHANGE_WALLPAPER,
       mojom::kPersonalizationSectionPath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOpenWallpaper},
       {IDS_OS_SETTINGS_TAG_CHANGE_WALLPAPER_ALT1,
        IDS_OS_SETTINGS_TAG_CHANGE_WALLPAPER_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CHANGE_DEVICE_ACCOUNT_IMAGE,
       mojom::kChangePictureSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeDeviceAccountImage},
       {IDS_OS_SETTINGS_TAG_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT1,
        IDS_OS_SETTINGS_TAG_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT2,
        IDS_OS_SETTINGS_TAG_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT3,
        IDS_OS_SETTINGS_TAG_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT4,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

PersonalizationSection::PersonalizationSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry) {
  // Personalization search tags are not added in guest mode.
  if (features::IsGuestModeActive())
    return;

  if (ash::features::IsPersonalizationHubEnabled()) {
    // Personalization search is handled by Personalization Hub when feature is
    // on.
    return;
  }

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPersonalizationSearchConcepts());
}

PersonalizationSection::~PersonalizationSection() = default;

void PersonalizationSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"changePictureTitle", IDS_OS_SETTINGS_CHANGE_PICTURE_TITLE},
      {"openWallpaperApp", IDS_OS_SETTINGS_OPEN_WALLPAPER_APP},
      {"personalizationPageTitle", IDS_OS_SETTINGS_PERSONALIZATION},
      {"setWallpaper", IDS_OS_SETTINGS_SET_WALLPAPER},
      {"takePhoto", IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO},
      {"captureVideo", IDS_SETTINGS_CHANGE_PICTURE_CAPTURE_VIDEO},
      {"discardPhoto", IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO},
      {"previewAltText", IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT},
      {"switchModeToVideo", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_VIDEO},
      {"profilePhoto", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO},
      {"changePicturePageDescription", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT},
      {"switchModeToCamera", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_CAMERA},
      {"chooseFile", IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE},
      {"oldPhoto", IDS_SETTINGS_CHANGE_PICTURE_OLD_PHOTO},
      {"oldVideo", IDS_SETTINGS_CHANGE_PICTURE_OLD_VIDEO},
      {"authorCreditText", IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_CREDIT_TEXT},
      {"photoCaptureAccessibleText",
       IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT},
      {"photoDiscardAccessibleText",
       IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT},
      {"photoModeAccessibleText", IDS_SETTINGS_PHOTO_MODE_ACCESSIBLE_TEXT},
      {"videoModeAccessibleText", IDS_SETTINGS_VIDEO_MODE_ACCESSIBLE_TEXT},
      {"personalizationHubTitle", IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB},
      {"personalizationHubSubtitle",
       IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB_SUBTITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(::features::kChangePictureVideoMode));
}

void PersonalizationSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::WallpaperHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::ChangePictureHandler>());
  if (ash::features::IsPersonalizationHubEnabled()) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::PersonalizationHubHandler>());
  }
}

int PersonalizationSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_PERSONALIZATION;
}

mojom::Section PersonalizationSection::GetSection() const {
  return mojom::Section::kPersonalization;
}

mojom::SearchResultIcon PersonalizationSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPaintbrush;
}

std::string PersonalizationSection::GetSectionPath() const {
  return mojom::kPersonalizationSectionPath;
}

bool PersonalizationSection::LogMetric(mojom::Setting setting,
                                       base::Value& value) const {
  // Unimplemented.
  return false;
}

void PersonalizationSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kOpenWallpaper);

  // Change picture.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_CHANGE_PICTURE_TITLE, mojom::Subpage::kChangePicture,
      mojom::SearchResultIcon::kAvatar, mojom::SearchResultDefaultRank::kMedium,
      mojom::kChangePictureSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kChangeDeviceAccountImage,
                                   mojom::Subpage::kChangePicture);
}

}  // namespace settings
}  // namespace chromeos
