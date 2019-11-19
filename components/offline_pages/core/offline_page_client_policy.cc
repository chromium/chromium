// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_client_policy.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_client_policy.h"

namespace offline_pages {

namespace {
struct PolicyData {
  std::map<std::string, OfflinePageClientPolicy> policies;
  std::vector<std::string> temporary_namespaces;
  std::vector<std::string> persistent_namespaces;
  std::vector<std::string> all_namespaces;
};

PolicyData BuildPolicies() {
  std::vector<OfflinePageClientPolicy> all_policies;
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kBookmarkNamespace, base::TimeDelta::FromDays(7));
    policy.pages_allowed_per_url = 1;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kLastNNamespace, base::TimeDelta::FromDays(30));
    policy.is_restricted_to_tab_from_client_id = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreatePersistent(kAsyncNamespace);
    policy.is_supported_by_download = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kCCTNamespace, base::TimeDelta::FromDays(2));
    policy.pages_allowed_per_url = 1;
    policy.requires_specific_user_settings = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreatePersistent(kDownloadNamespace);
    policy.is_supported_by_download = true;
    all_policies.push_back(policy);
  }
  {
    auto policy =
        OfflinePageClientPolicy::CreatePersistent(kNTPSuggestionsNamespace);
    policy.is_supported_by_download = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kSuggestedArticlesNamespace, base::TimeDelta::FromDays(30));
    policy.is_supported_by_download = 1;
    policy.is_suggested = true;
    all_policies.push_back(policy);
  }
  {
    auto policy =
        OfflinePageClientPolicy::CreatePersistent(kBrowserActionsNamespace);
    policy.is_supported_by_download = true;
    policy.allows_conversion_to_background_file_download = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kLivePageSharingNamespace, base::TimeDelta::FromHours(1));
    policy.pages_allowed_per_url = 1;
    policy.is_restricted_to_tab_from_client_id = true;
    all_policies.push_back(policy);
  }
  {
    auto policy = OfflinePageClientPolicy::CreateTemporary(
        kAutoAsyncNamespace, base::TimeDelta::FromDays(30));
    policy.pages_allowed_per_url = 1;
    policy.defer_background_fetch_while_page_is_active = true;
    all_policies.push_back(policy);
  }

  // Fallback policy.
  {
    OfflinePageClientPolicy policy = OfflinePageClientPolicy::CreateTemporary(
        kDefaultNamespace, base::TimeDelta::FromDays(1));
    policy.page_limit = 10;
    policy.pages_allowed_per_url = 1;
    all_policies.push_back(policy);
  }

  PolicyData policy_data;
  for (const auto& policy : all_policies) {
    policy_data.all_namespaces.push_back(policy.name_space);
    switch (policy.lifetime_type) {
      case LifetimeType::TEMPORARY:
        policy_data.temporary_namespaces.push_back(policy.name_space);
        break;
      case LifetimeType::PERSISTENT:
        policy_data.persistent_namespaces.push_back(policy.name_space);
        break;
    }
    policy_data.policies.emplace(policy.name_space, policy);
  }

  return policy_data;
}

const PolicyData& GetPolicyData() {
  static base::NoDestructor<PolicyData> instance(BuildPolicies());
  return *instance;
}

}  // namespace

OfflinePageClientPolicy::OfflinePageClientPolicy(std::string namespace_val,
                                                 LifetimeType lifetime_type_val)
    : name_space(namespace_val), lifetime_type(lifetime_type_val) {}

// static
OfflinePageClientPolicy OfflinePageClientPolicy::CreateTemporary(
    const std::string& name_space,
    const base::TimeDelta& expiration_period) {
  OfflinePageClientPolicy policy(name_space, LifetimeType::TEMPORARY);
  policy.expiration_period = expiration_period;
  return policy;
}

// static
OfflinePageClientPolicy OfflinePageClientPolicy::CreatePersistent(
    const std::string& name_space) {
  return {name_space, LifetimeType::PERSISTENT};
}

OfflinePageClientPolicy::OfflinePageClientPolicy(
    const OfflinePageClientPolicy& other) = default;

OfflinePageClientPolicy::~OfflinePageClientPolicy() = default;

const OfflinePageClientPolicy& GetPolicy(const std::string& name) {
  const std::map<std::string, OfflinePageClientPolicy>& policies =
      GetPolicyData().policies;
  const auto& iter = policies.find(name);
  if (iter != policies.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies.at(kDefaultNamespace);
}

const std::vector<std::string>& GetAllPolicyNamespaces() {
  return GetPolicyData().all_namespaces;
}
const std::vector<std::string>& GetTemporaryPolicyNamespaces() {
  return GetPolicyData().temporary_namespaces;
}
const std::vector<std::string>& GetPersistentPolicyNamespaces() {
  return GetPolicyData().persistent_namespaces;
}

}  // namespace offline_pages
