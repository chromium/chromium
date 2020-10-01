// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_section.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_result_icon.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetBluetoothSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kBluetoothDevices}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_OFF,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothOnOff},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_ON,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothOnOff},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothConnectableSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_CONNECT,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothConnectToDevice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_DISCONNECT,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothDisconnectFromDevice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothPairableSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_PAIR,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothPairDevice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothPairedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_UNPAIR,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothUnpairDevice},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_UNPAIR_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

BluetoothSection::BluetoothSection(Profile* profile,
                                   SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  // Note: May be uninitialized in tests.
  if (bluez::BluezDBusManager::IsInitialized()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::Bind(&BluetoothSection::OnFetchBluetoothAdapter,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

BluetoothSection::~BluetoothSection() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void BluetoothSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bluetoothConnected", IDS_SETTINGS_BLUETOOTH_CONNECTED},
      {"bluetoothConnectedWithBattery",
       IDS_SETTINGS_BLUETOOTH_CONNECTED_WITH_BATTERY},
      {"bluetoothConnecting", IDS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothDeviceListPaired", IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_PAIRED},
      {"bluetoothDeviceListUnpaired",
       IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_UNPAIRED},
      {"bluetoothConnect", IDS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDisconnect", IDS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothToggleA11yLabel",
       IDS_SETTINGS_BLUETOOTH_TOGGLE_ACCESSIBILITY_LABEL},
      {"bluetoothExpandA11yLabel",
       IDS_SETTINGS_BLUETOOTH_EXPAND_ACCESSIBILITY_LABEL},
      {"bluetoothNoDevices", IDS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothNoDevicesFound", IDS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND},
      {"bluetoothNotConnected", IDS_SETTINGS_BLUETOOTH_NOT_CONNECTED},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothPrimaryUserControlled",
       IDS_SETTINGS_BLUETOOTH_PRIMARY_USER_CONTROLLED},
      {"bluetoothDeviceType_computer",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_COMPUTER},
      {"bluetoothDeviceType_phone",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PHONE},
      {"bluetoothDeviceType_modem",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MODEM},
      {"bluetoothDeviceType_audio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_AUDIO},
      {"bluetoothDeviceType_carAudio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_CAR_AUDIO},
      {"bluetoothDeviceType_video",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_VIDEO},
      {"bluetoothDeviceType_peripheral",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PERIPHERAL},
      {"bluetoothDeviceType_joystick",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_JOYSTICK},
      {"bluetoothDeviceType_gamepad",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_GAMEPAD},
      {"bluetoothDeviceType_keyboard",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD},
      {"bluetoothDeviceType_mouse",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MOUSE},
      {"bluetoothDeviceType_tablet",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_TABLET},
      {"bluetoothDeviceType_keyboardMouseCombo",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {"bluetoothDeviceType_unknown",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_UNKNOWN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}

int BluetoothSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_BLUETOOTH;
}

mojom::Section BluetoothSection::GetSection() const {
  return mojom::Section::kBluetooth;
}

mojom::SearchResultIcon BluetoothSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kBluetooth;
}

std::string BluetoothSection::GetSectionPath() const {
  return mojom::kBluetoothSectionPath;
}

bool BluetoothSection::LogMetric(mojom::Setting setting,
                                 base::Value& value) const {
  // Unimplemented.
  return false;
}

void BluetoothSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_BLUETOOTH,
                                     mojom::Subpage::kBluetoothDevices,
                                     mojom::SearchResultIcon::kBluetooth,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kBluetoothDevicesSubpagePath);
  static constexpr mojom::Setting kBluetoothDevicesSettings[] = {
      mojom::Setting::kBluetoothOnOff,
      mojom::Setting::kBluetoothConnectToDevice,
      mojom::Setting::kBluetoothDisconnectFromDevice,
      mojom::Setting::kBluetoothPairDevice,
      mojom::Setting::kBluetoothUnpairDevice,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kBluetoothDevices,
                            kBluetoothDevicesSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kBluetoothOnOff);
}

void BluetoothSection::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                             bool present) {
  UpdateSearchTags();
}

void BluetoothSection::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                             bool powered) {
  UpdateSearchTags();
}

void BluetoothSection::DeviceAdded(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device) {
  UpdateSearchTags();
}

void BluetoothSection::DeviceChanged(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  UpdateSearchTags();
}

void BluetoothSection::DeviceRemoved(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  UpdateSearchTags();
}

void BluetoothSection::OnFetchBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  bluetooth_adapter_ = bluetooth_adapter;
  bluetooth_adapter_->AddObserver(this);
  UpdateSearchTags();
}

void BluetoothSection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // Start with no search tags, then add them below if appropriate.
  updater.RemoveSearchTags(GetBluetoothSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothOnSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothOffSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothConnectableSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothConnectedSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothPairableSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothPairedSearchConcepts());

  if (!bluetooth_adapter_->IsPresent())
    return;

  updater.AddSearchTags(GetBluetoothSearchConcepts());

  if (!bluetooth_adapter_->IsPowered()) {
    updater.AddSearchTags(GetBluetoothOffSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetBluetoothOnSearchConcepts());

  // Filter devices so that only those shown in the UI are returned. Note that
  // passing |max_devices| of 0 indicates that there is no maximum.
  device::BluetoothAdapter::DeviceList devices =
      device::FilterBluetoothDeviceList(bluetooth_adapter_->GetDevices(),
                                        device::BluetoothFilterType::KNOWN,
                                        /*max_devices=*/0);

  bool connectable_device_exists = false;
  bool connected_device_exists = false;
  bool pairable_device_exists = false;
  bool paired_device_exists = false;
  for (const device::BluetoothDevice* device : devices) {
    // Note: Device must be paired to be connectable.
    if (device->IsPaired() && device->IsConnectable() && !device->IsConnected())
      connectable_device_exists = true;
    if (device->IsConnected())
      connected_device_exists = true;
    if (device->IsPairable() && !device->IsPaired())
      pairable_device_exists = true;
    if (device->IsPaired())
      paired_device_exists = true;
  }

  if (connectable_device_exists)
    updater.AddSearchTags(GetBluetoothConnectableSearchConcepts());
  if (connected_device_exists)
    updater.AddSearchTags(GetBluetoothConnectedSearchConcepts());
  if (pairable_device_exists)
    updater.AddSearchTags(GetBluetoothPairableSearchConcepts());
  if (paired_device_exists)
    updater.AddSearchTags(GetBluetoothPairedSearchConcepts());
}

}  // namespace settings
}  // namespace chromeos
