// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_MAIN_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_MAIN_SECTION_H_

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

class PluralStringHandler;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace ash::settings {

// Provides UI strings for the main settings page, including the toolbar, search
// functionality, and common strings. Note that no search tags are provided,
// since they only apply to specific pages/settings.
class MainSection : public OsSettingsSection {
 public:
  // `browser_policy_connector_ash` must be non-null and must outlive `this`.
  MainSection(policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
              Profile* profile,
              SearchTagRegistry* search_tag_registry);
  ~MainSection() override;

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
  void AddChromeOSUserStrings(content::WebUIDataSource* html_source);
  std::unique_ptr<PluralStringHandler> CreatePluralStringHandler();

  const raw_ref<policy::BrowserPolicyConnectorAsh>
      browser_policy_connector_ash_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_MAIN_SECTION_H_
