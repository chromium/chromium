// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefService;

namespace policy {

class MachineLevelUserCloudPolicyStore;

// Implements a cloud policy manager that initializes the machine level user
// cloud policy.
class POLICY_EXPORT MachineLevelUserCloudPolicyManager
    : public CloudPolicyManager {
 public:
  MachineLevelUserCloudPolicyManager(
      std::unique_ptr<MachineLevelUserCloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const base::FilePath& policy_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  MachineLevelUserCloudPolicyManager(
      const MachineLevelUserCloudPolicyManager&) = delete;
  MachineLevelUserCloudPolicyManager& operator=(
      const MachineLevelUserCloudPolicyManager&) = delete;
  ~MachineLevelUserCloudPolicyManager() override;

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted or DisconnectAndRemovePolicy gets called.
  void Connect(PrefService* local_state,
               std::unique_ptr<CloudPolicyClient> client) override;

  // Add or remove |observer| to/from the CloudPolicyClient embedded in |core_|.
  void AddClientObserver(CloudPolicyClient::Observer* observer);
  void RemoveClientObserver(CloudPolicyClient::Observer* observer);

  MachineLevelUserCloudPolicyStore* store() { return user_store_.get(); }

  // Shuts down the MachineLevelUserCloudPolicyManager (removes and stops
  // refreshing the cached cloud policy).
  void DisconnectAndRemovePolicy() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;

 private:
  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;

  raw_ptr<MachineLevelUserCloudPolicyStore> user_store_;
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  const base::FilePath policy_dir_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_
