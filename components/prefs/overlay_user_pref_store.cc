// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/overlay_user_pref_store.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/prefs/in_memory_pref_store.h"

// Allows us to monitor two pref stores and tell updates from them apart. It
// essentially mimics a Callback for the Observer interface (e.g. it allows
// binding additional arguments).
class OverlayUserPrefStore::ObserverAdapter : public PrefStore::Observer {
 public:
  ObserverAdapter(bool ephemeral, OverlayUserPrefStore* parent)
      : ephemeral_user_pref_store_(ephemeral), parent_(parent) {}

  // Methods of PrefStore::Observer.
  void OnPrefValueChanged(const std::string& key) override {
    parent_->OnPrefValueChanged(ephemeral_user_pref_store_, key);
  }
  void OnInitializationCompleted(bool succeeded) override {
    parent_->OnInitializationCompleted(ephemeral_user_pref_store_, succeeded);
  }

 private:
  // Is the update for the ephemeral?
  const bool ephemeral_user_pref_store_;
  OverlayUserPrefStore* const parent_;
};

OverlayUserPrefStore::OverlayUserPrefStore(PersistentPrefStore* persistent)
    : OverlayUserPrefStore(new InMemoryPrefStore(), persistent) {}

OverlayUserPrefStore::OverlayUserPrefStore(PersistentPrefStore* ephemeral,
                                           PersistentPrefStore* persistent)
    : ephemeral_pref_store_observer_(
          std::make_unique<OverlayUserPrefStore::ObserverAdapter>(true, this)),
      persistent_pref_store_observer_(
          std::make_unique<OverlayUserPrefStore::ObserverAdapter>(false, this)),
      ephemeral_user_pref_store_(ephemeral),
      persistent_user_pref_store_(persistent) {
  DCHECK(ephemeral->IsInitializationComplete());
  ephemeral_user_pref_store_->AddObserver(ephemeral_pref_store_observer_.get());
  persistent_user_pref_store_->AddObserver(
      persistent_pref_store_observer_.get());
}

bool OverlayUserPrefStore::IsSetInOverlay(const std::string& key) const {
  return ephemeral_user_pref_store_->GetValue(key, nullptr);
}

void OverlayUserPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void OverlayUserPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool OverlayUserPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool OverlayUserPrefStore::IsInitializationComplete() const {
  return persistent_user_pref_store_->IsInitializationComplete() &&
         ephemeral_user_pref_store_->IsInitializationComplete();
}

bool OverlayUserPrefStore::GetValue(const std::string& key,
                                    const base::Value** result) const {
  // If the |key| shall NOT be stored in the ephemeral store, there must not
  // be an entry.
  DCHECK(!ShallBeStoredInPersistent(key) ||
         !ephemeral_user_pref_store_->GetValue(key, nullptr));

  if (ephemeral_user_pref_store_->GetValue(key, result))
    return true;
  return persistent_user_pref_store_->GetValue(key, result);
}

std::unique_ptr<base::DictionaryValue> OverlayUserPrefStore::GetValues() const {
  auto values = ephemeral_user_pref_store_->GetValues();
  auto persistent_values = persistent_user_pref_store_->GetValues();

  // Output |values| are read from |ephemeral_user_pref_store_| (in-memory
  // store). Then the values of preferences in |persistent_names_set_| are
  // overwritten by the content of |persistent_user_pref_store_| (the persistent
  // store).
  for (const auto& key : persistent_names_set_) {
    std::unique_ptr<base::Value> out_value;
    persistent_values->Remove(key, &out_value);
    if (out_value) {
      values->Set(key, std::move(out_value));
    }
  }
  return values;
}

bool OverlayUserPrefStore::GetMutableValue(const std::string& key,
                                           base::Value** result) {
  if (ShallBeStoredInPersistent(key))
    return persistent_user_pref_store_->GetMutableValue(key, result);

  written_ephemeral_names_.insert(key);
  if (ephemeral_user_pref_store_->GetMutableValue(key, result))
    return true;

  // Try to create copy of persistent if the ephemeral does not contain a value.
  base::Value* persistent_value = nullptr;
  if (!persistent_user_pref_store_->GetMutableValue(key, &persistent_value))
    return false;

  ephemeral_user_pref_store_->SetValue(
      key, persistent_value->CreateDeepCopy(),
      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  ephemeral_user_pref_store_->GetMutableValue(key, result);
  return true;
}

void OverlayUserPrefStore::SetValue(const std::string& key,
                                    std::unique_ptr<base::Value> value,
                                    uint32_t flags) {
  if (ShallBeStoredInPersistent(key)) {
    persistent_user_pref_store_->SetValue(key, std::move(value), flags);
    return;
  }

  // TODO(https://crbug.com/861722): If we always store in in-memory storage
  // and conditionally also stored in persistent one, we wouldn't have to do a
  // complex merge in GetValues().
  written_ephemeral_names_.insert(key);
  ephemeral_user_pref_store_->SetValue(key, std::move(value), flags);
}

void OverlayUserPrefStore::SetValueSilently(const std::string& key,
                                            std::unique_ptr<base::Value> value,
                                            uint32_t flags) {
  if (ShallBeStoredInPersistent(key)) {
    persistent_user_pref_store_->SetValueSilently(key, std::move(value), flags);
    return;
  }

  written_ephemeral_names_.insert(key);
  ephemeral_user_pref_store_->SetValueSilently(key, std::move(value), flags);
}

void OverlayUserPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  if (ShallBeStoredInPersistent(key)) {
    persistent_user_pref_store_->RemoveValue(key, flags);
    return;
  }

  written_ephemeral_names_.insert(key);
  ephemeral_user_pref_store_->RemoveValue(key, flags);
}

void OverlayUserPrefStore::RemoveValuesByPrefixSilently(
    const std::string& prefix) {
  NOTIMPLEMENTED();
}

bool OverlayUserPrefStore::ReadOnly() const {
  return false;
}

PersistentPrefStore::PrefReadError OverlayUserPrefStore::GetReadError() const {
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

PersistentPrefStore::PrefReadError OverlayUserPrefStore::ReadPrefs() {
  // We do not read intentionally.
  OnInitializationCompleted(/* ephemeral */ false, true);
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

void OverlayUserPrefStore::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate_raw) {
  std::unique_ptr<ReadErrorDelegate> error_delegate(error_delegate_raw);
  // We do not read intentionally.
  OnInitializationCompleted(/* ephemeral */ false, true);
}

void OverlayUserPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  persistent_user_pref_store_->CommitPendingWrite(
      std::move(reply_callback), std::move(synchronous_done_callback));
  // We do not write our content intentionally.
}

void OverlayUserPrefStore::SchedulePendingLossyWrites() {
  persistent_user_pref_store_->SchedulePendingLossyWrites();
}

void OverlayUserPrefStore::ReportValueChanged(const std::string& key,
                                              uint32_t flags) {
  for (PrefStore::Observer& observer : observers_)
    observer.OnPrefValueChanged(key);
}

void OverlayUserPrefStore::RegisterPersistentPref(const std::string& key) {
  DCHECK(!key.empty()) << "Key is empty";
  DCHECK(persistent_names_set_.find(key) == persistent_names_set_.end())
      << "Key already registered: " << key;
  persistent_names_set_.insert(key);
}

void OverlayUserPrefStore::ClearMutableValues() {
  for (const auto& key : written_ephemeral_names_) {
    ephemeral_user_pref_store_->RemoveValue(
        key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void OverlayUserPrefStore::OnStoreDeletionFromDisk() {
  persistent_user_pref_store_->OnStoreDeletionFromDisk();
}

OverlayUserPrefStore::~OverlayUserPrefStore() {
  ephemeral_user_pref_store_->RemoveObserver(
      ephemeral_pref_store_observer_.get());
  persistent_user_pref_store_->RemoveObserver(
      persistent_pref_store_observer_.get());
}

void OverlayUserPrefStore::OnPrefValueChanged(bool ephemeral,
                                              const std::string& key) {
  if (ephemeral) {
    ReportValueChanged(key, DEFAULT_PREF_WRITE_FLAGS);
  } else {
    if (!ephemeral_user_pref_store_->GetValue(key, nullptr))
      ReportValueChanged(key, DEFAULT_PREF_WRITE_FLAGS);
  }
}

void OverlayUserPrefStore::OnInitializationCompleted(bool ephemeral,
                                                     bool succeeded) {
  if (!IsInitializationComplete())
    return;
  for (PrefStore::Observer& observer : observers_)
    observer.OnInitializationCompleted(succeeded);
}

bool OverlayUserPrefStore::ShallBeStoredInPersistent(
    const std::string& key) const {
  return persistent_names_set_.find(key) != persistent_names_set_.end();
}
