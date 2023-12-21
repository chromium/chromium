// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_POWER_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_POWER_SECTION_H_

#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/power/battery_saver_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/power/device_power_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for power settings.
class PowerSection : public OsSettingsSection,
                     public chromeos::PowerManagerClient::Observer {
 public:
  PowerSection(Profile* profile,
               SearchTagRegistry* search_tag_registry,
               PrefService* pref_service);
  ~PowerSection() override;

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
  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void OnGotSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> result);

  // False until we observe the first PowerChanged event.
  bool has_observed_power_status_{false};

  raw_ptr<PrefService> pref_service_;
  base::WeakPtrFactory<PowerSection> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_POWER_SECTION_H_
