// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ash::system {

// Activation date key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kActivateDateKey[] = "ActivateDate";

// The key that will be present in VPD if the device was enrolled in a domain
// that blocks dev mode.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kBlockDevModeKey[] = "block_devmode";
// The key that will be present in VPD if the device ever was enrolled.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kCheckEnrollmentKey[] = "check_enrollment";

// The key and values present in VPD to indicate if RLZ ping should be sent.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kShouldSendRlzPingKey[] = "should_send_rlz_ping";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kShouldSendRlzPingValueFalse[] = "0";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kShouldSendRlzPingValueTrue[] = "1";

// The key present in VPD that indicates the date after which the RLZ ping is
// allowed to be sent. It is in the format of "yyyy-mm-dd".
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kRlzEmbargoEndDateKey[] = "rlz_embargo_end_date";

// The key present in VPD that indicates the date after which enterprise
// management pings are allowed to be sent. It is in the format of "yyyy-mm-dd".
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kEnterpriseManagementEmbargoEndDateKey[] =
    "enterprise_management_embargo_end_date";

// Customization ID key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kCustomizationIdKey[] = "customization_id";

// Developer switch value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kDevSwitchBootKey[] = "devsw_boot";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kDevSwitchBootValueDev[] = "1";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kDevSwitchBootValueVerified[] = "0";

// Dock MAC address key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kDockMacAddressKey[] = "dock_mac";

// Ethernet MAC address key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kEthernetMacAddressKey[] = "ethernet_mac0";

// Firmware write protect switch value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareWriteProtectCurrentKey[] = "wpsw_cur";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareWriteProtectCurrentValueOn[] = "1";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareWriteProtectCurrentValueOff[] = "0";

// Firmware type and associated values. The values are from crossystem output
// for the mainfw_type key. Normal and developer correspond to Chrome OS
// firmware with MP and developer keys respectively, nonchrome indicates the
// machine doesn't run on Chrome OS firmware. See crossystem source for more
// details.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareTypeKey[] = "mainfw_type";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareTypeValueDeveloper[] = "developer";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareTypeValueNonchrome[] = "nonchrome";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFirmwareTypeValueNormal[] = "normal";

// HWID key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kHardwareClassKey[] = "hardware_class";

// Key/values reporting if Chrome OS is running in a VM or not. These values are
// read from crossystem output. See crossystem source for VM detection logic.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsVmKey[] = "is_vm";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsVmValueFalse[] = "0";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsVmValueTrue[] = "1";

// Key/values reporting if ChromeOS is running in debug mode or not. These
// values are read from crossystem output. See crossystem source for cros_debug
// detection logic.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsCrosDebugKey[] = "is_cros_debug";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsCrosDebugValueFalse[] = "0";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kIsCrosDebugValueTrue[] = "1";

// Manufacture date key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kManufactureDateKey[] = "mfg_date";

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOemCanExitEnterpriseEnrollmentKey[] =
    "oem_can_exit_enrollment";

// OEM customization directive that specified intended device purpose.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOemDeviceRequisitionKey[] = "oem_device_requisition";

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";

// Offer coupon code key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOffersCouponCodeKey[] = "ubind_attribute";

// Offer group key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kOffersGroupCodeKey[] = "gbind_attribute";

// Release Brand Code key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kRlzBrandCodeKey[] = "rlz_brand_code";

// Regional data
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kRegionKey[] = "region";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kInitialLocaleKey[] = "initial_locale";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kInitialTimezoneKey[] = "initial_timezone";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kKeyboardLayoutKey[] = "keyboard_layout";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kKeyboardMechanicalLayoutKey[] =
    "keyboard_mechanical_layout";

// The key that will be present in RO VPD to indicate what identifier is used
// for attestation-based registration of a device.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kAttestedDeviceIdKey[] = "attested_device_id";

// Serial number key (legacy VPD devices). In most cases,
// GetEnterpriseMachineID() is the appropriate way to obtain the serial number.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kLegacySerialNumberKey[] = "Product_S/N";

// Serial number key (VPD v2+ devices, Samsung: caroline and later). In most
// cases, GetEnterpriseMachineID() is the appropriate way to obtain the serial
// number.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kSerialNumberKey[] = "serial_number";

// Serial number key for Flex devices. In most cases, GetEnterpriseMachineID()
// is the appropriate way to obtain the serial number.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kFlexIdKey[] = "flex_id";

// Display Profiles key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kDisplayProfilesKey[] = "display_profiles";

// Machine model and oem names.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kMachineModelName[] = "model_name";
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
inline constexpr char kMachineOemName[] = "oem_name";

// This interface provides access to Chrome OS statistics.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) StatisticsProvider {
 public:
  // Represents the status of the VPD statistics source.
  enum class VpdStatus {
    kUnknown = 0,
    kValid = 1,
    kRoInvalid = 2,
    kRwInvalid = 3,
    kInvalid = 4
  };

  enum class FlagValue {
    kUnset,
    kTrue,
    kFalse,
  };

  // Converts `value` to bool. Returns corresponding true or false, or
  // `default_value` if unset.
  static bool FlagValueToBool(FlagValue value, bool default_value);

  // Starts loading the machine statistics.
  virtual void StartLoadingMachineStatistics(bool load_oem_manifest) = 0;

  // Schedules `callback` on the current sequence when machine statistics are
  // loaded. That can be immediately if machine statistics are already loaded.
  virtual void ScheduleOnMachineStatisticsLoaded(
      base::OnceClosure callback) = 0;

  // `GetMachineStatistic`, `GetMachineFlag` and `GetMachineID` will block if
  // called before statistics have been loaded. To avoid this, call from a
  // callback passed to ScheduleOnMachineStatisticsLoaded(). These methods are
  // safe to call on any sequence. `StartLoadingMachineStatistics` must be
  // called before these methods.

  // Returns statistic value if the named machine statistic (e.g.
  // "hardware_class") is found. Returns nullopt otherwise.
  // Once statistics are loaded, returned std::string_view will never become
  // dangling as statistics are loaded only once and `StatisticsProvider` is
  // a singleton.
  virtual std::optional<std::string_view> GetMachineStatistic(
      std::string_view name) = 0;

  // Similar to `GetMachineStatistic` for boolean flags. As optional and bool do
  // not work safely together, returns custom tribool value.
  virtual FlagValue GetMachineFlag(std::string_view name) = 0;

  // Returns the machine serial number after examining a set of well-known
  // keys. In case no serial is found nullopt is returned.
  // Caveat: On older Samsung devices, the last letter is omitted from the
  // serial number for historical reasons. This is fine.
  std::optional<std::string_view> GetMachineID();

  // Cancels any pending file operations.
  virtual void Shutdown() = 0;

  // Returns true if the machine is a VM.
  virtual bool IsRunningOnVm() = 0;

  // Returns true if the ChromeOS machine is in debug mode.
  virtual bool IsCrosDebugMode() = 0;

  // Returns the status of RO_VPD and RW_VPD partitions.
  virtual VpdStatus GetVpdStatus() const = 0;

  // Get the Singleton instance.
  static StatisticsProvider* GetInstance();

  // Set the instance returned by GetInstance() for testing.
  static void SetTestProvider(StatisticsProvider* test_provider);

 protected:
  virtual ~StatisticsProvider() = default;
};

}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_
