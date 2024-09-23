// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/system_preferences_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/date_time_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/files_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/languages/languages_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/power/power_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/reset/reset_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/storage_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/startup_section.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {
const std::vector<SearchConcept>& GetSystemPreferencesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SYSTEM_PREFERENCES,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kSystemPreferences,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kSystemPreferences}},
  });
  return *tags;
}
}  // namespace

SystemPreferencesSection::SystemPreferencesSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      date_time_subsection_(profile, search_tag_registry),
      files_subsection_(profile, search_tag_registry),
      languages_subsection_(profile, search_tag_registry, pref_service),
      multitasking_subsection_(profile, search_tag_registry),
      power_subsection_(profile, search_tag_registry, pref_service),
      reset_subsection_(profile, search_tag_registry),
      search_subsection_(profile, search_tag_registry),
      startup_subsection_(profile, search_tag_registry),
      storage_subsection_(profile, search_tag_registry) {
  CHECK(profile);
  CHECK(search_tag_registry);
  CHECK(pref_service);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetSystemPreferencesSearchConcepts());
}

SystemPreferencesSection::~SystemPreferencesSection() = default;

void SystemPreferencesSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  date_time_subsection_.AddLoadTimeData(html_source);
  files_subsection_.AddLoadTimeData(html_source);
  languages_subsection_.AddLoadTimeData(html_source);
  multitasking_subsection_.AddLoadTimeData(html_source);
  power_subsection_.AddLoadTimeData(html_source);
  reset_subsection_.AddLoadTimeData(html_source);
  search_subsection_.AddLoadTimeData(html_source);
  startup_subsection_.AddLoadTimeData(html_source);
  storage_subsection_.AddLoadTimeData(html_source);

  webui::LocalizedString kLocalizedStrings[] = {
      {"storageAndPowerTitle",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_STORAGE_AND_POWER_TITLE},
      {"systemPreferencesTitle", IDS_OS_SETTINGS_SYSTEM_PREFERENCES_TITLE},
      {"systemPreferencesMenuItemDescription",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_MENU_ITEM_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void SystemPreferencesSection::AddHandlers(content::WebUI* web_ui) {
  date_time_subsection_.AddHandlers(web_ui);
  files_subsection_.AddHandlers(web_ui);
  languages_subsection_.AddHandlers(web_ui);
  multitasking_subsection_.AddHandlers(web_ui);
  power_subsection_.AddHandlers(web_ui);
  reset_subsection_.AddHandlers(web_ui);
  search_subsection_.AddHandlers(web_ui);
  startup_subsection_.AddHandlers(web_ui);
  storage_subsection_.AddHandlers(web_ui);
}

int SystemPreferencesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_SYSTEM_PREFERENCES_TITLE;
}

mojom::Section SystemPreferencesSection::GetSection() const {
  return mojom::Section::kSystemPreferences;
}

mojom::SearchResultIcon SystemPreferencesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kSystemPreferences;
}

const char* SystemPreferencesSection::GetSectionPath() const {
  return mojom::kSystemPreferencesSectionPath;
}

bool SystemPreferencesSection::LogMetric(mojom::Setting setting,
                                         base::Value& value) const {
  return date_time_subsection_.LogMetric(setting, value) ||
         files_subsection_.LogMetric(setting, value) ||
         languages_subsection_.LogMetric(setting, value) ||
         multitasking_subsection_.LogMetric(setting, value) ||
         power_subsection_.LogMetric(setting, value) ||
         reset_subsection_.LogMetric(setting, value) ||
         search_subsection_.LogMetric(setting, value) ||
         startup_subsection_.LogMetric(setting, value) ||
         storage_subsection_.LogMetric(setting, value);
}

void SystemPreferencesSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  date_time_subsection_.RegisterHierarchy(generator);
  files_subsection_.RegisterHierarchy(generator);
  languages_subsection_.RegisterHierarchy(generator);
  multitasking_subsection_.RegisterHierarchy(generator);
  power_subsection_.RegisterHierarchy(generator);
  reset_subsection_.RegisterHierarchy(generator);
  search_subsection_.RegisterHierarchy(generator);
  startup_subsection_.RegisterHierarchy(generator);
  storage_subsection_.RegisterHierarchy(generator);
}

}  // namespace ash::settings
