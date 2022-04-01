// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_shared_load_time_data_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_result_icon.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
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

const std::vector<SearchConcept>& GetFastPairOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FAST_PAIR_TURN_ON,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFastPairOnOff},
       {IDS_OS_SETTINGS_TAG_FAST_PAIR_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFastPairOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FAST_PAIR_TURN_OFF,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFastPairOnOff},
       {IDS_OS_SETTINGS_TAG_FAST_PAIR_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

BluetoothSection::BluetoothSection(Profile* profile,
                                   SearchTagRegistry* search_tag_registry,
                                   PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  bool is_initialized = false;
  if (base::FeatureList::IsEnabled(floss::features::kFlossEnabled)) {
    is_initialized = floss::FlossDBusManager::IsInitialized();
  } else {
    is_initialized = bluez::BluezDBusManager::IsInitialized();
  }

  // Note: May be uninitialized in tests.
  if (is_initialized) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindOnce(&BluetoothSection::OnFetchBluetoothAdapter,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (chromeos::features::IsBluetoothRevampEnabled() &&
      ash::features::IsFastPairEnabled()) {
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        ash::prefs::kFastPairEnabled,
        base::BindRepeating(&BluetoothSection::OnFastPairEnabledChanged,
                            base::Unretained(this)));
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
      {"bluetoothDeviceDetailConnected",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CONNECTED},
      {"bluetoothDeviceDetailDisconnected",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_DISCONNECTED},
      {"bluetoothDeviceDetailForget",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_FORGET},
      {"bluetoothDeviceDetailForgetA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_FORGET_A11Y_LABEL},
      {"bluetoothDeviceDetailHIDMessageConnected",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_HID_MESSAGE_CONNECTED},
      {"bluetoothDeviceDetailHIDMessageDisconnected",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_HID_MESSAGE_DISCONNECTED},
      {"bluetoothDeviceDetailName", IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_NAME},
      {"bluetoothDeviceDetailChangeNameDialogNewName",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_NEW_NAME},
      {"bluetoothDeviceDetailChangeNameDialogCancel",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_CANCEL},
      {"bluetoothDeviceDetailChangeNameDialogDone",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_DONE},
      {"bluetoothDeviceDetailChangeDeviceSettingsKeyboard",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CHANGE_DEVICE_SETTINGS_KEYBOARD},
      {"bluetoothDeviceDetailChangeDeviceSettingsMouse",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CHANGE_DEVICE_SETTINGS_MOUSE},
      {"bluetoothDeviceDetailChangeDeviceName",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CHANGE_DEVICE_NAME},
      {"bluetoothDeviceDetailBatteryPercentageA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_BATTERY_PERCENTAGE_A11Y_LABEL},
      {"bluetoothDeviceDetailConnectedA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CONNECTED_A11Y_LABEL},
      {"bluetoothDeviceDetailConnectingA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CONNECTING_A11Y_LABEL},
      {"bluetoothDeviceDetailConnectionFailureA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CONNECTION_FAILURE_A11Y_LABEL},
      {"bluetoothDeviceDetailLeftBudBatteryPercentageA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_LEFT_BUD_BATTERY_PERCENTAGE_A11Y_LABEL},
      {"bluetoothDeviceDetailCaseBatteryPercentageA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CASE_BATTERY_PERCENTAGE_A11Y_LABEL},
      {"bluetoothDeviceDetailRightBudBatteryPercentageA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_RIGHT_BUD_BATTERY_PERCENTAGE_A11Y_LABEL},
      {"bluetoothTrueWirelessLeftBudLabel",
       IDS_SETTINGS_BLUETOOTH_TRUE_WIRELESS_LEFT_BUD_LABEL},
      {"bluetoothTrueWirelessCaseLabel",
       IDS_SETTINGS_BLUETOOTH_TRUE_WIRELESS_CASE_LABEL},
      {"bluetoothTrueWirelessRightBudLabel",
       IDS_SETTINGS_BLUETOOTH_TRUE_WIRELESS_RIGHT_BUD_LABEL},
      {"bluetoothDeviceDetailConnectionFailureLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CONNECTION_FAILURE_LABEL},
      {"bluetoothDeviceDetailDisconnectedA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_DISCONNECTED_A11Y_LABEL},
      {"bluetoothChangeNameDialogInputA11yLabel",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_INPUT_A11Y_LABEL},
      {"bluetoothChangeNameDialogInputSubtitle",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_INPUT_SUBTITLE},
      {"bluetoothChangeNameDialogInputCharCount",
       IDS_SETTINGS_BLUETOOTH_CHANGE_DEVICE_NAME_DIALOG_INPUT_CHARACTER_COUNT},
      {"bluetoothDeviceDetailChangeDeviceNameBtnA11yLabel",
       IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_CHANGE_DEVICE_NAME_BTN_A11Y_LABEL},
      {"bluetoothToggleA11yLabel",
       IDS_SETTINGS_BLUETOOTH_TOGGLE_ACCESSIBILITY_LABEL},
      {"bluetoothExpandA11yLabel",
       IDS_SETTINGS_BLUETOOTH_EXPAND_ACCESSIBILITY_LABEL},
      {"bluetoothNoDevices", IDS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothNoDevicesFound", IDS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND},
      {"bluetoothNotConnected", IDS_SETTINGS_BLUETOOTH_NOT_CONNECTED},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothSummaryPageConnectedA11yOneDevice",
       IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_CONNECTED_A11Y_ONE_DEVICE},
      {"bluetoothSummaryPageConnectedA11yTwoDevices",
       IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_CONNECTED_A11Y_TWO_DEVICES},
      {"bluetoothSummaryPageConnectedA11yTwoOrMoreDevices",
       IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_CONNECTED_A11Y_TWO_OR_MORE_DEVICES},
      {"bluetoothSummaryPageTwoOrMoreDevicesDescription",
       IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_TWO_OR_MORE_DEVICES_DESCRIPTION},
      {"bluetoothSummaryPageTwoDevicesDescription",
       IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_TWO_DEVICES_DESCRIPTION},
      {"bluetoothSummaryPageOff", IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_OFF},
      {"bluetoothSummaryPageOn", IDS_SETTINGS_BLUETOOTH_SUMMARY_PAGE_ON},
      {"bluetoothA11yDeviceTypeUnknown",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_UNKNOWN},
      {"bluetoothA11yDeviceTypeComputer",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_COMPUTER},
      {"bluetoothA11yDeviceTypePhone", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_PHONE},
      {"bluetoothA11yDeviceTypeHeadset",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_HEADSET},
      {"bluetoothA11yDeviceTypeVideoCamera",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_VIDEO_CAMERA},
      {"bluetoothA11yDeviceTypeGameController",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_GAME_CONTROLLER},
      {"bluetoothA11yDeviceTypeKeyboard",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD},
      {"bluetoothA11yDeviceTypeKeyboardMouseCombo",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {"bluetoothA11yDeviceTypeMouse", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_MOUSE},
      {"bluetoothA11yDeviceTypeMouse", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_MOUSE},
      {"bluetoothA11yDeviceTypeTablet", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_TABLET},
      {"bluetoothA11yDeviceConnectionStateConnected",
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTED},
      {"bluetoothA11yDeviceConnectionStateConnecting",
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTING},
      {"bluetoothA11yDeviceConnectionStateNotConnected",
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_NOT_CONNECTED},
      {"bluetoothA11yDeviceBatteryInfo",
       IDS_BLUETOOTH_A11Y_DEVICE_BATTERY_INFO},
      {"bluetoothA11yDeviceNamedBatteryInfoLeftBud",
       IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_LEFT_BUD},
      {"bluetoothA11yDeviceNamedBatteryInfoCase",
       IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_CASE},
      {"bluetoothA11yDeviceNamedBatteryInfoRightBud",
       IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_RIGHT_BUD},
      {"bluetoothA11yDeviceName", IDS_BLUETOOTH_A11Y_DEVICE_NAME},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothManaged", IDS_SETTINGS_BLUETOOTH_MANAGED},
      {"enableFastPairLabel", IDS_BLUETOOTH_ENABLE_FAST_PAIR_LABEL},
      {"enableFastPairSubtitle", IDS_BLUETOOTH_ENABLE_FAST_PAIR_SUBTITLE},
      {"bluetoothPrimaryUserControlled",
       IDS_SETTINGS_BLUETOOTH_PRIMARY_USER_CONTROLLED},
      {"bluetoothDeviceWithConnectionStatus",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_AND_CONNECTION_STATUS},
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
      {"bluetoothDeviceListCurrentlyConnected",
       IDS_BLUETOOTH_DEVICE_LIST_CURRENTLY_CONNECTED},
      {"bluetoothDeviceListPreviouslyConnected",
       IDS_BLUETOOTH_DEVICE_LIST_PREVIOUSLY_CONNECTED},
      {"bluetoothDeviceListNoConnectedDevices",
       IDS_BLUETOOTH_DEVICE_LIST_NO_CONNECTED_DEVICES},
      {"bluetoothEnabledA11YLabel", IDS_SETTINGS_BLUETOOTH_ENABLED_A11Y_LABEL},
      {"bluetoothDisabledA11YLabel", IDS_SETTINGS_BLUETOOTH_DISABLED_A11Y_LABEL}

  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("enableFastPairFlag", features::IsFastPairEnabled());
  chromeos::bluetooth::AddLoadTimeData(html_source);
}

void BluetoothSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<BluetoothHandler>());
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
  switch (setting) {
    case mojom::Setting::kBluetoothOnOff:
      base::UmaHistogramBoolean("ChromeOS.Settings.Bluetooth.BluetoothOnOff",
                                value.GetBool());
      return true;

    default:
      return false;
  }
}

void BluetoothSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_BLUETOOTH,
                                     mojom::Subpage::kBluetoothDevices,
                                     mojom::SearchResultIcon::kBluetooth,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kBluetoothDevicesSubpagePath);
  static constexpr mojom::Setting kBluetoothDevicesSettings[] = {
      mojom::Setting::kBluetoothOnOff, mojom::Setting::kBluetoothPairDevice,
      mojom::Setting::kBluetoothUnpairDevice, mojom::Setting::kFastPairOnOff};
  static constexpr mojom::Setting kBluetoothDevicesSettingsLegacy[] = {
      mojom::Setting::kBluetoothConnectToDevice,
      mojom::Setting::kBluetoothDisconnectFromDevice,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kBluetoothDevices,
                            kBluetoothDevicesSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kBluetoothOnOff);
  generator->RegisterNestedSubpage(IDS_SETTINGS_BLUETOOTH_DEVICE_DETAILS,
                                   mojom::Subpage::kBluetoothDeviceDetail,
                                   mojom::Subpage::kBluetoothDevices,
                                   mojom::SearchResultIcon::kBluetooth,
                                   mojom::SearchResultDefaultRank::kMedium,
                                   mojom::kBluetoothDeviceDetailSubpagePath);
  RegisterNestedSettingBulk(ash::features::IsBluetoothRevampEnabled()
                                ? mojom::Subpage::kBluetoothDeviceDetail
                                : mojom::Subpage::kBluetoothDevices,
                            kBluetoothDevicesSettingsLegacy, generator);
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

void BluetoothSection::OnFastPairEnabledChanged() {
  UpdateSearchTags();
}

void BluetoothSection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // Start with no search tags, then add them below if appropriate.
  updater.RemoveSearchTags(GetBluetoothSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothOnSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothOffSearchConcepts());
  updater.RemoveSearchTags(GetFastPairOnSearchConcepts());
  updater.RemoveSearchTags(GetFastPairOffSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothConnectableSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothConnectedSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothPairableSearchConcepts());
  updater.RemoveSearchTags(GetBluetoothPairedSearchConcepts());

  if (!bluetooth_adapter_->IsPresent())
    return;

  updater.AddSearchTags(GetBluetoothSearchConcepts());

  if (ash::features::IsFastPairEnabled() &&
      ash::features::IsBluetoothRevampEnabled()) {
    if (pref_service_->GetBoolean(ash::prefs::kFastPairEnabled)) {
      updater.AddSearchTags(GetFastPairOnSearchConcepts());
    } else {
      updater.AddSearchTags(GetFastPairOffSearchConcepts());
    }
  }

  if (!bluetooth_adapter_->IsPowered()) {
    updater.AddSearchTags(GetBluetoothOffSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetBluetoothOnSearchConcepts());

  // Always include the option to pair devices, but skip any device-specific
  // search options since we have no way to determine which device the user is
  // interested in.
  if (ash::features::IsBluetoothRevampEnabled()) {
    updater.AddSearchTags(GetBluetoothPairableSearchConcepts());
    return;
  }

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
