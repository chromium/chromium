// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/version_info/version_info.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

ConnectorsServiceBase::DmToken::DmToken(const std::string& value,
                                        policy::PolicyScope scope)
    : value(value), scope(scope) {}
ConnectorsServiceBase::DmToken::DmToken(DmToken&&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    DmToken&&) = default;
ConnectorsServiceBase::DmToken::DmToken(const DmToken&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    const DmToken&) = default;
ConnectorsServiceBase::DmToken::~DmToken() = default;

base::expected<std::string,
               ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason>
ConnectorsServiceBase::GetDMTokenForRealTimeUrlCheck() const {
  if (!ConnectorsEnabled()) {
    return base::unexpected(
        NoDMTokenForRealTimeUrlCheckReason::kConnectorsDisabled);
  }

  if (GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode) ==
      REAL_TIME_CHECK_DISABLED) {
    return base::unexpected(
        NoDMTokenForRealTimeUrlCheckReason::kPolicyDisabled);
  }

  std::optional<DmToken> dm_token =
      GetDmToken(kEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value()) {
    return dm_token.value().value;
  }
  return base::unexpected(NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
}

EnterpriseRealTimeUrlCheckMode
ConnectorsServiceBase::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(kEnterpriseRealTimeUrlCheckScope).has_value()) {
    return REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<EnterpriseRealTimeUrlCheckMode>(
      GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode));
}

std::optional<policy::PolicyScope>
ConnectorsServiceBase::GetRealtimeUrlCheckScope() const {
  std::optional<policy::PolicyScope> policy_scope = std::nullopt;
  if (std::optional<DmToken> dm_token =
          GetDmToken(kEnterpriseRealTimeUrlCheckScope)) {
    policy_scope = dm_token.value().scope;
  }
  return policy_scope;
}

std::vector<std::string>
ConnectorsServiceBase::GetReportingServiceProviderNames() {
  if (!ConnectorsEnabled()) {
    return {};
  }

  if (!GetDmToken(kOnSecurityEventScopePref).has_value()) {
    return {};
  }

  return GetConnectorsManagerBase()->GetReportingServiceProviderNames();
}

std::optional<ReportingSettings> ConnectorsServiceBase::GetReportingSettings() {
  if (!ConnectorsEnabled()) {
    return std::nullopt;
  }

  std::optional<ReportingSettings> settings =
      GetConnectorsManagerBase()->GetReportingSettings();
  if (!settings.has_value()) {
    return std::nullopt;
  }

  std::optional<DmToken> dm_token = GetDmToken(kOnSecurityEventScopePref);
  if (!dm_token.has_value()) {
    return std::nullopt;
  }

  settings.value().dm_token = dm_token.value().value;
  settings.value().per_profile =
      dm_token.value().scope == policy::POLICY_SCOPE_USER;

  return settings;
}

#if !BUILDFLAG(IS_CHROMEOS)
std::optional<std::string> ConnectorsServiceBase::GetProfileDmToken() const {
  policy::CloudPolicyManager* policy_manager =
      GetManagedUserCloudPolicyManager();
  if (policy_manager && policy_manager->core() &&
      policy_manager->core()->store() &&
      policy_manager->core()->store()->has_policy() &&
      policy_manager->core()->store()->policy()->has_request_token()) {
    return policy_manager->core()->store()->policy()->request_token();
  }

  return std::nullopt;
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void ConnectorsServiceBase::PopulateBrowserMetadata(
    bool include_device_info,
    ClientMetadata::Browser* browser_proto) {
  base::FilePath browser_id;
  if (base::PathService::Get(base::DIR_EXE, &browser_id)) {
    browser_proto->set_browser_id(browser_id.AsUTF8Unsafe());
  }
  browser_proto->set_chrome_version(
      std::string(version_info::GetVersionNumber()));
  if (include_device_info) {
    browser_proto->set_machine_user(policy::GetOSUsername());
  }
}

void ConnectorsServiceBase::PopulateDeviceMetadata(
    const ReportingSettings& reporting_settings,
    const std::string& client_id,
    ClientMetadata::Device* device_proto) {
  if (!reporting_settings.per_profile && !device_proto->has_dm_token()) {
    device_proto->set_dm_token(reporting_settings.dm_token);
  }
  device_proto->set_client_id(client_id);
  device_proto->set_os_version(policy::GetOSVersion());
  device_proto->set_os_platform(policy::GetOSPlatform());
  device_proto->set_name(policy::GetDeviceName());
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    device_proto->set_device_fqdn(policy::GetDeviceFqdn());
    device_proto->set_network_name(policy::GetNetworkName());
  }
}

}  // namespace enterprise_connectors
