// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_member.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace policy {

class PolicyMap;

// CloudPolicyManager is the main switching central between cloud policy and the
// upper layers of the policy stack. It wires up a CloudPolicyCore to the
// ConfigurationPolicyProvider interface.
//
// This class contains the base functionality, there are subclasses that add
// functionality specific to user-level and device-level cloud policy, such as
// blocking on initial user policy fetch or device enrollment.
class POLICY_EXPORT CloudPolicyManager
    : public ConfigurationPolicyProvider,
      public CloudPolicyStore::Observer,
      public ComponentCloudPolicyService::Delegate {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  CloudPolicyManager(
      const std::string& policy_type,
      const std::string& settings_entity_id,
      std::unique_ptr<CloudPolicyStore> cloud_policy_store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  CloudPolicyManager(const CloudPolicyManager&) = delete;
  CloudPolicyManager& operator=(const CloudPolicyManager&) = delete;
  ~CloudPolicyManager() override;

  CloudPolicyCore* core() { return &core_; }
  const CloudPolicyCore* core() const { return &core_; }
  ComponentCloudPolicyService* component_policy_service() const {
    return component_policy_service_.get();
  }

  // Returns true if the underlying CloudPolicyClient is already registered.
  // Virtual for mocking.
  virtual bool IsClientRegistered() const;

  virtual void Connect(PrefService* local_state,
                       std::unique_ptr<CloudPolicyClient> client) {}

  // Shuts down the CloudPolicyManager (removes and stops refreshing any
  // cached cloud policy).
  virtual void DisconnectAndRemovePolicy() {}

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;
  void RefreshPolicies(PolicyFetchReason reason) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;
  void OnStoreError(CloudPolicyStore* cloud_policy_store) override;

  // ComponentCloudPolicyService::Delegate:
  void OnComponentCloudPolicyUpdated() override;

 protected:
  // Check whether fully initialized and if so, publish policy by calling
  // ConfigurationPolicyStore::UpdatePolicy().
  void CheckAndPublishPolicy();

  // Writes Chrome policy into |policy_map|. This is intended to be overridden
  // by subclasses that want to post-process policy before publishing it. The
  // default implementation just copies over |store()->policy_map()|.
  virtual void GetChromePolicy(PolicyMap* policy_map);

  void CreateComponentCloudPolicyService(
      const std::string& policy_type,
      const base::FilePath& policy_cache_path,
      CloudPolicyClient* client,
      SchemaRegistry* schema_registry);

  void ClearAndDestroyComponentCloudPolicyService();

  // Convenience accessors to core() components.
  CloudPolicyClient* client() { return core_.client(); }
  const CloudPolicyClient* client() const { return core_.client(); }
  CloudPolicyStore* store() { return store_.get(); }
  const CloudPolicyStore* store() const { return store_.get(); }
  CloudPolicyService* service() { return core_.service(); }
  const CloudPolicyService* service() const { return core_.service(); }

 private:
  // Completion handler for policy refresh operations.
  void OnRefreshComplete(bool success);

  std::unique_ptr<CloudPolicyStore> store_;
  CloudPolicyCore core_;
  std::unique_ptr<ComponentCloudPolicyService> component_policy_service_;

  // Whether there's a policy refresh operation pending, in which case all
  // policy update notifications are deferred until after it completes.
  bool waiting_for_policy_refresh_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_
