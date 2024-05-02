// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/value_map_pref_store.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/observer_list.h"
#include "base/values.h"

ValueMapPrefStore::ValueMapPrefStore() {}

bool ValueMapPrefStore::GetValue(std::string_view key,
                                 const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

base::Value::Dict ValueMapPrefStore::GetValues() const {
  return prefs_.AsDict();
}

void ValueMapPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ValueMapPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ValueMapPrefStore::HasObservers() const {
  return !observers_.empty();
}

void ValueMapPrefStore::SetValue(const std::string& key,
                                 base::Value value,
                                 uint32_t flags) {
  if (prefs_.SetValue(key, std::move(value))) {
    for (Observer& observer : observers_)
      observer.OnPrefValueChanged(key);
  }
}

void ValueMapPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  if (prefs_.RemoveValue(key)) {
    for (Observer& observer : observers_)
      observer.OnPrefValueChanged(key);
  }
}

bool ValueMapPrefStore::GetMutableValue(const std::string& key,
                                        base::Value** value) {
  return prefs_.GetValue(key, value);
}

void ValueMapPrefStore::ReportValueChanged(const std::string& key,
                                           uint32_t flags) {
  for (Observer& observer : observers_)
    observer.OnPrefValueChanged(key);
}

void ValueMapPrefStore::SetValueSilently(const std::string& key,
                                         base::Value value,
                                         uint32_t flags) {
  prefs_.SetValue(key, std::move(value));
}

ValueMapPrefStore::~ValueMapPrefStore() {}

void ValueMapPrefStore::NotifyInitializationCompleted() {
  for (Observer& observer : observers_)
    observer.OnInitializationCompleted(true);
}

void ValueMapPrefStore::RemoveValuesByPrefixSilently(
    const std::string& prefix) {
  prefs_.ClearWithPrefix(prefix);
}
