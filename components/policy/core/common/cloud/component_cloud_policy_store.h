// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {
class ExternalPolicyData;
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

class ResourceCache;

// Validates protobufs for external policy data, validates the data itself, and
// caches both locally.
//
// The policy data is validated using the credentials that have to be passed
// beforehand using |SetCredentials|. The expectation is that these credentials
// should be the same as used for validating the superior policy (e.g. the user
// policy, the device-local account policy, etc.).
class POLICY_EXPORT ComponentCloudPolicyStore {
 public:
  class POLICY_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Invoked whenever the policies served by policy() have changed, except
    // for the initial Load().
    virtual void OnComponentCloudPolicyStoreUpdated() = 0;
  };

  struct DomainConstants;

  using PurgeFilter =
      base::RepeatingCallback<bool(const PolicyDomain domain,
                                   const std::string& component_id)>;

  // Both the |delegate| and the |cache| must outlive this object.
  // |policy_type| only supports kChromeSigninExtensionPolicyType,
  // kChromeExtensionPolicyType, kChromeMachineLevelExtensionCloudPolicyType.
  // Please update component_cloud_policy_store.cc in case there is new policy
  // type added.
  ComponentCloudPolicyStore(Delegate* delegate,
                            ResourceCache* cache,
                            const std::string& policy_type);
  ComponentCloudPolicyStore(const ComponentCloudPolicyStore&) = delete;
  ComponentCloudPolicyStore& operator=(const ComponentCloudPolicyStore&) =
      delete;
  ~ComponentCloudPolicyStore();

  // Helper that returns true for PolicyDomains that can be managed by this
  // store.
  static bool SupportsDomain(PolicyDomain domain);

  // Returns true if |policy_type| corresponds to a policy domain that can be
  // managed by this store; in that case, the domain constants is assigned to
  // |domain|. Otherwise returns false.
  static bool GetPolicyDomain(const std::string& policy_type,
                              PolicyDomain* domain);

  // The current list of policies.
  const PolicyBundle& policy() const { return policy_bundle_; }

  // The cached hash for namespace |ns|, or the empty string if |ns| is not
  // cached.
  const std::string& GetCachedHash(const PolicyNamespace& ns) const;

  // The passed credentials are used to validate the cached data, and data
  // stored later.
  // All ValidatePolicy() requests without credentials fail.
  void SetCredentials(const std::string& username,
                      const std::string& gaia_id,
                      const std::string& dm_token,
                      const std::string& device_id,
                      const std::string& public_key,
                      int public_key_version);

  // Loads and validates the currently cached protobufs and policy data that are
  // owned by this PolicyStore.
  // This is performed synchronously, and policy() will return the cached
  // policies after this call.
  void Load();

  // Stores the protobuf and |data| for namespace |ns|. The protobuf is passed
  // serialized in |serialized_policy_proto|, and must have been validated
  // before. The protobuf |policy_data| contain the corresponding policy data.
  // The |data| is validated during this call, and its secure hash must match
  // |secure_hash|.
  // Returns false if |data| failed validation, otherwise returns true and the
  // data was stored in the cache.
  bool Store(const PolicyNamespace& ns,
             const std::string& serialized_policy_proto,
             const enterprise_management::PolicyData* policy_data,
             const std::string& secure_hash,
             const std::string& data);

  // Deletes the storage of namespace |ns| and stops serving its policies.
  void Delete(const PolicyNamespace& ns);

  // Deletes the storage of all components that pass for the given
  // |filter|, and stops serving their policies.
  void Purge(const PurgeFilter& filter);

  // Deletes the storage of every component that is owned by this PolicyStore.
  void Clear();

  // Validates |proto| and returns the parsed PolicyData in |policy_data| and
  // parsed ExternalPolicyData in |payload|. It is also validated that |proto|
  // has the policy namespace equal to |ns|.
  // If |proto| validates successfully then its |payload| can be trusted, and
  // the data referenced there can be downloaded. A |proto| must be validated
  // before attempting to download the data, and before storing both.
  bool ValidatePolicy(
      const PolicyNamespace& ns,
      std::unique_ptr<enterprise_management::PolicyFetchResponse> proto,
      enterprise_management::PolicyData* policy_data,
      enterprise_management::ExternalPolicyData* payload,
      std::string* error);

 private:
  // Validates the JSON policy serialized in |data|, and verifies its hash
  // with |secure_hash|. Returns true on success, and in that case stores the
  // parsed policies in |policy|.
  bool ValidateData(const std::string& data,
                    const std::string& secure_hash,
                    PolicyMap* policy,
                    std::string* error);

  // Parses the JSON policy in |data| into |policy|, and returns true if the
  // parse was successful.
  bool ParsePolicy(const std::string& data,
                   PolicyMap* policy,
                   std::string* error);

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<ResourceCache> cache_;

  // The following fields contain credentials used for validating the policy.
  std::string username_;
  std::string gaia_id_;
  std::string dm_token_;
  std::string device_id_;
  std::string public_key_;
  int public_key_version_ = -1;

  // The current list of policies.
  PolicyBundle policy_bundle_;
  // Mapping from policy namespace to data hashes for each currently exposed
  // component.
  std::map<PolicyNamespace, std::string> cached_hashes_;
  // Mapping from policy namespace to policy timestamp for each currently
  // exposed component.
  std::map<PolicyNamespace, base::Time> stored_policy_times_;

  raw_ptr<const DomainConstants> domain_constants_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_
