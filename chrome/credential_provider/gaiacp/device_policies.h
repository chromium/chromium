// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chrome/credential_provider/gaiacp/user_policies.h"

namespace credential_provider {

// Structure to hold the policies for the device.
struct COMPONENT_EXPORT(GCPW_POLICIES) DevicePolicies {
  // Controls whether MDM enrollment is enabled/disabled.
  bool enable_dm_enrollment;

  // Controls whether GCPW should be automatically updated by Omaha/Google
  // Update.
  bool enable_gcpw_auto_update;

  // The GCPW version to pin the device to.
  GcpwVersion gcpw_pinned_version;

  // If set to disabled only a single GCPW user can be created on the device.
  bool enable_multi_user_login;

  // The list of domains from which the users are allowed to sign in to the
  // device.
  std::vector<std::wstring> domains_allowed_to_login;

  // Creates a default policy for the device honoring any existing registry
  // settings.
  DevicePolicies();

  ~DevicePolicies();

  DevicePolicies(const DevicePolicies& other);

  // Creates a device policy from the policy specified for the user.
  static DevicePolicies FromUserPolicies(const UserPolicies& user_policies);

  // Merges the existing policies with the given policies resolving any
  // conflicts.
  void MergeWith(const DevicePolicies& other);

  bool operator==(const DevicePolicies& other) const;

  // Get a string with comma separated values from domains_allowed_to_login.
  std::wstring GetAllowedDomainsStr() const;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_H_
