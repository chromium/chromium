// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_H_

#include "base/component_export.h"
#include "base/values.h"
#include "chrome/credential_provider/gaiacp/gcpw_version.h"

namespace credential_provider {

// Structure to hold the policies for each user.
struct COMPONENT_EXPORT(GCPW_POLICIES) UserPolicies {
  // Controls whether MDM enrollment is enabled/disabled.
  bool enable_dm_enrollment;

  // Controls whether GCPW should be automatically updated by Omaha/Google
  bool enable_gcpw_auto_update;

  // The GCPW version to pin the device to.
  GcpwVersion gcpw_pinned_version;

  // If set to disabled only 1 GCPW user can be created on the device.
  bool enable_multi_user_login;

  // Number of days after which online login is enforced.
  uint32_t validity_period_days;

  // Creates a default policy for a user on this device honoring any existing
  // registry settings.
  UserPolicies();

  // Creates the user policies by reading the values found in the |dict|
  // dictionary.
  static UserPolicies FromValue(const base::Value::Dict& dict);

  base::Value ToValue() const;

  bool operator==(const UserPolicies& other) const;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_H_
