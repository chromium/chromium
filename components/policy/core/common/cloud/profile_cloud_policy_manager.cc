// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_store.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace {

// Directory inside the profile directory where policy-related resources are
// stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under `kPolicy`, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentPolicyCacheDir[] =
    FILE_PATH_LITERAL("Profile Cloud Components");

}  // namespace

// TODO (crbug/1421330): Replace policy type with
// "google/chrome/profile-level-user" when ready.
ProfileCloudPolicyManager::ProfileCloudPolicyManager(
    std::unique_ptr<ProfileCloudPolicyStore> profile_store,
    const base::FilePath& component_policy_cache_path,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter,
    bool is_dasherless)
    : CloudPolicyManager(
          is_dasherless ? dm_protocol::kChromeUserPolicyType
                        : dm_protocol::kChromeMachineLevelUserCloudPolicyType,
          /*settings_entity_id=*/std::string(),
          std::move(profile_store),
          task_runner,
          std::move(network_connection_tracker_getter)),
      profile_store_(static_cast<ProfileCloudPolicyStore*>(store())),
      external_data_manager_(std::move(external_data_manager)),
      component_policy_cache_path_(component_policy_cache_path),
      is_dasherless_(is_dasherless) {
  if (is_dasherless_) {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "ProfileCloudPolicyManager created for Dasherless profile.";
  }
}

ProfileCloudPolicyManager::~ProfileCloudPolicyManager() = default;

// static
std::unique_ptr<ProfileCloudPolicyManager> ProfileCloudPolicyManager::Create(
    const base::FilePath& profile_path,
    SchemaRegistry* schema_registry,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter,
    bool is_dasherless) {
  std::unique_ptr<policy::ProfileCloudPolicyStore> store =
      policy::ProfileCloudPolicyStore::Create(
          profile_path, background_task_runner, is_dasherless);
  if (force_immediate_load) {
    store->LoadImmediately();
  }

  const base::FilePath component_policy_cache_dir =
      profile_path.Append(kPolicy).Append(kComponentPolicyCacheDir);

  auto manager = std::make_unique<policy::ProfileCloudPolicyManager>(
      std::move(store), component_policy_cache_dir,
      std::unique_ptr<CloudExternalDataManager>(),
      base::SequencedTaskRunner::GetCurrentDefault(),
      network_connection_tracker_getter, is_dasherless);
  manager->Init(schema_registry);
  return manager;
}

void ProfileCloudPolicyManager::Connect(
    PrefService* local_state,
    std::unique_ptr<CloudPolicyClient> client) {
  CHECK(!core()->client());

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      client->GetURLLoaderFactory();

  CreateComponentCloudPolicyService(
      is_dasherless_ ? dm_protocol::kChromeExtensionPolicyType
                     : dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
      component_policy_cache_path_, client.get(), schema_registry());
  core()->Connect(std::move(client));
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state,
                                policy_prefs::kUserPolicyRefreshRate);
  if (external_data_manager_) {
    external_data_manager_->Connect(std::move(url_loader_factory));
  }
}

void ProfileCloudPolicyManager::DisconnectAndRemovePolicy() {
  if (is_dasherless_) {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "Disconnecting policy manager and removing policies for Dasherless "
           "profile.";
  } else {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "Disconnecting policy manager and removing profile-level policies.";
  }

  if (external_data_manager_) {
    external_data_manager_->Disconnect();
  }

  core()->Disconnect();

  // store_->Clear() will publish the updated, empty policy. The component
  // policy service must be cleared before OnStoreLoaded() is issued, so that
  // component policies are also empty at CheckAndPublishPolicy().
  ClearAndDestroyComponentCloudPolicyService();

  // When the |profile_store_| is cleared, it informs the
  // |external_data_manager_| that all external data references have been
  // removed, causing the |external_data_manager_| to clear its cache as well.
  profile_store_->Clear();
}

void ProfileCloudPolicyManager::Shutdown() {
  if (external_data_manager_) {
    external_data_manager_->Disconnect();
  }
  CloudPolicyManager::Shutdown();
}

}  // namespace policy
