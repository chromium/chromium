// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/statistics_provider.h"

#include "base/memory/singleton.h"
#include "chromeos/ash/components/system/statistics_provider_impl.h"

namespace chromeos::system {

namespace {
// These are the machine serial number keys that we check in order until we find
// a non-empty serial number.
//
// On older Samsung devices the VPD contains two serial numbers: "Product_S/N"
// and "serial_number" which are based on the same value except that the latter
// has a letter appended that serves as a check digit. Unfortunately, the
// sticker on the device packaging didn't include that check digit (the sticker
// on the device did though!). The former sticker was the source of the serial
// number used by device management service, so we preferred "Product_S/N" over
// "serial_number" to match the server. As an unintended consequence, older
// Samsung devices display and report a serial number that doesn't match the
// sticker on the device (the check digit is missing).
//
// "Product_S/N" is known to be used on celes, lumpy, pi, pit, snow, winky and
// some kevin devices and thus needs to be supported until AUE of these
// devices. It's known *not* to be present on caroline.
// TODO(tnagel): Remove "Product_S/N" after all devices that have it are AUE.
const char* const kMachineInfoSerialNumberKeys[] = {
    "flex_id",        // Used by Reven devices
    "Product_S/N",    // Samsung legacy
    "serial_number",  // VPD v2+ devices (Samsung: caroline and later)
};
}  // namespace

// Key values for `GetMachineStatistic()`/`GetMachineFlag()` calls.
const char kActivateDateKey[] = "ActivateDate";
const char kBlockDevModeKey[] = "block_devmode";
const char kCheckEnrollmentKey[] = "check_enrollment";
const char kShouldSendRlzPingKey[] = "should_send_rlz_ping";
const char kShouldSendRlzPingValueFalse[] = "0";
const char kShouldSendRlzPingValueTrue[] = "1";
const char kRlzEmbargoEndDateKey[] = "rlz_embargo_end_date";
const char kEnterpriseManagementEmbargoEndDateKey[] =
    "enterprise_management_embargo_end_date";
const char kCustomizationIdKey[] = "customization_id";
const char kDevSwitchBootKey[] = "devsw_boot";
const char kDevSwitchBootValueDev[] = "1";
const char kDevSwitchBootValueVerified[] = "0";
const char kDockMacAddressKey[] = "dock_mac";
const char kEthernetMacAddressKey[] = "ethernet_mac0";
const char kFirmwareWriteProtectCurrentKey[] = "wpsw_cur";
const char kFirmwareWriteProtectCurrentValueOn[] = "1";
const char kFirmwareWriteProtectCurrentValueOff[] = "0";
const char kFirmwareTypeKey[] = "mainfw_type";
const char kFirmwareTypeValueDeveloper[] = "developer";
const char kFirmwareTypeValueNonchrome[] = "nonchrome";
const char kFirmwareTypeValueNormal[] = "normal";
const char kHardwareClassKey[] = "hardware_class";
const char kIsVmKey[] = "is_vm";
const char kIsVmValueFalse[] = "0";
const char kIsVmValueTrue[] = "1";
const char kManufactureDateKey[] = "mfg_date";
const char kOffersCouponCodeKey[] = "ubind_attribute";
const char kOffersGroupCodeKey[] = "gbind_attribute";
const char kRlzBrandCodeKey[] = "rlz_brand_code";
const char kRegionKey[] = "region";
const char kSerialNumberKeyForTest[] = "serial_number";
const char kInitialLocaleKey[] = "initial_locale";
const char kInitialTimezoneKey[] = "initial_timezone";
const char kKeyboardLayoutKey[] = "keyboard_layout";
const char kKeyboardMechanicalLayoutKey[] = "keyboard_mechanical_layout";
const char kAttestedDeviceIdKey[] = "attested_device_id";

// OEM specific statistics. Must be prefixed with "oem_".
const char kOemCanExitEnterpriseEnrollmentKey[] = "oem_can_exit_enrollment";
const char kOemDeviceRequisitionKey[] = "oem_device_requisition";
const char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";
const char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";

// The StatisticsProvider implementation used in production.
class StatisticsProviderSingleton final : public StatisticsProviderImpl {
 public:
  static StatisticsProviderSingleton* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<StatisticsProviderSingleton>;

  StatisticsProviderSingleton() = default;
  ~StatisticsProviderSingleton() override = default;
};

// static
StatisticsProviderSingleton* StatisticsProviderSingleton::GetInstance() {
  return base::Singleton<
      StatisticsProviderSingleton,
      base::DefaultSingletonTraits<StatisticsProviderSingleton>>::get();
}

// static
bool StatisticsProvider::FlagValueToBool(FlagValue value, bool default_value) {
  switch (value) {
    case FlagValue::kUnset:
      return default_value;
    case FlagValue::kTrue:
      return true;
    case FlagValue::kFalse:
      return false;
  }
}

absl::optional<base::StringPiece> StatisticsProvider::GetMachineID() {
  for (const char* key : kMachineInfoSerialNumberKeys) {
    auto machine_id = GetMachineStatistic(key);
    if (machine_id && !machine_id->empty()) {
      return machine_id.value();
    }
  }
  return absl::nullopt;
}

static StatisticsProvider* g_test_statistics_provider = nullptr;

// static
StatisticsProvider* StatisticsProvider::GetInstance() {
  if (g_test_statistics_provider)
    return g_test_statistics_provider;
  return StatisticsProviderSingleton::GetInstance();
}

// static
void StatisticsProvider::SetTestProvider(StatisticsProvider* test_provider) {
  g_test_statistics_provider = test_provider;
}

}  // namespace chromeos::system
