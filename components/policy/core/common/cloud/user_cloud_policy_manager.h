// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/policy_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class AccountId;
class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudExternalDataManager;
class DeviceManagementService;
class UserCloudPolicyStore;

// UserCloudPolicyManager handles initialization of user policy.
class POLICY_EXPORT UserCloudPolicyManager : public CloudPolicyManager {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  UserCloudPolicyManager(
      std::unique_ptr<UserCloudPolicyStore> store,
      const base::FilePath& component_policy_cache_path,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  ~UserCloudPolicyManager() override;

  // ConfigurationPolicyProvider overrides:
  void Shutdown() override;

  void SetSigninAccountId(const AccountId& account_id);

  // Initializes the cloud connection. |local_state| must stay valid until this
  // object is deleted or DisconnectAndRemovePolicy() gets called. Virtual for
  // mocking.
  virtual void Connect(
      PrefService* local_state,
      std::unique_ptr<CloudPolicyClient> client);

  // Shuts down the UserCloudPolicyManager (removes and stops refreshing the
  // cached cloud policy). This is typically called when a profile is being
  // disassociated from a given user (e.g. during signout). No policy will be
  // provided by this object until the next time Initialize() is invoked.
  void DisconnectAndRemovePolicy();

  // Returns true if the underlying CloudPolicyClient is already registered.
  // Virtual for mocking.
  virtual bool IsClientRegistered() const;

  // Creates a CloudPolicyClient for this client. Used in situations where
  // callers want to create a DMToken without actually initializing the
  // profile's policy infrastructure (for example, during signin when we
  // want to check if the user's domain requires policy).
  static std::unique_ptr<CloudPolicyClient> CreateCloudPolicyClient(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  // CloudPolicyManager:
  void GetChromePolicy(PolicyMap* policy_map) override;

  // Typed pointer to the store owned by UserCloudPolicyManager. Note that
  // CloudPolicyManager only keeps a plain CloudPolicyStore pointer.
  std::unique_ptr<UserCloudPolicyStore> store_;

  // Path where policy for components will be cached.
  base::FilePath component_policy_cache_path_;

  // Manages external data referenced by policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_MANAGER_H_
