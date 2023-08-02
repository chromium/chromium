// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/system_preferences_section.h"

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
    SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {}

SystemPreferencesSection::~SystemPreferencesSection() {}

void SystemPreferencesSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"systemPreferencesTitle", IDS_OS_SETTINGS_SYSTEM_PREFERENCES_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void SystemPreferencesSection::AddHandlers(content::WebUI* web_ui) {
  // TODO(b/292678609) Register handlers.
  NOTIMPLEMENTED();
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
  return false;
}

void SystemPreferencesSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  // TODO(b/292678609) Register subpages and list of settings contained in
  // this Section.
  NOTIMPLEMENTED();
}

}  // namespace ash::settings
