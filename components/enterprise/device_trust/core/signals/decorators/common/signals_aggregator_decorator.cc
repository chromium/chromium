// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/core/signals/decorators/common/signals_aggregator_decorator.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace enterprise_connectors {

namespace {

// Converts PasswordProtectionTrigger to the Device Trust payload values:
// unset=0, off=1, password reuse=2, and phishing reuse=3.
int ConvertPasswordProtectionTrigger(
    const std::optional<safe_browsing::PasswordProtectionTrigger>& trigger) {
  if (!trigger.has_value()) {
    return static_cast<int>(DeviceTrustPasswordProtectionTrigger::kUnset);
  }
  switch (trigger.value()) {
    case safe_browsing::PASSWORD_PROTECTION_OFF:
      return static_cast<int>(DeviceTrustPasswordProtectionTrigger::kOff);
    case safe_browsing::PASSWORD_REUSE:
      return static_cast<int>(
          DeviceTrustPasswordProtectionTrigger::kPasswordReuse);
    case safe_browsing::PHISHING_REUSE:
      return static_cast<int>(
          DeviceTrustPasswordProtectionTrigger::kPhishingReuse);
    case safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX:
      NOTREACHED();
  }
}

base::ListValue ToListValue(const std::vector<std::string>& values) {
  base::ListValue list;
  for (const std::string& value : values) {
    list.Append(value);
  }
  return list;
}

base::ListValue ToListValueOrEmpty(
    const std::optional<std::vector<std::string>>& values) {
  if (!values.has_value()) {
    return base::ListValue();
  }
  return ToListValue(values.value());
}

void AddOptionalString(base::DictValue& dict,
                       const std::string& key,
                       const std::optional<std::string>& value) {
  if (value.has_value() && !value.value().empty()) {
    dict.Set(key, value.value());
  }
}

void AddOsSignals(const device_signals::OsSignalsResponse& os_signals,
                  base::DictValue& signals_dict) {
  if (os_signals.collection_error.has_value()) {
    return;
  }

  // These keys are required by the Device Trust signal contract. Preserve
  // them with empty values when the aggregator does not provide data.
  signals_dict.Set(device_signals::names::kOsVersion, os_signals.os_version);
  signals_dict.Set(device_signals::names::kOs, os_signals.operating_system);
  signals_dict.Set(device_signals::names::kBrowserVersion,
                   os_signals.browser_version);
  signals_dict.Set(device_signals::names::kDeviceModel,
                   os_signals.device_model);
  signals_dict.Set(device_signals::names::kDeviceManufacturer,
                   os_signals.device_manufacturer);

  signals_dict.Set(device_signals::names::kDisplayName,
                   os_signals.display_name.value_or(std::string()));
  signals_dict.Set(device_signals::names::kDeviceAffiliationIds,
                   ToListValue(os_signals.device_affiliation_ids));
  signals_dict.Set(device_signals::names::kMacAddresses,
                   ToListValueOrEmpty(os_signals.mac_addresses));
  signals_dict.Set(device_signals::names::kSystemDnsServers,
                   ToListValueOrEmpty(os_signals.system_dns_servers));
  AddOptionalString(signals_dict, device_signals::names::kDeviceHostName,
                    os_signals.hostname);
  AddOptionalString(signals_dict, device_signals::names::kVendorId,
                    os_signals.vendor_id);
  AddOptionalString(signals_dict,
                    device_signals::names::kDeviceEnrollmentDomain,
                    os_signals.device_enrollment_domain);
  AddOptionalString(signals_dict, device_signals::names::kSerialNumber,
                    os_signals.serial_number);
  AddOptionalString(signals_dict, device_signals::names::kWindowsMachineDomain,
                    os_signals.windows_machine_domain);
  AddOptionalString(signals_dict, device_signals::names::kWindowsUserDomain,
                    os_signals.windows_user_domain);

  signals_dict.Set(device_signals::names::kDiskEncrypted,
                   static_cast<int>(os_signals.disk_encryption));
  signals_dict.Set(device_signals::names::kScreenLockSecured,
                   static_cast<int>(os_signals.screen_lock_secured));
  signals_dict.Set(device_signals::names::kOsFirewall,
                   static_cast<int>(os_signals.os_firewall));

  if (os_signals.secure_boot_mode.has_value()) {
    signals_dict.Set(device_signals::names::kSecureBootEnabled,
                     static_cast<int>(os_signals.secure_boot_mode.value()));
  }
}

void AddProfileSignals(
    const device_signals::ProfileSignalsResponse& profile_signals,
    base::DictValue& signals_dict) {
  if (profile_signals.collection_error.has_value()) {
    return;
  }

  signals_dict.Set(device_signals::names::kBuiltInDnsClientEnabled,
                   profile_signals.built_in_dns_client_enabled);
  signals_dict.Set(device_signals::names::kProfileAffiliationIds,
                   ToListValue(profile_signals.profile_affiliation_ids));
  signals_dict.Set(device_signals::names::kChromeRemoteDesktopAppBlocked,
                   profile_signals.chrome_remote_desktop_app_blocked);
  signals_dict.Set(
      device_signals::names::kSafeBrowsingProtectionLevel,
      static_cast<int>(profile_signals.safe_browsing_protection_level));
  signals_dict.Set(device_signals::names::kSiteIsolationEnabled,
                   profile_signals.site_isolation_enabled);
  signals_dict.Set(device_signals::names::kRealtimeUrlCheckMode,
                   static_cast<int>(profile_signals.realtime_url_check_mode));
  signals_dict.Set(device_signals::names::kPasswordProtectionWarningTrigger,
                   ConvertPasswordProtectionTrigger(
                       profile_signals.password_protection_warning_trigger));

  AddOptionalString(signals_dict, device_signals::names::kUserEnrollmentDomain,
                    profile_signals.profile_enrollment_domain);
}

}  // namespace

SignalsAggregatorDecorator::SignalsAggregatorDecorator(
    device_signals::SignalsAggregator* signals_aggregator)
    : signals_aggregator_(signals_aggregator) {
  CHECK(signals_aggregator_);
}

SignalsAggregatorDecorator::~SignalsAggregatorDecorator() = default;

void SignalsAggregatorDecorator::Decorate(base::DictValue& signals,
                                          base::OnceClosure done_closure) {
  device_signals::SignalsAggregationRequest request;
  request.signal_names.insert(device_signals::SignalName::kOsSignals);
  request.signal_names.insert(
      device_signals::SignalName::kBrowserContextSignals);

  signals_aggregator_->GetSignals(
      request, base::BindOnce(&SignalsAggregatorDecorator::OnSignalsAggregated,
                              weak_ptr_factory_.GetWeakPtr(), std::ref(signals),
                              std::move(done_closure)));
}

void SignalsAggregatorDecorator::OnSignalsAggregated(
    base::DictValue& signals,
    base::OnceClosure done_closure,
    device_signals::SignalsAggregationResponse response) {
  if (response.top_level_error.has_value()) {
    std::move(done_closure).Run();
    return;
  }

  signals.Set(device_signals::names::kTrigger,
              static_cast<int>(device_signals::Trigger::kBrowserNavigation));

  if (response.os_signals_response.has_value()) {
    AddOsSignals(response.os_signals_response.value(), signals);
  }
  if (response.profile_signals_response.has_value()) {
    AddProfileSignals(response.profile_signals_response.value(), signals);
  }

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
