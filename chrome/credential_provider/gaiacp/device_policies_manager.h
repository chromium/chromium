// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_MANAGER_H_

#include "base/component_export.h"
#include "chrome/credential_provider/gaiacp/device_policies.h"

namespace credential_provider {

// Manager used to fetch user policies from GCPW backends.
class COMPONENT_EXPORT(GCPW_POLICIES) DevicePoliciesManager {
 public:
  // Get the user policies manager instance.
  static DevicePoliciesManager* Get();

  // Return true if cloud policies feature is enabled.
  bool CloudPoliciesEnabled() const;

  // Returns the effective policy to follow on the device by combining the
  // policies of all the existing users on the device.
  virtual void GetDevicePolicies(DevicePolicies* device_policies);

  // Make sure GCPW update is set up correctly.
  void EnforceGcpwUpdatePolicy();

  // Creates an Omaha policy with the list of allowed |domains| for GCPW to be
  // used in tests.
  bool SetAllowedDomainsOmahaPolicyForTesting(
      const std::vector<std::wstring>& domains);

 protected:
  // Returns the storage used for the instance pointer.
  static DevicePoliciesManager** GetInstanceStorage();

  DevicePoliciesManager();
  virtual ~DevicePoliciesManager();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_DEVICE_POLICIES_MANAGER_H_
