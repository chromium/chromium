// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_policy_metrics_recorder.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class AccountId;
class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudExternalDataManager;
class SchemaRegistry;
class UserCloudPolicyStore;

// UserCloudPolicyManager handles initialization of user policy.
class POLICY_EXPORT UserCloudPolicyManager : public CloudPolicyManager {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  UserCloudPolicyManager(
      std::unique_ptr<UserCloudPolicyStore> user_store,
      const base::FilePath& component_policy_cache_path,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  UserCloudPolicyManager(const UserCloudPolicyManager&) = delete;
  UserCloudPolicyManager& operator=(const UserCloudPolicyManager&) = delete;
  ~UserCloudPolicyManager() override;

  static std::unique_ptr<UserCloudPolicyManager> Create(
      const base::FilePath& profile_path,
      SchemaRegistry* schema_registry,
      bool force_immediate_load,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);

  // ConfigurationPolicyProvider overrides:
  void Shutdown() override;

  void SetSigninAccountId(const AccountId& account_id);

  // Sets whether or not policies are required for this policy manager.
  // This might be set to false if the user profile is an unmanaged consumer
  // profile.
  //
  // As a side effect, this also calls `RefreshPolicies`, which is why the
  // `reason` parameter is required.
  void SetPoliciesRequired(bool required, PolicyFetchReason reason);
  bool ArePoliciesRequired() const;

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted or DisconnectAndRemovePolicy() gets called. Virtual for
  // mocking.
  void Connect(PrefService* local_state,
               std::unique_ptr<CloudPolicyClient> client) override;

  // Shuts down the UserCloudPolicyManager (removes and stops refreshing the
  // cached cloud policy). This is typically called when a profile is being
  // disassociated from a given user (e.g. during signout). No policy will be
  // provided by this object until the next time Initialize() is invoked.
  void DisconnectAndRemovePolicy() override;

  // ConfigurationPolicyProvider:
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;

  UserCloudPolicyStore* user_store() const { return user_store_; }

 private:
  // CloudPolicyManager:
  void GetChromePolicy(PolicyMap* policy_map) override;

  // Starts recording metrics.
  void StartRecordingMetric();

  bool policies_required_ = false;

  // Typed pointer to the store owned by CloudPolicyManager.
  raw_ptr<UserCloudPolicyStore> user_store_;

  // Path where policy for components will be cached.
  base::FilePath component_policy_cache_path_;

  // Manages external data referenced by policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  // Metrics recorder.
  std::unique_ptr<UserPolicyMetricsRecorder> metrics_recorder_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
