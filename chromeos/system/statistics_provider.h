// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace system {

// Activation date key.
CHROMEOS_EXPORT extern const char kActivateDateKey[];

// The key that will be present in VPD if the device was enrolled in a domain
// that blocks dev mode.
CHROMEOS_EXPORT extern const char kBlockDevModeKey[];
// The key that will be present in VPD if the device ever was enrolled.
CHROMEOS_EXPORT extern const char kCheckEnrollmentKey[];

// The key and values present in VPD to indicate if RLZ ping should be sent.
CHROMEOS_EXPORT extern const char kShouldSendRlzPingKey[];
CHROMEOS_EXPORT extern const char kShouldSendRlzPingValueFalse[];
CHROMEOS_EXPORT extern const char kShouldSendRlzPingValueTrue[];

// The key present in VPD that indicates the date after which the RLZ ping is
// allowed to be sent. It is in the format of "yyyy-mm-dd".
CHROMEOS_EXPORT extern const char kRlzEmbargoEndDateKey[];

// Customization ID key.
CHROMEOS_EXPORT extern const char kCustomizationIdKey[];

// Developer switch value.
CHROMEOS_EXPORT extern const char kDevSwitchBootKey[];
CHROMEOS_EXPORT extern const char kDevSwitchBootValueDev[];
CHROMEOS_EXPORT extern const char kDevSwitchBootValueVerified[];

// Firmware write protect switch value.
CHROMEOS_EXPORT extern const char kFirmwareWriteProtectBootKey[];
CHROMEOS_EXPORT extern const char kFirmwareWriteProtectBootValueOn[];
CHROMEOS_EXPORT extern const char kFirmwareWriteProtectBootValueOff[];

// Firmware type and associated values. The values are from crossystem output
// for the mainfw_type key. Normal and developer correspond to Chrome OS
// firmware with MP and developer keys respectively, nonchrome indicates the
// machine doesn't run on Chrome OS firmware. See crossystem source for more
// details.
CHROMEOS_EXPORT extern const char kFirmwareTypeKey[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueDeveloper[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueNonchrome[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueNormal[];

// HWID key.
CHROMEOS_EXPORT extern const char kHardwareClassKey[];

// Key/values reporting if Chrome OS is running in a VM or not. These values are
// read from crossystem output. See crossystem source for VM detection logic.
CHROMEOS_EXPORT extern const char kIsVmKey[];
CHROMEOS_EXPORT extern const char kIsVmValueFalse[];
CHROMEOS_EXPORT extern const char kIsVmValueTrue[];

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
CHROMEOS_EXPORT extern const char kOemCanExitEnterpriseEnrollmentKey[];

// OEM customization directive that specified intended device purpose.
CHROMEOS_EXPORT extern const char kOemDeviceRequisitionKey[];

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
CHROMEOS_EXPORT extern const char kOemIsEnterpriseManagedKey[];

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
CHROMEOS_EXPORT extern const char kOemKeyboardDrivenOobeKey[];

// Offer coupon code key.
CHROMEOS_EXPORT extern const char kOffersCouponCodeKey[];

// Offer group key.
CHROMEOS_EXPORT extern const char kOffersGroupCodeKey[];

// Release Brand Code key.
CHROMEOS_EXPORT extern const char kRlzBrandCodeKey[];

// Regional data
CHROMEOS_EXPORT extern const char kRegionKey[];
CHROMEOS_EXPORT extern const char kInitialLocaleKey[];
CHROMEOS_EXPORT extern const char kInitialTimezoneKey[];
CHROMEOS_EXPORT extern const char kKeyboardLayoutKey[];

// Serial number key (VPD v2+ devices, Samsung: caroline and later) for use in
// tests. Outside of tests GetEnterpriseMachineID() is the backward-compatible
// way to obtain the serial number.
CHROMEOS_EXPORT extern const char kSerialNumberKeyForTest[];

// This interface provides access to Chrome OS statistics.
class CHROMEOS_EXPORT StatisticsProvider {
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
