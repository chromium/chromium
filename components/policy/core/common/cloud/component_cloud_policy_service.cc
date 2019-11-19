// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_service.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_updater.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

using ScopedResponseMap =
    std::unordered_map<policy::PolicyNamespace,
                       std::unique_ptr<em::PolicyFetchResponse>,
                       policy::PolicyNamespaceHash>;

namespace policy {

namespace {

bool NotInResponseMap(const ScopedResponseMap& map,
                      PolicyDomain domain,
                      const std::string& component_id) {
  return map.find(PolicyNamespace(domain, component_id)) == map.end();
}

bool ToPolicyNamespace(const std::pair<std::string, std::string>& key,
                       PolicyNamespace* ns) {
  if (!ComponentCloudPolicyStore::GetPolicyDomain(key.first, &ns->domain))
    return false;
  ns->component_id = key.second;
  return true;
}

}  // namespace

ComponentCloudPolicyService::Delegate::~Delegate() {}

// Owns the objects that live on the background thread, and posts back to the
// thread that the ComponentCloudPolicyService runs on whenever the policy
// changes.
class ComponentCloudPolicyService::Backend
    : public ComponentCloudPolicyStore::Delegate {
 public:
  // This class can be instantiated on any thread but from then on, may be
  // accessed via the |task_runner_| only. Policy changes are posted to the
  // |service| via the |service_task_runner|. The |cache| is used to load and
  // store local copies of the downloaded policies.
  Backend(
      base::WeakPtr<ComponentCloudPolicyService> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> service_task_runner,
      std::unique_ptr<ResourceCache> cache,
      std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
      const std::string& policy_type,
      PolicySource policy_source);

  ~Backend() override;

  // Deletes all cached component policies from the store.
  void ClearCache();

  // The passed credentials will be used to validate the policies.
  void SetCredentials(const std::string& username,
                      const std::string& gaia_id,
                      const std::string& dm_token,
                      const std::string& device_id,
                      const std::string& public_key,
                      int public_key_version);

  // When called for the first time, loads the |store_| and exposes the loaded
  // cached policies.
  void InitIfNeeded();

  // Passes a map with all the PolicyFetchResponses for components currently
  // set at the server. Any components without an entry in |responses|
  // will have their cache purged after this call.
  // Otherwise the backend will start the validation and eventual download of
  // the policy data for each PolicyFetchResponse in |responses|.
  void SetFetchedPolicy(std::unique_ptr<ScopedResponseMap> responses);

  // ComponentCloudPolicyStore::Delegate implementation:
  void OnComponentCloudPolicyStoreUpdated() override;

 private:
  // Triggers an update of the policies from the last policy fetch response
  // stored in |last_fetched_policies_|.
  void UpdateWithLastFetchedPolicy();

  // The ComponentCloudPolicyService that owns |this|. Used to inform the
  // |service_| when policy changes.
  base::WeakPtr<ComponentCloudPolicyService> service_;

  // The thread that |this| runs on. Used to post tasks to be run by |this|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The thread that the |service_| runs on. Used to post policy changes to the
  // right thread.
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  std::unique_ptr<ResourceCache> cache_;
  std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher_;
  ComponentCloudPolicyStore store_;
  std::unique_ptr<ComponentCloudPolicyUpdater> updater_;
  bool initialized_ = false;
  bool has_credentials_set_ = false;
  std::unique_ptr<ScopedResponseMap> last_fetched_policy_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

ComponentCloudPolicyService::Backend::Backend(
    base::WeakPtr<ComponentCloudPolicyService> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> service_task_runner,
    std::unique_ptr<ResourceCache> cache,
    std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
    const std::string& policy_type,
    PolicySource policy_source)
    : service_(service),
      task_runner_(task_runner),
      service_task_runner_(service_task_runner),
      cache_(std::move(cache)),
      external_policy_data_fetcher_(std::move(external_policy_data_fetcher)),
      store_(this, cache_.get(), policy_type, policy_source) {
  // This class is allowed to be instantiated on any thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ComponentCloudPolicyService::Backend::~Backend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ComponentCloudPolicyService::Backend::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Clearing cache";
  store_.Clear();
  has_credentials_set_ = false;
}

void ComponentCloudPolicyService::Backend::SetCredentials(
    const std::string& username,
    const std::string& gaia_id,
    const std::string& dm_token,
    const std::string& device_id,
    const std::string& public_key,
    int public_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!username.empty());
  DCHECK(!dm_token.empty());
  DVLOG(1) << "Updating credentials: username = " << username
           << ", public_key_version = " << public_key_version;
  store_.SetCredentials(username, gaia_id, dm_token, device_id, public_key,
                        public_key_version);
  has_credentials_set_ = true;
  // Trigger an additional update against the last fetched policies. This helps
  // to deal with transient validation errors during signing key rotation, if
  // the component cloud policy validation begins before the superior policy is
  // validated and stored.
  UpdateWithLastFetchedPolicy();
}

void ComponentCloudPolicyService::Backend::InitIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialized_)
    return;

  DVLOG(2) << "Initializing backend";

  // Load the cached policy. Note that this does not trigger notifications
  // through OnComponentCloudPolicyStoreUpdated. Note also that the cached
  // data may contain names or values that don't match the schema for that
  // component; the data must be cached without modifications so that its
  // integrity can be verified using the hash, but it must also be filtered
  // right after a Load().
  store_.Load();

  // Start downloading any pending data.
  updater_.reset(new ComponentCloudPolicyUpdater(
      task_runner_, std::move(external_policy_data_fetcher_), &store_));

  std::unique_ptr<PolicyBundle> bundle(std::make_unique<PolicyBundle>());
  bundle->CopyFrom(store_.policy());
  service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentCloudPolicyService::SetPolicy,
                                service_, std::move(bundle)));

  initialized_ = true;

  // Start processing the fetched policy, when there is some already.
  UpdateWithLastFetchedPolicy();
}

void ComponentCloudPolicyService::Backend::SetFetchedPolicy(
    std::unique_ptr<ScopedResponseMap> responses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Updating last fetched policies (count = " << responses->size()
           << ")";
  last_fetched_policy_ = std::move(responses);
  UpdateWithLastFetchedPolicy();
}

void ComponentCloudPolicyService::Backend::
    OnComponentCloudPolicyStoreUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!initialized_) {
    // Ignore notifications triggered by the initial Purge or Clear.
    return;
  }
  DVLOG(2) << "Installing updated policy from the component policy store";

  std::unique_ptr<PolicyBundle> bundle(std::make_unique<PolicyBundle>());
  bundle->CopyFrom(store_.policy());
  service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentCloudPolicyService::SetPolicy,
                                service_, std::move(bundle)));
}

void ComponentCloudPolicyService::Backend::UpdateWithLastFetchedPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!has_credentials_set_ || !last_fetched_policy_ || !initialized_)
    return;

  DVLOG(1) << "Processing the last fetched policies (count = "
           << last_fetched_policy_->size() << ")";

  // Purge any components that don't have a policy configured at the server.
  // TODO(emaxx): This is insecure, as it happens before the policy validation:
  // see crbug.com/668733.
  store_.Purge(
      base::BindRepeating(&NotInResponseMap, std::cref(*last_fetched_policy_)));

  for (auto it = last_fetched_policy_->begin();
       it != last_fetched_policy_->end(); ++it) {
    updater_->UpdateExternalPolicy(
        it->first, std::make_unique<em::PolicyFetchResponse>(*it->second));
  }
}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    const std::string& policy_type,
    PolicySource policy_source,
    Delegate* delegate,
    SchemaRegistry* schema_registry,
    CloudPolicyCore* core,
    CloudPolicyClient* client,
    std::unique_ptr<ResourceCache> cache,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : policy_type_(policy_type),
      delegate_(delegate),
      schema_registry_(schema_registry),
      core_(core),
      backend_task_runner_(backend_task_runner) {
  DCHECK(policy_type == dm_protocol::kChromeExtensionPolicyType ||
         policy_type ==
             dm_protocol::kChromeMachineLevelExtensionCloudPolicyType ||
         policy_type == dm_protocol::kChromeSigninExtensionPolicyType);
  CHECK(!core_->client());

  backend_.reset(
      new Backend(weak_ptr_factory_.GetWeakPtr(), backend_task_runner_,
                  base::ThreadTaskRunnerHandle::Get(), std::move(cache),
                  std::make_unique<ExternalPolicyDataFetcher>(
                      client->GetURLLoaderFactory(), backend_task_runner_),
                  policy_type, policy_source));

  // Observe the schema registry for keeping |current_schema_map_| up to date.
  schema_registry_->AddObserver(this);
  UpdateFromSchemaRegistry();

  // Observe the superior store load, so that the backend can get the cached
  // credentials to validate the cached policies.
  core_->store()->AddObserver(this);
  if (core_->store()->is_initialized())
    UpdateFromSuperiorStore();

  core_->AddObserver(this);
  client->AddObserver(this);

  // Register the supported policy domain for being downloaded in future policy
  // fetches.
  client->AddPolicyTypeToFetch(policy_type_,
                               std::string() /* settings_entity_id */);
}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  schema_registry_->RemoveObserver(this);
  core_->store()->RemoveObserver(this);
  core_->RemoveObserver(this);
  if (core_->client())
    Disconnect();

  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return ComponentCloudPolicyStore::SupportsDomain(domain);
}

void ComponentCloudPolicyService::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::ClearCache, base::Unretained(backend_.get())));
}

void ComponentCloudPolicyService::OnSchemaRegistryReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateFromSchemaRegistry();
}

void ComponentCloudPolicyService::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateFromSchemaRegistry();
}

void ComponentCloudPolicyService::OnCoreConnected(CloudPolicyCore* core) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(core_, core);
  // Immediately update with any PolicyFetchResponses that the client may
  // already have.
  UpdateFromClient();
}

void ComponentCloudPolicyService::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(core_, core);
  Disconnect();
}

void ComponentCloudPolicyService::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(core_->store(), store);
  UpdateFromSuperiorStore();
}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(core_->store(), store);
  UpdateFromSuperiorStore();
}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(core_->client(), client);
  UpdateFromClient();
}

void ComponentCloudPolicyService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored; the registration state is tracked by looking at the
  // CloudPolicyStore instead.
}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
}

void ComponentCloudPolicyService::UpdateFromSuperiorStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(2) << "Obtaining credentials from the superior policy store";

  const em::PolicyData* policy = core_->store()->policy();
  if (!policy || !policy->has_username() || !policy->has_request_token()) {
    // Clear the cache in case there is no policy or there are no credentials -
    // e.g. when the user signs out.
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Backend::ClearCache, base::Unretained(backend_.get())));
  } else {
    // Send the current credentials to the backend; do this whenever the store
    // updates, to handle the case of the user registering for policy after the
    // session starts.
    std::string username = policy->username();
    std::string gaia_id = policy->gaia_id();
    std::string request_token = policy->request_token();
    std::string device_id =
        policy->has_device_id() ? policy->device_id() : std::string();
    std::string public_key = core_->store()->policy_signature_public_key();
    int public_key_version =
        policy->has_public_key_version() ? policy->public_key_version() : -1;
    backend_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Backend::SetCredentials,
                                  base::Unretained(backend_.get()), username,
                                  gaia_id, request_token, device_id, public_key,
                                  public_key_version));
  }

  // Initialize the backend to load the initial policy if not done yet,
  // regardless of the signin state.
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::InitIfNeeded, base::Unretained(backend_.get())));
}

void ComponentCloudPolicyService::UpdateFromClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (core_->client()->responses().empty()) {
    // The client's responses will be empty if it hasn't fetched policy from the
    // DMServer yet. Make sure we don't purge the caches in this case.
    return;
  }

  DVLOG(2) << "Obtaining fetched policies from the policy client";

  std::unique_ptr<ScopedResponseMap> valid_responses =
      std::make_unique<ScopedResponseMap>();
  for (const auto& response : core_->client()->responses()) {
    PolicyNamespace ns;
    if (!ToPolicyNamespace(response.first, &ns)) {
      DVLOG(1) << "Ignored policy with type = " << response.first.first;
      continue;
    }
    (*valid_responses)[ns] =
        std::make_unique<em::PolicyFetchResponse>(*response.second);
  }

  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::SetFetchedPolicy,
                                base::Unretained(backend_.get()),
                                std::move(valid_responses)));
}

void ComponentCloudPolicyService::UpdateFromSchemaRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!schema_registry_->IsReady()) {
    // Ignore notifications from the registry which is not ready yet.
    return;
  }
  DVLOG(2) << "Updating schema map";
  current_schema_map_ = schema_registry_->schema_map();
  FilterAndInstallPolicy();
}

void ComponentCloudPolicyService::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->client()->RemoveObserver(this);

  // Unregister the policy domain from being downloaded in the future policy
  // fetches.
  core_->client()->RemovePolicyTypeToFetch(
      policy_type_, std::string() /* settings_entity_id */);
}

void ComponentCloudPolicyService::SetPolicy(
    std::unique_ptr<PolicyBundle> policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Store the current unfiltered policies.
  unfiltered_policy_ = std::move(policy);

  FilterAndInstallPolicy();
}

void ComponentCloudPolicyService::FilterAndInstallPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!unfiltered_policy_ || !current_schema_map_)
    return;

  // Make a copy in |policy_| and filter it and validate against the schemas;
  // this is what's passed to the outside world.
  policy_.CopyFrom(*unfiltered_policy_);
  current_schema_map_->FilterBundle(&policy_);

  policy_installed_ = true;
  DVLOG(1) << "Installed policy (count = "
           << std::distance(policy_.begin(), policy_.end()) << ")";
  delegate_->OnComponentCloudPolicyUpdated();
}

}  // namespace policy
