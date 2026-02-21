// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_service.h"
#include "device_management_backend.pb.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/core/common/cloud/resource_cache.h"
#endif

namespace policy {

namespace {

const enterprise_management::PolicyData* GetPolicyData(
    const CloudPolicyManager* manager) {
  CHECK(manager);
  const policy::CloudPolicyStore* store = manager->core()->store();
  return store && store->has_policy() ? store->policy() : nullptr;
}

std::string PolicyTypeToExtensionInstallPolicyType(
    const std::string& policy_type) {
  if (policy_type == dm_protocol::kChromeMachineLevelUserCloudPolicyType) {
    return dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType;
  }
  if (policy_type == dm_protocol::GetChromeUserPolicyType()) {
    return dm_protocol::kChromeExtensionInstallUserCloudPolicyType;
  }
  NOTREACHED() << "Unsupported policy type: " << policy_type;
}
}  // namespace

BASE_FEATURE(kPublishPolicyWithoutWaiting, base::FEATURE_ENABLED_BY_DEFAULT);

CloudPolicyManager::CloudPolicyManager(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    std::unique_ptr<CloudPolicyStore> cloud_policy_store,
    std::unique_ptr<CloudPolicyStore> extension_install_store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : store_(std::move(cloud_policy_store)),
      extension_install_store_(std::move(extension_install_store)),
      core_(policy_type,
            settings_entity_id,
            store_.get(),
            task_runner,
            std::move(network_connection_tracker_getter)) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(!extension_install_store_.get());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

CloudPolicyManager::~CloudPolicyManager() = default;

std::optional<policy::DMToken> CloudPolicyManager::GetDMToken() const {
  const auto* data = GetPolicyData(this);
  if (!data || !data->has_request_token()) {
    return std::nullopt;
  }
  return policy::DMToken::CreateValidToken(data->request_token());
}

std::optional<std::string> CloudPolicyManager::GetClientId() const {
  const auto* data = GetPolicyData(this);
  if (!data || !data->has_device_id()) {
    return std::nullopt;
  }
  return data->device_id();
}

bool CloudPolicyManager::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void CloudPolicyManager::InitExtensionInstallPolicies(
    PrefService* local_state,
    std::unique_ptr<CloudPolicyClient> client,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter) {
  CHECK(IsClientRegistered());
  CHECK(extension_install_store_);
  // Extension install core should only be initialized once.
  if (extension_install_core_) {
    return;
  }

  extension_install_store_observation_.Observe(extension_install_store());
  if (extension_install_store()->is_initialized()) {
    OnStoreLoaded(extension_install_store());
  } else {
    extension_install_store()->Load();
  }

  extension_install_core_ = std::make_unique<CloudPolicyCore>(
      PolicyTypeToExtensionInstallPolicyType(core_.policy_type()),
      core_.settings_entity_id(), extension_install_store(),
      core_.GetTaskRunner(), std::move(network_connection_tracker_getter));

  bool has_service = client->service() != nullptr;
  extension_install_core()->Connect(std::move(client));

  // In tests the client might not always have a real device management service.
  if (has_service) {
    extension_install_core()->StartRefreshScheduler();
    extension_install_core()->TrackRefreshDelayPref(
        local_state, policy_prefs::kUserPolicyRefreshRate);
  }
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
  if (extension_install_core_) {
    extension_install_core_->Disconnect();
  }
  extension_install_store_observation_.Reset();
  ConfigurationPolicyProvider::Shutdown();
}

bool CloudPolicyManager::IsInitializationComplete(PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store()->is_initialized();
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      component_policy_service_) {
    return component_policy_service_->is_initialized();
  }
  if (domain == POLICY_DOMAIN_EXTENSION_INSTALL) {
    return !extension_install_core() ||
           extension_install_store()->is_initialized();
  }
  return true;
}

bool CloudPolicyManager::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_EXTENSION_INSTALL) {
    return !extension_install_store() ||
           extension_install_store()->first_policies_loaded();
  }
  return store()->first_policies_loaded();
}

void CloudPolicyManager::RefreshPolicies(PolicyFetchReason reason) {
  std::array<CloudPolicyService*, 2> services = {service(),
                                                 extension_install_service()};
  waiting_for_policy_refresh_count_ =
      std::ranges::count_if(services, std::identity{});
  if (waiting_for_policy_refresh_count_ == 0) {
    OnRefreshComplete(false);
    return;
  }
  if (service()) {
    service()->RefreshPolicy(
        base::BindOnce(&CloudPolicyManager::OnRefreshComplete,
                       base::Unretained(this)),
        reason);
  }
  if (extension_install_service()) {
    extension_install_service()->RefreshPolicy(
        base::BindOnce(&CloudPolicyManager::OnRefreshComplete,
                       base::Unretained(this)),
        reason);
  }
}

void CloudPolicyManager::OnStoreLoaded(CloudPolicyStore* cloud_policy_store) {
  CHECK(cloud_policy_store == store() ||
        cloud_policy_store == extension_install_store());
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnStoreError(CloudPolicyStore* cloud_policy_store) {
  CHECK(cloud_policy_store == store() ||
        cloud_policy_store == extension_install_store());
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this
  // is only required on the first load, but doesn't hurt in any case.
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnComponentCloudPolicyUpdated() {
  CheckAndPublishPolicy();
}

bool CloudPolicyManager::CanPublishPolicy() const {
  if (!IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!IsInitializationComplete(POLICY_DOMAIN_EXTENSION_INSTALL)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  if (waiting_for_policy_refresh_count_ == 0) {
    return true;
  }

  // Component policy service initializaion is async. Its first publish might be
  // blocked by first cloud policy refresh.
  //
  // Skip the `waiting_for_policy_refresh_count_` check if component policies
  // are ready but never published.
  if (base::FeatureList::IsEnabled(kPublishPolicyWithoutWaiting) &&
      component_policy_service_ &&
      component_policy_service_->is_initialized() &&
      !is_component_policy_published_) {
    return true;
  }

  return false;
}

void CloudPolicyManager::CheckAndPublishPolicy() {
  if (!CanPublishPolicy()) {
    return;
  }
  PolicyBundle bundle;
  GetChromePolicy(
      &bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  GetExtensionInstallPolicy(&bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_EXTENSION_INSTALL, std::string())));
  if (component_policy_service_ &&
      component_policy_service_->is_initialized()) {
    bundle.MergeFrom(component_policy_service_->policy());
    is_component_policy_published_ = true;
  }
  UpdatePolicy(std::move(bundle));
}

void CloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  *policy_map = store()->policy_map().Clone();
}

void CloudPolicyManager::GetExtensionInstallPolicy(PolicyMap* policy_map) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  *policy_map = extension_install_store()
                    ? extension_install_store()->policy_map().Clone()
                    : PolicyMap();
#else
  *policy_map = PolicyMap();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
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
  if (waiting_for_policy_refresh_count_ > 0) {
    waiting_for_policy_refresh_count_--;
  }
  CheckAndPublishPolicy();
}

}  // namespace policy
