// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APPS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APPS_SECTION_H_

#include "ash/public/cpp/message_center_ash.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/startup_section.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Apps settings.
class AppsSection : public OsSettingsSection,
                    public ArcAppListPrefs::Observer,
                    public MessageCenterAsh::Observer {
 public:
  AppsSection(Profile* profile,
              SearchTagRegistry* search_tag_registry,
              PrefService* pref_service,
              ArcAppListPrefs* arc_app_list_prefs,
              apps::AppServiceProxy* app_service_proxy);
  ~AppsSection() override;

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
  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;

  // MessageCenterAsh::Observer override:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  void AddAndroidAppStrings(content::WebUIDataSource* html_source);
  void AddPluginVmLoadTimeData(content::WebUIDataSource* html_source);

  void UpdateAndroidSearchTags();

  std::optional<StartupSection> startup_subsection_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<ArcAppListPrefs> arc_app_list_prefs_;
  raw_ptr<apps::AppServiceProxy> app_service_proxy_;
  PrefChangeRegistrar pref_change_registrar_;
  const bool is_arc_allowed_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APPS_SECTION_H_
