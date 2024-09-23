// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/multitasking/multitasking_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Personalization settings. Search tags
// are only added when not in guest mode, and Ambient mode settings are added
// depending on whether the feature is allowed and enabled.
class PersonalizationSection : public OsSettingsSection {
 public:
  PersonalizationSection(Profile* profile,
                         SearchTagRegistry* search_tag_registry,
                         PrefService* pref_service);
  ~PersonalizationSection() override;

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
  PrefChangeRegistrar pref_change_registrar_;
  bool isRevampEnabled_;
  std::optional<MultitaskingSection> multitasking_subsection_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_SECTION_H_
