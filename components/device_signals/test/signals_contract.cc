// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/test/signals_contract.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals::test {

namespace {

#if BUILDFLAG(IS_WIN)
// Only return false if the value is set to something other than a string.
bool VerifyOptionalString(const std::string& signal_name,
                          const base::Value::Dict& signals) {
  if (!signals.Find(signal_name)) {
    return true;
  }

  return signals.FindString(signal_name);
}
#endif  // BUILDFLAG(IS_WIN)

bool VerifyIsString(const std::string& signal_name,
                    const base::Value::Dict& signals) {
  return signals.FindString(signal_name);
}

bool VerifyIsBoolean(const std::string& signal_name,
                     const base::Value::Dict& signals) {
  return signals.FindBool(signal_name).has_value();
}

// `min_value` and `max_value` are inclusive.
bool VerifyIsIntegerWithRange(const std::string& signal_name,
                              int32_t min_value,
                              int32_t max_value,
                              const base::Value::Dict& signals) {
  auto int_value = signals.FindInt(signal_name);
  if (!int_value) {
    return false;
  }

  return int_value >= min_value && int_value <= max_value;
}

bool VerifyIsSettingInteger(const std::string& signal_name,
                            const base::Value::Dict& signals) {
  // Verify the value is in the valid enum values range.
  // Enum defined at:
  // //chrome/browser/enterprise/signals/signals_common.h
  return VerifyIsIntegerWithRange(signal_name, 0, 2, signals);
}

// `enforce_value` can be set to true when we definitely expect a value to be
// set in the array.
bool VerifyIsStringArray(const std::string& signal_name,
                         bool enforce_value,
                         const base::Value::Dict& signals) {
  const auto* list_value = signals.FindList(signal_name);
  if (!list_value) {
    return false;
  }

  if (list_value->empty()) {
    return !enforce_value;
  }

  for (const auto& value : *list_value) {
    if (!value.is_string()) {
      return false;
    }
  }

  return true;
}

bool VerifyUnset(const std::string& signal_name,
                 const base::Value::Dict& signals) {
  return !signals.Find(signal_name);
}

}  // namespace

base::flat_map<std::string,
               base::RepeatingCallback<bool(const base::Value::Dict&)>>
GetSignalsContract() {
  base::flat_map<std::string,
                 base::RepeatingCallback<bool(const base::Value::Dict&)>>
      contract;

  // Common signals.
  contract[names::kOs] = base::BindRepeating(VerifyIsString, names::kOs);
  contract[names::kOsVersion] =
      base::BindRepeating(VerifyIsString, names::kOsVersion);
  contract[names::kDisplayName] =
      base::BindRepeating(VerifyIsString, names::kDisplayName);
  contract[names::kBrowserVersion] =
      base::BindRepeating(VerifyIsString, names::kBrowserVersion);
  contract[names::kDeviceModel] =
      base::BindRepeating(VerifyIsString, names::kDeviceModel);
  contract[names::kDeviceManufacturer] =
      base::BindRepeating(VerifyIsString, names::kDeviceManufacturer);
  contract[names::kDeviceAffiliationIds] =
      base::BindRepeating(VerifyIsStringArray, names::kDeviceAffiliationIds,
                          /*enforce_value=*/true);
  contract[names::kProfileAffiliationIds] =
      base::BindRepeating(VerifyIsStringArray, names::kProfileAffiliationIds,
                          /*enforce_value=*/true);
  contract[names::kRealtimeUrlCheckMode] = base::BindRepeating(
      VerifyIsIntegerWithRange, names::kRealtimeUrlCheckMode, 0, 1);
  contract[names::kSafeBrowsingProtectionLevel] = base::BindRepeating(
      VerifyIsIntegerWithRange, names::kSafeBrowsingProtectionLevel, 0, 2);
  contract[names::kSiteIsolationEnabled] =
      base::BindRepeating(VerifyIsBoolean, names::kSiteIsolationEnabled);
  contract[names::kPasswordProtectionWarningTrigger] = base::BindRepeating(
      VerifyIsIntegerWithRange, names::kPasswordProtectionWarningTrigger, 0, 3);
  contract[names::kChromeRemoteDesktopAppBlocked] = base::BindRepeating(
      VerifyIsBoolean, names::kChromeRemoteDesktopAppBlocked);
  contract[names::kBuiltInDnsClientEnabled] =
      base::BindRepeating(VerifyIsBoolean, names::kBuiltInDnsClientEnabled);
  contract[names::kOsFirewall] =
      base::BindRepeating(VerifyIsSettingInteger, names::kOsFirewall);
  contract[names::kSystemDnsServers] = base::BindRepeating(
      VerifyIsStringArray, names::kSystemDnsServers, /*enforce_value=*/false);

  // Signals added for both CrOS and Browser but from different collection
  // locations.
  contract[names::kDeviceEnrollmentDomain] =
      base::BindRepeating(VerifyIsString, names::kDeviceEnrollmentDomain);
  contract[names::kDiskEncrypted] =
      base::BindRepeating(VerifyIsSettingInteger, names::kDiskEncrypted);
  contract[names::kSerialNumber] =
      base::BindRepeating(VerifyIsString, names::kSerialNumber);
  contract[names::kDeviceHostName] =
      base::BindRepeating(VerifyIsString, names::kDeviceHostName);
  contract[names::kMacAddresses] = base::BindRepeating(
      VerifyIsStringArray, names::kMacAddresses, /*enforce_value=*/false);
  contract[names::kScreenLockSecured] =
      base::BindRepeating(VerifyIsSettingInteger, names::kScreenLockSecured);

#if BUILDFLAG(IS_WIN)
  contract[names::kWindowsMachineDomain] =
      base::BindRepeating(VerifyOptionalString, names::kWindowsMachineDomain);
  contract[names::kWindowsUserDomain] =
      base::BindRepeating(VerifyOptionalString, names::kWindowsUserDomain);
  contract[names::kSecureBootEnabled] =
      base::BindRepeating(VerifyIsSettingInteger, names::kSecureBootEnabled);

  contract[names::kCrowdStrike] =
      base::BindLambdaForTesting([](const base::Value::Dict& signals) {
        // CrowdStrike signals are optional. But if the object is set, then at
        // least one of the values must be present.
        auto* cs_value = signals.Find(device_signals::names::kCrowdStrike);
        if (!cs_value) {
          return true;
        }

        if (!cs_value->is_dict()) {
          return false;
        }

        const auto& cs_dict = cs_value->GetDict();
        auto* customer_id =
            cs_dict.FindString(device_signals::names::kCustomerId);
        auto* agent_id = cs_dict.FindString(device_signals::names::kAgentId);

        return (customer_id && !customer_id->empty()) ||
               (agent_id && !agent_id->empty());
      });

#else
  // Windows-only signals that shouldn't be set on other platforms.
  contract[names::kWindowsMachineDomain] =
      base::BindRepeating(VerifyUnset, names::kWindowsMachineDomain);
  contract[names::kWindowsUserDomain] =
      base::BindRepeating(VerifyUnset, names::kWindowsUserDomain);
  contract[names::kSecureBootEnabled] =
      base::BindRepeating(VerifyUnset, names::kSecureBootEnabled);
  contract[names::kCrowdStrike] =
      base::BindRepeating(VerifyUnset, names::kCrowdStrike);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  contract[names::kAllowScreenLock] =
      base::BindRepeating(VerifyUnset, names::kAllowScreenLock);
  contract[names::kImei] = base::BindRepeating(VerifyUnset, names::kImei);
  contract[names::kMeid] = base::BindRepeating(VerifyUnset, names::kMeid);
#else
  // Chrome OS Signals.
  contract[names::kAllowScreenLock] =
      base::BindRepeating(VerifyIsBoolean, names::kAllowScreenLock);
  contract[names::kImei] = base::BindRepeating(
      VerifyIsStringArray, names::kImei, /*enforce_value=*/false);
  contract[names::kMeid] = base::BindRepeating(
      VerifyIsStringArray, names::kMeid, /*enforce_value=*/false);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return contract;
}

}  // namespace device_signals::test
