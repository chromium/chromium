// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/storage/storage_section.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kDeviceSectionPath;
using ::chromeos::settings::mojom::kExternalStorageSubpagePath;
using ::chromeos::settings::mojom::kStorageSubpagePath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetDefaultSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_STORAGE,
       mojom::kStorageSubpagePath,
       mojom::SearchResultIcon::kStorage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kStorage},
       {IDS_OS_SETTINGS_TAG_STORAGE_ALT1, IDS_OS_SETTINGS_TAG_STORAGE_ALT2,
        IDS_OS_SETTINGS_TAG_STORAGE_ALT3, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetExternalStorageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_EXTERNAL_STORAGE,
       mojom::kExternalStorageSubpagePath,
       mojom::SearchResultIcon::kStorage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kExternalStorage}},
  });
  return *tags;
}

}  // namespace

StorageSection::StorageSection(Profile* profile,
                               SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  CHECK(profile);
  CHECK(search_tag_registry);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetDefaultSearchConcepts());

  if (IsExternalStorageEnabled(profile)) {
    updater.AddSearchTags(GetExternalStorageSearchConcepts());
  }
}

StorageSection::~StorageSection() = default;

void StorageSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kStorageStrings[] = {
      {"storageExternal", IDS_SETTINGS_STORAGE_EXTERNAL},
      {"storageExternalStorageEmptyListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_EMPTY_LIST_HEADER},
      {"storageExternalStorageListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_LIST_HEADER},
      {"storageItemApps", kIsRevampEnabled ? IDS_OS_SETTINGS_STORAGE_ITEM_APPS
                                           : IDS_SETTINGS_STORAGE_ITEM_APPS},
      {"storageItemOffline", IDS_SETTINGS_STORAGE_ITEM_OFFLINE},
      {"storageItemAvailable", IDS_SETTINGS_STORAGE_ITEM_AVAILABLE},
      {"storageItemCrostini", IDS_SETTINGS_STORAGE_ITEM_CROSTINI},
      {"storageItemInUse", IDS_SETTINGS_STORAGE_ITEM_IN_USE},
      {"storageItemMyFiles", IDS_SETTINGS_STORAGE_ITEM_MY_FILES},
      {"storageItemOtherUsers", IDS_SETTINGS_STORAGE_ITEM_OTHER_USERS},
      {"storageItemSystem", IDS_SETTINGS_STORAGE_ITEM_SYSTEM},
      {"storageItemEncryption", IDS_SETTINGS_STORAGE_ITEM_ENCRYPTION_LABEL},
      {"storageOverviewAriaLabel", IDS_SETTINGS_STORAGE_OVERVIEW_ARIA_LABEL},
      {"storageSizeComputing", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"storageSizeUnknown", IDS_SETTINGS_STORAGE_SIZE_UNKNOWN},
      {"storageSpaceCriticallyLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1},
      {"storageSpaceCriticallyLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2},
      {"storageSpaceCriticallyLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE},
      {"storageSpaceLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1},
      {"storageSpaceLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2},
      {"storageSpaceLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE},
      {"storageTitle", IDS_SETTINGS_STORAGE_TITLE},
  };
  html_source->AddLocalizedStrings(kStorageStrings);

  html_source->AddString(
      "storageAndroidAppsExternalDrivesNote",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_STORAGE_ANDROID_APPS_ACCESS_EXTERNAL_DRIVES_NOTE,
          chrome::kArcExternalStorageLearnMoreURL));

  // If Lacros is enabled the browsing data settings control will open a Lacros
  // window for browsing data belonging to the Lacros primary profile. The
  // signed-in ash user profile corresponds directly to the Lacros primary
  // profile so use the ash user's display name.
  // TODO(crbug.com/41484354): Explore exposing a better setting that allows
  // browsing data management for all browser profiles.
  if (crosapi::browser_util::IsLacrosEnabled()) {
    const user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(profile());
    CHECK(user);
    html_source->AddString("storageItemBrowsingData",
                           l10n_util::GetStringFUTF16(
                               IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA_LACROS,
                               user->GetDisplayName()));
  } else {
    html_source->AddString(
        "storageItemBrowsingData",
        l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA));
  }

  html_source->AddBoolean("isExternalStorageEnabled",
                          IsExternalStorageEnabled(profile()));
}

void StorageSection::AddHandlers(content::WebUI* web_ui) {
  // TODO(b/300151715) Currently StorageHandler is registered in
  // os_settings_ui.cc and should be registered here if possible.
}

int StorageSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_STORAGE_TITLE;
}

mojom::Section StorageSection::GetSection() const {
  // Note: This is a subsection that exists under Device or System Preferences.
  // This section will no longer exist under the Device section once the
  // OsSettingsRevampWayfinding feature is fully launched.
  // This is not a top-level section and does not have a respective declaration
  // in chromeos::settings::mojom::Section.
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kDevice;
}

mojom::SearchResultIcon StorageSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kStorage;
}

const char* StorageSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kDeviceSectionPath;
}

bool StorageSection::LogMetric(mojom::Setting setting,
                               base::Value& value) const {
  // No metrics are logged.
  return false;
}

void StorageSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_STORAGE_TITLE, mojom::Subpage::kStorage,
      mojom::SearchResultIcon::kStorage,
      mojom::SearchResultDefaultRank::kMedium, mojom::kStorageSubpagePath);
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_STORAGE_EXTERNAL, mojom::Subpage::kExternalStorage,
      mojom::Subpage::kStorage, mojom::SearchResultIcon::kStorage,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kExternalStorageSubpagePath);
}

}  // namespace ash::settings
