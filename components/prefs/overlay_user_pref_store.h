// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_OVERLAY_USER_PREF_STORE_H_
#define COMPONENTS_PREFS_OVERLAY_USER_PREF_STORE_H_

#include <stdint.h>

#include <map>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/prefs_export.h"

// PersistentPrefStore that directs all write operations into an in-memory
// PrefValueMap. Read operations are first answered by the PrefValueMap.
// If the PrefValueMap does not contain a value for the requested key,
// the look-up is passed on to an underlying PersistentPrefStore
// |persistent_user_pref_store_|.
class COMPONENTS_PREFS_EXPORT OverlayUserPrefStore
    : public PersistentPrefStore {
 public:
  explicit OverlayUserPrefStore(PersistentPrefStore* persistent);
  // The |ephemeral| store must already be initialized.
  OverlayUserPrefStore(PersistentPrefStore* ephemeral,
                       PersistentPrefStore* persistent);

  OverlayUserPrefStore(const OverlayUserPrefStore&) = delete;
  OverlayUserPrefStore& operator=(const OverlayUserPrefStore&) = delete;

  // Returns true if a value has been set for the |key| in this
  // OverlayUserPrefStore, i.e. if it potentially overrides a value
  // from the |persistent_user_pref_store_|.
  virtual bool IsSetInOverlay(std::string_view key) const;

  // Methods of PrefStore.
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;

  // Methods of PersistentPrefStore.
  bool GetMutableValue(std::string_view key, base::Value** result) override;
  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(std::string_view key, uint32_t flags) override;
  void RemoveValuesByPrefixSilently(std::string_view prefix) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* delegate) override;
  void CommitPendingWrite(base::OnceClosure reply_callback,
                          base::OnceClosure synchronous_done_callback) override;
  void SchedulePendingLossyWrites() override;
  void ReportValueChanged(std::string_view key, uint32_t flags) override;

  // Registers preferences that should be stored in the persistent preferences
  // (|persistent_user_pref_store_|).
  void RegisterPersistentPref(std::string_view key);

  void OnStoreDeletionFromDisk() override;
  bool HasReadErrorDelegate() const override;

 protected:
  ~OverlayUserPrefStore() override;

 private:
  class ObserverAdapter;

  void OnPrefValueChanged(bool ephemeral, std::string_view key);
  void OnInitializationCompleted(bool ephemeral, bool succeeded);

  // Returns true if |key| corresponds to a preference that shall be stored in
  // persistent PrefStore.
  bool ShallBeStoredInPersistent(std::string_view key) const;

  base::ObserverList<PrefStore::Observer, true> observers_;
  std::unique_ptr<ObserverAdapter> ephemeral_pref_store_observer_;
  std::unique_ptr<ObserverAdapter> persistent_pref_store_observer_;
  scoped_refptr<PersistentPrefStore> ephemeral_user_pref_store_;
  scoped_refptr<PersistentPrefStore> persistent_user_pref_store_;
  PrefNameSet persistent_names_set_;
};

#endif  // COMPONENTS_PREFS_OVERLAY_USER_PREF_STORE_H_
