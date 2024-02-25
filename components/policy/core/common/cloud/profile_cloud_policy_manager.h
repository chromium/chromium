// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class CloudExternalDataManager;
class CloudPolicyClient;
class ProfileCloudPolicyStore;
class SchemaRegistry;

// Implements a cloud policy manager that initializes the profile level user
// cloud policy.
class POLICY_EXPORT ProfileCloudPolicyManager : public CloudPolicyManager {
 public:
  ProfileCloudPolicyManager(
      std::unique_ptr<ProfileCloudPolicyStore> profile_store,
      const base::FilePath& component_policy_cache_path,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter network_connection_tracker_getter,
      bool is_dasherless = false);
  ProfileCloudPolicyManager(const ProfileCloudPolicyManager&) = delete;
  ProfileCloudPolicyManager& operator=(const ProfileCloudPolicyManager&) =
      delete;
  ~ProfileCloudPolicyManager() override;

  // Creates and initializes an instance of `ProfileCloudPolicyManager`. This
  // should be used instead of the constructor for immediate use of the manager.
  static std::unique_ptr<ProfileCloudPolicyManager> Create(
      const base::FilePath& profile_path,
      SchemaRegistry* schema_registry,
      bool force_immediate_load,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      network::NetworkConnectionTrackerGetter network_connection_tracker_getter,
      bool is_dasherless = false);

  // ConfigurationPolicyProvider:
  void Shutdown() override;

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted or DisconnectAndRemovePolicy gets called.
  void Connect(PrefService* local_state,
               std::unique_ptr<CloudPolicyClient> client) override;

  // Shuts down the ProfileCloudPolicyManager (removes and stops
  // refreshing the cached cloud policy).
  void DisconnectAndRemovePolicy() override;

 private:
  raw_ptr<ProfileCloudPolicyStore> profile_store_;
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;
  const base::FilePath component_policy_cache_path_;
  bool is_dasherless_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_MANAGER_H_
