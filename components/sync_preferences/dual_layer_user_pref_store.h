// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_
#define COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/sync/base/model_type.h"

namespace sync_preferences {

class PrefModelAssociatorClient;

// A two-layer user PrefStore that combines local preferences (scoped to this
// profile) with account-scoped preferences (scoped to the user's signed-in
// account).
// * Account takes precedence: If a pref has a value in both stores, then
//   typically the account-scoped one takes precedence. However, for some prefs,
//   the two values may be merged.
// * Dual writes: Any changes made to prefs *on this device* are written to both
//   stores. However, incoming changes made on other devices only go into the
//   account store.
class DualLayerUserPrefStore : public PersistentPrefStore {
 public:
  DualLayerUserPrefStore(
      scoped_refptr<PersistentPrefStore> local_pref_store,
      const PrefModelAssociatorClient* pref_model_associator_client);

  DualLayerUserPrefStore(const DualLayerUserPrefStore&) = delete;
  DualLayerUserPrefStore& operator=(const DualLayerUserPrefStore&) = delete;

  // Marks `model_type` as enabled for account storage. This should be called
  // when a data type starts syncing.
  void EnableType(syncer::ModelType model_type);
  // Unmarks `model_type` as enabled for account storage and removes all
  // corresponding preference entries(belonging to this type) from account
  // storage. This should be called when a data type stops syncing.
  void DisableTypeAndClearAccountStore(syncer::ModelType model_type);

  scoped_refptr<PersistentPrefStore> GetLocalPrefStore();
  scoped_refptr<WriteablePrefStore> GetAccountPrefStore();

  // PrefStore implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(base::StringPiece key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;

  // WriteablePrefStore implementation.
  void SetValue(const std::string& key,
                base::Value value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValuesByPrefixSilently(const std::string& prefix) override;

  // PersistentPrefStore implementation.
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

 protected:
  ~DualLayerUserPrefStore() override;

 private:
  // Forwards events from the underlying stores to the provided "outer"
  // `DualLayerUserPrefStore` to synthesize external events via `observers_`.
  class UnderlyingPrefStoreObserver : public PrefStore::Observer {
   public:
    explicit UnderlyingPrefStoreObserver(DualLayerUserPrefStore* outer,
                                         bool is_account_store);

    UnderlyingPrefStoreObserver(const UnderlyingPrefStoreObserver&) = delete;
    UnderlyingPrefStoreObserver& operator=(const UnderlyingPrefStoreObserver&) =
        delete;

    // PrefStore::Observer implementation.
    void OnPrefValueChanged(const std::string& key) override;
    void OnInitializationCompleted(bool succeeded) override;

   private:
    const raw_ptr<DualLayerUserPrefStore> outer_;
    const bool is_account_store_;
  };

  // Returns whether the pref with the given `key` is registered as syncable.
  bool IsPrefKeySyncable(const std::string& key) const;

  // Returns whether the pref with the given `key` is mergeable.
  // TODO(crbug.com/1416479): This does not cover prefs with custom merge logic
  // yet.
  bool IsPrefKeyMergeable(const std::string& key) const;

  // Produces a "merged" view of `account_value` and `local_value`. In case
  // `pref_name` is a mergeable pref, a new merged pref is returned, which is
  // owned by `merged_prefs_`. Else, it returns a pointer to the account value,
  // given that in this case the account value overrides the local value.
  const base::Value* MaybeMerge(const std::string& pref_name,
                                const base::Value& local_value,
                                const base::Value& account_value) const;
  base::Value* MaybeMerge(const std::string& pref_name,
                          base::Value& local_value,
                          base::Value& account_value);

  // Unmerges `value` and returns the new local value and the account value (in
  // that order).
  std::pair<base::Value, base::Value> UnmergeValue(const std::string& pref_name,
                                                   base::Value value,
                                                   uint32_t flags) const;

  // The two underlying pref stores, scoped to this device/profile and to the
  // user's signed-in account, respectively.
  const scoped_refptr<PersistentPrefStore> local_pref_store_;
  const scoped_refptr<ValueMapPrefStore> account_pref_store_;

  // This stores the merged value of a mergeable pref, if required - i.e. if the
  // pref is queried while it exists on both the stores. This is needed to
  // tackle the issue regarding the ownership of the newly created merged values
  // on calls to GetValue().
  // Note: Marked as mutable to allow update by GetValue() method which is
  // const.
  mutable PrefValueMap merged_prefs_;

  // Observers for the two underlying pref stores, used to propagate pref-change
  // notifications the this class's own observers.
  UnderlyingPrefStoreObserver local_pref_store_observer_;
  UnderlyingPrefStoreObserver account_pref_store_observer_;

  // List of preference types currently syncing.
  base::flat_set<syncer::ModelType> active_types_;

  // Set to true while this store is setting prefs in the underlying stores.
  // Used to avoid self-notifications.
  bool is_setting_prefs_ = false;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;

  const raw_ptr<const PrefModelAssociatorClient> pref_model_associator_client_ =
      nullptr;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_
