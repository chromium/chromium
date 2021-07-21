// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

const std::u16string GetLocalizedString(
    PolicyMap::Entry::L10nLookupFunction lookup,
    const std::map<int, absl::optional<std::vector<std::u16string>>>&
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

}  // namespace

PolicyMap::Entry::Entry() = default;
PolicyMap::Entry::Entry(
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    absl::optional<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher)
    : level(level),
      scope(scope),
      source(source),
      external_data_fetcher(std::move(external_data_fetcher)),
      value_(std::move(value)) {}

PolicyMap::Entry::~Entry() = default;

PolicyMap::Entry::Entry(Entry&&) noexcept = default;
PolicyMap::Entry& PolicyMap::Entry::operator=(Entry&&) noexcept = default;

PolicyMap::Entry PolicyMap::Entry::DeepCopy() const {
  Entry copy(level, scope, source,
             value_ ? absl::make_optional<base::Value>(value_->Clone())
                    : absl::nullopt,
             external_data_fetcher
                 ? std::make_unique<ExternalDataFetcher>(*external_data_fetcher)
                 : nullptr);
  copy.ignored_ = ignored_;
  copy.message_ids_ = message_ids_;
  copy.is_default_value_ = is_default_value_;
  copy.conflicts.reserve(conflicts.size());
  for (const auto& conflict : conflicts) {
    copy.AddConflictingPolicy(conflict.entry().DeepCopy());
  }
  return copy;
}

base::Value* PolicyMap::Entry::value() {
  return base::OptionalOrNullptr(value_);
}

const base::Value* PolicyMap::Entry::value() const {
  return base::OptionalOrNullptr(value_);
}

void PolicyMap::Entry::set_value(absl::optional<base::Value> val) {
  value_ = std::move(val);
}

bool PolicyMap::Entry::has_higher_priority_than(
    const PolicyMap::Entry& other) const {
  return std::tie(level, scope, source) >
         std::tie(other.level, other.scope, other.source);
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
      ((!value_ && !other.value()) ||
       (value_ && other.value() && *value_ == *other.value())) &&
      ExternalDataFetcher::Equals(external_data_fetcher.get(),
                                  other.external_data_fetcher.get());
  return equals;
}

void PolicyMap::Entry::AddMessage(MessageType type, int message_id) {
  message_ids_[type].emplace(message_id, absl::nullopt);
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

  bool is_value_equal = (!this->value() && !conflict.value()) ||
                        (this->value() && conflict.value() &&
                         *this->value() == *conflict.value());

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

PolicyMap::PolicyMap() = default;
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

const base::Value* PolicyMap::GetValue(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.ignored() ? entry->second.value()
                                                         : nullptr;
}

base::Value* PolicyMap::GetMutableValue(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.ignored() ? entry->second.value()
                                                         : nullptr;
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

void PolicyMap::Set(
    const std::string& policy,
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    absl::optional<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  Entry entry(level, scope, source, std::move(value),
              std::move(external_data_fetcher));
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

void PolicyMap::EraseMatching(
    const base::RepeatingCallback<bool(const const_iterator)>& filter) {
  FilterErase(filter, true);
}

void PolicyMap::EraseNonmatching(
    const base::RepeatingCallback<bool(const const_iterator)>& filter) {
  FilterErase(filter, false);
}

void PolicyMap::Swap(PolicyMap* other) {
  map_.swap(other->map_);
}

PolicyMap PolicyMap::Clone() const {
  PolicyMap clone;
  for (const auto& it : map_)
    clone.Set(it.first, it.second.DeepCopy());

  clone.SetUserAffiliationIds(user_affiliation_ids_);
  clone.SetDeviceAffiliationIds(device_affiliation_ids_);

  return clone;
}

void PolicyMap::MergeFrom(const PolicyMap& other) {
  DCHECK_NE(this, &other);

  for (const auto& policy_and_entry : other) {
    Entry* current_policy = GetMutableUntrusted(policy_and_entry.first);
    Entry other_policy = policy_and_entry.second.DeepCopy();

    if (!current_policy) {
      Set(policy_and_entry.first, std::move(other_policy));
      continue;
    }

    const bool other_is_higher_priority =
        policy_and_entry.second.has_higher_priority_than(*current_policy);

    Entry& higher_policy =
        other_is_higher_priority ? other_policy : *current_policy;
    Entry& conflicting_policy =
        other_is_higher_priority ? *current_policy : other_policy;

    const bool overwriting_default_policy =
        higher_policy.source != conflicting_policy.source &&
        conflicting_policy.source == POLICY_SOURCE_ENTERPRISE_DEFAULT;
    if (!overwriting_default_policy) {
      current_policy->value() &&
              *policy_and_entry.second.value() == *current_policy->value()
          ? higher_policy.AddMessage(MessageType::kInfo,
                                     IDS_POLICY_CONFLICT_SAME_VALUE)
          : higher_policy.AddMessage(MessageType::kWarning,
                                     IDS_POLICY_CONFLICT_DIFF_VALUE);
      higher_policy.AddConflictingPolicy(std::move(conflicting_policy));
    }

    if (other_is_higher_priority)
      *current_policy = std::move(other_policy);
  }

  SetUserAffiliationIds(
      CombineIds(GetUserAffiliationIds(), other.GetUserAffiliationIds()));
  SetDeviceAffiliationIds(
      CombineIds(GetDeviceAffiliationIds(), other.GetDeviceAffiliationIds()));
}

void PolicyMap::MergeValues(const std::vector<PolicyMerger*>& mergers) {
  for (const auto* it : mergers)
    it->Merge(&map_);
}

void PolicyMap::LoadFrom(const base::DictionaryValue* policies,
                         PolicyLevel level,
                         PolicyScope scope,
                         PolicySource source) {
  for (base::DictionaryValue::Iterator it(*policies); !it.IsAtEnd();
       it.Advance()) {
    Set(it.key(), level, scope, source, it.value().Clone(), nullptr);
  }
}

bool PolicyMap::Equals(const PolicyMap& other) const {
  return other.size() == size() &&
         std::equal(begin(), end(), other.begin(), MapEntryEquals);
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
bool PolicyMap::MapEntryEquals(const PolicyMap::PolicyMapType::value_type& a,
                               const PolicyMap::PolicyMapType::value_type& b) {
  bool equals = a.first == b.first && a.second.Equals(b.second);
  return equals;
}

void PolicyMap::FilterErase(
    const base::RepeatingCallback<bool(const const_iterator)>& filter,
    bool deletion_value) {
  auto iter(map_.begin());
  while (iter != map_.end()) {
    if (filter.Run(iter) == deletion_value) {
      map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

bool PolicyMap::IsUserAffiliated() const {
  return IsAffiliated(user_affiliation_ids_, device_affiliation_ids_);
}

void PolicyMap::SetUserAffiliationIds(
    const base::flat_set<std::string>& user_ids) {
  user_affiliation_ids_ = {user_ids.begin(), user_ids.end()};
}

const base::flat_set<std::string>& PolicyMap::GetUserAffiliationIds() const {
  return user_affiliation_ids_;
}

void PolicyMap::SetDeviceAffiliationIds(
    const base::flat_set<std::string>& device_ids) {
  device_affiliation_ids_ = {device_ids.begin(), device_ids.end()};
}

const base::flat_set<std::string>& PolicyMap::GetDeviceAffiliationIds() const {
  return device_affiliation_ids_;
}

}  // namespace policy
