// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/macros.h"
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
  ~MachineLevelUserCloudPolicyManager() override;

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted.
  void Connect(PrefService* local_state,
               std::unique_ptr<CloudPolicyClient> client);

  // Returns true if the underlying CloudPolicyClient is already registered.
  bool IsClientRegistered();

  MachineLevelUserCloudPolicyStore* store() { return store_.get(); }

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;

 private:
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store_;
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  const base::FilePath policy_dir_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_MANAGER_H_
