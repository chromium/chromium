// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/dual_layer_user_pref_store.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
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
    std::string_view key) {
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
  if (outer_->read_error_delegate_.has_value() &&
      outer_->read_error_delegate_.value()) {
    if (auto read_error = outer_->GetReadError();
        read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
      outer_->read_error_delegate_.value()->OnError(read_error);
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
    scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client)
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

bool DualLayerUserPrefStore::GetValue(std::string_view key,
                                      const base::Value** result) const {
  const std::string pref_name(key);
  if (!ShouldGetValueFromAccountStore(pref_name)) {
    return local_pref_store_->GetValue(key, result);
  }

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
      *result = MaybeMerge(pref_name, *local_value, *account_value);
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
    // Filter out prefs which should not be queried from the account store, for
    // example, prefs requiring history opt-in if history sync is off.
    if (ShouldGetValueFromAccountStore(pref_name)) {
      const base::Value* value = nullptr;
      // GetValue() will merge the value if needed.
      GetValue(pref_name, &value);
      CHECK(value);
      values.SetByDottedPath(pref_name, value->Clone());
    }
  }
  return values;
}

void DualLayerUserPrefStore::SetValue(std::string_view key,
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
    if (ShouldSetValueInAccountStore(key)) {
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
    }
  }

  if (should_notify) {
    for (PrefStore::Observer& observer : observers_) {
      observer.OnPrefValueChanged(key);
    }
  }
}

void DualLayerUserPrefStore::RemoveValue(std::string_view key, uint32_t flags) {
  // Only proceed if the pref exists.
  if (!GetValue(key, nullptr)) {
    return;
  }

  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    local_pref_store_->RemoveValue(key, flags);
    if (ShouldSetValueInAccountStore(key)) {
      account_pref_store_->RemoveValue(key, flags);
    }
  }

  // Remove from the list of merge prefs if exists.
  merged_prefs_.RemoveValue(key);

  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

bool DualLayerUserPrefStore::GetMutableValue(std::string_view key,
                                             base::Value** result) {
  if (!ShouldGetValueFromAccountStore(key)) {
    return local_pref_store_->GetMutableValue(key, result);
  }

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

void DualLayerUserPrefStore::ReportValueChanged(std::string_view key,
                                                uint32_t flags) {
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    if (ShouldSetValueInAccountStore(key)) {
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
    }
    // Forward the ReportValueChanged() call to the underlying stores, so they
    // can notify their own observers.
    local_pref_store_->ReportValueChanged(key, flags);
    if (ShouldSetValueInAccountStore(key)) {
      account_pref_store_->ReportValueChanged(key, flags);
    }
  }

  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void DualLayerUserPrefStore::SetValueSilently(std::string_view key,
                                              base::Value value,
                                              uint32_t flags) {
  if (ShouldSetValueInAccountStore(key)) {
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
  }
}

void DualLayerUserPrefStore::RemoveValuesByPrefixSilently(
    std::string_view prefix) {
  local_pref_store_->RemoveValuesByPrefixSilently(prefix);

  // RemoveValuesByPrefixSilently() is not used for the account store since it
  // will remove values which are not being synced yet(for e.g. prefs behind
  // history opt-in). Instead, each pref in the account store is checked and
  // removed if it is writeable right now.
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    // Clear all synced preferences with the prefix from the account store.
    for (const std::string& pref_name : GetPrefNamesInAccountStore()) {
      if (base::StartsWith(pref_name, prefix) &&
          ShouldSetValueInAccountStore(pref_name)) {
        account_pref_store_->RemoveValue(
            pref_name, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }
    }
  }

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
  read_error_delegate_.emplace(error_delegate);
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

bool DualLayerUserPrefStore::ShouldSetValueInAccountStore(
    std::string_view key) const {
  // A preference `key` is added to account store only if it is syncable,  the
  // corresponding pref type is active, and falls under the current user
  // consent, i.e. "privacy-sensitive" prefs require history opt-in.

  // Never write to the account store if it's not read from the account store.
  if (!ShouldGetValueFromAccountStore(key)) {
    return false;
  }
  auto metadata = pref_model_associator_client_->GetSyncablePrefsDatabase()
                      .GetSyncablePrefMetadata(key);
  // Checks if the pref type is active.
  if (!active_types_.contains(metadata->data_type()) &&
      // Checks if the pref already exists in the account store.
      // This is to handle cases where a pref might pre-exist before sync is
      // initialized and the type is marked as active.
      !account_pref_store_->GetValue(key, nullptr)) {
    return false;
  }
  return true;
}

bool DualLayerUserPrefStore::ShouldGetValueFromAccountStore(
    std::string_view key) const {
  // A preference `key` is queried from account store only if it is syncable and
  // falls under the current user consent, i.e. "privacy-sensitive" prefs
  // require history opt-in.
  // Note: There is no check if the pref type is active because they are
  // determined only after the Sync machinery is initialized, but account-store
  // values need to be applied even before that.

  if (!pref_model_associator_client_) {
    // Safer this way.
    return false;
  }
  auto metadata = pref_model_associator_client_->GetSyncablePrefsDatabase()
                      .GetSyncablePrefMetadata(key);
  // Checks if the pref is a syncable pref.
  if (!metadata.has_value()) {
    return false;
  }
  // Checks if the pref requires a history opt-in.
  if (metadata->is_history_opt_in_required() && !IsHistorySyncEnabled()) {
    return false;
  }
  return true;
}

void DualLayerUserPrefStore::EnableType(syncer::DataType data_type) {
  CHECK(data_type == syncer::PREFERENCES ||
        data_type == syncer::PRIORITY_PREFERENCES
#if BUILDFLAG(IS_CHROMEOS)
        || data_type == syncer::OS_PREFERENCES ||
        data_type == syncer::OS_PRIORITY_PREFERENCES
#endif
  );
  active_types_.insert(data_type);
}

void DualLayerUserPrefStore::DisableTypeAndClearAccountStore(
    syncer::DataType data_type) {
  CHECK(data_type == syncer::PREFERENCES ||
        data_type == syncer::PRIORITY_PREFERENCES
#if BUILDFLAG(IS_CHROMEOS)
        || data_type == syncer::OS_PREFERENCES ||
        data_type == syncer::OS_PRIORITY_PREFERENCES
#endif
  );
  active_types_.erase(data_type);

  if (!pref_model_associator_client_) {
    // No pref is treated as syncable in this case. No need to clear the account
    // store.
    return;
  }

  // Clear all synced preferences from the account store.
  for (const std::string& pref_name : GetPrefNamesInAccountStore()) {
    std::optional<SyncablePrefMetadata> metadata =
        pref_model_associator_client_->GetSyncablePrefsDatabase()
            .GetSyncablePrefMetadata(pref_name);
    CHECK(metadata.has_value());
    if (metadata->data_type() != data_type) {
      continue;
    }
    const base::Value* value = nullptr;
    // Should only notify observers if the effective value changes.
    // Note: A notification is still sent if a pref goes from an
    // explicitly-set value to an equal default value.
    // Note: If the pref requires history opt-in, but history sync is
    // disabled, GetValue() will not return the account value, and in case
    // no value for the pref exists in the local store, no notification should
    // be sent out.
    bool should_notify = GetValue(pref_name, &value);
    if (const base::Value* local_value = nullptr;
        value && local_pref_store_->GetValue(pref_name, &local_value)) {
      should_notify = (*local_value != *value);
    }
    {
      base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
      // The write flags only affect persistence, and the default flag is the
      // safer choice.
      account_pref_store_->RemoveValue(
          pref_name, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      merged_prefs_.RemoveValue(pref_name);
    }
    if (should_notify) {
      for (PrefStore::Observer& observer : observers_) {
        observer.OnPrefValueChanged(pref_name);
      }
    }
  }

  if (active_types_.empty()) {
    // Clear the account store of any garbage value without notifications. This
    // can happen if a previously syncable pref was persisted to the account
    // store but is no longer syncable.
    // TODO(crbug.com/40067768): Look into if the garbage values can cleared on
    // browser startup.

    // Since there's no direct way to clear the pref store or get a list of all
    // keys (because of the dotted paths) and `RemoveValuesByPrefixSilently("")`
    // is disallowed, the following workaround is used to clear the store.
    for (auto [key, value] : account_pref_store_->GetValues()) {
      account_pref_store_->RemoveValuesByPrefixSilently(key);
    }
  }
}

bool DualLayerUserPrefStore::IsPrefKeyMergeable(std::string_view key) const {
  if (!pref_model_associator_client_) {
    return false;
  }
  const auto& syncable_prefs_database =
      pref_model_associator_client_->GetSyncablePrefsDatabase();
  return syncable_prefs_database.IsPreferenceSyncable(key) &&
         syncable_prefs_database.IsPreferenceMergeable(key);
}

const base::Value* DualLayerUserPrefStore::MaybeMerge(
    std::string_view pref_name,
    const base::Value& local_value,
    const base::Value& account_value) const {
  // Return the account value if `pref_name` is not mergeable.
  if (!IsPrefKeyMergeable(pref_name)) {
    return &account_value;
  }

  // Note: The merged value is evaluated every time and not re-used from
  // `merged_prefs_`. This is to:
  // 1. Handle the cases where SetValueSilently() or
  // RemoveValueByPrefixSilently() is called on the underlying stores directly,
  // without a corresponding call to ReportValueChanged().
  // 2. Avoid removing the entry from `merged_prefs_` every time pref is
  // updated.
  base::Value merged_value =
      helper::MergePreference(pref_model_associator_client_.get(), pref_name,
                              local_value, account_value);

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

base::Value* DualLayerUserPrefStore::MaybeMerge(std::string_view pref_name,
                                                base::Value& local_value,
                                                base::Value& account_value) {
  // Doing const_cast should be safe as ultimately the value being pointed to is
  // a non-const object.
  return const_cast<base::Value*>(
      std::as_const(*this).MaybeMerge(pref_name, local_value, account_value));
}

std::pair<base::Value, base::Value> DualLayerUserPrefStore::UnmergeValue(
    std::string_view pref_name,
    base::Value value,
    uint32_t flags) const {
  CHECK(ShouldSetValueInAccountStore(pref_name));

  // Note: There is no "standard" unmerging logic for list or scalar prefs.
  // TODO(crbug.com/40256874): Allow support for custom unmerge logic.
  if (pref_model_associator_client_->GetSyncablePrefsDatabase()
          .GetSyncablePrefMetadata(pref_name)
          ->merge_behavior() == MergeBehavior::kMergeableDict) {
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
    // different from ShouldSetValueInAccountStore() which checks whether or not
    // a pref should synced right now based on enabled DataTypes.
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

base::flat_set<syncer::DataType> DualLayerUserPrefStore::GetActiveTypesForTest()
    const {
  return active_types_;
}

bool DualLayerUserPrefStore::IsHistorySyncEnabled() const {
  return is_history_sync_enabled_;
}

bool DualLayerUserPrefStore::IsHistorySyncEnabledForTest() const {
  return IsHistorySyncEnabled();
}

void DualLayerUserPrefStore::SetIsHistorySyncEnabledForTest(
    bool is_history_sync_enabled) {
  is_history_sync_enabled_ = is_history_sync_enabled;
}

void DualLayerUserPrefStore::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service->AddObserver(this);
  // `sync_service` init should be considered as a state change.
  OnStateChanged(sync_service);
}

void DualLayerUserPrefStore::OnStateChanged(syncer::SyncService* sync_service) {
  bool is_history_sync_enabled =
      sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory);
  if (is_history_sync_enabled == is_history_sync_enabled_) {
    return;
  }

  if (!pref_model_associator_client_) {
    is_history_sync_enabled_ = is_history_sync_enabled;
    return;
  }

  // Store the old values for sensitive prefs in a map and only inform the
  // observers if the effective values change.
  // Note: std::optional is used as the value type since it makes the
  // comparison with the new values easier.
  std::map<std::string, std::optional<base::Value>> old_values;
  for (const std::string& pref_name : GetPrefNamesInAccountStore()) {
    auto metadata = pref_model_associator_client_->GetSyncablePrefsDatabase()
                        .GetSyncablePrefMetadata(pref_name);
    CHECK(metadata.has_value());
    // Add effective value for sensitive prefs to `old_values`.
    if (metadata->is_history_opt_in_required()) {
      if (const base::Value* value = nullptr; GetValue(pref_name, &value)) {
        old_values.emplace(pref_name, value->Clone());
      } else {
        // Put in std::nullopt to mark pref not existing in the store. This
        // helps avoid an extra call to GetPrefNamesInAccount() later.
        old_values.emplace(pref_name, std::nullopt);
      }
    }
  }

  is_history_sync_enabled_ = is_history_sync_enabled;

  // The history sync state has changed. Check for any change in the effective
  // values of any of the sensitive prefs as a consequence.
  for (const auto& [pref_name, old_value] : old_values) {
    std::optional<base::Value> new_value;
    if (const base::Value* value = nullptr; GetValue(pref_name, &value)) {
      new_value = value->Clone();
    }

    // Only notify the observers if the effective value is changing.
    if (old_value != new_value) {
      for (PrefStore::Observer& observer : observers_) {
        observer.OnPrefValueChanged(pref_name);
      }
    }
  }
}

void DualLayerUserPrefStore::OnSyncShutdown(syncer::SyncService* sync_service) {
  // Pref service and hence the pref store outlives sync service.
  sync_service->RemoveObserver(this);
}

void DualLayerUserPrefStore::SetValueInAccountStoreOnly(std::string_view key,
                                                        base::Value value,
                                                        uint32_t flags) {
  const base::Value* initial_value = nullptr;
  // Only notify if the effective value actually changes.
  bool should_notify =
      !GetValue(key, &initial_value) || (*initial_value != value);
  {
    base::AutoReset<bool> setting_prefs(&is_setting_prefs_, true);
    account_pref_store_->SetValue(key, std::move(value), flags);
  }

  if (should_notify) {
    for (PrefStore::Observer& observer : observers_) {
      observer.OnPrefValueChanged(key);
    }
  }
}

bool DualLayerUserPrefStore::HasReadErrorDelegate() const {
  return read_error_delegate_.has_value();
}

}  // namespace sync_preferences
