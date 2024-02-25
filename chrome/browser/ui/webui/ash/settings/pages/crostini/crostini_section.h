// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Crostini settings. Search tags are
// only added if Crostini is available, and subpage search tags are added only
// when those subpages are available.
class CrostiniSection : public OsSettingsSection {
 public:
  CrostiniSection(Profile* profile,
                  SearchTagRegistry* search_tag_registry,
                  PrefService* pref_service);
  ~CrostiniSection() override;

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
  friend class CrostiniSectionTest;

  static bool ShouldShowBruschetta(Profile* profile);

  bool IsExportImportAllowed() const;
  bool IsContainerUpgradeAllowed() const;
  bool IsPortForwardingAllowed() const;
  bool IsMultiContainerAllowed() const;

  void UpdateSearchTags();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  const raw_ptr<Profile> profile_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_SECTION_H_
