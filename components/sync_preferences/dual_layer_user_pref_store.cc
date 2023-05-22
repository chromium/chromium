// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/dual_layer_user_pref_store.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/preferences_merge_helper.h"
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
  // changes.
  // Note: The effective value will not change if this is a write to the local
  // store, but the account store has a value that overrides it.
  if (!is_account_store_ &&
      outer_->GetAccountPrefStore()->GetValue(key, nullptr) &&
      !outer_->IsPrefKeyMergeable(key)) {
    return;
  }

  for (PrefStore::Observer& observer : outer_->observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void DualLayerUserPrefStore::UnderlyingPrefStoreObserver::
    OnInitializationCompleted(bool succeeded) {
  initialization_succeeded_ = succeeded;

  // Notify observers only after all underlying PrefStores are initialized.
  if (!outer_->IsInitializationComplete()) {
    return;
  }

  // Forward error if any of the underlying store reported error upon
  // ReadPrefsAsync().
  if (outer_->read_error_delegate_) {
    if (auto read_error = outer_->GetReadError();
        read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
      outer_->read_error_delegate_->OnError(read_error);
    }
  }

  for (auto& observer : outer_->observers_) {
    observer.OnInitializationCompleted(outer_->IsInitializationSuccessful());
  }
}

bool DualLayerUserPrefStore::UnderlyingPrefStoreObserver::
    initialization_succeeded() const {
  CHECK(outer_->IsInitializationComplete());
  return initialization_succeeded_;
}

DualLayerUserPrefStore::DualLayerUserPrefStore(
    scoped_refptr<PersistentPrefStore> local_pref_store,
    scoped_refptr<PersistentPrefStore> account_pref_store,
    const PrefModelAssociatorClient* pref_model_associator_client)
    : local_pref_store_(std::move(local_pref_store)),
      account_pref_store_(std::move(account_pref_store)),
      local_pref_store_observer_(this, /*is_account_store=*/false),
      account_pref_store_observer_(this, /*is_account_store=*/true),
      pref_model_associator_client_(pref_model_associator_client) {
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
  return local_pref_store_->IsInitializationComplete() &&
         account_pref_store_->IsInitializationComplete();
}

bool DualLayerUserPrefStore::GetValue(base::StringPiece key,
                                      const base::Value** result) const {
  // Use any existing value from the account store, independent of
  // IsPrefKeySyncable(). This is because IsPrefKeySyncable() only becomes
  // true after the Sync machinery is initialized, but account-store values
  // should apply even before that.

  const base::Value* account_value = nullptr;
  const base::Value* local_value = nullptr;

  account_pref_store_->GetValue(key, &account_value);
  local_pref_store_->GetValue(key, &local_value);

  if (!account_value && !local_value) {
    // Pref doesn't exist.
    return false;
  }

  if (result) {
    // Merge pref if `key` exists in both the stores.
    if (account_value && local_value) {
      *result = MaybeMerge(std::string(key), *local_value, *account_value);
      CHECK(*result);
    } else if (account_value) {
      *result = account_value;
    } else {
      *result = local_value;
    }
  }
  return true;
}

base::Value::Dict DualLayerUserPrefStore::GetValues() const {
  base::Value::Dict values = local_pref_store_->GetValues();

  for (const std::string& pref_name : GetPrefNamesInAccountStore()) {
    const base::Value* value = nullptr;
    // GetValue() will merge the value if needed.
    GetValue(pref_name, &value);
    CHECK(value);
    values.SetByDottedPath(pref_name, value->Clone());
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
    if (IsPrefKeySyncable(key)) {
      if (IsPrefKeyMergeable(key)) {
        auto [new_local_value, new_account_value] =
            UnmergeValue(key, std::move(value), flags);
        account_pref_store_->SetValue(key, std::move(new_account_value), flags);
        local_pref_store_->SetValue(key, std::move(new_local_value), flags);
      } else {
        account_pref_store_->SetValue(key, value.Clone(), flags);
        local_pref_store_->SetValue(key, std::move(value), flags);
      }
    } else {
      local_pref_store_->SetValue(key, std::move(value), flags);
      // Try removing it from account store for the case where sync machinery
      // has not loaded yet and the type has not been enabled.
      account_pref_store_->RemoveValue(key, flags);
    }
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
    account_pref_store_->RemoveValue(key, flags);
  }

  // Remove from the list of merge prefs if exists.
  merged_prefs_.RemoveValue(key);

  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

bool DualLayerUserPrefStore::GetMutableValue(const std::string& key,
                                             base::Value** result) {
  // Use any existing value from the account store, independent of
  // IsPrefKeySyncable(). This is because IsPrefKeySyncable() only becomes
  // true after the Sync machinery is initialized, but account-store values
  // should apply even before that.

  base::Value* local_value = nullptr;
  local_pref_store_->GetMutableValue(key, &local_value);
  base::Value* account_value = nullptr;
  account_pref_store_->GetMutableValue(key, &account_value);

  if (!account_value && !local_value) {
    // Pref doesn't exist.
    return false;
  }
  if (result) {
    // Note: Any changes to the returned Value will only directly take effect
    // in the underlying store. However, callers of this method are required to
    // call ReportValueChanged() once they're done modifying it, and that
    // propagates the change to all the underlying stores.

    // If pref exists it both stores, create a merged pref.
    if (account_value && local_value) {
      *result = MaybeMerge(key, *local_value, *account_value);
      CHECK(*result);
    } else if (account_value) {
      *result = account_value;
    } else {
      *result = local_value;
    }
  }
  return true;
}

void DualLayerUserPrefStore::ReportValueChanged(const std::string& key,
                                                uint32_t flags) {
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    if (IsPrefKeySyncable(key)) {
      const base::Value* new_value = nullptr;
      // In case a merged value was updated, it would exist in `merged_prefs_`.
      // Else, get the new value from whichever store has it and copy it to the
      // other one.
      if (merged_prefs_.GetValue(key, &new_value)) {
        auto [new_local_value, new_account_value] =
            UnmergeValue(key, new_value->Clone(), flags);
        account_pref_store_->SetValueSilently(key, std::move(new_account_value),
                                              flags);
        local_pref_store_->SetValueSilently(key, std::move(new_local_value),
                                            flags);
      } else if (account_pref_store_->GetValue(key, &new_value)) {
        local_pref_store_->SetValueSilently(key, new_value->Clone(), flags);
      } else if (local_pref_store_->GetValue(key, &new_value)) {
        account_pref_store_->SetValueSilently(key, new_value->Clone(), flags);
      }
      // It is possible that the pref just doesn't exist (anymore).
    } else {
      // Try removing it from account store for the case where sync machinery
      // has not loaded yet and the type has not been enabled.
      account_pref_store_->RemoveValue(key, flags);
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
    if (IsPrefKeyMergeable(key)) {
      auto [new_local_value, new_account_value] =
          UnmergeValue(key, std::move(value), flags);
      account_pref_store_->SetValueSilently(key, std::move(new_account_value),
                                            flags);
      local_pref_store_->SetValueSilently(key, std::move(new_local_value),
                                          flags);
    } else {
      account_pref_store_->SetValueSilently(key, value.Clone(), flags);
      local_pref_store_->SetValueSilently(key, std::move(value), flags);
    }
  } else {
    local_pref_store_->SetValueSilently(key, std::move(value), flags);
    {
      base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
      // Try removing it from account store for the case where sync machinery
      // has not loaded yet and the type has not been enabled.
      account_pref_store_->RemoveValue(key, flags);
    }
  }
}

void DualLayerUserPrefStore::RemoveValuesByPrefixSilently(
    const std::string& prefix) {
  local_pref_store_->RemoveValuesByPrefixSilently(prefix);
  // Note: There's no good way to check for syncability of the prefix, but
  // silently removing some values that don't exist in the first place is
  // harmless.
  account_pref_store_->RemoveValuesByPrefixSilently(prefix);

  // Remove from the list of merged prefs if exists.
  merged_prefs_.ClearWithPrefix(prefix);
}

bool DualLayerUserPrefStore::ReadOnly() const {
  return local_pref_store_->ReadOnly() || account_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError DualLayerUserPrefStore::GetReadError()
    const {
  if (auto local_prefs_read_error = local_pref_store_->GetReadError();
      local_prefs_read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    return local_prefs_read_error;
  }
  return account_pref_store_->GetReadError();
}

PersistentPrefStore::PrefReadError DualLayerUserPrefStore::ReadPrefs() {
  // Call ReadPrefs() on both stores before reporting error.
  auto local_prefs_read_error = local_pref_store_->ReadPrefs();
  auto account_prefs_read_error = account_pref_store_->ReadPrefs();

  if (local_prefs_read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    return local_prefs_read_error;
  }
  return account_prefs_read_error;
}

void DualLayerUserPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  // The store is expected to take ownership of `error_delegate`, thus it's not
  // valid to forward the same to the two underlying stores. Instead, if any
  // error occurs, it's reported in OnInitializationCompleted() handle.
  read_error_delegate_.reset(error_delegate);
  local_pref_store_->ReadPrefsAsync(nullptr);
  account_pref_store_->ReadPrefsAsync(nullptr);
}

void DualLayerUserPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // A BarrierClosure will run its callback wherever the last instance of the
  // returned wrapper is invoked. As such it is guaranteed to respect the reply
  // vs synchronous semantics assuming `local_pref_store_` and
  // `account_pref_store_` honor it.

  static constexpr int kNumStores = 2;

  base::RepeatingClosure reply_callback_wrapper =
      reply_callback
          ? base::BarrierClosure(kNumStores, std::move(reply_callback))
          : base::RepeatingClosure();

  base::RepeatingClosure synchronous_callback_wrapper =
      synchronous_done_callback
          ? base::BarrierClosure(kNumStores,
                                 std::move(synchronous_done_callback))
          : base::RepeatingClosure();

  local_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                        synchronous_callback_wrapper);
  account_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                          synchronous_callback_wrapper);
}

void DualLayerUserPrefStore::SchedulePendingLossyWrites() {
  local_pref_store_->SchedulePendingLossyWrites();
  account_pref_store_->SchedulePendingLossyWrites();
}

void DualLayerUserPrefStore::OnStoreDeletionFromDisk() {
  local_pref_store_->OnStoreDeletionFromDisk();
  account_pref_store_->OnStoreDeletionFromDisk();
}

bool DualLayerUserPrefStore::IsPrefKeySyncable(const std::string& key) const {
  if (!pref_model_associator_client_) {
    // Safer this way.
    return false;
  }
  auto metadata = pref_model_associator_client_->GetSyncablePrefsDatabase()
                      .GetSyncablePrefMetadata(key);
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

  if (!pref_model_associator_client_) {
    // No pref is treated as syncable in this case. No need to clear the account
    // store.
    return;
  }

  // Clear all synced preferences from the account store.
  for (const std::string& pref_name : GetPrefNamesInAccountStore()) {
    if (!IsPrefKeySyncable(pref_name)) {
      // The write flags only affect persistence, and the default flag is the
      // safer choice.
      account_pref_store_->RemoveValue(
          pref_name, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }

  CHECK(!active_types_.empty() || account_pref_store_->GetValues().empty());
}

bool DualLayerUserPrefStore::IsPrefKeyMergeable(const std::string& key) const {
  if (!pref_model_associator_client_) {
    return false;
  }
  // TODO(crbug.com/1416479): Also cover prefs with custom merge logic.
  return pref_model_associator_client_->IsMergeableListPreference(key) ||
         pref_model_associator_client_->IsMergeableDictionaryPreference(key);
}

const base::Value* DualLayerUserPrefStore::MaybeMerge(
    const std::string& pref_name,
    const base::Value& local_value,
    const base::Value& account_value) const {
  // Note: The merged value is evaluated every time and not re-used from
  // `merged_prefs_`. This is to:
  // 1. Handle the cases where SetValueSilently() or
  // RemoveValueByPrefixSilently() is called on the underlying stores directly,
  // without a corresponding call to ReportValueChanged().
  // 2. Avoid removing the entry from `merged_prefs_` every time pref is
  // updated.
  base::Value merged_value = helper::MergePreference(
      pref_model_associator_client_, pref_name, local_value, account_value);

  if (merged_value == account_value) {
    // Most likely this is not a mergeable pref. Should be safe to just return
    // the account value.
    // This check is workaround as there doesn't exist a reliable way to check
    // if a pref is mergeable.
    // TODO(crbug.com/1416479): Use IsPrefKeyMergeable() instead once it covers
    // custom prefs with custom merge logic.
    return &account_value;
  }
  // Now it is definitely a mergeable pref.

  // Add to `merged_prefs_` only if value doesn't already exist. This is done
  // because the previously returned value might be in use and replacing the
  // value would be risky - multiple successive calls to the getter shouldn't
  // invalidate previous results.
  if (base::Value* original_value = nullptr;
      !merged_prefs_.GetValue(pref_name, &original_value) ||
      *original_value != merged_value) {
    merged_prefs_.SetValue(pref_name, std::move(merged_value));
  }

  const base::Value* merged_pref = nullptr;
  merged_prefs_.GetValue(pref_name, &merged_pref);
  DCHECK(merged_pref);
  return merged_pref;
}

base::Value* DualLayerUserPrefStore::MaybeMerge(const std::string& pref_name,
                                                base::Value& local_value,
                                                base::Value& account_value) {
  // Doing const_cast should be safe as ultimately the value being pointed to is
  // a non-const object.
  return const_cast<base::Value*>(
      std::as_const(*this).MaybeMerge(pref_name, local_value, account_value));
}

std::pair<base::Value, base::Value> DualLayerUserPrefStore::UnmergeValue(
    const std::string& pref_name,
    base::Value value,
    uint32_t flags) const {
  DCHECK(IsPrefKeySyncable(pref_name));

  // Note: There is no "standard" unmerging logic for list or scalar prefs.
  // TODO(crbug.com/1416479): Allow support for custom unmerge logic.
  if (pref_model_associator_client_->IsMergeableDictionaryPreference(
          pref_name)) {
    // Per crbug.com/1430854, it is possible for the value to not be of dict
    // type. However, in this case, whatever is the type of `value` it's bound
    // to be correct, as UnmergeValue() is called by setters which in turn are
    // only called after a type check.
    if (value.is_dict()) {
      base::Value::Dict local_dict;
      if (const base::Value* local_dict_value = nullptr;
          local_pref_store_->GetValue(pref_name, &local_dict_value)) {
        // It is assumed that the local store cannot contain value of incorrect
        // type.
        local_dict = local_dict_value->GetDict().Clone();
      }
      base::Value::Dict account_dict;
      if (const base::Value* account_dict_value = nullptr;
          account_pref_store_->GetValue(pref_name, &account_dict_value)) {
        // It is assumed that the account store cannot contain value of
        // incorrect type.
        account_dict = account_dict_value->GetDict().Clone();
      }
      auto [new_local_dict, new_account_dict] = helper::UnmergeDictionaryValues(
          std::move(value).TakeDict(), local_dict, account_dict);
      // Note: This would still return an empty dict even if the pref didn't
      // exist in either of the stores. This should however be okay since no
      // actual pref value is leaked to the other.
      return {base::Value(std::move(new_local_dict)),
              base::Value(std::move(new_account_dict))};
    } else {
      DLOG(ERROR) << pref_name
                  << " marked as a mergeable dict pref but is of type "
                  << value.type();
    }
  }

  // Directly pass the new value as both the local value and the account value
  // for prefs with no specific merge logic.
  base::Value new_account_value(value.Clone());
  base::Value new_local_value(std::move(value));
  return {std::move(new_local_value), std::move(new_account_value)};
}

bool DualLayerUserPrefStore::IsInitializationSuccessful() const {
  return local_pref_store_observer_.initialization_succeeded() &&
         account_pref_store_observer_.initialization_succeeded();
}

std::vector<std::string> DualLayerUserPrefStore::GetPrefNamesInAccountStore()
    const {
  std::vector<std::string> keys;

  if (!pref_model_associator_client_) {
    return keys;
  }

  // GetValues() returns a dict which is set using SetByDottedPaths(). That
  // means, a key "a.b.c" is presented as: `{'a': {'b': {'c': ... }}}`. This
  // util recurses over the nested dicts with keys being joined with a dot, till
  // the string forms a valid pref name, for eg. it will recurse with keys, "a",
  // "a.b", and then "a.b.c" which was the original key.
  auto recurse_and_insert = [&](const std::string& key,
                                const base::Value& value,
                                auto& recurse_and_insert_ref) -> void {
    // Checks if `key` is a pref name using syncable pref database. This is
    // different from IsPrefKeySyncable() which checks whether or not a pref
    // should synced right now based on enabled ModelTypes.
    // TODO(crbug.com/1446256): Consider renaming IsPrefKeySyncable() to
    // ShouldSyncPrefKey() to make intent more clear.
    if (pref_model_associator_client_->GetSyncablePrefsDatabase()
            .IsPreferenceSyncable(key)) {
      keys.push_back(key);
    } else if (value.is_dict()) {
      for (auto [k, v] : value.GetDict()) {
        recurse_and_insert_ref(key + "." + k, v, recurse_and_insert_ref);
      }
    }
  };

  for (auto [key, value] : account_pref_store_->GetValues()) {
    recurse_and_insert(key, value, recurse_and_insert);
  }

  return keys;
}

base::flat_set<syncer::ModelType>
DualLayerUserPrefStore::GetActiveTypesForTest() const {
  return active_types_;
}

}  // namespace sync_preferences
