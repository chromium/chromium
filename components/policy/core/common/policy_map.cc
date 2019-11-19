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
#include "components/policy/core/common/policy_merger.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

const base::string16 GetLocalizedString(
    PolicyMap::Entry::L10nLookupFunction lookup,
    const base::string16& initial_string,
    const std::set<int>& localized_string_ids) {
  base::string16 result = initial_string;
  base::string16 line_feed = base::UTF8ToUTF16("\n");
  for (int id : localized_string_ids) {
    result += lookup.Run(id);
    result += line_feed;
  }
  // Remove the trailing newline.
  if (!result.empty() && result[result.length() - 1] == line_feed[0])
    result.pop_back();
  return result;
}

}  // namespace

PolicyMap::Entry::Entry() = default;
PolicyMap::Entry::Entry(
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    std::unique_ptr<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher)
    : level(level),
      scope(scope),
      source(source),
      value(std::move(value)),
      external_data_fetcher(std::move(external_data_fetcher)) {}

PolicyMap::Entry::~Entry() = default;

PolicyMap::Entry::Entry(Entry&&) noexcept = default;
PolicyMap::Entry& PolicyMap::Entry::operator=(Entry&&) noexcept = default;

PolicyMap::Entry PolicyMap::Entry::DeepCopy() const {
  Entry copy(level, scope, source, value ? value->CreateDeepCopy() : nullptr,
             external_data_fetcher
                 ? std::make_unique<ExternalDataFetcher>(*external_data_fetcher)
                 : nullptr);
  copy.error_strings_ = error_strings_;
  copy.error_message_ids_ = error_message_ids_;
  copy.warning_message_ids_ = warning_message_ids_;
  copy.conflicts.reserve(conflicts.size());
  for (const auto& conflict : conflicts) {
    copy.AddConflictingPolicy(conflict.DeepCopy());
  }
  return copy;
}

bool PolicyMap::Entry::has_higher_priority_than(
    const PolicyMap::Entry& other) const {
  return std::tie(level, scope, source) >
         std::tie(other.level, other.scope, other.source);
}

bool PolicyMap::Entry::Equals(const PolicyMap::Entry& other) const {
  bool conflicts_are_equal = conflicts.size() == other.conflicts.size();
  for (size_t i = 0; conflicts_are_equal && i < conflicts.size(); ++i)
    conflicts_are_equal &= conflicts[i].Equals(other.conflicts[i]);

  const bool equals =
      conflicts_are_equal && level == other.level && scope == other.scope &&
      source == other.source &&  // Necessary for PolicyUIHandler observers.
                                 // They have to update when sources change.
      error_strings_ == other.error_strings_ &&
      error_message_ids_ == other.error_message_ids_ &&
      warning_message_ids_ == other.warning_message_ids_ &&
      ((!value && !other.value) ||
       (value && other.value && *value == *other.value)) &&
      ExternalDataFetcher::Equals(external_data_fetcher.get(),
                                  other.external_data_fetcher.get());
  return equals;
}

void PolicyMap::Entry::AddError(base::StringPiece error) {
  base::StrAppend(&error_strings_, {error, "\n"});
}

void PolicyMap::Entry::AddError(int message_id) {
  error_message_ids_.insert(message_id);
}

void PolicyMap::Entry::AddWarning(int message_id) {
  warning_message_ids_.insert(message_id);
}

void PolicyMap::Entry::AddConflictingPolicy(Entry&& conflict) {
  // Move all of the newly conflicting Entry's conflicts into this Entry.
  std::move(conflict.conflicts.begin(), conflict.conflicts.end(),
            std::back_inserter(conflicts));

  // Avoid conflict nesting
  conflicts.emplace_back(conflict.level, conflict.scope, conflict.source,
                         std::move(conflict.value),
                         std::move(conflict.external_data_fetcher));
}

void PolicyMap::Entry::ClearConflicts() {
  conflicts.clear();
  error_message_ids_.erase(IDS_POLICY_CONFLICT_SAME_VALUE);
  error_message_ids_.erase(IDS_POLICY_CONFLICT_DIFF_VALUE);
}

base::string16 PolicyMap::Entry::GetLocalizedErrors(
    L10nLookupFunction lookup) const {
  return GetLocalizedString(lookup, base::UTF8ToUTF16(error_strings_),
                            error_message_ids_);
}

base::string16 PolicyMap::Entry::GetLocalizedWarnings(
    L10nLookupFunction lookup) const {
  return GetLocalizedString(lookup, base::string16(), warning_message_ids_);
}

bool PolicyMap::Entry::IsBlockedOrIgnored() const {
  return error_message_ids_.find(IDS_POLICY_BLOCKED) !=
             error_message_ids_.end() ||
         IsIgnoredByAtomicGroup();
}

void PolicyMap::Entry::SetBlocked() {
  error_message_ids_.insert(IDS_POLICY_BLOCKED);
}

void PolicyMap::Entry::SetIgnoredByPolicyAtomicGroup() {
  error_message_ids_.insert(IDS_POLICY_IGNORED_BY_GROUP_MERGING);
}

bool PolicyMap::Entry::IsIgnoredByAtomicGroup() const {
  return error_message_ids_.find(IDS_POLICY_IGNORED_BY_GROUP_MERGING) !=
         error_message_ids_.end();
}

PolicyMap::PolicyMap() {}

PolicyMap::~PolicyMap() {
  Clear();
}

const PolicyMap::Entry* PolicyMap::Get(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.IsBlockedOrIgnored()
             ? &entry->second
             : nullptr;
}

PolicyMap::Entry* PolicyMap::GetMutable(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.IsBlockedOrIgnored()
             ? &entry->second
             : nullptr;
}

const base::Value* PolicyMap::GetValue(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.IsBlockedOrIgnored()
             ? entry->second.value.get()
             : nullptr;
}

base::Value* PolicyMap::GetMutableValue(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry != map_.end() && !entry->second.IsBlockedOrIgnored()
             ? entry->second.value.get()
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
    std::unique_ptr<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  Entry entry(level, scope, source, std::move(value),
              std::move(external_data_fetcher));
  Set(policy, std::move(entry));
}

void PolicyMap::Set(const std::string& policy, Entry entry) {
  map_[policy] = std::move(entry);
}

void PolicyMap::AddError(const std::string& policy, const std::string& error) {
  map_[policy].AddError(error);
}

void PolicyMap::AddError(const std::string& policy, int message_id) {
  map_[policy].AddError(message_id);
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

void PolicyMap::Erase(const std::string& policy) {
  map_.erase(policy);
}

void PolicyMap::EraseMatching(
    const base::Callback<bool(const const_iterator)>& filter) {
  FilterErase(filter, true);
}

void PolicyMap::EraseNonmatching(
    const base::Callback<bool(const const_iterator)>& filter) {
  FilterErase(filter, false);
}

void PolicyMap::Swap(PolicyMap* other) {
  map_.swap(other->map_);
}

void PolicyMap::CopyFrom(const PolicyMap& other) {
  Clear();
  for (const auto& it : other)
    Set(it.first, it.second.DeepCopy());
}

std::unique_ptr<PolicyMap> PolicyMap::DeepCopy() const {
  std::unique_ptr<PolicyMap> copy(new PolicyMap());
  copy->CopyFrom(*this);
  return copy;
}

void PolicyMap::MergeFrom(const PolicyMap& other) {
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
      higher_policy.AddConflictingPolicy(std::move(conflicting_policy));
      higher_policy.AddWarning(
          (current_policy->value &&
           *policy_and_entry.second.value == *current_policy->value)
              ? IDS_POLICY_CONFLICT_SAME_VALUE
              : IDS_POLICY_CONFLICT_DIFF_VALUE);
    }

    if (other_is_higher_priority)
      *current_policy = std::move(other_policy);
  }
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
    Set(it.key(), level, scope, source, it.value().CreateDeepCopy(), nullptr);
  }
}

void PolicyMap::GetDifferingKeys(const PolicyMap& other,
                                 std::set<std::string>* differing_keys) const {
  // Walk over the maps in lockstep, adding everything that is different.
  auto iter_this(begin());
  auto iter_other(other.begin());
  while (iter_this != end() && iter_other != other.end()) {
    const int diff = iter_this->first.compare(iter_other->first);
    if (diff == 0) {
      if (!iter_this->second.Equals(iter_other->second))
        differing_keys->insert(iter_this->first);
      ++iter_this;
      ++iter_other;
    } else if (diff < 0) {
      differing_keys->insert(iter_this->first);
      ++iter_this;
    } else {
      differing_keys->insert(iter_other->first);
      ++iter_other;
    }
  }

  // Add the remaining entries.
  for (; iter_this != end(); ++iter_this)
    differing_keys->insert(iter_this->first);
  for (; iter_other != other.end(); ++iter_other)
    differing_keys->insert(iter_other->first);
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
    const base::Callback<bool(const const_iterator)>& filter,
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

}  // namespace policy
