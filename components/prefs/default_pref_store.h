// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_DEFAULT_PREF_STORE_H_
#define COMPONENTS_PREFS_DEFAULT_PREF_STORE_H_

#include <memory>
#include <string_view>

#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/prefs_export.h"

// Used within a PrefRegistry to keep track of default preference values.
class COMPONENTS_PREFS_EXPORT DefaultPrefStore : public PrefStore {
 public:
  typedef PrefValueMap::const_iterator const_iterator;

  DefaultPrefStore();

  DefaultPrefStore(const DefaultPrefStore&) = delete;
  DefaultPrefStore& operator=(const DefaultPrefStore&) = delete;

  // PrefStore implementation:
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;

  // Sets a `value` for `key`. Should only be called if a value has not been
  // set yet; otherwise call ReplaceDefaultValue().
  void SetDefaultValue(std::string_view key, base::Value value);

  // Replaces the the value for `key` with a new value. Should only be called
  // if a value has already been set; otherwise call SetDefaultValue().
  void ReplaceDefaultValue(std::string_view key, base::Value value);

  const_iterator begin() const;
  const_iterator end() const;

 private:
  ~DefaultPrefStore() override;

  PrefValueMap prefs_;

  base::ObserverList<PrefStore::Observer, true> observers_;
};

#endif  // COMPONENTS_PREFS_DEFAULT_PREF_STORE_H_
