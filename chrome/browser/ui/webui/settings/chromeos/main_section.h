// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MAIN_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MAIN_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings for the main settings page, including the toolbar, search
// functionality, and common strings. Note that no search tags are provided,
// since they only apply to specific pages/settings.
class MainSection : public OsSettingsSection {
 public:
  MainSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~MainSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  void AddChromeOSUserStrings(content::WebUIDataSource* html_source);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MAIN_SECTION_H_
