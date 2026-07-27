// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_value_map.h"

#include <limits.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/values.h"

PrefValueMap::PrefValueMap() = default;

PrefValueMap::~PrefValueMap() = default;

bool PrefValueMap::GetValue(std::string_view key,
                            const base::Value** value) const {
  auto it = prefs_.find(key);
  if (it == prefs_.end())
    return false;

  if (value)
    *value = &it->second;

  return true;
}

bool PrefValueMap::GetValue(std::string_view key, base::Value** value) {
  auto it = prefs_.find(key);
  if (it == prefs_.end())
    return false;

  if (value)
    *value = &it->second;

  return true;
}

bool PrefValueMap::SetValue(std::string_view key, base::Value value) {
  // Once C++26 is supported, just do `base::Value& existing_value =
  // prefs_[key]`.
  auto it = prefs_.find(key);
  if (it == prefs_.end()) {
    it = prefs_.insert({std::string(key), base::Value()}).first;
  }
  base::Value& existing_value = it->second;
  if (value == existing_value)
    return false;

  existing_value = std::move(value);
  return true;
}

bool PrefValueMap::RemoveValue(std::string_view key) {
  // Once C++23 is supported, just do `return prefs_.erase(key)`;
  auto it = prefs_.find(key);
  if (it == prefs_.end()) {
    return false;
  }
  prefs_.erase(it);
  return true;
}

void PrefValueMap::Clear() {
  prefs_.clear();
}

void PrefValueMap::ClearWithPrefix(std::string_view prefix) {
  Map::iterator low = prefs_.lower_bound(prefix);
  // Appending maximum possible character so that there will be no string with
  // prefix |prefix| that we may miss.
  Map::iterator high = prefs_.upper_bound(std::string(prefix) + char(CHAR_MAX));
  prefs_.erase(low, high);
}

void PrefValueMap::Swap(PrefValueMap* other) {
  prefs_.swap(other->prefs_);
}

PrefValueMap::iterator PrefValueMap::begin() {
  return prefs_.begin();
}

PrefValueMap::iterator PrefValueMap::end() {
  return prefs_.end();
}

PrefValueMap::const_iterator PrefValueMap::begin() const {
  return prefs_.begin();
}

PrefValueMap::const_iterator PrefValueMap::end() const {
  return prefs_.end();
}

bool PrefValueMap::empty() const {
  return prefs_.empty();
}

bool PrefValueMap::GetBoolean(std::string_view key, bool* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_bool()) {
    *value = stored_value->GetBool();
    return true;
  }
  return false;
}

void PrefValueMap::SetBoolean(std::string_view key, bool value) {
  SetValue(key, base::Value(value));
}

bool PrefValueMap::GetString(std::string_view key, std::string* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_string()) {
    *value = stored_value->GetString();
    return true;
  }
  return false;
}

void PrefValueMap::SetString(std::string_view key, std::string_view value) {
  SetValue(key, base::Value(value));
}

bool PrefValueMap::GetInteger(std::string_view key, int* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_int()) {
    *value = stored_value->GetInt();
    return true;
  }
  return false;
}

void PrefValueMap::SetInteger(std::string_view key, const int value) {
  SetValue(key, base::Value(value));
}

void PrefValueMap::SetDouble(std::string_view key, const double value) {
  SetValue(key, base::Value(value));
}

void PrefValueMap::GetDifferingKeys(
    const PrefValueMap* other,
    std::vector<std::string>* differing_keys) const {
  static_assert(
      std::is_same_v<decltype(prefs_),
                     std::map<std::string, base::Value, std::less<void>>>,
      "If the type of the prefs_ map changes, be sure that the new type is "
      "still sorted or adapt this function.");
  differing_keys->clear();

  // prefs_ is already an ordered map, so walk both maps directly in
  // lockstep instead of copying into intermediate maps first.
  auto this_pref = prefs_.begin();
  auto other_pref = other->prefs_.begin();
  while (this_pref != prefs_.end() && other_pref != other->prefs_.end()) {
    const int diff = this_pref->first.compare(other_pref->first);
    if (diff == 0) {
      if (this_pref->second != other_pref->second) {
        differing_keys->push_back(this_pref->first);
      }
      ++this_pref;
      ++other_pref;
    } else if (diff < 0) {
      differing_keys->push_back(this_pref->first);
      ++this_pref;
    } else {
      differing_keys->push_back(other_pref->first);
      ++other_pref;
    }
  }

  // Add the remaining entries.
  for (; this_pref != prefs_.end(); ++this_pref) {
    differing_keys->push_back(this_pref->first);
  }
  for (; other_pref != other->prefs_.end(); ++other_pref) {
    differing_keys->push_back(other_pref->first);
  }
}

base::DictValue PrefValueMap::AsDict() const {
  base::DictValue dictionary;
  for (const auto& value : prefs_)
    dictionary.SetByDottedPath(value.first, value.second.Clone());

  return dictionary;
}
