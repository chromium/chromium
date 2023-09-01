// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/system_preferences_section.h"

#include "chrome/browser/ui/webui/settings/ash/date_time_section.h"
#include "chrome/browser/ui/webui/settings/ash/languages_section.h"
#include "chrome/browser/ui/webui/settings/ash/reset_section.h"
#include "chrome/browser/ui/webui/settings/ash/search_section.h"
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

SystemPreferencesSection::SystemPreferencesSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      date_time_subsection_(profile, search_tag_registry),
      languages_subsection_(profile, search_tag_registry, pref_service),
      reset_subsection_(profile, search_tag_registry),
      search_subsection_(profile, search_tag_registry) {}

SystemPreferencesSection::~SystemPreferencesSection() = default;

void SystemPreferencesSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  date_time_subsection_.AddLoadTimeData(html_source);
  languages_subsection_.AddLoadTimeData(html_source);
  reset_subsection_.AddLoadTimeData(html_source);
  search_subsection_.AddLoadTimeData(html_source);

  webui::LocalizedString kLocalizedStrings[] = {
      {"storageAndPowerTitle",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_STORAGE_AND_POWER_TITLE},
      {"systemPreferencesTitle", IDS_OS_SETTINGS_SYSTEM_PREFERENCES_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void SystemPreferencesSection::AddHandlers(content::WebUI* web_ui) {
  date_time_subsection_.AddHandlers(web_ui);
  languages_subsection_.AddHandlers(web_ui);
  reset_subsection_.AddHandlers(web_ui);
  search_subsection_.AddHandlers(web_ui);
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
         languages_subsection_.LogMetric(setting, value) ||
         reset_subsection_.LogMetric(setting, value) ||
         search_subsection_.LogMetric(setting, value);
}

void SystemPreferencesSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  date_time_subsection_.RegisterHierarchy(generator);
  languages_subsection_.RegisterHierarchy(generator);
  reset_subsection_.RegisterHierarchy(generator);
  search_subsection_.RegisterHierarchy(generator);
}

}  // namespace ash::settings
