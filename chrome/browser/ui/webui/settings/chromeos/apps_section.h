// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APPS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APPS_SECTION_H_

#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Apps settings.
class AppsSection : public OsSettingsSection, public ArcAppListPrefs::Observer {
 public:
  AppsSection(Profile* profile,
              SearchTagRegistry* search_tag_registry,
              PrefService* pref_service,
              ArcAppListPrefs* arc_app_list_prefs,
              apps::AppServiceProxyChromeOs* app_service_proxy);
  ~AppsSection() override;

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

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;

  void AddAndroidAppStrings(content::WebUIDataSource* html_source);
  void AddPluginVmLoadTimeData(content::WebUIDataSource* html_source);

  void UpdateAndroidSearchTags();

  PrefService* pref_service_;
  ArcAppListPrefs* arc_app_list_prefs_;
  apps::AppServiceProxyChromeOs* app_service_proxy_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APPS_SECTION_H_
