// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/startup_section.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAppsSectionPath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetAppRestoreSearchConcepts(
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_RESTORE_APPS_AND_PAGES,
       section_path,
       mojom::SearchResultIcon::kRestore,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRestoreAppsAndPages},
       {IDS_OS_SETTINGS_TAG_ON_STARTUP, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

StartupSection::StartupSection(Profile* profile,
                               SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  CHECK(profile);
  CHECK(search_tag_registry);

  if (IsAppRestoreAvailableForProfile(profile)) {
    SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
    updater.AddSearchTags(GetAppRestoreSearchConcepts(GetSectionPath()));
  }
}

StartupSection::~StartupSection() = default;

void StartupSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"onStartupSettingsCardTitle",
       IDS_OS_SETTINGS_ON_STARTUP_SETTINGS_CARD_TITLE},
      {"onStartupTitle", kIsRevampEnabled
                             ? IDS_OS_SETTINGS_REVAMP_ON_STARTUP_TITLE
                             : IDS_OS_SETTINGS_ON_STARTUP_TITLE},
      {"onStartupDescription", IDS_OS_SETTINGS_REVAMP_ON_STARTUP_DESCRIPTION},
      {"onStartupAlways", IDS_OS_SETTINGS_ON_STARTUP_ALWAYS},
      {"onStartupAskEveryTime", IDS_OS_SETTINGS_ON_STARTUP_ASK_EVERY_TIME},
      {"onStartupDoNotRestore", IDS_OS_SETTINGS_ON_STARTUP_DO_NOT_RESTORE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("shouldShowStartup",
                          IsAppRestoreAvailableForProfile(profile()));
}

void StartupSection::AddHandlers(content::WebUI* web_ui) {
  // No handlers registered.
}

int StartupSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_ON_STARTUP_SETTINGS_CARD_TITLE;
}

mojom::Section StartupSection::GetSection() const {
  // Note: This is a subsection that exists under Apps or System Preferences.
  // This section will no longer exist under the Apps section once the
  // OsSettingsRevampWayfinding feature is fully launched.
  // This is not a top-level section and does not have a respective declaration
  // in chromeos::settings::mojom::Section.
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kApps;
}

mojom::SearchResultIcon StartupSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kRestore;
}

const char* StartupSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kAppsSectionPath;
}

bool StartupSection::LogMetric(mojom::Setting setting,
                               base::Value& value) const {
  // No metrics are logged.
  return false;
}

void StartupSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kRestoreAppsAndPages);
}

}  // namespace ash::settings
