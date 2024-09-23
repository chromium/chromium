// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

const std::u16string GetLocalizedString(
    PolicyMap::Entry::L10nLookupFunction lookup,
    const std::map<int, std::optional<std::vector<std::u16string>>>&
        localized_string_ids) {
  std::u16string result = std::u16string();
  std::u16string line_feed = u"\n";
  for (const auto& string_pairs : localized_string_ids) {
    if (string_pairs.second)
      result += l10n_util::GetStringFUTF16(
          string_pairs.first, string_pairs.second.value(), nullptr);
    else
      result += lookup.Run(string_pairs.first);
    result += line_feed;
  }
  // Remove the trailing newline.
  if (!result.empty() && result[result.length() - 1] == line_feed[0])
    result.pop_back();
  return result;
}

// Inserts additional user affiliation IDs to the existing set.
base::flat_set<std::string> CombineIds(
    const base::flat_set<std::string>& ids_first,
    const base::flat_set<std::string>& ids_second) {
  base::flat_set<std::string> combined_ids;
  combined_ids.insert(ids_first.begin(), ids_first.end());
  combined_ids.insert(ids_second.begin(), ids_second.end());
  return combined_ids;
}

#if !BUILDFLAG(IS_CHROMEOS)
// Returns the calculated priority of the policy entry based on the policy's
// scope and source, in addition to external factors such as precedence
// metapolicy values. Used for browser policies.
PolicyPriorityBrowser GetPriority(
    PolicySource source,
    PolicyScope scope,
    bool cloud_policy_overrides_platform_policy,
    bool cloud_user_policy_overrides_cloud_machine_policy,
    bool is_user_affiliated,
    const PolicyDetails* details) {
  switch (source) {
    case POLICY_SOURCE_ENTERPRISE_DEFAULT:
      return POLICY_PRIORITY_BROWSER_ENTERPRISE_DEFAULT;
    case POLICY_SOURCE_COMMAND_LINE:
      return POLICY_PRIORITY_BROWSER_COMMAND_LINE;
    case POLICY_SOURCE_CLOUD:
      if (scope == POLICY_SCOPE_MACHINE) {
        // Raise the priority of cloud machine policies only when the metapolicy
        // CloudPolicyOverridesPlatformPolicy is set to true.
        return cloud_policy_overrides_platform_policy
                   ? POLICY_PRIORITY_BROWSER_CLOUD_MACHINE_RAISED
                   : POLICY_PRIORITY_BROWSER_CLOUD_MACHINE;
      }
      // For policies that can only be set with managed account, raise the
      // priority of correct source to highest.
      if (details && details->scope == kSingleProfile) {
        return POLICY_PRIORITY_BROWSER_CLOUD_USER_DOUBLE_RAISED;
      }
      if (cloud_user_policy_overrides_cloud_machine_policy &&
          is_user_affiliated) {
        // Raise the priority of cloud user policies only when the metapolicy
        // CloudUserPolicyOverridesCloudMachinePolicy is set to true and the
        // user is affiliated. Its priority relative to cloud machine policies
        // also depends on the value of CloudPolicyOverridesPlatformPolicy.
        return cloud_policy_overrides_platform_policy
                   ? POLICY_PRIORITY_BROWSER_CLOUD_USER_DOUBLE_RAISED
                   : POLICY_PRIORITY_BROWSER_CLOUD_USER_RAISED;
      }
      return POLICY_PRIORITY_BROWSER_CLOUD_USER;
    case POLICY_SOURCE_PLATFORM:
      return scope == POLICY_SCOPE_MACHINE
                 ? POLICY_PRIORITY_BROWSER_PLATFORM_MACHINE
                 : POLICY_PRIORITY_BROWSER_PLATFORM_USER;
    case POLICY_SOURCE_MERGED:
      return POLICY_PRIORITY_BROWSER_MERGED;
    default:
      NOTREACHED_IN_MIGRATION();
      return POLICY_PRIORITY_BROWSER_ENTERPRISE_DEFAULT;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Helper function used in the invocation of `PolicyMap::CloneIf` from
// `PolicyMap::Clone`.
bool AllowAllPolicies(PolicyMap::const_reference unused) {
  return true;
}

}  // namespace

PolicyMap::Entry::Entry() = default;
PolicyMap::Entry::Entry(
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    std::optional<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher,
    const PolicyDetails* details)
    : level(level),
      scope(scope),
      source(source),
      external_data_fetcher(std::move(external_data_fetcher)),
      details(details),
      value_(std::move(value)) {}

PolicyMap::Entry::~Entry() = default;

PolicyMap::Entry::Entry(Entry&&) noexcept = default;
PolicyMap::Entry& PolicyMap::Entry::operator=(Entry&&) noexcept = default;

PolicyMap::Entry PolicyMap::Entry::DeepCopy() const {
  Entry copy(
      level, scope, source,
      value_ ? std::make_optional<base::Value>(value_->Clone()) : std::nullopt,
      external_data_fetcher
          ? std::make_unique<ExternalDataFetcher>(*external_data_fetcher)
          : nullptr,
      details);
  copy.ignored_ = ignored_;
  copy.message_ids_ = message_ids_;
  copy.is_default_value_ = is_default_value_;
  copy.conflicts.reserve(conflicts.size());
  for (const auto& conflict : conflicts) {
    copy.AddConflictingPolicy(conflict.entry().DeepCopy());
  }
  return copy;
}

const base::Value* PolicyMap::Entry::value(base::Value::Type value_type) const {
  const base::Value* value = value_unsafe();
  return value && value->type() == value_type ? value : nullptr;
}

base::Value* PolicyMap::Entry::value(base::Value::Type value_type) {
  base::Value* value = value_unsafe();
  return value && value->type() == value_type ? value : nullptr;
}

const base::Value* PolicyMap::Entry::value_unsafe() const {
  return base::OptionalToPtr(value_);
}

base::Value* PolicyMap::Entry::value_unsafe() {
  return base::OptionalToPtr(value_);
}

void PolicyMap::Entry::set_value(std::optional<base::Value> val) {
  value_ = std::move(val);
}

bool PolicyMap::Entry::Equals(const PolicyMap::Entry& other) const {
  bool conflicts_are_equal = conflicts.size() == other.conflicts.size();
  for (size_t i = 0; conflicts_are_equal && i < conflicts.size(); ++i)
    conflicts_are_equal &=
        conflicts[i].entry().Equals(other.conflicts[i].entry());

  const bool equals =
      conflicts_are_equal && level == other.level && scope == other.scope &&
      source == other.source &&  // Necessary for PolicyUIHandler observers.
                                 // They have to update when sources change.
      message_ids_ == other.message_ids_ &&
      is_default_value_ == other.is_default_value_ &&
      ((!value_ && !other.value_unsafe()) ||
       (value_ && other.value_unsafe() && *value_ == *other.value_unsafe())) &&
      ExternalDataFetcher::Equals(external_data_fetcher.get(),
                                  other.external_data_fetcher.get());
  return equals;
}

void PolicyMap::Entry::AddMessage(MessageType type, int message_id) {
  message_ids_[type].emplace(message_id, std::nullopt);
}

void PolicyMap::Entry::AddMessage(MessageType type,
                                  int message_id,
                                  std::vector<std::u16string>&& message_args) {
  message_ids_[type].emplace(message_id, std::move(message_args));
}

void PolicyMap::Entry::ClearMessage(MessageType type, int message_id) {
  if (message_ids_.find(type) == message_ids_.end() ||
      message_ids_[type].find(message_id) == message_ids_[type].end()) {
    return;
  }
  message_ids_[type].erase(message_id);
  if (message_ids_[type].size() == 0)
    message_ids_.erase(type);
}

void PolicyMap::Entry::AddConflictingPolicy(Entry&& conflict) {
  // Move all of the newly conflicting Entry's conflicts into this Entry.
  std::move(conflict.conflicts.begin(), conflict.conflicts.end(),
            std::back_inserter(conflicts));

  bool is_value_equal = (!this->value_unsafe() && !conflict.value_unsafe()) ||
                        (this->value_unsafe() && conflict.value_unsafe() &&
                         *this->value_unsafe() == *conflict.value_unsafe());

  ConflictType type =
      is_value_equal ? ConflictType::Supersede : ConflictType::Override;

  // Clean up conflict Entry to ensure there's no duplication since entire Entry
  // is moved and treated as a freshly constructed Entry.
  conflict.ClearConflicts();
  conflict.is_default_value_ = false;
  conflict.message_ids_.clear();

  // Avoid conflict nesting
  conflicts.emplace_back(type, std::move(conflict));
}

void PolicyMap::Entry::ClearConflicts() {
  conflicts.clear();
  ClearMessage(MessageType::kInfo, IDS_POLICY_CONFLICT_SAME_VALUE);
  ClearMessage(MessageType::kWarning, IDS_POLICY_CONFLICT_DIFF_VALUE);
}

bool PolicyMap::Entry::HasConflicts() {
  return !conflicts.empty();
}

bool PolicyMap::Entry::HasMessage(MessageType type) const {
  return message_ids_.find(type) != message_ids_.end();
}

std::u16string PolicyMap::Entry::GetLocalizedMessages(
    MessageType type,
    L10nLookupFunction lookup) const {
  if (!HasMessage(type)) {
    return std::u16string();
  }
  return GetLocalizedString(lookup, message_ids_.at(type));
}

bool PolicyMap::Entry::ignored() const {
  return ignored_;
}

void PolicyMap::Entry::SetIgnored() {
  ignored_ = true;
}

void PolicyMap::Entry::SetBlocked() {
  SetIgnored();
  AddMessage(MessageType::kError, IDS_POLICY_BLOCKED);
}

void PolicyMap::Entry::SetInvalid() {
  SetIgnored();
  AddMessage(MessageType::kError, IDS_POLICY_INVALID);
}

void PolicyMap::Entry::SetIgnoredByPolicyAtomicGroup() {
  SetIgnored();
  AddMessage(MessageType::kError, IDS_POLICY_IGNORED_BY_GROUP_MERGING);
}

bool PolicyMap::Entry::IsIgnoredByAtomicGroup() const {
  return message_ids_.find(MessageType::kError) != message_ids_.end() &&
         message_ids_.at(MessageType::kError)
                 .find(IDS_POLICY_IGNORED_BY_GROUP_MERGING) !=
             message_ids_.at(MessageType::kError).end();
}

void PolicyMap::Entry::SetIsDefaultValue() {
  is_default_value_ = true;
}

bool PolicyMap::Entry::IsDefaultValue() const {
  return is_default_value_;
}

PolicyMap::EntryConflict::EntryConflict() = default;
PolicyMap::EntryConflict::EntryConflict(ConflictType type, Entry&& entry)
    : conflict_type_(type), entry_(std::move(entry)) {}

PolicyMap::EntryConflict::~EntryConflict() = default;

PolicyMap::EntryConflict::EntryConflict(EntryConflict&&) noexcept = default;
PolicyMap::EntryConflict& PolicyMap::EntryConflict::operator=(
    EntryConflict&&) noexcept = default;

void PolicyMap::EntryConflict::SetConflictType(ConflictType type) {
  conflict_type_ = type;
}

PolicyMap::ConflictType PolicyMap::EntryConflict::conflict_type() const {
  return conflict_type_;
}

const PolicyMap::Entry& PolicyMap::EntryConflict::entry() const {
  return entry_;
}

PolicyMap::PolicyMap()
    : details_callback_{base::BindRepeating(&GetChromePolicyDetails)} {}
PolicyMap::PolicyMap(PolicyMap&&) noexcept = default;
PolicyMap& PolicyMap::operator=(PolicyMap&&) noexcept = default;
PolicyMap::~PolicyMap() = default;

const PolicyMap::Entry* PolicyMap::Get(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.ignored() ? &entry->second
                                                         : nullptr;
}

PolicyMap::Entry* PolicyMap::GetMutable(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.ignored() ? &entry->second
                                                         : nullptr;
}

const base::Value* PolicyMap::GetValue(const std::string& policy,
                                       base::Value::Type value_type) const {
  auto* entry = Get(policy);
  return entry ? entry->value(value_type) : nullptr;
}

base::Value* PolicyMap::GetMutableValue(const std::string& policy,
                                        base::Value::Type value_type) {
  auto* entry = GetMutable(policy);
  return entry ? entry->value(value_type) : nullptr;
}

const base::Value* PolicyMap::GetValueUnsafe(const std::string& policy) const {
  auto* entry = Get(policy);
  return entry ? entry->value_unsafe() : nullptr;
}

base::Value* PolicyMap::GetMutableValueUnsafe(const std::string& policy) {
  auto* entry = GetMutable(policy);
  return entry ? entry->value_unsafe() : nullptr;
}

const PolicyMap::Entry* PolicyMap::GetUntrusted(
    const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry != map_.end() ? &entry->second : nullptr;
}

PolicyMap::Entry* PolicyMap::GetMutableUntrusted(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry != map_.end() ? &entry->second : nullptr;
}

bool PolicyMap::IsPolicySet(const std::string& policy) const {
  return GetValueUnsafe(policy) != nullptr;
}

void PolicyMap::Set(
    const std::string& policy,
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    std::optional<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  Entry entry(level, scope, source, std::move(value),
              std::move(external_data_fetcher), GetPolicyDetails(policy));
  Set(policy, std::move(entry));
}

void PolicyMap::Set(const std::string& policy, Entry entry) {
  map_[policy] = std::move(entry);
}

void PolicyMap::AddMessage(const std::string& policy,
                           MessageType type,
                           int message_id) {
  map_[policy].AddMessage(type, message_id);
}

void PolicyMap::AddMessage(const std::string& policy,
                           MessageType type,
                           int message_id,
                           std::vector<std::u16string>&& message_args) {
  map_[policy].AddMessage(type, message_id, std::move(message_args));
}

bool PolicyMap::IsPolicyIgnoredByAtomicGroup(const std::string& policy) const {
  const auto& entry = map_.find(policy);
  return entry != map_.end() && entry->second.IsIgnoredByAtomicGroup();
}

void PolicyMap::SetSourceForAll(PolicySource source) {
  for (auto& it : map_) {
    it.second.source = source;
  }
}

void PolicyMap::SetAllInvalid() {
  for (auto& it : map_) {
    it.second.SetInvalid();
  }
}

void PolicyMap::Erase(const std::string& policy) {
  map_.erase(policy);
}

PolicyMap::iterator PolicyMap::EraseIt(const_iterator it) {
  return map_.erase(it);
}

void PolicyMap::Swap(PolicyMap* other) {
  map_.swap(other->map_);
}

PolicyMap PolicyMap::Clone() const {
  return CloneIf(base::BindRepeating(&AllowAllPolicies));
}

PolicyMap PolicyMap::CloneIf(
    const base::RepeatingCallback<bool(const_reference)>& filter) const {
  PolicyMap clone;
  for (const_reference it : map_) {
    if (filter.Run(it)) {
      clone.Set(it.first, it.second.DeepCopy());
    }
  }

  clone.cloud_policy_overrides_platform_policy_ =
      cloud_policy_overrides_platform_policy_;
  clone.cloud_user_policy_overrides_cloud_machine_policy_ =
      cloud_user_policy_overrides_cloud_machine_policy_;
  clone.SetUserAffiliationIds(user_affiliation_ids_);
  clone.SetDeviceAffiliationIds(device_affiliation_ids_);

  return clone;
}

void PolicyMap::MergePolicy(const std::string& policy_name,
                            const PolicyMap& other,
                            bool using_default_precedence) {
  const Entry* other_policy = other.GetUntrusted(policy_name);
  if (!other_policy)
    return;

  Entry* policy = GetMutableUntrusted(policy_name);
  Entry other_policy_copy = other_policy->DeepCopy();

  if (!policy) {
    Set(policy_name, std::move(other_policy_copy));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  const bool other_is_higher_priority =
      EntryHasHigherPriority(other_policy_copy, *policy);
#else   // BUILDFLAG(IS_CHROMEOS)
  const bool other_is_higher_priority = EntryHasHigherPriority(
      other_policy_copy, *policy, using_default_precedence);
#endif  // BUILDFLAG(IS_CHROMEOS)

  Entry& higher_policy = other_is_higher_priority ? other_policy_copy : *policy;
  Entry& conflicting_policy =
      other_is_higher_priority ? *policy : other_policy_copy;

  const bool overwriting_default_policy =
      higher_policy.source != conflicting_policy.source &&
      conflicting_policy.source == POLICY_SOURCE_ENTERPRISE_DEFAULT;
  if (!overwriting_default_policy) {
    policy->value_unsafe() &&
            *other_policy_copy.value_unsafe() == *policy->value_unsafe()
        ? higher_policy.AddMessage(MessageType::kInfo,
                                   IDS_POLICY_CONFLICT_SAME_VALUE)
        : higher_policy.AddMessage(MessageType::kWarning,
                                   IDS_POLICY_CONFLICT_DIFF_VALUE);
    higher_policy.AddConflictingPolicy(std::move(conflicting_policy));
  }

  if (other_is_higher_priority)
    *policy = std::move(other_policy_copy);
}

void PolicyMap::MergeFrom(const PolicyMap& other) {
  DCHECK_NE(this, &other);
  // Set affiliation IDs before merging policy values because user affiliation
  // affects the policy precedence check.
  SetUserAffiliationIds(
      CombineIds(GetUserAffiliationIds(), other.GetUserAffiliationIds()));
  SetDeviceAffiliationIds(
      CombineIds(GetDeviceAffiliationIds(), other.GetDeviceAffiliationIds()));

#if !BUILDFLAG(IS_CHROMEOS)
  // Precedence metapolicies are merged before all other policies, including
  // merging metapolicies, because their value affects policy overriding.
  for (auto* policy : metapolicy::kPrecedence) {
    // Default precedence is used during merging of precedence metapolicies to
    // prevent circular dependencies.
    MergePolicy(policy, other, true);
  }

  UpdateStoredComputedMetapolicies();
#endif

  for (const auto& policy_and_entry : other) {
#if !BUILDFLAG(IS_CHROMEOS)
    // Skip precedence metapolicies since they have already been merged into the
    // current PolicyMap.
    if (base::Contains(metapolicy::kPrecedence, policy_and_entry.first)) {
      continue;
    }
#endif

    // External factors, such as the values of metapolicies, are considered.
    MergePolicy(policy_and_entry.first, other, false);
  }
}

void PolicyMap::MergeValues(const std::vector<PolicyMerger*>& mergers) {
  for (const auto* it : mergers)
    it->Merge(this);
}

void PolicyMap::set_chrome_policy_details_callback_for_test(
    const GetChromePolicyDetailsCallback& details_callback) {
  details_callback_ = details_callback;
}

bool PolicyMap::IsPolicyExternal(const std::string& policy) {
  const PolicyDetails* policy_details = GetPolicyDetails(policy);
  if (policy_details && policy_details->max_external_data_size > 0)
    return true;
  return false;
}

const PolicyDetails* PolicyMap::GetPolicyDetails(
    const std::string& policy) const {
  return details_callback_.Run(policy);
}

void PolicyMap::LoadFrom(const base::Value::Dict& policies,
                         PolicyLevel level,
                         PolicyScope scope,
                         PolicySource source) {
  for (auto it : policies) {
    if (IsPolicyExternal(it.first)) {
      LOG_POLICY(WARNING, POLICY_PROCESSING)
          << "Ignoring external policy: " << it.first;
      continue;
    }
    Set(it.first, level, scope, source, it.second.Clone(), nullptr);
  }
}

bool PolicyMap::Equals(const PolicyMap& other) const {
  return base::ranges::equal(*this, other, MapEntryEquals);
}

bool PolicyMap::empty() const {
  return map_.empty();
}

size_t PolicyMap::size() const {
  return map_.size();
}

PolicyMap::const_iterator PolicyMap::begin() const {
  return map_.begin();
}

PolicyMap::const_iterator PolicyMap::end() const {
  return map_.end();
}

PolicyMap::iterator PolicyMap::begin() {
  return map_.begin();
}

PolicyMap::iterator PolicyMap::end() {
  return map_.end();
}

void PolicyMap::Clear() {
  map_.clear();
}

// static
bool PolicyMap::MapEntryEquals(const_reference a, const_reference b) {
  bool equals = a.first == b.first && a.second.Equals(b.second);
  return equals;
}

bool PolicyMap::EntryHasHigherPriority(const PolicyMap::Entry& lhs,
                                       const PolicyMap::Entry& rhs) const {
  return EntryHasHigherPriority(lhs, rhs, false);
}

bool PolicyMap::EntryHasHigherPriority(const PolicyMap::Entry& lhs,
                                       const PolicyMap::Entry& rhs,
                                       bool using_default_precedence) const {
#if BUILDFLAG(IS_CHROMEOS)
  return std::tie(lhs.level, lhs.scope, lhs.source) >
         std::tie(rhs.level, rhs.scope, rhs.source);
#else   // BUILDFLAG(IS_CHROMEOS)
  const PolicyDetails* details = lhs.details ? lhs.details : rhs.details;
  PolicyPriorityBrowser lhs_priority =
      using_default_precedence
          ? GetPriority(lhs.source, lhs.scope, false, false, false, details)
          : GetPriority(lhs.source, lhs.scope,
                        cloud_policy_overrides_platform_policy_,
                        cloud_user_policy_overrides_cloud_machine_policy_,
                        IsUserAffiliated(), details);
  PolicyPriorityBrowser rhs_priority =
      using_default_precedence
          ? GetPriority(rhs.source, rhs.scope, false, false, false, details)
          : GetPriority(rhs.source, rhs.scope,
                        cloud_policy_overrides_platform_policy_,
                        cloud_user_policy_overrides_cloud_machine_policy_,
                        IsUserAffiliated(), details);
  return std::tie(lhs.level, lhs_priority) > std::tie(rhs.level, rhs_priority);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool PolicyMap::IsUserAffiliated() const {
  return is_user_affiliated_;
}

void PolicyMap::SetUserAffiliationIds(
    const base::flat_set<std::string>& user_ids) {
  user_affiliation_ids_ = {user_ids.begin(), user_ids.end()};
  UpdateStoredUserAffiliation();
}

const base::flat_set<std::string>& PolicyMap::GetUserAffiliationIds() const {
  return user_affiliation_ids_;
}

void PolicyMap::SetDeviceAffiliationIds(
    const base::flat_set<std::string>& device_ids) {
  device_affiliation_ids_ = {device_ids.begin(), device_ids.end()};
  UpdateStoredUserAffiliation();
}

const base::flat_set<std::string>& PolicyMap::GetDeviceAffiliationIds() const {
  return device_affiliation_ids_;
}

#if !BUILDFLAG(IS_CHROMEOS)
void PolicyMap::UpdateStoredComputedMetapolicies() {
  cloud_policy_overrides_platform_policy_ =
      GetValue(key::kCloudPolicyOverridesPlatformPolicy,
               base::Value::Type::BOOLEAN) &&
      GetValue(key::kCloudPolicyOverridesPlatformPolicy,
               base::Value::Type::BOOLEAN)
          ->GetBool();

  cloud_user_policy_overrides_cloud_machine_policy_ =
      GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
               base::Value::Type::BOOLEAN) &&
      GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy,
               base::Value::Type::BOOLEAN)
          ->GetBool();
}
#endif

void PolicyMap::UpdateStoredUserAffiliation() {
  is_user_affiliated_ =
      IsAffiliated(user_affiliation_ids_, device_affiliation_ids_);
}

}  // namespace policy
