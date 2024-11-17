// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace {

// Directory inside the profile directory where policy-related resources are
// stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under `kPolicy`, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

}  // namespace

namespace policy {

UserCloudPolicyManager::UserCloudPolicyManager(
    std::unique_ptr<UserCloudPolicyStore> user_store,
    const base::FilePath& component_policy_cache_path,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : CloudPolicyManager(dm_protocol::kChromeUserPolicyType,
                         std::string(),
                         std::move(user_store),
                         task_runner,
                         network_connection_tracker_getter),
      user_store_(static_cast<UserCloudPolicyStore*>(store())),
      component_policy_cache_path_(component_policy_cache_path),
      external_data_manager_(std::move(external_data_manager)) {}

UserCloudPolicyManager::~UserCloudPolicyManager() = default;

std::unique_ptr<UserCloudPolicyManager> UserCloudPolicyManager::Create(
    const base::FilePath& profile_path,
    SchemaRegistry* schema_registry,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter) {
  std::unique_ptr<UserCloudPolicyStore> store =
      UserCloudPolicyStore::Create(profile_path, background_task_runner);
  if (force_immediate_load)
    store->LoadImmediately();

  const base::FilePath component_policy_cache_dir =
      profile_path.Append(kPolicy).Append(kComponentsDir);

  auto policy_manager = std::make_unique<UserCloudPolicyManager>(
      std::move(store), component_policy_cache_dir,
      std::unique_ptr<CloudExternalDataManager>(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network_connection_tracker_getter);
  policy_manager->Init(schema_registry);
  return policy_manager;
}

void UserCloudPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

void UserCloudPolicyManager::SetSigninAccountId(const AccountId& account_id) {
  if (account_id.is_valid()) {
    // Start the recorder as it is assumed that there is now a valid managed
    // account.
    StartRecordingMetric();
  }

  user_store_->SetSigninAccountId(account_id);
}

void UserCloudPolicyManager::SetPoliciesRequired(bool required,
                                                 PolicyFetchReason reason) {
  policies_required_ = required;
  RefreshPolicies(reason);
}

bool UserCloudPolicyManager::ArePoliciesRequired() const {
  return policies_required_;
}

void UserCloudPolicyManager::Connect(
    PrefService* local_state,
    std::unique_ptr<CloudPolicyClient> client) {
  CHECK(!core()->client());

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      client->GetURLLoaderFactory();

  CreateComponentCloudPolicyService(dm_protocol::kChromeExtensionPolicyType,
                                    component_policy_cache_path_, client.get(),
                                    schema_registry());
  core()->Connect(std::move(client));
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state,
                                policy_prefs::kUserPolicyRefreshRate);
  if (external_data_manager_)
    external_data_manager_->Connect(std::move(url_loader_factory));
}

void UserCloudPolicyManager::DisconnectAndRemovePolicy() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  core()->Disconnect();

  // store_->Clear() will publish the updated, empty policy. The component
  // policy service must be cleared before OnStoreLoaded() is issued, so that
  // component policies are also empty at CheckAndPublishPolicy().
  ClearAndDestroyComponentCloudPolicyService();

  // When the |user_store_| is cleared, it informs the |external_data_manager_|
  // that all external data references have been removed, causing the
  // |external_data_manager_| to clear its cache as well.
  user_store_->Clear();
  SetPoliciesRequired(false, PolicyFetchReason::kDisconnect);
}

void UserCloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  CloudPolicyManager::GetChromePolicy(policy_map);

  // If the store has a verified policy blob received from the server then apply
  // the defaults for policies that haven't been configured by the administrator
  // given that this is an enterprise user.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!store()->has_policy())
    return;

  // TODO(https://crbug.com/1206315): Don't apply enterprise defaults for Child
  // user.
  SetEnterpriseUsersProfileDefaults(policy_map);
#endif
#if BUILDFLAG(IS_ANDROID)
  if (store()->has_policy() &&
      !policy_map->Get(key::kNTPContentSuggestionsEnabled)) {
    policy_map->Set(key::kNTPContentSuggestionsEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                    base::Value(false), nullptr /* external_data_fetcher */);
  }
#endif
}

bool UserCloudPolicyManager::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return !policies_required_ ||
         CloudPolicyManager::IsFirstPolicyLoadComplete(domain);
}

void UserCloudPolicyManager::StartRecordingMetric() {
  // Starts a recording session by creating the recorder.
  metrics_recorder_ = std::make_unique<UserPolicyMetricsRecorder>(this);
}

}  // namespace policy
