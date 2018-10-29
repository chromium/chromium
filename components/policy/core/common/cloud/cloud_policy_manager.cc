// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_service.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/policy/core/common/cloud/resource_cache.h"
#endif

namespace policy {

CloudPolicyManager::CloudPolicyManager(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    CloudPolicyStore* cloud_policy_store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : core_(policy_type,
            settings_entity_id,
            cloud_policy_store,
            task_runner,
            network_connection_tracker_getter),
      waiting_for_policy_refresh_(false) {}

CloudPolicyManager::~CloudPolicyManager() {}

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

void CloudPolicyManager::RefreshPolicies() {
  if (service()) {
    waiting_for_policy_refresh_ = true;
    service()->RefreshPolicy(
        base::Bind(&CloudPolicyManager::OnRefreshComplete,
                   base::Unretained(this)));
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
    std::unique_ptr<PolicyBundle> bundle(new PolicyBundle);
    GetChromePolicy(
        &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
    if (component_policy_service_)
      bundle->MergeFrom(component_policy_service_->policy());
    UpdatePolicy(std::move(bundle));
  }
}

void CloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  policy_map->CopyFrom(store()->policy_map());
}

void CloudPolicyManager::CreateComponentCloudPolicyService(
    const std::string& policy_type,
    const base::FilePath& policy_cache_path,
    CloudPolicyClient* client,
    SchemaRegistry* schema_registry) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Init() must have been called.
  CHECK(schema_registry);
  // Called at most once.
  CHECK(!component_policy_service_);
  // The core can't be connected yet.
  // See the comments on ComponentCloudPolicyService for the details.
  CHECK(!core()->client());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableComponentCloudPolicy) ||
      policy_cache_path.empty()) {
    return;
  }

  // TODO(emaxx, 729082): Make ComponentCloudPolicyStore (and other
  // implementation details of it) not use the blocking task runner whenever
  // possible because the real file operations are only done by ResourceCache,
  // and most of the rest doesn't need the blocking behaviour. Also
  // ComponentCloudPolicyService's |backend_task_runner| and |cache| must live
  // on the same task runner.
  const auto task_runner =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  std::unique_ptr<ResourceCache> resource_cache(
      new ResourceCache(policy_cache_path, task_runner));
  component_policy_service_.reset(new ComponentCloudPolicyService(
      policy_type, this, schema_registry, core(), client,
      std::move(resource_cache), task_runner));
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)
}

void CloudPolicyManager::ClearAndDestroyComponentCloudPolicyService() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (component_policy_service_) {
    component_policy_service_->ClearCache();
    component_policy_service_.reset();
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)
}

void CloudPolicyManager::OnRefreshComplete(bool success) {
  waiting_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
