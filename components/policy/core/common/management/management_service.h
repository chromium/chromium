// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "components/policy/policy_export.h"

namespace policy {

class ScopedManagementServiceOverrideForTesting;

enum class ManagementTarget { PLATFORM = 0, BROWSER = 1, kMaxValue = BROWSER };

enum class ManagementAuthorityTrustworthiness {
  NONE = 0,           // No management authority found
  LOW = 1,            // Local device management authority
  TRUSTED = 2,        // Non-local management authority
  FULLY_TRUSTED = 3,  // Cryptographically verifiable policy source e.g. CBCM,
                      // ChromeOS
  kMaxValue = FULLY_TRUSTED
};

enum class EnterpriseManagementAuthority {
  COMPUTER_LOCAL = 0,  // local GPO or registry, /etc files, local root profile
  DOMAIN_LOCAL = 1,    // AD joined, puppet
  CLOUD = 2,           // MDM, GSuite user
  CLOUD_DOMAIN = 3,    // Azure AD, CBCM, CrosEnrolled
  kMaxValue = CLOUD_DOMAIN
};

// Interface to provide management information from a single source on an entity
// to a ManagementService. All implmementations of this interface must be used
// by a ManagementService.
class POLICY_EXPORT ManagementStatusProvider {
 public:
  virtual ~ManagementStatusProvider();

  // Returns |true| if the service or component is managed.
  virtual bool IsManaged() = 0;

  // Returns the authority responsible for the management.
  virtual EnterpriseManagementAuthority GetAuthority() = 0;
};

// Interface to gives information related to an entity's management state.
class POLICY_EXPORT ManagementService {
 public:
  explicit ManagementService(ManagementTarget target);
  virtual ~ManagementService();

  // Returns all the active management authorities on the managed entity.
  // Returns an empty set if the entity is not managed.
  base::flat_set<EnterpriseManagementAuthority> GetManagementAuthorities();

  // Returns the highest trustworthiness of the active management authorities.
  ManagementAuthorityTrustworthiness GetManagementAuthorityTrustworthiness();

  // Returns whether there is any management authority at all.
  bool IsManaged();

 protected:
  // Initializes the management status providers.
  virtual void InitManagementStatusProviders() = 0;

  // Sets the management status providers to be used by the service.
  void SetManagementStatusProvider(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

 private:
  std::vector<std::unique_ptr<ManagementStatusProvider>>
      management_status_providers_;
  ManagementTarget target_;

  static void SetManagementAuthoritiesForTesting(
      ManagementTarget target,
      base::flat_set<EnterpriseManagementAuthority> authorities);
  static void RemoveManagementAuthoritiesForTesting(ManagementTarget target);
  friend ScopedManagementServiceOverrideForTesting;
};

}  // namespace policy

#endif  // #define
        // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
