// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_WRAP_WITH_PREFIX_PREF_STORE_H_
#define COMPONENTS_PREFS_WRAP_WITH_PREFIX_PREF_STORE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/observer_list.h"
#include "components/prefs/persistent_pref_store.h"

// This is a wrapper over another PersistentPrefStore.
// This can be used to implement a pref store over a dictionary in the
// PersistentPrefStore.
// For example, consider the following JSON being handled by a JsonPrefStore:
// {
//   "foo": "Hello World",
//   "bar": {
//     "foobar": "Goodbye World"
//   }
// }
//
// A WrapWithPrefixPrefStore can help operate on the dict for "bar", directly.
// That is, any query for "foobar" on this store will correspond to a query for
// "bar.foobar" in the JsonPrefStore.
//
// This is achieved by prefixing all the queries with the provided prefix.
//
// This can be used to merge separate pref stores into one single storage under
// separate dictionary items.
//
// NOTE: Users are responsible for ensuring the prefix is not an existing pref.
class COMPONENTS_PREFS_EXPORT WrapWithPrefixPrefStore
    : public PersistentPrefStore,
      public PrefStore::Observer {
 public:
  WrapWithPrefixPrefStore(scoped_refptr<PersistentPrefStore> target_pref_store,
                          std::string_view path_prefix);

  WrapWithPrefixPrefStore(const WrapWithPrefixPrefStore&) = delete;
  WrapWithPrefixPrefStore& operator=(const WrapWithPrefixPrefStore&) = delete;

  // PrefStore implementation.
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore implementation.
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValue(const std::string& key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PersistentPrefStore::PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void SchedulePendingLossyWrites() override;
  void OnStoreDeletionFromDisk() override;
  void RemoveValuesByPrefixSilently(const std::string& prefix) override;
  bool HasReadErrorDelegate() const override;

  // PrefStore::Observer implementation.
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

 protected:
  ~WrapWithPrefixPrefStore() override;

 private:
  std::string AddDottedPrefix(std::string_view path) const;
  std::string_view RemoveDottedPrefix(std::string_view path) const;
  bool HasDottedPrefix(std::string_view path) const;

  scoped_refptr<PersistentPrefStore> target_pref_store_;
  const std::string dotted_prefix_;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;

  // Optional so we can differentiate `nullopt` from `nullptr`.
  std::optional<std::unique_ptr<PersistentPrefStore::ReadErrorDelegate>>
      read_error_delegate_;
};

#endif  // COMPONENTS_PREFS_WRAP_WITH_PREFIX_PREF_STORE_H_
