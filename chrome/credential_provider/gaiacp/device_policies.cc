// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace {

// Default value of each device policy.
constexpr bool kDevicePolicyDefaultDeviceEnrollment = true;
constexpr bool kDevicePolicyDefaultGcpwAutoUpdate = true;
constexpr bool kDevicePolicyDefaultMultiUserLogin = true;

// Read the list of domains allowed to login from registry.
std::vector<std::wstring> GetRegistryEmailDomains() {
  std::wstring email_domains_reg =
      GetGlobalFlagOrDefault(kEmailDomainsKey, L"");
  std::wstring email_domains_reg_new =
      GetGlobalFlagOrDefault(kEmailDomainsKeyNew, L"");
  std::wstring email_domains =
      email_domains_reg.empty() ? email_domains_reg_new : email_domains_reg;
  return base::SplitString(email_domains, L",",
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}
}  // namespace

// static
DevicePolicies DevicePolicies::FromUserPolicies(
    const UserPolicies& user_policies) {
  DevicePolicies device_policies;

  // Override with what is set for the user.
  device_policies.enable_dm_enrollment = user_policies.enable_dm_enrollment;
  device_policies.enable_gcpw_auto_update =
      user_policies.enable_gcpw_auto_update;
  device_policies.gcpw_pinned_version = user_policies.gcpw_pinned_version;
  device_policies.enable_multi_user_login =
      user_policies.enable_multi_user_login;

  return device_policies;
}

DevicePolicies::~DevicePolicies() = default;

DevicePolicies::DevicePolicies(const DevicePolicies& other) = default;

DevicePolicies::DevicePolicies()
    : enable_dm_enrollment(kDevicePolicyDefaultDeviceEnrollment),
      enable_gcpw_auto_update(kDevicePolicyDefaultGcpwAutoUpdate),
      enable_multi_user_login(kDevicePolicyDefaultMultiUserLogin) {
  // Override with the policies set in the registry.
  domains_allowed_to_login = GetRegistryEmailDomains();

  std::wstring mdm_url = GetGlobalFlagOrDefault(kRegMdmUrl, kDefaultMdmUrl);
  DWORD reg_enable_dm_enrollment;
  HRESULT hr = GetGlobalFlag(kRegEnableDmEnrollment, &reg_enable_dm_enrollment);
  if (SUCCEEDED(hr)) {
    enable_dm_enrollment = reg_enable_dm_enrollment && !mdm_url.empty();
  } else {
    enable_dm_enrollment = !mdm_url.empty();
  }

  DWORD reg_supports_multi_user;
  hr = GetGlobalFlag(kRegMdmSupportsMultiUser, &reg_supports_multi_user);
  if (SUCCEEDED(hr)) {
    enable_multi_user_login = reg_supports_multi_user == 1;
  }
}

void DevicePolicies::MergeWith(const DevicePolicies& other) {
  // If any user is allowed to do perform these then it should be allowed on the
  // device.
  enable_dm_enrollment |= other.enable_dm_enrollment;
  enable_gcpw_auto_update |= other.enable_gcpw_auto_update;
  enable_multi_user_login |= other.enable_multi_user_login;

  // Choose the latest allowed GCPW version.
  gcpw_pinned_version = (gcpw_pinned_version < other.gcpw_pinned_version)
                            ? other.gcpw_pinned_version
                            : gcpw_pinned_version;
}

bool DevicePolicies::operator==(const DevicePolicies& other) const {
  return (enable_dm_enrollment == other.enable_dm_enrollment) &&
         (enable_gcpw_auto_update == other.enable_gcpw_auto_update) &&
         (gcpw_pinned_version == other.gcpw_pinned_version) &&
         (enable_multi_user_login == other.enable_multi_user_login) &&
         (domains_allowed_to_login == other.domains_allowed_to_login);
}

std::wstring DevicePolicies::GetAllowedDomainsStr() const {
  return base::JoinString(domains_allowed_to_login, L",");
}

}  // namespace credential_provider
