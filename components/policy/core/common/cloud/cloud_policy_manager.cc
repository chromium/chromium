// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/core/common/cloud/resource_cache.h"
#endif

namespace policy {

CloudPolicyManager::CloudPolicyManager(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    std::unique_ptr<CloudPolicyStore> cloud_policy_store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : store_(std::move(cloud_policy_store)),
      core_(policy_type,
            settings_entity_id,
            store_.get(),
            task_runner,
            std::move(network_connection_tracker_getter)),
      waiting_for_policy_refresh_(false) {}

CloudPolicyManager::~CloudPolicyManager() = default;

bool CloudPolicyManager::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void CloudPolicyManager::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store()->AddObserver(this);

  // If the underlying store is already initialized, pretend it was loaded now.
  // Note: It is not enough to just copy OnStoreLoaded's contents here because
  // subclasses can override it.
  if (store()->is_initialized())
    OnStoreLoaded(store());
  else
    store()->Load();
}

void CloudPolicyManager::Shutdown() {
  component_policy_service_.reset();
  core_.Disconnect();
  store()->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool CloudPolicyManager::IsInitializationComplete(PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store()->is_initialized();
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      component_policy_service_) {
    return component_policy_service_->is_initialized();
  }
  return true;
}

bool CloudPolicyManager::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  return store()->first_policies_loaded();
}

void CloudPolicyManager::RefreshPolicies(PolicyFetchReason reason) {
  if (service()) {
    waiting_for_policy_refresh_ = true;
    service()->RefreshPolicy(
        base::BindOnce(&CloudPolicyManager::OnRefreshComplete,
                       base::Unretained(this)),
        reason);
  } else {
    OnRefreshComplete(false);
  }
}

void CloudPolicyManager::OnStoreLoaded(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnStoreError(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this
  // is only required on the first load, but doesn't hurt in any case.
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnComponentCloudPolicyUpdated() {
  CheckAndPublishPolicy();
}

void CloudPolicyManager::CheckAndPublishPolicy() {
  if (IsInitializationComplete(POLICY_DOMAIN_CHROME) &&
      !waiting_for_policy_refresh_) {
    PolicyBundle bundle;
    GetChromePolicy(
        &bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
    if (component_policy_service_)
      bundle.MergeFrom(component_policy_service_->policy());
    UpdatePolicy(std::move(bundle));
  }
}

void CloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  *policy_map = store()->policy_map().Clone();
}

void CloudPolicyManager::CreateComponentCloudPolicyService(
    const std::string& policy_type,
    const base::FilePath& policy_cache_path,
    CloudPolicyClient* client,
    SchemaRegistry* schema_registry) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Init() must have been called.
  CHECK(schema_registry);
  // Called at most once.
  CHECK(!component_policy_service_);
  // The core can't be connected yet.
  // See the comments on ComponentCloudPolicyService for the details.
  CHECK(!core()->client());

  if (policy_cache_path.empty())
    return;

  // TODO(emaxx, 729082): Make ComponentCloudPolicyStore (and other
  // implementation details of it) not use the blocking task runner whenever
  // possible because the real file operations are only done by ResourceCache,
  // and most of the rest doesn't need the blocking behaviour. Also
  // ComponentCloudPolicyService's |backend_task_runner| and |cache| must live
  // on the same task runner.
  const auto task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  std::unique_ptr<ResourceCache> resource_cache(new ResourceCache(
      policy_cache_path, task_runner, /* max_cache_size */ std::nullopt));
  component_policy_service_ = std::make_unique<ComponentCloudPolicyService>(
      policy_type, this, schema_registry, core(), client,
      std::move(resource_cache), task_runner);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

void CloudPolicyManager::ClearAndDestroyComponentCloudPolicyService() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (component_policy_service_) {
    component_policy_service_->ClearCache();
    component_policy_service_.reset();
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

void CloudPolicyManager::OnRefreshComplete(bool success) {
  waiting_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
