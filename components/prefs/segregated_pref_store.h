// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_SEGREGATED_PREF_STORE_H_
#define COMPONENTS_PREFS_SEGREGATED_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/prefs_export.h"

// Provides a unified PersistentPrefStore implementation that splits its storage
// and retrieval between two underlying PersistentPrefStore instances: a set of
// preference names is used to partition the preferences.
//
// Combines properties of the two stores as follows:
//   * The unified read error will be:
//                           Selected Store Error
//    Default Store Error | NO_ERROR      | NO_FILE       | other selected |
//               NO_ERROR | NO_ERROR      | NO_ERROR      | other selected |
//               NO_FILE  | NO_FILE       | NO_FILE       | NO_FILE        |
//          other default | other default | other default | other default  |
//   * The unified initialization success, initialization completion, and
//     read-only state are the boolean OR of the underlying stores' properties.
class COMPONENTS_PREFS_EXPORT SegregatedPrefStore : public PersistentPrefStore {
 public:
  // Creates an instance that delegates to |selected_pref_store| for the
  // preferences named in |selected_pref_names| and to |default_pref_store|
  // for all others. If an unselected preference is present in
  // |selected_pref_store| (i.e. because it was previously selected) it will
  // be migrated back to |default_pref_store| upon access via a non-const
  // method.
  SegregatedPrefStore(scoped_refptr<PersistentPrefStore> default_pref_store,
                      scoped_refptr<PersistentPrefStore> selected_pref_store,
                      PrefNameSet selected_pref_names);

  SegregatedPrefStore(const SegregatedPrefStore&) = delete;
  SegregatedPrefStore& operator=(const SegregatedPrefStore&) = delete;

  // PrefStore implementation
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;

  // WriteablePrefStore implementation
  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override;
  void RemoveValue(std::string_view key, uint32_t flags) override;
  void RemoveValuesByPrefixSilently(std::string_view prefix) override;

  // PersistentPrefStore implementation
  bool GetMutableValue(std::string_view key, base::Value** result) override;
  void ReportValueChanged(std::string_view key, uint32_t flags) override;
  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback =
          base::OnceClosure()) override;
  void SchedulePendingLossyWrites() override;
  void OnStoreDeletionFromDisk() override;
  bool HasReadErrorDelegate() const override;

 protected:
  ~SegregatedPrefStore() override;

 private:
  // Caches event state from the underlying stores and exposes the state to the
  // provided "outer" SegregatedPrefStore to synthesize external events via
  // |read_error_delegate_| and |observers_|.
  class UnderlyingPrefStoreObserver : public PrefStore::Observer {
   public:
    explicit UnderlyingPrefStoreObserver(SegregatedPrefStore* outer);

    UnderlyingPrefStoreObserver(const UnderlyingPrefStoreObserver&) = delete;
    UnderlyingPrefStoreObserver& operator=(const UnderlyingPrefStoreObserver&) =
        delete;

    // PrefStore::Observer implementation
    void OnPrefValueChanged(std::string_view key) override;
    void OnInitializationCompleted(bool succeeded) override;

    bool initialization_succeeded() const { return initialization_succeeded_; }

   private:
    const raw_ptr<SegregatedPrefStore> outer_;
    bool initialization_succeeded_ = false;
  };

  // Returns true only if all underlying PrefStores have initialized
  // successfully, otherwise false.
  bool IsInitializationSuccessful() const;

  // Returns |selected_pref_store| if |key| is selected and
  // |default_pref_store| otherwise.
  PersistentPrefStore* StoreForKey(std::string_view key);
  const PersistentPrefStore* StoreForKey(std::string_view key) const;

  const scoped_refptr<PersistentPrefStore> default_pref_store_;
  const scoped_refptr<PersistentPrefStore> selected_pref_store_;
  const PrefNameSet selected_preference_names_;

  // Optional so we can differentiate `nullopt` from `nullptr`.
  std::optional<std::unique_ptr<PersistentPrefStore::ReadErrorDelegate>>
      read_error_delegate_;
  base::ObserverList<PrefStore::Observer, true> observers_;
  UnderlyingPrefStoreObserver default_observer_;
  UnderlyingPrefStoreObserver selected_observer_;
};

#endif  // COMPONENTS_PREFS_SEGREGATED_PREF_STORE_H_
