// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_
#define COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

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
class DualLayerUserPrefStore : public PersistentPrefStore,
                               public syncer::SyncServiceObserver {
 public:
  DualLayerUserPrefStore(
      scoped_refptr<PersistentPrefStore> local_pref_store,
      scoped_refptr<PersistentPrefStore> account_pref_store,
      scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client);

  DualLayerUserPrefStore(const DualLayerUserPrefStore&) = delete;
  DualLayerUserPrefStore& operator=(const DualLayerUserPrefStore&) = delete;

  // Marks `data_type` as enabled for account storage. This should be called
  // when a data type starts syncing.
  void EnableType(syncer::DataType data_type);
  // Unmarks `data_type` as enabled for account storage and removes all
  // corresponding preference entries(belonging to this type) from account
  // storage. This should be called when a data type stops syncing.
  void DisableTypeAndClearAccountStore(syncer::DataType data_type);

  // Sets value for preference `key` only in the account store. This is meant
  // for use by sync components (specifically PrefModelAssociator) to insert
  // value in the account store directly but only leads to notifications for
  // DualLayerUserPrefStore observers if the effective value changes.
  // Note: This does not do any merge/unmerge and does not check whether the
  // pref `key` is syncable.
  // TODO(crbug.com/40277783): Implement a better way to handle this usage by
  // the sync components.
  void SetValueInAccountStoreOnly(std::string_view key,
                                  base::Value value,
                                  uint32_t flags);

  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  scoped_refptr<PersistentPrefStore> GetLocalPrefStore();
  scoped_refptr<WriteablePrefStore> GetAccountPrefStore();

  // PrefStore implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;

  // WriteablePrefStore implementation.
  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override;
  void RemoveValue(std::string_view key, uint32_t flags) override;
  bool GetMutableValue(std::string_view key, base::Value** result) override;
  void ReportValueChanged(std::string_view key, uint32_t flags) override;
  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValuesByPrefixSilently(std::string_view prefix) override;

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
  bool HasReadErrorDelegate() const override;

  // SyncServiceObserver implementation
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // Return the set of active pref types.
  base::flat_set<syncer::DataType> GetActiveTypesForTest() const;

  bool IsHistorySyncEnabledForTest() const;
  void SetIsHistorySyncEnabledForTest(bool is_history_sync_enabled);

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
    void OnPrefValueChanged(std::string_view key) override;
    void OnInitializationCompleted(bool succeeded) override;

    bool initialization_succeeded() const;

   private:
    const raw_ptr<DualLayerUserPrefStore> outer_;
    const bool is_account_store_;
    // Start with `initialization_succeeded_` as true as some persistent
    // stores do not issue OnInitializationCompleted().
    bool initialization_succeeded_ = true;
  };

  bool IsInitializationSuccessful() const;

  // Returns whether the pref with the given `key` should be inserted into the
  // account pref store. Note that the account store keeps in sync with the
  // account.
  bool ShouldSetValueInAccountStore(std::string_view key) const;
  // Returns whether the pref with the given `key` should be queried from the
  // account pref store. Note that the account store keeps in sync with the
  // account.
  bool ShouldGetValueFromAccountStore(std::string_view key) const;

  // Returns whether the pref with the given `key` is mergeable.
  bool IsPrefKeyMergeable(std::string_view key) const;

  // Produces a "merged" view of `account_value` and `local_value`. In case
  // `pref_name` is a mergeable pref, a new merged pref is returned, which is
  // owned by `merged_prefs_`. Else, it returns a pointer to the account value,
  // given that in this case the account value overrides the local value.
  const base::Value* MaybeMerge(std::string_view pref_name,
                                const base::Value& local_value,
                                const base::Value& account_value) const;
  base::Value* MaybeMerge(std::string_view pref_name,
                          base::Value& local_value,
                          base::Value& account_value);

  // Unmerges `value` and returns the new local value and the account value (in
  // that order).
  std::pair<base::Value, base::Value> UnmergeValue(std::string_view pref_name,
                                                   base::Value value,
                                                   uint32_t flags) const;

  // Get all prefs currently present in the account store.
  // Note that this will also return prefs which can not be queried from the
  // account store. For example, this method will return prefs requiring history
  // opt-in even if history sync is disabled. A GetValue() call for such a pref
  // will not query the account store. Thus it is the role of the callers to
  // check the history opt-in.
  std::vector<std::string> GetPrefNamesInAccountStore() const;

  // Returns whether the user has history sync turned on.
  bool IsHistorySyncEnabled() const;

  // The two underlying pref stores, scoped to this device/profile and to the
  // user's signed-in account, respectively.
  const scoped_refptr<PersistentPrefStore> local_pref_store_;
  const scoped_refptr<PersistentPrefStore> account_pref_store_;

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

  // Optional so we can differentiate `nullopt` from `nullptr`.
  std::optional<std::unique_ptr<PersistentPrefStore::ReadErrorDelegate>>
      read_error_delegate_;

  // List of preference types currently syncing.
  base::flat_set<syncer::DataType> active_types_;

  // Set to true while this store is setting prefs in the underlying stores.
  // Used to avoid self-notifications.
  bool is_setting_prefs_ = false;

  bool is_history_sync_enabled_ = false;

  base::ObserverList<PrefStore::Observer, true> observers_;

  const scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client_;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_DUAL_LAYER_USER_PREF_STORE_H_
