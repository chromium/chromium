// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_store.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

struct ComponentCloudPolicyStore::DomainConstants {
  const char* policy_type;
  const PolicyDomain domain;
  const PolicyScope scope;
  const char* proto_cache_key;
  const char* data_cache_key;
};

namespace {

const ComponentCloudPolicyStore::DomainConstants kDomains[] = {
    {
        dm_protocol::kChromeExtensionPolicyType,
        POLICY_DOMAIN_EXTENSIONS,
        POLICY_SCOPE_USER,
        "extension-policy",
        "extension-policy-data",
    },
    {
        dm_protocol::kChromeSigninExtensionPolicyType,
        POLICY_DOMAIN_SIGNIN_EXTENSIONS,
        POLICY_SCOPE_USER,
        "signinextension-policy",
        "signinextension-policy-data",
    },
    {
        dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
        POLICY_DOMAIN_EXTENSIONS,
        POLICY_SCOPE_MACHINE,
        "extension-policy",
        "extension-policy-data",
    },
};

const ComponentCloudPolicyStore::DomainConstants* GetDomainConstantsForType(
    const std::string& type) {
  for (const ComponentCloudPolicyStore::DomainConstants& constants : kDomains) {
    if (constants.policy_type == type)
      return &constants;
  }
  return nullptr;
}

}  // namespace

ComponentCloudPolicyStore::Delegate::~Delegate() = default;

ComponentCloudPolicyStore::ComponentCloudPolicyStore(
    Delegate* delegate,
    ResourceCache* cache,
    const std::string& policy_type)
    : delegate_(delegate),
      cache_(cache),
      domain_constants_(GetDomainConstantsForType(policy_type)) {
  // Allow the store to be created on a different thread than the thread that
  // will end up using it.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(domain_constants_);
}

ComponentCloudPolicyStore::~ComponentCloudPolicyStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool ComponentCloudPolicyStore::SupportsDomain(PolicyDomain domain) {
  for (const DomainConstants& constants : kDomains) {
    if (constants.domain == domain)
      return true;
  }
  return false;
}

// static
bool ComponentCloudPolicyStore::GetPolicyDomain(const std::string& policy_type,
                                                PolicyDomain* domain) {
  const DomainConstants* constants = GetDomainConstantsForType(policy_type);
  if (!constants)
    return false;
  *domain = constants->domain;
  return true;
}

const std::string& ComponentCloudPolicyStore::GetCachedHash(
    const PolicyNamespace& ns) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = cached_hashes_.find(ns);
  return it == cached_hashes_.end() ? base::EmptyString() : it->second;
}

void ComponentCloudPolicyStore::SetCredentials(const std::string& username,
                                               const std::string& gaia_id,
                                               const std::string& dm_token,
                                               const std::string& device_id,
                                               const std::string& public_key,
                                               int public_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(username_.empty() || username == username_);
  DCHECK(dm_token_.empty() || dm_token == dm_token_);
  DCHECK(device_id_.empty() || device_id == device_id_);
  username_ = username;
  gaia_id_ = gaia_id;
  dm_token_ = dm_token;
  device_id_ = device_id;
  public_key_ = public_key;
  public_key_version_ = public_key_version;
}

void ComponentCloudPolicyStore::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  typedef std::map<std::string, std::string> ContentMap;

  // Load cached policy protobuf for the assoicated domain.
  ContentMap protos;
  cache_->LoadAllSubkeys(domain_constants_->proto_cache_key, &protos);
  for (auto it = protos.begin(); it != protos.end(); ++it) {
    const std::string& id(it->first);
    const PolicyNamespace ns(domain_constants_->domain, id);

    // Validate the protobuf.
    auto proto = std::make_unique<em::PolicyFetchResponse>();
    if (!proto->ParseFromString(it->second)) {
      LOG_POLICY(ERROR, CBCM_ENROLLMENT)
          << "Failed to parse the cached policy fetch response.";
      Delete(ns);
      continue;
    }
    em::ExternalPolicyData payload;
    em::PolicyData policy_data;
    std::string policy_error;
    if (!ValidatePolicy(ns, std::move(proto), &policy_data, &payload,
                        &policy_error)) {
      // The policy fetch response is corrupted.
      LOG_POLICY(ERROR, CBCM_ENROLLMENT)
          << "Discarding policy for component " << ns.component_id
          << " due to policy validation failure: " << policy_error;
      Delete(ns);
      continue;
    }

    // The protobuf looks good; load the policy data.
    std::string data;
    if (cache_->Load(domain_constants_->data_cache_key, id, &data).empty()) {
      LOG_POLICY(ERROR, CBCM_ENROLLMENT)
          << "Failed to load the cached policy data.";
      Delete(ns);
      continue;
    }
    PolicyMap policy;
    std::string data_error;
    if (!ValidateData(data, payload.secure_hash(), &policy, &data_error)) {
      // The data for this proto is corrupted.
      LOG_POLICY(ERROR, POLICY_PROCESSING)
          << "Discarding policy for component " << ns.component_id
          << " due to data validation failure: " << data_error;
      Delete(ns);
      continue;
    }

    // The data is also good; expose the policies.
    policy_bundle_.Get(ns).Swap(&policy);
    cached_hashes_[ns] = payload.secure_hash();
    stored_policy_times_[ns] =
        base::Time::FromMillisecondsSinceUnixEpoch(policy_data.timestamp());
  }
  delegate_->OnComponentCloudPolicyStoreUpdated();
}

bool ComponentCloudPolicyStore::Store(const PolicyNamespace& ns,
                                      const std::string& serialized_policy,
                                      const em::PolicyData* policy_data,
                                      const std::string& secure_hash,
                                      const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (domain_constants_->domain != ns.domain) {
    return false;
  }

  // |serialized_policy| has already been validated; validate the data now.
  PolicyMap policy;
  std::string error;
  if (!ValidateData(data, secure_hash, &policy, &error)) {
    LOG_POLICY(ERROR, CBCM_ENROLLMENT)
        << "Discarding policy for component " << ns.component_id
        << " due to data validation failure: " << error;
    return false;
  }

  // Flush the proto and the data to the cache.
  cache_->Store(domain_constants_->proto_cache_key, ns.component_id,
                serialized_policy);
  cache_->Store(domain_constants_->data_cache_key, ns.component_id, data);
  // And expose the policy.
  policy_bundle_.Get(ns).Swap(&policy);
  cached_hashes_[ns] = secure_hash;
  stored_policy_times_[ns] =
      base::Time::FromMillisecondsSinceUnixEpoch(policy_data->timestamp());
  delegate_->OnComponentCloudPolicyStoreUpdated();
  return true;
}

void ComponentCloudPolicyStore::Delete(const PolicyNamespace& ns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (domain_constants_->domain != ns.domain) {
    return;
  }

  cache_->Delete(domain_constants_->proto_cache_key, ns.component_id);
  cache_->Delete(domain_constants_->data_cache_key, ns.component_id);

  if (!policy_bundle_.Get(ns).empty()) {
    policy_bundle_.Get(ns).Clear();
    delegate_->OnComponentCloudPolicyStoreUpdated();
  }
}

void ComponentCloudPolicyStore::Purge(const PurgeFilter& filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto subkey_filter = base::BindRepeating(filter, domain_constants_->domain);
  cache_->FilterSubkeys(domain_constants_->proto_cache_key, subkey_filter);
  cache_->FilterSubkeys(domain_constants_->data_cache_key, subkey_filter);

  // Stop serving policies for purged namespaces.
  bool purged_current_policies = false;
  for (PolicyBundle::const_iterator it = policy_bundle_.begin();
       it != policy_bundle_.end(); ++it) {
    DCHECK_EQ(it->first.domain, domain_constants_->domain);
    if (filter.Run(domain_constants_->domain, it->first.component_id) &&
        !policy_bundle_.Get(it->first).empty()) {
      policy_bundle_.Get(it->first).Clear();
      purged_current_policies = true;
    }
  }

  // Purge cached hashes, so that those namespaces can be fetched again if the
  // policy state changes.
  auto it = cached_hashes_.begin();
  while (it != cached_hashes_.end()) {
    const PolicyNamespace ns(it->first);
    DCHECK_EQ(ns.domain, domain_constants_->domain);
    if (filter.Run(domain_constants_->domain, ns.component_id)) {
      auto prev = it;
      ++it;
      cached_hashes_.erase(prev);
      DCHECK(stored_policy_times_.count(ns));
      stored_policy_times_.erase(ns);
    } else {
      ++it;
    }
  }

  if (purged_current_policies) {
    delegate_->OnComponentCloudPolicyStoreUpdated();
  }
}

void ComponentCloudPolicyStore::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_->Clear(domain_constants_->proto_cache_key);
  cache_->Clear(domain_constants_->data_cache_key);

  cached_hashes_.clear();
  stored_policy_times_.clear();
  const PolicyBundle empty_bundle;
  if (!policy_bundle_.Equals(empty_bundle)) {
    policy_bundle_.Clear();
    delegate_->OnComponentCloudPolicyStoreUpdated();
  }
}

bool ComponentCloudPolicyStore::ValidatePolicy(
    const PolicyNamespace& ns,
    std::unique_ptr<em::PolicyFetchResponse> proto,
    em::PolicyData* policy_data,
    em::ExternalPolicyData* payload,
    std::string* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (domain_constants_->domain != ns.domain) {
    *error = "Domains do not match.";
    return false;
  }

  if (ns.component_id.empty()) {
    *error = "Empty component id.";
    return false;
  }

  if (username_.empty() || dm_token_.empty() || device_id_.empty() ||
      public_key_.empty() || public_key_version_ == -1) {
    *error = "Credentials are not loaded yet.";
    return false;
  }

  // A valid policy should be not older than the currently stored policy, which
  // allows to prevent the rollback of the policy.
  base::Time time_not_before;
  const auto stored_policy_times_iter = stored_policy_times_.find(ns);
  if (stored_policy_times_iter != stored_policy_times_.end())
    time_not_before = stored_policy_times_iter->second;

  auto validator = std::make_unique<ComponentCloudPolicyValidator>(
      std::move(proto), scoped_refptr<base::SequencedTaskRunner>());
  validator->ValidateTimestamp(time_not_before,
                               CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);
  validator->ValidateUsernameAndGaiaId(username_, gaia_id_);
  validator->ValidateDMToken(dm_token_,
                             ComponentCloudPolicyValidator::DM_TOKEN_REQUIRED);
  validator->ValidateDeviceId(device_id_,
                              CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePolicyType(domain_constants_->policy_type);
  validator->ValidateSettingsEntityId(ns.component_id);
  validator->ValidatePayload();
  validator->ValidateSignature(public_key_);
  validator->RunValidation();
  if (!validator->success()) {
    *error = base::StrCat(
        {"Unsuccessful validation with Status ",
         CloudPolicyValidatorBase::StatusToString(validator->status()), "."});
    return false;
  }

  if (!validator->policy_data()->has_public_key_version()) {
    *error = "Public key version missing.";
    return false;
  }
  if (validator->policy_data()->public_key_version() != public_key_version_) {
    *error = base::StrCat(
        {"Wrong public key version ",
         base::NumberToString(validator->policy_data()->public_key_version()),
         " - expected ", base::NumberToString(public_key_version_), "."});
    return false;
  }

  em::ExternalPolicyData* data = validator->payload().get();
  // The download URL must be empty, or must be a valid URL.
  // An empty download URL signals that this component doesn't have cloud
  // policy, or that the policy has been removed.
  if (data->has_download_url() && !data->download_url().empty()) {
    if (!GURL(data->download_url()).is_valid()) {
      *error = base::StrCat({"Invalid URL: ", data->download_url(), " ."});
      return false;
    }
    if (!data->has_secure_hash() || data->secure_hash().empty()) {
      *error = "Secure hash missing.";
      return false;
    }
  } else if (data->has_secure_hash()) {
    *error = "URL missing.";
    return false;
  }

  if (policy_data)
    policy_data->Swap(validator->policy_data().get());
  if (payload)
    payload->Swap(validator->payload().get());
  return true;
}

bool ComponentCloudPolicyStore::ValidateData(const std::string& data,
                                             const std::string& secure_hash,
                                             PolicyMap* policy,
                                             std::string* error) {
  if (crypto::SHA256HashString(data) != secure_hash) {
    *error = "The received data doesn't match the expected hash.";
    return false;
  }
  return ParsePolicy(data, policy, error);
}

bool ComponentCloudPolicyStore::ParsePolicy(const std::string& data,
                                            PolicyMap* policy,
                                            std::string* error) {
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    *error = "Invalid JSON blob: " + value_with_error.error().message;
    return false;
  }
  base::Value json = std::move(*value_with_error);
  if (!json.is_dict()) {
    *error = "The JSON blob is not a dictionary.";
    return false;
  }

  return ParseComponentPolicy(std::move(json).TakeDict(),
                              domain_constants_->scope, POLICY_SOURCE_CLOUD,
                              policy, error);
}

}  // namespace policy
