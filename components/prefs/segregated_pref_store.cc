// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/segregated_pref_store.h"

#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_name_set.h"

SegregatedPrefStore::UnderlyingPrefStoreObserver::UnderlyingPrefStoreObserver(
    SegregatedPrefStore* outer)
    : outer_(outer) {
  DCHECK(outer_);
}

void SegregatedPrefStore::UnderlyingPrefStoreObserver::OnPrefValueChanged(
    std::string_view key) {
  // Notify Observers only after all underlying PrefStores of the outer
  // SegregatedPrefStore are initialized.
  if (!outer_->IsInitializationComplete())
    return;

  for (auto& observer : outer_->observers_)
    observer.OnPrefValueChanged(key);
}

void SegregatedPrefStore::UnderlyingPrefStoreObserver::
    OnInitializationCompleted(bool succeeded) {
  initialization_succeeded_ = succeeded;

  // Notify Observers only after all underlying PrefStores of the outer
  // SegregatedPrefStore are initialized.
  if (!outer_->IsInitializationComplete())
    return;

  if (outer_->read_error_delegate_.has_value() &&
      outer_->read_error_delegate_.value()) {
    PersistentPrefStore::PrefReadError read_error = outer_->GetReadError();
    if (read_error != PersistentPrefStore::PREF_READ_ERROR_NONE)
      outer_->read_error_delegate_.value()->OnError(read_error);
  }

  for (auto& observer : outer_->observers_)
    observer.OnInitializationCompleted(outer_->IsInitializationSuccessful());
}

SegregatedPrefStore::SegregatedPrefStore(
    scoped_refptr<PersistentPrefStore> default_pref_store,
    scoped_refptr<PersistentPrefStore> selected_pref_store,
    PrefNameSet selected_pref_names)
    : default_pref_store_(std::move(default_pref_store)),
      selected_pref_store_(std::move(selected_pref_store)),
      selected_preference_names_(std::move(selected_pref_names)),
      default_observer_(this),
      selected_observer_(this) {
  default_pref_store_->AddObserver(&default_observer_);
  selected_pref_store_->AddObserver(&selected_observer_);
}

void SegregatedPrefStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SegregatedPrefStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SegregatedPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool SegregatedPrefStore::IsInitializationComplete() const {
  return default_pref_store_->IsInitializationComplete() &&
         selected_pref_store_->IsInitializationComplete();
}

bool SegregatedPrefStore::IsInitializationSuccessful() const {
  return default_observer_.initialization_succeeded() &&
         selected_observer_.initialization_succeeded();
}

bool SegregatedPrefStore::GetValue(std::string_view key,
                                   const base::Value** result) const {
  return StoreForKey(key)->GetValue(key, result);
}

base::Value::Dict SegregatedPrefStore::GetValues() const {
  base::Value::Dict values = default_pref_store_->GetValues();
  base::Value::Dict selected_pref_store_values =
      selected_pref_store_->GetValues();
  for (const auto& key : selected_preference_names_) {
    if (base::Value* value = selected_pref_store_values.FindByDottedPath(key)) {
      values.SetByDottedPath(key, std::move(*value));
    } else {
      values.Remove(key);
    }
  }
  return values;
}

void SegregatedPrefStore::SetValue(std::string_view key,
                                   base::Value value,
                                   uint32_t flags) {
  StoreForKey(key)->SetValue(key, std::move(value), flags);
}

void SegregatedPrefStore::RemoveValue(std::string_view key, uint32_t flags) {
  StoreForKey(key)->RemoveValue(key, flags);
}

void SegregatedPrefStore::RemoveValuesByPrefixSilently(
    std::string_view prefix) {
  // Since we can't guarantee to have all the prefs in one the pref stores, we
  // have to push the removal command down to both of them.
  default_pref_store_->RemoveValuesByPrefixSilently(prefix);
  selected_pref_store_->RemoveValuesByPrefixSilently(prefix);
}

bool SegregatedPrefStore::GetMutableValue(std::string_view key,
                                          base::Value** result) {
  return StoreForKey(key)->GetMutableValue(key, result);
}

void SegregatedPrefStore::ReportValueChanged(std::string_view key,
                                             uint32_t flags) {
  StoreForKey(key)->ReportValueChanged(key, flags);
}

void SegregatedPrefStore::SetValueSilently(std::string_view key,
                                           base::Value value,
                                           uint32_t flags) {
  StoreForKey(key)->SetValueSilently(key, std::move(value), flags);
}

bool SegregatedPrefStore::ReadOnly() const {
  return selected_pref_store_->ReadOnly() || default_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::GetReadError() const {
  PersistentPrefStore::PrefReadError read_error =
      default_pref_store_->GetReadError();
  if (read_error == PersistentPrefStore::PREF_READ_ERROR_NONE) {
    read_error = selected_pref_store_->GetReadError();
    // Ignore NO_FILE from selected_pref_store_.
    if (read_error == PersistentPrefStore::PREF_READ_ERROR_NO_FILE)
      read_error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  }
  return read_error;
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::ReadPrefs() {
  // Note: Both of these stores own PrefFilters which makes ReadPrefs
  // asynchronous. This is okay in this case as only the first call will be
  // truly asynchronous, the second call will then unblock the migration in
  // TrackedPreferencesMigrator and complete synchronously.
  default_pref_store_->ReadPrefs();
  PersistentPrefStore::PrefReadError selected_store_read_error =
      selected_pref_store_->ReadPrefs();
  DCHECK_NE(PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE,
            selected_store_read_error);

  return GetReadError();
}

void SegregatedPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  read_error_delegate_.emplace(error_delegate);
  default_pref_store_->ReadPrefsAsync(nullptr);
  selected_pref_store_->ReadPrefsAsync(nullptr);
}

void SegregatedPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // A BarrierClosure will run its callback wherever the last instance of the
  // returned wrapper is invoked. As such it is guaranteed to respect the reply
  // vs synchronous semantics assuming |default_pref_store_| and
  // |selected_pref_store_| honor it.

  base::RepeatingClosure reply_callback_wrapper =
      reply_callback ? base::BarrierClosure(2, std::move(reply_callback))
                     : base::RepeatingClosure();

  base::RepeatingClosure synchronous_callback_wrapper =
      synchronous_done_callback
          ? base::BarrierClosure(2, std::move(synchronous_done_callback))
          : base::RepeatingClosure();

  default_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                          synchronous_callback_wrapper);
  selected_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                           synchronous_callback_wrapper);
}

void SegregatedPrefStore::SchedulePendingLossyWrites() {
  default_pref_store_->SchedulePendingLossyWrites();
  selected_pref_store_->SchedulePendingLossyWrites();
}

void SegregatedPrefStore::OnStoreDeletionFromDisk() {
  default_pref_store_->OnStoreDeletionFromDisk();
  selected_pref_store_->OnStoreDeletionFromDisk();
}

SegregatedPrefStore::~SegregatedPrefStore() {
  default_pref_store_->RemoveObserver(&default_observer_);
  selected_pref_store_->RemoveObserver(&selected_observer_);
}

PersistentPrefStore* SegregatedPrefStore::StoreForKey(std::string_view key) {
  return (base::Contains(selected_preference_names_, key) ? selected_pref_store_
                                                          : default_pref_store_)
      .get();
}

const PersistentPrefStore* SegregatedPrefStore::StoreForKey(
    std::string_view key) const {
  return (base::Contains(selected_preference_names_, key) ? selected_pref_store_
                                                          : default_pref_store_)
      .get();
}

bool SegregatedPrefStore::HasReadErrorDelegate() const {
  return read_error_delegate_.has_value();
}
