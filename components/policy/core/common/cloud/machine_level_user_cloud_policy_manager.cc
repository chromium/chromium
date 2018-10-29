// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"

#include <string>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace {

const base::FilePath::CharType kComponentPolicyCache[] =
    FILE_PATH_LITERAL("Machine Level User Cloud Component Policy");

}  // namespace

MachineLevelUserCloudPolicyManager::MachineLevelUserCloudPolicyManager(
    std::unique_ptr<MachineLevelUserCloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const base::FilePath& policy_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : CloudPolicyManager(dm_protocol::kChromeMachineLevelUserCloudPolicyType,
                         std::string(),
                         store.get(),
                         task_runner,
                         network_connection_tracker_getter),
      store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)),
      policy_dir_(policy_dir) {}

MachineLevelUserCloudPolicyManager::~MachineLevelUserCloudPolicyManager() {}

void MachineLevelUserCloudPolicyManager::Connect(
    PrefService* local_state,
    std::unique_ptr<CloudPolicyClient> client) {
  CHECK(!core()->client());

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      client->GetURLLoaderFactory();

  CreateComponentCloudPolicyService(
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
      policy_dir_.Append(kComponentPolicyCache), client.get(),
      schema_registry());
  core()->Connect(std::move(client));
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state,
                                policy_prefs::kUserPolicyRefreshRate);
  if (external_data_manager_)
    external_data_manager_->Connect(std::move(url_loader_factory));
}

bool MachineLevelUserCloudPolicyManager::IsClientRegistered() {
  return client() && client()->is_registered();
}

void MachineLevelUserCloudPolicyManager::Init(SchemaRegistry* registry) {
  DVLOG(1) << "Machine level cloud policy manager initialized";
  ConfigurationPolicyProvider::Init(registry);

  store()->AddObserver(this);

  // Load the policy from disk synchronously once the manager is initalized
  // during Chrome launch if the cache and the global dm token exist.
  store()->LoadImmediately();
}

void MachineLevelUserCloudPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

}  // namespace policy
