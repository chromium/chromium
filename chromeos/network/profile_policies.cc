// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/profile_policies.h"

#include <iterator>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chromeos/network/policy_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

bool DefaultShillPropertiesMatcher(const base::Value& onc_network_configuration,
                                   const base::Value& shill_properties) {
  return policy_util::IsPolicyMatching(onc_network_configuration,
                                       shill_properties);
}

}  // namespace

ProfilePolicies::NetworkPolicy::NetworkPolicy(base::Value onc_policy)
    : onc_policy_(std::move(onc_policy)) {}
ProfilePolicies::NetworkPolicy::~NetworkPolicy() = default;

ProfilePolicies::NetworkPolicy::NetworkPolicy(NetworkPolicy&& other) = default;
ProfilePolicies::NetworkPolicy& ProfilePolicies::NetworkPolicy::operator=(
    ProfilePolicies::NetworkPolicy&& other) = default;

bool ProfilePolicies::NetworkPolicy::UpdateFrom(const base::Value& new_policy) {
  if (new_policy == onc_policy_)
    return false;
  onc_policy_ = new_policy.Clone();
  return true;
}

const base::Value* ProfilePolicies::NetworkPolicy::GetPolicy() const {
  return &onc_policy_;
}

ProfilePolicies::ProfilePolicies()
    : shill_properties_matcher_(
          base::BindRepeating(&DefaultShillPropertiesMatcher)) {}
ProfilePolicies::~ProfilePolicies() = default;

base::flat_set<std::string> ProfilePolicies::ApplyOncNetworkConfigurationList(
    const base::Value& network_configs_onc) {
  DCHECK(network_configs_onc.is_list());
  base::flat_set<std::string> processed_guids;
  base::flat_set<std::string> new_or_modified_guids;
  base::flat_set<std::string> removed_guids = GetAllPolicyGuids();

  for (const base::Value& network : network_configs_onc.GetList()) {
    const std::string* guid_str =
        network.FindStringKey(::onc::network_config::kGUID);
    DCHECK(guid_str && !guid_str->empty());
    std::string guid = *guid_str;
    if (processed_guids.find(guid) != processed_guids.end()) {
      NET_LOG(ERROR) << "ONC Contains multiple entries for the same guid: "
                     << guid;
      continue;
    }
    processed_guids.insert(guid);

    NetworkPolicy* existing_policy = FindPolicy(guid);
    if (!existing_policy) {
      guid_to_policy_.insert(
          std::make_pair(guid, NetworkPolicy(network.Clone())));
      new_or_modified_guids.insert(guid);
      continue;
    }
    removed_guids.erase(guid);
    if (existing_policy->UpdateFrom(network)) {
      new_or_modified_guids.insert(guid);
    }
  }

  for (const std::string& removed_guid : removed_guids) {
    guid_to_policy_.erase(removed_guid);
  }

  return new_or_modified_guids;
}

void ProfilePolicies::SetGlobalNetworkConfig(
    const base::Value& global_network_config) {
  DCHECK(global_network_config.is_dict());
  global_network_config_ = global_network_config.Clone();
}

const base::Value* ProfilePolicies::GetPolicyByGuid(
    const std::string& guid) const {
  const NetworkPolicy* policy = FindPolicy(guid);
  return policy ? policy->GetPolicy() : nullptr;
}

bool ProfilePolicies::HasPolicyMatchingShillProperties(
    const base::Value& shill_properties) const {
  for (const auto& [guid, policy] : guid_to_policy_) {
    if (shill_properties_matcher_.Run(*(policy.GetPolicy()),
                                      shill_properties)) {
      return true;
    }
  }
  return false;
}

base::flat_map<std::string, base::Value> ProfilePolicies::GetGuidToPolicyMap()
    const {
  std::vector<std::pair<std::string, base::Value>> result;
  result.reserve(guid_to_policy_.size());
  for (const auto& [guid, policy] : guid_to_policy_) {
    result.push_back(std::make_pair(guid, policy.GetPolicy()->Clone()));
  }
  return base::flat_map<std::string, base::Value>(std::move(result));
}

void ProfilePolicies::SetShillPropertiesMatcherForTesting(
    const ShillPropertiesMatcher& shill_properties_matcher) {
  shill_properties_matcher_ = shill_properties_matcher;
}

base::flat_set<std::string> ProfilePolicies::GetAllPolicyGuids() const {
  std::vector<std::string> result;
  result.reserve(guid_to_policy_.size());
  for (const auto& [guid, _] : guid_to_policy_) {
    result.push_back(guid);
  }
  return base::flat_set<std::string>(result);
}

ProfilePolicies::NetworkPolicy* ProfilePolicies::FindPolicy(
    const std::string& guid) {
  auto iter = guid_to_policy_.find(guid);
  return iter != guid_to_policy_.end() ? &(iter->second) : nullptr;
}

const ProfilePolicies::NetworkPolicy* ProfilePolicies::FindPolicy(
    const std::string& guid) const {
  auto iter = guid_to_policy_.find(guid);
  return iter != guid_to_policy_.end() ? &(iter->second) : nullptr;
}

}  // namespace chromeos
