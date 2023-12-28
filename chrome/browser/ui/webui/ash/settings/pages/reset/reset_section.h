// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_RESET_RESET_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_RESET_RESET_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

struct SearchConcept;
class SearchTagRegistry;

// Provides UI strings and search tags for Reset settings. Note that search tags
// are only added when powerwashing is allowed, since currently this is the only
// setting in the Reset section.
class ResetSection : public OsSettingsSection {
 public:
  ResetSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~ResetSection() override;

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
  const bool isRevampWayfindingEnabled_;
  const std::vector<SearchConcept>& GetPowerwashSearchConcept();
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_RESET_RESET_SECTION_H_
