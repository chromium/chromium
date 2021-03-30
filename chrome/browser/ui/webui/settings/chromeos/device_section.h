// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_SECTION_H_

#include <vector>

#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/ash/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Device settings.
class DeviceSection : public OsSettingsSection,
                      public system::PointerDeviceObserver::Observer,
                      public ui::InputDeviceEventObserver,
                      public ash::NightLightController::Observer,
                      public ash::mojom::CrosDisplayConfigObserver,
                      public PowerManagerClient::Observer {
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
  mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // system::PointerDeviceObserver::Observer:
  void TouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;
  void PointingStickExists(bool exists) override;

  // ui::InputDeviceObserver:
  void OnDeviceListsComplete() override;

  // ash::NightLightController::Observer:
  void OnNightLightEnabledChanged(bool enabled) override;

  // ash::mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

  // PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void OnGotSwitchStates(
      base::Optional<PowerManagerClient::SwitchStates> result);

  void UpdateStylusSearchTags();

  void OnGetDisplayUnitInfoList(
      std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list);
  void OnGetDisplayLayoutInfo(
      std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list,
      ash::mojom::DisplayLayoutInfoPtr display_layout_info);

  void AddDevicePointersStrings(content::WebUIDataSource* html_source);

  PrefService* pref_service_;
  system::PointerDeviceObserver pointer_device_observer_;
  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;
  mojo::AssociatedReceiver<ash::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};
  base::WeakPtrFactory<DeviceSection> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_SECTION_H_
