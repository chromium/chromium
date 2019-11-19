// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace chromeos {
namespace system {

// Activation date key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kActivateDateKey[];

// The key that will be present in VPD if the device was enrolled in a domain
// that blocks dev mode.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kBlockDevModeKey[];
// The key that will be present in VPD if the device ever was enrolled.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kCheckEnrollmentKey[];

// The key and values present in VPD to indicate if RLZ ping should be sent.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kShouldSendRlzPingKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kShouldSendRlzPingValueFalse[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kShouldSendRlzPingValueTrue[];

// The key present in VPD that indicates the date after which the RLZ ping is
// allowed to be sent. It is in the format of "yyyy-mm-dd".
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kRlzEmbargoEndDateKey[];

// Customization ID key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kCustomizationIdKey[];

// Developer switch value.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kDevSwitchBootKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kDevSwitchBootValueDev[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kDevSwitchBootValueVerified[];

// Dock MAC address key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kDockMacAddressKey[];

// Ethernet MAC address key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kEthernetMacAddressKey[];

// Firmware write protect switch value.
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kFirmwareWriteProtectBootKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kFirmwareWriteProtectBootValueOn[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kFirmwareWriteProtectBootValueOff[];

// Firmware type and associated values. The values are from crossystem output
// for the mainfw_type key. Normal and developer correspond to Chrome OS
// firmware with MP and developer keys respectively, nonchrome indicates the
// machine doesn't run on Chrome OS firmware. See crossystem source for more
// details.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kFirmwareTypeKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kFirmwareTypeValueDeveloper[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kFirmwareTypeValueNonchrome[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kFirmwareTypeValueNormal[];

// HWID key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kHardwareClassKey[];

// Key/values reporting if Chrome OS is running in a VM or not. These values are
// read from crossystem output. See crossystem source for VM detection logic.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kIsVmKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kIsVmValueFalse[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kIsVmValueTrue[];

// Manufacture date key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kManufactureDateKey[];

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kOemCanExitEnterpriseEnrollmentKey[];

// OEM customization directive that specified intended device purpose.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kOemDeviceRequisitionKey[];

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
COMPONENT_EXPORT(CHROMEOS_SYSTEM)
extern const char kOemIsEnterpriseManagedKey[];

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kOemKeyboardDrivenOobeKey[];

// Offer coupon code key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kOffersCouponCodeKey[];

// Offer group key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kOffersGroupCodeKey[];

// Release Brand Code key.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kRlzBrandCodeKey[];

// Regional data
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kRegionKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kInitialLocaleKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kInitialTimezoneKey[];
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kKeyboardLayoutKey[];

// Serial number key (VPD v2+ devices, Samsung: caroline and later) for use in
// tests. Outside of tests GetEnterpriseMachineID() is the backward-compatible
// way to obtain the serial number.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kSerialNumberKeyForTest[];

// This interface provides access to Chrome OS statistics.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) StatisticsProvider {
 public:
  // Starts loading the machine statistics.
  virtual void StartLoadingMachineStatistics(bool load_oem_manifest) = 0;

  // Schedules |callback| on the current sequence when machine statistics are
  // loaded. That can be immediately if machine statistics are already loaded.
  virtual void ScheduleOnMachineStatisticsLoaded(
      base::OnceClosure callback) = 0;

  // GetMachineStatistic(), GetMachineFlag() and GetEnterpriseMachineId() will
  // block if called before statistics have been loaded. To avoid this, call
  // from a callback passed to ScheduleOnMachineStatisticsLoaded(). These
  // methods are safe to call on any sequence. StartLoadingMachineStatistics()
  // must be called before these methods.

  // Returns true if the named machine statistic (e.g. "hardware_class") is
  // found and stores it in |result| (if provided). Probing for the existence of
  // a statistic by setting |result| to nullptr supresses the usual warning in
  // case the statistic is not found.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  // Similar to GetMachineStatistic for boolean flags.
  virtual bool GetMachineFlag(const std::string& name, bool* result) = 0;

  // Returns the machine serial number after examining a set of well-known
  // keys. In case no serial is found an empty string is returned.
  // Caveat: On older Samsung devices, the last letter is omitted from the
  // serial number for historical reasons. This is fine.
  // TODO(tnagel): Drop "Enterprise" from the method name and remove Samsung
  // special casing after kevin EOL.
  std::string GetEnterpriseMachineID();

  // Cancels any pending file operations.
  virtual void Shutdown() = 0;

  // Returns true if the machine is a VM.
  virtual bool IsRunningOnVm() = 0;

  // Get the Singleton instance.
  static StatisticsProvider* GetInstance();

  // Set the instance returned by GetInstance() for testing.
  static void SetTestProvider(StatisticsProvider* test_provider);

 protected:
  virtual ~StatisticsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
