// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_

#include <memory>
#include <vector>

#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

enum class ManagementAuthorityTrustworthiness {
  NONE = 0,           // No management authority found
  LOW = 1,            // Local device management authority
  TRUSTED = 2,        // Non-local management authority
  FULLY_TRUSTED = 3,  // Cryptographically verifiable policy source e.g. CBCM,
                      // ChromeOS
  kMaxValue = FULLY_TRUSTED
};

enum EnterpriseManagementAuthority : uint64_t {
  NONE = 0,
  COMPUTER_LOCAL =
      1 << 0,  // local GPO or registry, /etc files, local root profile
  DOMAIN_LOCAL = 1 << 1,  // AD joined, puppet
  CLOUD = 1 << 2,         // MDM, GSuite user
  CLOUD_DOMAIN = 1 << 3,  // Azure AD, CBCM, CrosEnrolled
  kMaxValue = CLOUD_DOMAIN
};

// Interface to provide management information from a single source on an entity
// to a ManagementService. All implmementations of this interface must be used
// by a ManagementService.
class POLICY_EXPORT ManagementStatusProvider {
 public:
  virtual ~ManagementStatusProvider();

  // Returns a valid authority if the service or component is managed.
  virtual EnterpriseManagementAuthority GetAuthority() = 0;
};

// Interface to gives information related to an entity's management state.
class POLICY_EXPORT ManagementService {
 public:
  explicit ManagementService(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);
  virtual ~ManagementService();

  // Returns true if `authority` is are actively managed.
  bool HasManagementAuthority(EnterpriseManagementAuthority authority);

  // Returns the highest trustworthiness of the active management authorities.
  ManagementAuthorityTrustworthiness GetManagementAuthorityTrustworthiness();

  // Returns whether there is any management authority at all.
  bool IsManaged();

  const absl::optional<uint64_t>& management_authorities_for_testing() {
    return management_authorities_for_testing_;
  }
  void SetManagementAuthoritiesForTesting(uint64_t management_authorities);
  void ClearManagementAuthoritiesForTesting();

 protected:
  // Sets the management status providers to be used by the service.
  void SetManagementStatusProvider(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

 private:
  // Returns a bitset of with the active `EnterpriseManagementAuthority` on the
  // managed entity.
  uint64_t GetManagementAuthorities();

  absl::optional<uint64_t> management_authorities_for_testing_;
  std::vector<std::unique_ptr<ManagementStatusProvider>>
      management_status_providers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
