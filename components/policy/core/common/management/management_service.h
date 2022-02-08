// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class PersistentPrefStore;
class PrefService;

namespace policy {

enum class ManagementAuthorityTrustworthiness {
  NONE = 0,           // No management authority found
  LOW = 1,            // Local device management authority
  TRUSTED = 2,        // Non-local management authority
  FULLY_TRUSTED = 3,  // Cryptographically verifiable policy source e.g. CBCM,
                      // ChromeOS
  kMaxValue = FULLY_TRUSTED
};

enum EnterpriseManagementAuthority : int {
  NONE = 0,
  COMPUTER_LOCAL =
      1 << 0,  // local GPO or registry, /etc files, local root profile
  DOMAIN_LOCAL = 1 << 1,  // AD joined, puppet
  CLOUD = 1 << 2,         // MDM, GSuite user
  CLOUD_DOMAIN = 1 << 3   // Azure AD, CBCM, CrosEnrolled
};

using CacheRefreshCallback =
    base::OnceCallback<void(ManagementAuthorityTrustworthiness,
                            ManagementAuthorityTrustworthiness)>;

// Interface to provide management information from a single source on an entity
// to a ManagementService. All implmementations of this interface must be used
// by a ManagementService.
class POLICY_EXPORT ManagementStatusProvider {
 public:
  ManagementStatusProvider();
  explicit ManagementStatusProvider(const std::string& cache_pref_name);
  virtual ~ManagementStatusProvider();

  // Returns a valid authority if the service or component is managed.
  EnterpriseManagementAuthority GetAuthority();

  bool RequiresCache() const;
  void RefreshCache();

  void UsePrefStoreAsCache(scoped_refptr<PersistentPrefStore> pref_store);
  virtual void UsePrefServiceAsCache(PrefService* prefs);

 protected:
  // Returns a valid authority if the service or component is managed.
  virtual EnterpriseManagementAuthority FetchAuthority() = 0;
  const std::string& cache_pref_name() const { return cache_pref_name_; }

 private:
  absl::optional<int> management_authority_cache_;
  absl::variant<scoped_refptr<PersistentPrefStore>, PrefService*> cache_;
  const std::string cache_pref_name_;
};

// Interface to gives information related to an entity's management state.
class POLICY_EXPORT ManagementService {
 public:
  explicit ManagementService(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);
  ManagementService(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers,
      const std::string& cache_pref_name);
  virtual ~ManagementService();

  // Sets `prefs` as a read-write cache.
  void UsePrefServiceAsCache(PrefService* prefs);

  // Sets `pref_store` as a readonly cache.
  // Use only if a PrefService is not yet available.
  void UsePrefStoreAsCache(scoped_refptr<PersistentPrefStore> pref_store);

  // Refreshes the cached values and call `callback` with the previous and new
  // management authority thrustworthiness.
  void RefreshCache(CacheRefreshCallback callback);

  // Returns true if `authority` is are actively managed.
  bool HasManagementAuthority(EnterpriseManagementAuthority authority);

  // Returns the highest trustworthiness of the active management authorities.
  ManagementAuthorityTrustworthiness GetManagementAuthorityTrustworthiness();

  // Returns whether there is any management authority at all.
  bool IsManaged();

  const absl::optional<int>& management_authorities_for_testing() {
    return management_authorities_for_testing_;
  }

  void SetManagementAuthoritiesForTesting(int management_authorities);
  void ClearManagementAuthoritiesForTesting();
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

 protected:
  // Sets the management status providers to be used by the service.
  void SetManagementStatusProvider(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

 private:
  void RefreshCacheImpl(CacheRefreshCallback callback);
  // Returns a bitset of with the active `EnterpriseManagementAuthority` on the
  // managed entity.
  int GetManagementAuthorities();

  absl::optional<int> management_authorities_for_testing_;
  std::vector<std::unique_ptr<ManagementStatusProvider>>
      management_status_providers_;
  base::WeakPtrFactory<ManagementService> weak_ptr_factory{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
