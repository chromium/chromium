// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/user_policies.h"

#include <limits>

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/device_policies.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace {

// Parameter names that are used in the JSON payload of the response.
const char kGcpwPolicyDmEnrollmentParameterName[] = "enableDmEnrollment";
const char kGcpwPolicyAutoUpdateParameterName[] = "enableGcpwAutoUpdate";
const char kGcpwPolicyPinnerVersionParameterName[] = "gcpwPinnedVersion";
const char kGcpwPolicMultiUserLoginParameterName[] = "enableMultiUserLogin";
const char kGcpwPolicyValidityPeriodParameterName[] = "validityPeriodDays";

// Default value of each user policy.
constexpr bool kUserPolicyDefaultDeviceEnrollment = true;
constexpr bool kUserPolicyDefaultGcpwAutoUpdate = true;
constexpr bool kUserPolicyDefaultMultiUserLogin = true;
constexpr int kUserPolicyDefaultValidityPeriodDays =
    std::numeric_limits<int>::max();

}  // namespace

UserPolicies::UserPolicies()
    : enable_dm_enrollment(kUserPolicyDefaultDeviceEnrollment),
      enable_gcpw_auto_update(kUserPolicyDefaultGcpwAutoUpdate),
      enable_multi_user_login(kUserPolicyDefaultMultiUserLogin),
      validity_period_days(kUserPolicyDefaultValidityPeriodDays) {
  // Override with the default device policy.
  DevicePolicies device_policies;
  enable_dm_enrollment = device_policies.enable_dm_enrollment;
  enable_gcpw_auto_update = device_policies.enable_gcpw_auto_update;
  enable_multi_user_login = device_policies.enable_multi_user_login;

  // Override with existing registry entry if any.
  DWORD reg_validity_period_days;
  HRESULT hr = GetGlobalFlag(base::UTF8ToWide(kKeyValidityPeriodInDays),
                             &reg_validity_period_days);
  if (SUCCEEDED(hr)) {
    validity_period_days = reg_validity_period_days;
  }
}

// static
UserPolicies UserPolicies::FromValue(const base::Value& dict) {
  DCHECK(dict.is_dict());

  UserPolicies user_policies;

  absl::optional<bool> dm_enrollment =
      dict.FindBoolKey(kGcpwPolicyDmEnrollmentParameterName);
  if (dm_enrollment) {
    user_policies.enable_dm_enrollment = *dm_enrollment;
  }

  absl::optional<bool> gcpw_auto_update =
      dict.FindBoolKey(kGcpwPolicyAutoUpdateParameterName);
  if (gcpw_auto_update) {
    user_policies.enable_gcpw_auto_update = *gcpw_auto_update;
  }

  const std::string* pin_version =
      dict.FindStringKey(kGcpwPolicyPinnerVersionParameterName);
  if (pin_version) {
    user_policies.gcpw_pinned_version = GcpwVersion(*pin_version);
  }

  absl::optional<bool> multi_user_login =
      dict.FindBoolKey(kGcpwPolicMultiUserLoginParameterName);
  if (multi_user_login) {
    user_policies.enable_multi_user_login = *multi_user_login;
  }

  absl::optional<int> validity_period_days =
      dict.FindIntKey(kGcpwPolicyValidityPeriodParameterName);
  if (validity_period_days) {
    user_policies.validity_period_days = *validity_period_days;
  }

  return user_policies;
}

base::Value UserPolicies::ToValue() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey(kGcpwPolicyDmEnrollmentParameterName, enable_dm_enrollment);
  dict.SetBoolKey(kGcpwPolicyAutoUpdateParameterName, enable_gcpw_auto_update);
  dict.SetStringKey(kGcpwPolicyPinnerVersionParameterName,
                    gcpw_pinned_version.ToString());
  dict.SetBoolKey(kGcpwPolicMultiUserLoginParameterName,
                  enable_multi_user_login);
  dict.SetIntKey(kGcpwPolicyValidityPeriodParameterName, validity_period_days);
  return dict;
}

bool UserPolicies::operator==(const UserPolicies& other) const {
  return (enable_dm_enrollment == other.enable_dm_enrollment) &&
         (enable_gcpw_auto_update == other.enable_gcpw_auto_update) &&
         (gcpw_pinned_version == other.gcpw_pinned_version) &&
         (enable_multi_user_login == other.enable_multi_user_login) &&
         (validity_period_days == other.validity_period_days);
}

}  // namespace credential_provider
