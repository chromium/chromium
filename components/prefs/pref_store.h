// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_STORE_H_
#define COMPONENTS_PREFS_PREF_STORE_H_

#include <memory>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/prefs/prefs_export.h"

// This is an abstract interface for reading and writing from/to a persistent
// preference store, used by PrefService. An implementation using a JSON file
// can be found in JsonPrefStore, while an implementation without any backing
// store for testing can be found in TestingPrefStore. Furthermore, there is
// CommandLinePrefStore, which bridges command line options to preferences and
// ConfigurationPolicyPrefStore, which is used for hooking up configuration
// policy with the preference subsystem.
class COMPONENTS_PREFS_EXPORT PrefStore : public base::RefCounted<PrefStore> {
 public:
  // Observer interface for monitoring PrefStore.
  class COMPONENTS_PREFS_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when the value for the given `key` in the store changes.
    virtual void OnPrefValueChanged(std::string_view key) {}
    // Notification about the PrefStore being fully initialized.
    virtual void OnInitializationCompleted(bool succeeded) {}
  };

  PrefStore() = default;

  PrefStore(const PrefStore&) = delete;
  PrefStore& operator=(const PrefStore&) = delete;

  // Add and remove observers.
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool HasObservers() const;

  // Whether the store has completed all asynchronous initialization.
  virtual bool IsInitializationComplete() const;

  // Get the value for a given preference `key` and stores it in `*result`.
  // `*result` is only modified if the return value is true and if `result`
  // is not NULL. Ownership of the `*result` value remains with the PrefStore.
  virtual bool GetValue(std::string_view key,
                        const base::Value** result) const = 0;

  // Get all the values.
  virtual base::Value::Dict GetValues() const = 0;

 protected:
  friend class base::RefCounted<PrefStore>;
  virtual ~PrefStore() = default;
};

#endif  // COMPONENTS_PREFS_PREF_STORE_H_
