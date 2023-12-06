// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/profile_policies.h"

#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"

namespace ash {

namespace {

bool DefaultShillPropertiesMatcher(
    const base::Value::Dict& onc_network_configuration,
    const base::Value::Dict& shill_properties) {
  return policy_util::IsPolicyMatching(onc_network_configuration,
                                       shill_properties);
}

base::flat_map<std::string, std::string> GetAllExpansions(
    const base::flat_map<std::string, std::string>& profile_wide_expansions,
    const client_cert::ResolvedCert& resolved_cert) {
  base::flat_map<std::string, std::string> result;
  result.insert(profile_wide_expansions.begin(), profile_wide_expansions.end());
  if (resolved_cert.status() ==
      client_cert::ResolvedCert::Status::kCertMatched) {
    result.insert(resolved_cert.variable_expansions().begin(),
                  resolved_cert.variable_expansions().end());
  }
  return result;
}

base::Value::Dict DefaultRuntimeValuesSetter(
    const base::Value::Dict& onc_network_configuration,
    const base::flat_map<std::string, std::string>& profile_wide_expansions,
    const client_cert::ResolvedCert& resolved_cert) {
  // TODO(b/215163180): Change this to return a nullopt or the like instead of
  // cloning if the variable expansion doesn't change anything when this is the
  // only caller of ExpandStringsInOncObject.
  base::Value::Dict expanded = onc_network_configuration.Clone();
  chromeos::VariableExpander variable_expander(
      GetAllExpansions(profile_wide_expansions, resolved_cert));
  chromeos::onc::ExpandStringsInOncObject(
      chromeos::onc::kNetworkConfigurationSignature, variable_expander,
      &expanded);
  client_cert::SetResolvedCertInOnc(resolved_cert, expanded);
  return expanded;
}

}  // namespace

ProfilePolicies::NetworkPolicy::NetworkPolicy(const ProfilePolicies* parent,
                                              base::Value::Dict onc_policy)
    : parent_(parent), original_policy_(std::move(onc_policy)) {
  // There could already be profile-wide variable expansions (through parent_).
  ReapplyRuntimeValues();
}

ProfilePolicies::NetworkPolicy::~NetworkPolicy() = default;

ProfilePolicies::NetworkPolicy::NetworkPolicy(NetworkPolicy&& other) = default;
ProfilePolicies::NetworkPolicy& ProfilePolicies::NetworkPolicy::operator=(
    NetworkPolicy&& other) = default;

ProfilePolicies::ChangeEffect ProfilePolicies::NetworkPolicy::UpdateFrom(
    const base::Value::Dict& new_onc_policy) {
  if (new_onc_policy == original_policy_)
    return ChangeEffect::kNoChange;
  original_policy_ = new_onc_policy.Clone();
  ReapplyRuntimeValues();
  return ChangeEffect::kEffectivePolicyChanged;
}

ProfilePolicies::ProfilePolicies()
    : shill_properties_matcher_(
          base::BindRepeating(&DefaultShillPropertiesMatcher)),
      runtime_values_setter_(base::BindRepeating(&DefaultRuntimeValuesSetter)) {
}
ProfilePolicies::~ProfilePolicies() = default;

ProfilePolicies::ChangeEffect
ProfilePolicies::NetworkPolicy::SetResolvedClientCertificate(
    client_cert::ResolvedCert resolved_cert) {
  if (resolved_cert_ == resolved_cert)
    return ChangeEffect::kNoChange;
  resolved_cert_ = std::move(resolved_cert);
  return ReapplyRuntimeValues();
}

ProfilePolicies::ChangeEffect
ProfilePolicies::NetworkPolicy::OnProfileWideExpansionsChanged() {
  return ReapplyRuntimeValues();
}

const base::Value::Dict& ProfilePolicies::NetworkPolicy::GetOriginalPolicy()
    const {
  return original_policy_;
}

const base::Value::Dict&
ProfilePolicies::NetworkPolicy::GetPolicyWithRuntimeValues() const {
  if (!policy_with_runtime_values_.has_value()) {
    // Memory optimization to avoid storing the same value twice if setting
    // runtime values resulted in no change.
    return original_policy_;
  }
  return policy_with_runtime_values_.value();
}

ProfilePolicies::ChangeEffect
ProfilePolicies::NetworkPolicy::ReapplyRuntimeValues() {
  std::optional<base::Value::Dict> old_policy_with_runtime_values =
      std::move(policy_with_runtime_values_);

  policy_with_runtime_values_ = parent_->runtime_values_setter_.Run(
      original_policy_, parent_->profile_wide_expansions_, resolved_cert_);
  if (policy_with_runtime_values_ == original_policy_) {
    // Memory optimization to avoid storing the same value twice if variable
    // expansion had no effect.
    policy_with_runtime_values_ = {};
  }

  return old_policy_with_runtime_values == policy_with_runtime_values_
             ? ChangeEffect::kNoChange
             : ChangeEffect::kEffectivePolicyChanged;
}

base::flat_set<std::string> ProfilePolicies::ApplyOncNetworkConfigurationList(
    const base::Value::List& network_configs_onc) {
  base::flat_set<std::string> processed_guids;
  base::flat_set<std::string> new_or_modified_guids;
  base::flat_set<std::string> removed_guids = GetAllPolicyGuids();

  for (const base::Value& network_value : network_configs_onc) {
    const base::Value::Dict& network = network_value.GetDict();

    const std::string* guid_str =
        network.FindString(::onc::network_config::kGUID);
    DCHECK(guid_str && !guid_str->empty());
    std::string guid = *guid_str;
    if (base::Contains(processed_guids, guid)) {
      NET_LOG(ERROR) << "ONC Contains multiple entries for the same guid: "
                     << guid;
      continue;
    }
    processed_guids.insert(guid);

    NetworkPolicy* existing_policy = FindPolicy(guid);
    if (!existing_policy) {
      guid_to_policy_.insert(
          std::make_pair(guid, NetworkPolicy(this, network.Clone())));
      new_or_modified_guids.insert(guid);
      continue;
    }
    removed_guids.erase(guid);
    if (existing_policy->UpdateFrom(network) ==
        ChangeEffect::kEffectivePolicyChanged) {
      new_or_modified_guids.insert(guid);
    }
  }

  for (const std::string& removed_guid : removed_guids) {
    guid_to_policy_.erase(removed_guid);
  }

  return new_or_modified_guids;
}

void ProfilePolicies::SetGlobalNetworkConfig(
    const base::Value::Dict& global_network_config) {
  global_network_config_ = global_network_config.Clone();
}

base::flat_set<std::string> ProfilePolicies::SetProfileWideExpansions(
    base::flat_map<std::string, std::string> expansions) {
  if (profile_wide_expansions_ == expansions)
    return {};
  profile_wide_expansions_ = std::move(expansions);
  base::flat_set<std::string> modified_guids;
  for (auto& pair : guid_to_policy_) {
    if (pair.second.OnProfileWideExpansionsChanged() ==
        ChangeEffect::kEffectivePolicyChanged) {
      modified_guids.insert(pair.first);
    }
  }
  return modified_guids;
}

bool ProfilePolicies::SetResolvedClientCertificate(
    const std::string& guid,
    client_cert::ResolvedCert resolved_cert) {
  base::flat_set<std::string> modified_guids;
  NetworkPolicy* policy = FindPolicy(guid);
  if (!policy)
    return false;
  return policy->SetResolvedClientCertificate(std::move(resolved_cert)) ==
         ChangeEffect::kEffectivePolicyChanged;
}

const base::Value::Dict* ProfilePolicies::GetPolicyByGuid(
    const std::string& guid) const {
  const NetworkPolicy* policy = FindPolicy(guid);
  return policy ? &policy->GetPolicyWithRuntimeValues() : nullptr;
}

const base::Value::Dict* ProfilePolicies::GetOriginalPolicyByGuid(
    const std::string& guid) const {
  const NetworkPolicy* policy = FindPolicy(guid);
  return policy ? &policy->GetOriginalPolicy() : nullptr;
}

bool ProfilePolicies::HasPolicyMatchingShillProperties(
    const base::Value::Dict& shill_properties) const {
  for (const auto& [guid, policy] : guid_to_policy_) {
    if (shill_properties_matcher_.Run(policy.GetPolicyWithRuntimeValues(),
                                      shill_properties)) {
      return true;
    }
  }
  return false;
}

base::flat_map<std::string, base::Value::Dict>
ProfilePolicies::GetGuidToPolicyMap() const {
  std::vector<std::pair<std::string, base::Value::Dict>> result;
  result.reserve(guid_to_policy_.size());
  for (const auto& [guid, policy] : guid_to_policy_) {
    result.emplace_back(guid, policy.GetPolicyWithRuntimeValues().Clone());
  }
  return base::flat_map<std::string, base::Value::Dict>(std::move(result));
}

void ProfilePolicies::SetShillPropertiesMatcherForTesting(
    const ShillPropertiesMatcher& shill_properties_matcher) {
  shill_properties_matcher_ = shill_properties_matcher;
}

void ProfilePolicies::SetRuntimeValuesSetterForTesting(
    const RuntimeValuesSetter& runtime_values_setter) {
  runtime_values_setter_ = runtime_values_setter;
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

}  // namespace ash
