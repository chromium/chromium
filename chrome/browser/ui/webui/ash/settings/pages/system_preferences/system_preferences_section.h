// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SYSTEM_PREFERENCES_SYSTEM_PREFERENCES_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SYSTEM_PREFERENCES_SYSTEM_PREFERENCES_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/date_time_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/files_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/languages/languages_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/multitasking/multitasking_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/power/power_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/reset/reset_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/storage_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/startup_section.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for System Preferences settings.
// Includes the Date & Time, Files, Languages, Power, Reset, Search, Startup,
// and Storage sections.
class SystemPreferencesSection : public OsSettingsSection {
 public:
  SystemPreferencesSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry,
                           PrefService* pref_service);
  ~SystemPreferencesSection() override;

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

 private:
  DateTimeSection date_time_subsection_;
  FilesSection files_subsection_;
  LanguagesSection languages_subsection_;
  MultitaskingSection multitasking_subsection_;
  PowerSection power_subsection_;
  ResetSection reset_subsection_;
  SearchSection search_subsection_;
  StartupSection startup_subsection_;
  StorageSection storage_subsection_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SYSTEM_PREFERENCES_SYSTEM_PREFERENCES_SECTION_H_
