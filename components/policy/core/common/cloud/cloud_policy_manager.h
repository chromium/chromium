// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
      CloudPolicyStore* cloud_policy_store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  ~CloudPolicyManager() override;

  CloudPolicyCore* core() { return &core_; }
  const CloudPolicyCore* core() const { return &core_; }

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies() override;

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
      PolicySource policy_source,
      CloudPolicyClient* client,
      SchemaRegistry* schema_registry);

  void ClearAndDestroyComponentCloudPolicyService();

  // Convenience accessors to core() components.
  CloudPolicyClient* client() { return core_.client(); }
  const CloudPolicyClient* client() const { return core_.client(); }
  CloudPolicyStore* store() { return core_.store(); }
  const CloudPolicyStore* store() const { return core_.store(); }
  CloudPolicyService* service() { return core_.service(); }
  const CloudPolicyService* service() const { return core_.service(); }
  ComponentCloudPolicyService* component_policy_service() const {
    return component_policy_service_.get();
  }

 private:
  // Completion handler for policy refresh operations.
  void OnRefreshComplete(bool success);

  CloudPolicyCore core_;
  std::unique_ptr<ComponentCloudPolicyService> component_policy_service_;

  // Whether there's a policy refresh operation pending, in which case all
  // policy update notifications are deferred until after it completes.
  bool waiting_for_policy_refresh_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_MANAGER_H_
