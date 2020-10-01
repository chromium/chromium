// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/personalization_section.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/chromeos/wallpaper_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

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

const std::vector<SearchConcept>& GetAmbientModeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE,
       mojom::kAmbientModeSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAmbientMode}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAmbientModeOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_CHOOSE_SOURCE,
       mojom::kAmbientModeSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAmbientModeSource}},
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_TURN_OFF,
       mojom::kAmbientModeSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAmbientModeOnOff},
       {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_GOOGLE_PHOTOS_ALBUM,
       mojom::kAmbientModeGooglePhotosAlbumSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAmbientModeGooglePhotosAlbum}},
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_ART_GALLERY_ALBUM,
       mojom::kAmbientModeArtGalleryAlbumSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAmbientModeArtGalleryAlbum}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAmbientModeOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_TURN_ON,
       mojom::kAmbientModeSubpagePath,
       mojom::SearchResultIcon::kWallpaper,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAmbientModeOnOff},
       {IDS_OS_SETTINGS_TAG_AMBIENT_MODE_TURN_ON_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsAmbientModeAllowed() {
  return chromeos::features::IsAmbientModeEnabled() &&
         ash::AmbientClient::Get()->IsAmbientModeAllowed();
}

bool IsAmbientModePhotoPreviewAllowed() {
  return chromeos::features::IsAmbientModePhotoPreviewEnabled();
}

GURL GetGooglePhotosURL() {
  return GURL(chrome::kGooglePhotosURL);
}

}  // namespace

PersonalizationSection::PersonalizationSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  // Personalization search tags are not added in guest mode.
  if (features::IsGuestModeActive())
    return;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPersonalizationSearchConcepts());

  if (IsAmbientModeAllowed()) {
    updater.AddSearchTags(GetAmbientModeSearchConcepts());

    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        ash::ambient::prefs::kAmbientModeEnabled,
        base::BindRepeating(
            &PersonalizationSection::OnAmbientModeEnabledStateChanged,
            base::Unretained(this)));
    OnAmbientModeEnabledStateChanged();
  }
}

PersonalizationSection::~PersonalizationSection() = default;

void PersonalizationSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"ambientModeTitle", IDS_OS_SETTINGS_AMBIENT_MODE_TITLE},
      {"ambientModeEnabled", IDS_OS_SETTINGS_AMBIENT_MODE_ENABLED},
      {"ambientModeDisabled", IDS_OS_SETTINGS_AMBIENT_MODE_DISABLED},
      {"ambientModePageDescription",
       IDS_OS_SETTINGS_AMBIENT_MODE_PAGE_DESCRIPTION},
      {"ambientModeOn", IDS_OS_SETTINGS_AMBIENT_MODE_ON},
      {"ambientModeOff", IDS_OS_SETTINGS_AMBIENT_MODE_OFF},
      {"ambientModeTopicSourceTitle",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_TITLE},
      {"ambientModeTopicSourceGooglePhotos",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS},
      {"ambientModeTopicSourceGooglePhotosDescription",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC},
      {"ambientModeTopicSourceGooglePhotosDescriptionNoAlbum",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC_NO_ALBUM},
      {"ambientModeTopicSourceArtGallery",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY},
      {"ambientModeTopicSourceArtGalleryDescription",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY_DESCRIPTION},
      {"ambientModeTopicSourceSelectedRow",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_SELECTED_ROW},
      {"ambientModeTopicSourceUnselectedRow",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_UNSELECTED_ROW},
      {"ambientModeTopicSourceSubpage",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_SUBPAGE},
      {"ambientModeWeatherTitle", IDS_OS_SETTINGS_AMBIENT_MODE_WEATHER_TITLE},
      {"ambientModeTemperatureUnitFahrenheit",
       IDS_OS_SETTINGS_AMBIENT_MODE_TEMPERATURE_UNIT_FAHRENHEIT},
      {"ambientModeTemperatureUnitCelsius",
       IDS_OS_SETTINGS_AMBIENT_MODE_TEMPERATURE_UNIT_CELSIUS},
      {"ambientModeAlbumsSubpageAlbumSelected",
       IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_SELECTED},
      {"ambientModeAlbumsSubpageAlbumUnselected",
       IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_UNSELECTED},
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
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(::features::kChangePictureVideoMode));
  html_source->AddBoolean("isAmbientModeEnabled", IsAmbientModeAllowed());
  html_source->AddBoolean("isAmbientModePhotoPreviewEnabled",
                          IsAmbientModePhotoPreviewAllowed());
  html_source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosTitle",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_TITLE,
          base::UTF8ToUTF16(GetGooglePhotosURL().spec())));
  html_source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosNoAlbum",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_NO_ALBUM,
          base::UTF8ToUTF16(GetGooglePhotosURL().spec())));
}

void PersonalizationSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::WallpaperHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::ChangePictureHandler>());

  if (!profile()->IsGuestSession() &&
      chromeos::features::IsAmbientModeEnabled()) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::AmbientModeHandler>());
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

  // Ambient mode.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_AMBIENT_MODE_TITLE, mojom::Subpage::kAmbientMode,
      mojom::SearchResultIcon::kWallpaper,
      mojom::SearchResultDefaultRank::kMedium, mojom::kAmbientModeSubpagePath);
  static constexpr mojom::Setting kAmbientModeSettings[] = {
      mojom::Setting::kAmbientModeOnOff,
      mojom::Setting::kAmbientModeSource,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kAmbientMode, kAmbientModeSettings,
                            generator);
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_AMBIENT_MODE_TITLE,
      mojom::Subpage::kAmbientModeGooglePhotosAlbum,
      mojom::Subpage::kAmbientMode, mojom::SearchResultIcon::kWallpaper,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kAmbientModeGooglePhotosAlbumSubpagePath);
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_AMBIENT_MODE_TITLE,
      mojom::Subpage::kAmbientModeArtGalleryAlbum, mojom::Subpage::kAmbientMode,
      mojom::SearchResultIcon::kWallpaper,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kAmbientModeArtGalleryAlbumSubpagePath);
}

void PersonalizationSection::OnAmbientModeEnabledStateChanged() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (pref_service_->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled)) {
    updater.AddSearchTags(GetAmbientModeOnSearchConcepts());
    updater.RemoveSearchTags(GetAmbientModeOffSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetAmbientModeOnSearchConcepts());
    updater.AddSearchTags(GetAmbientModeOffSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
