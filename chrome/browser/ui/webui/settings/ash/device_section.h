// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_DEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_DEVICE_SECTION_H_

#include <vector>

#include "ash/public/cpp/night_light_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Device settings.
class DeviceSection : public OsSettingsSection,
                      public system::PointerDeviceObserver::Observer,
                      public ui::InputDeviceEventObserver,
                      public NightLightController::Observer,
                      public crosapi::mojom::CrosDisplayConfigObserver,
                      public chromeos::PowerManagerClient::Observer {
 public:
  DeviceSection(Profile* profile,
                SearchTagRegistry* search_tag_registry,
                PrefService* pref_service);
  ~DeviceSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // system::PointerDeviceObserver::Observer:
  void TouchpadExists(bool exists) override;
  void HapticTouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;
  void PointingStickExists(bool exists) override;

  // ui::InputDeviceObserver:
  void OnDeviceListsComplete() override;

  // NightLightController::Observer:
  void OnNightLightEnabledChanged(bool enabled) override;

  // mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void OnGotSwitchStates(
      absl::optional<chromeos::PowerManagerClient::SwitchStates> result);

  void UpdateStylusSearchTags();

  void OnGetDisplayUnitInfoList(
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> display_unit_info_list);
  void OnGetDisplayLayoutInfo(
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> display_unit_info_list,
      crosapi::mojom::DisplayLayoutInfoPtr display_layout_info);

  void AddDevicePointersStrings(content::WebUIDataSource* html_source);
  void AddDeviceGraphicsTabletStrings(
      content::WebUIDataSource* html_source) const;
  void AddDeviceDisplayStrings(content::WebUIDataSource* html_source) const;

  raw_ptr<PrefService, ExperimentalAsh> pref_service_;
  system::PointerDeviceObserver pointer_device_observer_;
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
  mojo::AssociatedReceiver<crosapi::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};
  base::WeakPtrFactory<DeviceSection> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_DEVICE_SECTION_H_
