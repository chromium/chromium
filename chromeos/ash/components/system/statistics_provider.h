// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos::system {

// Activation date key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kActivateDateKey[];

// The key that will be present in VPD if the device was enrolled in a domain
// that blocks dev mode.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kBlockDevModeKey[];
// The key that will be present in VPD if the device ever was enrolled.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kCheckEnrollmentKey[];

// The key and values present in VPD to indicate if RLZ ping should be sent.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kShouldSendRlzPingKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kShouldSendRlzPingValueFalse[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kShouldSendRlzPingValueTrue[];

// The key present in VPD that indicates the date after which the RLZ ping is
// allowed to be sent. It is in the format of "yyyy-mm-dd".
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kRlzEmbargoEndDateKey[];

// The key present in VPD that indicates the date after which enterprise
// management pings are allowed to be sent. It is in the format of "yyyy-mm-dd".
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kEnterpriseManagementEmbargoEndDateKey[];

// Customization ID key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kCustomizationIdKey[];

// Developer switch value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kDevSwitchBootKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kDevSwitchBootValueDev[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kDevSwitchBootValueVerified[];

// Dock MAC address key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kDockMacAddressKey[];

// Ethernet MAC address key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kEthernetMacAddressKey[];

// Firmware write protect switch value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareWriteProtectCurrentKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareWriteProtectCurrentValueOn[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareWriteProtectCurrentValueOff[];

// Firmware type and associated values. The values are from crossystem output
// for the mainfw_type key. Normal and developer correspond to Chrome OS
// firmware with MP and developer keys respectively, nonchrome indicates the
// machine doesn't run on Chrome OS firmware. See crossystem source for more
// details.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareTypeKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareTypeValueDeveloper[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareTypeValueNonchrome[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kFirmwareTypeValueNormal[];

// HWID key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kHardwareClassKey[];

// Key/values reporting if Chrome OS is running in a VM or not. These values are
// read from crossystem output. See crossystem source for VM detection logic.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) extern const char kIsVmKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kIsVmValueFalse[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kIsVmValueTrue[];

// Manufacture date key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kManufactureDateKey[];

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOemCanExitEnterpriseEnrollmentKey[];

// OEM customization directive that specified intended device purpose.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOemDeviceRequisitionKey[];

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOemIsEnterpriseManagedKey[];

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOemKeyboardDrivenOobeKey[];

// Offer coupon code key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOffersCouponCodeKey[];

// Offer group key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kOffersGroupCodeKey[];

// Release Brand Code key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kRlzBrandCodeKey[];

// Regional data
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) extern const char kRegionKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kInitialLocaleKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kInitialTimezoneKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kKeyboardLayoutKey[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kKeyboardMechanicalLayoutKey[];

// The key that will be present in RO VPD to indicate what identifier is used
// for attestation-based registration of a device.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kAttestedDeviceIdKey[];

// Serial number key (VPD v2+ devices, Samsung: caroline and later) for use in
// tests. Outside of tests GetEnterpriseMachineID() is the backward-compatible
// way to obtain the serial number.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kSerialNumberKeyForTest[];

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
  // Once statistics are loaded, returned base::StringPiece will never become
  // dangling as statistics are loaded only once and `StatisticsProvider` is
  // a singleton.
  virtual absl::optional<base::StringPiece> GetMachineStatistic(
      base::StringPiece name) = 0;

  // Similar to `GetMachineStatistic` for boolean flags. As optional and bool do
  // not work safely together, returns custom tribool value.
  virtual FlagValue GetMachineFlag(base::StringPiece name) = 0;

  // Returns the machine serial number after examining a set of well-known
  // keys. In case no serial is found nullopt is returned.
  // Caveat: On older Samsung devices, the last letter is omitted from the
  // serial number for historical reasons. This is fine.
  absl::optional<base::StringPiece> GetMachineID();

  // Cancels any pending file operations.
  virtual void Shutdown() = 0;

  // Returns true if the machine is a VM.
  virtual bool IsRunningOnVm() = 0;

  // Returns the status of RO_VPD and RW_VPD partitions.
  virtual VpdStatus GetVpdStatus() const = 0;

  // Get the Singleton instance.
  static StatisticsProvider* GetInstance();

  // Set the instance returned by GetInstance() for testing.
  static void SetTestProvider(StatisticsProvider* test_provider);

 protected:
  virtual ~StatisticsProvider() = default;
};

}  // namespace chromeos::system

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash::system {
using ::chromeos::system::kActivateDateKey;
using ::chromeos::system::kBlockDevModeKey;
using ::chromeos::system::kCheckEnrollmentKey;
using ::chromeos::system::kEnterpriseManagementEmbargoEndDateKey;
using ::chromeos::system::kHardwareClassKey;
using ::chromeos::system::kIsVmKey;
using ::chromeos::system::kIsVmValueFalse;
using ::chromeos::system::kIsVmValueTrue;
using ::chromeos::system::kOemKeyboardDrivenOobeKey;
using ::chromeos::system::kRlzBrandCodeKey;
using ::chromeos::system::kSerialNumberKeyForTest;
using ::chromeos::system::StatisticsProvider;
}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_H_
