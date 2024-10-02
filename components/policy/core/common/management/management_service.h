// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/policy/policy_export.h"
#include "components/prefs/persistent_pref_store.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// For more imformation about this file please read
// //components/policy/core/common/management/management_service.md

class PrefService;
class PrefRegistrySimple;

namespace ui {
class ImageModel;
}

namespace policy {

class ManagementService;

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

  // `cache_pref_name` is an optional that is the name of the pref used to store
  // the management authority from this provider. If this is empty, the provider
  // always returns the up-to-date management authority, otherwise returns the
  // value from the prefs.
  explicit ManagementStatusProvider(const std::string& cache_pref_name);
  virtual ~ManagementStatusProvider();

  // Returns a valid authority if the service or component is managed.
  // The returned value may be a cached value.
  EnterpriseManagementAuthority GetAuthority();

  // Returns a valid authority if the service or component is managed.
  // This value is never ached and may required blocking I/O to get.
  virtual EnterpriseManagementAuthority FetchAuthority() = 0;

  bool RequiresCache() const;
  void UpdateCache(EnterpriseManagementAuthority authority);

  void UsePrefStoreAsCache(scoped_refptr<PersistentPrefStore> pref_store);
  virtual void UsePrefServiceAsCache(PrefService* prefs);

 protected:
  const std::string& cache_pref_name() const { return cache_pref_name_; }

 private:
  absl::variant<PrefService*, scoped_refptr<PersistentPrefStore>> cache_ =
      nullptr;
  const std::string cache_pref_name_;
};

// Interface to gives information related to an entity's management state.
// This class must be used on the main thread at all times.
class POLICY_EXPORT ManagementService {
 public:
  explicit ManagementService(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);
  virtual ~ManagementService();

  // Sets `prefs` as a read-write cache.
  void UsePrefServiceAsCache(PrefService* prefs);

  // Sets `pref_store` as a readonly cache.
  // Use only if a PrefService is not yet available.
  void UsePrefStoreAsCache(scoped_refptr<PersistentPrefStore> pref_store);

  // Refreshes the cached values and call `callback` with the previous and new
  // management authority trustworthiness. This function must only be called on
  // on an instance of ManagementService that is certain to not be destroyed
  // until `callback` is called.
  virtual void RefreshCache(CacheRefreshCallback callback);

  virtual ui::ImageModel* GetManagementIcon();

  // Returns true if `authority` is are actively managed.
  bool HasManagementAuthority(EnterpriseManagementAuthority authority);

  // Returns the highest trustworthiness of the active management authorities.
  ManagementAuthorityTrustworthiness GetManagementAuthorityTrustworthiness();

  // Returns whether there is any management authority at all.
  bool IsManaged();

  // Returns whether the profile is managed because the signed in account is a
  // managed account.
  bool IsAccountManaged();

  // Returns whether the profile is managed because the whole browser is
  // managed.
  bool IsBrowserManaged();

  const std::optional<int>& management_authorities_for_testing() {
    return management_authorities_for_testing_;
  }

  void SetManagementAuthoritiesForTesting(int management_authorities);
  void ClearManagementAuthoritiesForTesting();
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 protected:
  // Sets the management status providers to be used by the service.
  void SetManagementStatusProvider(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers);

  void AddManagementStatusProvider(
      std::unique_ptr<ManagementStatusProvider> provider);

  const std::vector<std::unique_ptr<ManagementStatusProvider>>&
  management_status_providers() {
    return management_status_providers_;
  }

 private:
  // Returns a bitset of with the active `EnterpriseManagementAuthority` on the
  // managed entity.
  int GetManagementAuthorities();

  std::optional<int> management_authorities_for_testing_;
  std::vector<std::unique_ptr<ManagementStatusProvider>>
      management_status_providers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_MANAGEMENT_SERVICE_H_
