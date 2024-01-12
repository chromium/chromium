// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "device/bluetooth/bluetooth_adapter.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Bluetooth settings. Different search
// tags are registered depending on whether the device has a Bluetooth chip and
// whether it is turned on or off.
class BluetoothSection : public OsSettingsSection,
                         public device::BluetoothAdapter::Observer {
 public:
  BluetoothSection(Profile* profile,
                   SearchTagRegistry* search_tag_registry,
                   PrefService* pref_service);
  ~BluetoothSection() override;

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
  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void OnFastPairEnabledChanged();
  void OnFetchBluetoothAdapter(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void UpdateSearchTags();

  // Observes user profile prefs.
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::WeakPtrFactory<BluetoothSection> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_SECTION_H_
