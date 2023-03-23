// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/dual_layer_user_pref_store.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

DualLayerUserPrefStore::UnderlyingPrefStoreObserver::
    UnderlyingPrefStoreObserver(DualLayerUserPrefStore* outer,
                                bool is_account_store)
    : outer_(outer), is_account_store_(is_account_store) {
  DCHECK(outer_);
}

void DualLayerUserPrefStore::UnderlyingPrefStoreObserver::OnPrefValueChanged(
    const std::string& key) {
  // TODO(crbug.com/1416477): Directly accessing `outer_`'s private members is
  // icky - consider avoiding this, e.g. by passing in callbacks instead.

  // Ignore this notification if it originated from the outer store - in that
  // case, `DualLayerUserPrefStore` itself will send notifications as
  // appropriate. This avoids dual notifications even though there are dual
  // writes.
  if (outer_->is_setting_prefs_) {
    return;
  }
  // Otherwise: This must've been a write directly to the underlying store, so
  // notify any observers.
  // Note: Observers should only be notified if the effective value of a pref
  // changes - i.e. not if a pref gets modified in the local store which also
  // has a value in the account store.
  // TODO(crbug.com/1416479): Update the logic for mergeable prefs, since for
  // those, a change in the local store should generally lead to a change in the
  // effective value.
  if (!is_account_store_ &&
      outer_->GetAccountPrefStore()->GetValue(key, nullptr)) {
    return;
  }

  for (PrefStore::Observer& observer : outer_->observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void DualLayerUserPrefStore::UnderlyingPrefStoreObserver::
    OnInitializationCompleted(bool succeeded) {
  // The account store starts out already initialized, and should never send
  // OnInitializationCompleted() notifications.
  DCHECK(!is_account_store_);
  if (outer_->IsInitializationComplete()) {
    for (auto& observer : outer_->observers_) {
      observer.OnInitializationCompleted(succeeded);
    }
  }
}

DualLayerUserPrefStore::DualLayerUserPrefStore(
    scoped_refptr<PersistentPrefStore> local_pref_store,
    const SyncablePrefsDatabase* syncable_prefs_database)
    : local_pref_store_(std::move(local_pref_store)),
      account_pref_store_(base::MakeRefCounted<ValueMapPrefStore>()),
      local_pref_store_observer_(this, /*is_account_store=*/false),
      account_pref_store_observer_(this, /*is_account_store=*/true),
      syncable_prefs_database_(syncable_prefs_database) {
  local_pref_store_->AddObserver(&local_pref_store_observer_);
  account_pref_store_->AddObserver(&account_pref_store_observer_);
}

DualLayerUserPrefStore::~DualLayerUserPrefStore() {
  account_pref_store_->RemoveObserver(&account_pref_store_observer_);
  local_pref_store_->RemoveObserver(&local_pref_store_observer_);
}

scoped_refptr<PersistentPrefStore> DualLayerUserPrefStore::GetLocalPrefStore() {
  return local_pref_store_;
}

scoped_refptr<WriteablePrefStore>
DualLayerUserPrefStore::GetAccountPrefStore() {
  return account_pref_store_;
}

void DualLayerUserPrefStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DualLayerUserPrefStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DualLayerUserPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool DualLayerUserPrefStore::IsInitializationComplete() const {
  // `account_pref_store_` (a ValueMapPrefStore) is always initialized.
  DCHECK(account_pref_store_->IsInitializationComplete());
  return local_pref_store_->IsInitializationComplete();
}

bool DualLayerUserPrefStore::GetValue(base::StringPiece key,
                                      const base::Value** result) const {
  if (!IsPrefKeySyncable(std::string(key))) {
    return local_pref_store_->GetValue(key, result);
  }

  const base::Value* account_value = nullptr;
  account_pref_store_->GetValue(key, &account_value);
  if (account_value) {
    // TODO(crbug.com/1416479): Implement merging (where does the result go?)
    if (result) {
      *result = account_value;
    }
    return true;
  }

  const base::Value* local_value = nullptr;
  local_pref_store_->GetValue(key, &local_value);
  if (local_value) {
    if (result) {
      *result = local_value;
    }
    return true;
  }
  return false;
}

base::Value::Dict DualLayerUserPrefStore::GetValues() const {
  base::Value::Dict values = local_pref_store_->GetValues();
  for (auto [pref_name, value] : account_pref_store_->GetValues()) {
    // TODO(crbug.com/1416479): Implement merging.
    values.SetByDottedPath(pref_name, std::move(value));
  }
  return values;
}

void DualLayerUserPrefStore::SetValue(const std::string& key,
                                      base::Value value,
                                      uint32_t flags) {
  const base::Value* initial_value = nullptr;
  // Only notify if something actually changed.
  // Note: `value` is still added to the stores in case `key` was missing from
  // any or had a different value.
  bool should_notify =
      !GetValue(key, &initial_value) || (*initial_value != value);
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    // TODO(crbug.com/1416479): Implement un-merging, i.e. split updates and
    // write partially to both stores.
    if (IsPrefKeySyncable(key)) {
      account_pref_store_->SetValue(key, value.Clone(), flags);
    }
    local_pref_store_->SetValue(key, std::move(value), flags);
  }

  if (should_notify) {
    for (PrefStore::Observer& observer : observers_) {
      observer.OnPrefValueChanged(key);
    }
  }
}

void DualLayerUserPrefStore::RemoveValue(const std::string& key,
                                         uint32_t flags) {
  // Only proceed if the pref exists.
  if (!GetValue(key, nullptr)) {
    return;
  }

  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    local_pref_store_->RemoveValue(key, flags);
    if (IsPrefKeySyncable(key)) {
      account_pref_store_->RemoveValue(key, flags);
    }
  }

  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

bool DualLayerUserPrefStore::GetMutableValue(const std::string& key,
                                             base::Value** result) {
  if (!IsPrefKeySyncable(key)) {
    return local_pref_store_->GetMutableValue(key, result);
  }

  const base::Value* local_value = nullptr;
  local_pref_store_->GetValue(key, &local_value);
  base::Value* account_value = nullptr;
  account_pref_store_->GetMutableValue(key, &account_value);

  if (!account_value && !local_value) {
    return false;
  }

  if (!account_value) {
    // Only the local value exists - copy it over to the account store.
    DCHECK(local_value);
    account_pref_store_->SetValueSilently(key, local_value->Clone(),
                                          /*flags=*/0);
    account_pref_store_->GetMutableValue(key, &account_value);
  }
  // TODO(crbug.com/1416479): If both exist, merge if necessary.
  DCHECK(account_value);
  if (result) {
    // Note: Any changes to the returned Value will only directly take effect
    // in the account store. However, callers of this method are required to
    // call ReportValueChanged() once they're done modifying it, and that copies
    // the new value over into the local store too.
    *result = account_value;
  }
  return true;
}

void DualLayerUserPrefStore::ReportValueChanged(const std::string& key,
                                                uint32_t flags) {
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    if (IsPrefKeySyncable(key)) {
      // GetMutableValue() handed out a pointer to the account store value.
      // Copy the new value over to the local store.
      const base::Value* new_value = nullptr;
      if (account_pref_store_->GetValue(key, &new_value)) {
        local_pref_store_->SetValueSilently(key, new_value->Clone(),
                                            /*flags=*/0);
      } else {
        local_pref_store_->RemoveValuesByPrefixSilently(key);
      }
    }
    // Forward the ReportValueChanged() call to the underlying stores, so they
    // can notify their own observers.
    local_pref_store_->ReportValueChanged(key, flags);
    if (IsPrefKeySyncable(key)) {
      account_pref_store_->ReportValueChanged(key, flags);
    }
  }

  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void DualLayerUserPrefStore::SetValueSilently(const std::string& key,
                                              base::Value value,
                                              uint32_t flags) {
  if (IsPrefKeySyncable(key)) {
    account_pref_store_->SetValueSilently(key, value.Clone(), flags);
  }
  local_pref_store_->SetValueSilently(key, std::move(value), flags);
}

void DualLayerUserPrefStore::RemoveValuesByPrefixSilently(
    const std::string& prefix) {
  local_pref_store_->RemoveValuesByPrefixSilently(prefix);
  // Note: There's no good way to check for syncability of the prefix, but
  // silently removing some values that don't exist in the first place is
  // harmless.
  account_pref_store_->RemoveValuesByPrefixSilently(prefix);
}

bool DualLayerUserPrefStore::ReadOnly() const {
  // `account_pref_store_` (a ValueMapPrefStore) can't be read-only.
  return local_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError DualLayerUserPrefStore::GetReadError()
    const {
  // `account_pref_store_` (a ValueMapPrefStore) can't have read errors.
  return local_pref_store_->GetReadError();
}

PersistentPrefStore::PrefReadError DualLayerUserPrefStore::ReadPrefs() {
  // `account_pref_store_` (a ValueMapPrefStore) doesn't explicitly read prefs.
  return local_pref_store_->ReadPrefs();
}

void DualLayerUserPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  // `account_pref_store_` (a ValueMapPrefStore) doesn't explicitly read prefs.
  local_pref_store_->ReadPrefsAsync(error_delegate);
}

void DualLayerUserPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // `account_pref_store_` (a ValueMapPrefStore) doesn't need to commit.
  local_pref_store_->CommitPendingWrite(std::move(reply_callback),
                                        std::move(synchronous_done_callback));
}

void DualLayerUserPrefStore::SchedulePendingLossyWrites() {
  // `account_pref_store_` (a ValueMapPrefStore) doesn't schedule writes.
  local_pref_store_->SchedulePendingLossyWrites();
}

void DualLayerUserPrefStore::OnStoreDeletionFromDisk() {
  local_pref_store_->OnStoreDeletionFromDisk();
}

bool DualLayerUserPrefStore::IsPrefKeySyncable(const std::string& key) const {
  if (!syncable_prefs_database_) {
    // Safer this way.
    return false;
  }
  auto metadata = syncable_prefs_database_->GetSyncablePrefMetadata(key);
  return metadata.has_value() && active_types_.count(metadata->model_type());
}

void DualLayerUserPrefStore::EnableType(syncer::ModelType model_type) {
  CHECK(model_type == syncer::PREFERENCES ||
        model_type == syncer::PRIORITY_PREFERENCES
#if BUILDFLAG(IS_CHROMEOS)
        || model_type == syncer::OS_PREFERENCES ||
        model_type == syncer::OS_PRIORITY_PREFERENCES
#endif
  );
  active_types_.insert(model_type);
}

void DualLayerUserPrefStore::DisableTypeAndClearAccountStore(
    syncer::ModelType model_type) {
  CHECK(model_type == syncer::PREFERENCES ||
        model_type == syncer::PRIORITY_PREFERENCES
#if BUILDFLAG(IS_CHROMEOS)
        || model_type == syncer::OS_PREFERENCES ||
        model_type == syncer::OS_PRIORITY_PREFERENCES
#endif
  );
  active_types_.erase(model_type);

  if (!syncable_prefs_database_) {
    // No pref is treated as syncable in this case. No need to clear the account
    // store.
    return;
  }

  // Clear all synced preferences from the account store.
  for (auto [pref_name, pref_value] : account_pref_store_->GetValues()) {
    if (!IsPrefKeySyncable(pref_name)) {
      // The write flags only affect persistence, and the account store is in
      // memory only.
      account_pref_store_->RemoveValue(
          pref_name, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

}  // namespace sync_preferences
