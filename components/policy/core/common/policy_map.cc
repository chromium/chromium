// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include <algorithm>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"

namespace policy {

PolicyMap::Entry::Entry() = default;

PolicyMap::Entry::~Entry() = default;

PolicyMap::Entry::Entry(Entry&&) noexcept = default;
PolicyMap::Entry& PolicyMap::Entry::operator=(Entry&&) noexcept = default;

PolicyMap::Entry PolicyMap::Entry::DeepCopy() const {
  Entry copy;
  copy.level = level;
  copy.scope = scope;
  copy.source = source;
  if (value)
    copy.value = value->CreateDeepCopy();
  copy.error_strings_ = error_strings_;
  copy.error_message_ids_ = error_message_ids_;
  if (external_data_fetcher) {
    copy.external_data_fetcher.reset(
        new ExternalDataFetcher(*external_data_fetcher));
  }
  return copy;
}

bool PolicyMap::Entry::has_higher_priority_than(
    const PolicyMap::Entry& other) const {
  if (level != other.level)
    return level > other.level;

  if (scope != other.scope)
    return scope > other.scope;

  return source > other.source;
}

bool PolicyMap::Entry::Equals(const PolicyMap::Entry& other) const {
  return level == other.level && scope == other.scope &&
         source == other.source &&  // Necessary for PolicyUIHandler observers.
                                    // They have to update when sources change.
         error_strings_ == other.error_strings_ &&
         error_message_ids_ == other.error_message_ids_ &&
         ((!value && !other.value) ||
          (value && other.value && *value == *other.value)) &&
         ExternalDataFetcher::Equals(external_data_fetcher.get(),
                                     other.external_data_fetcher.get());
}

void PolicyMap::Entry::AddError(base::StringPiece error) {
  base::StrAppend(&error_strings_, {error, "\n"});
}

void PolicyMap::Entry::AddError(int message_id) {
  error_message_ids_.push_back(message_id);
}

base::string16 PolicyMap::Entry::GetLocalizedErrors(
    L10nLookupFunction lookup) const {
  base::string16 error_string = base::UTF8ToUTF16(error_strings_);
  base::string16 line_feed = base::UTF8ToUTF16("\n");
  for (int message_id : error_message_ids_) {
    error_string += lookup.Run(message_id);
    error_string += line_feed;
  }
  // Remove the trailing newline.
  if (!error_string.empty())
    error_string.pop_back();
  return error_string;
}

PolicyMap::PolicyMap() {}

PolicyMap::~PolicyMap() {
  Clear();
}

const PolicyMap::Entry* PolicyMap::Get(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry == map_.end() ? nullptr : &entry->second;
}

PolicyMap::Entry* PolicyMap::GetMutable(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry == map_.end() ? nullptr : &entry->second;
}

const base::Value* PolicyMap::GetValue(const std::string& policy) const {
  auto entry = map_.find(policy);
  return entry == map_.end() ? nullptr : entry->second.value.get();
}

base::Value* PolicyMap::GetMutableValue(const std::string& policy) {
  auto entry = map_.find(policy);
  return entry == map_.end() ? nullptr : entry->second.value.get();
}

void PolicyMap::Set(
    const std::string& policy,
    PolicyLevel level,
    PolicyScope scope,
    PolicySource source,
    std::unique_ptr<base::Value> value,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  Entry entry;
  entry.level = level;
  entry.scope = scope;
  entry.source = source;
  entry.value = std::move(value);
  entry.external_data_fetcher = std::move(external_data_fetcher);
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
  for (const auto& it : other) {
    const Entry* entry = Get(it.first);
    if (!entry || it.second.has_higher_priority_than(*entry))
      Set(it.first, it.second.DeepCopy());
  }
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
  return a.first == b.first && a.second.Equals(b.second);
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
