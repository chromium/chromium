// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/wrap_with_prefix_pref_store.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

WrapWithPrefixPrefStore::WrapWithPrefixPrefStore(
    scoped_refptr<PersistentPrefStore> target_pref_store,
    std::string_view path_prefix)
    : target_pref_store_(std::move(target_pref_store)),
      dotted_prefix_(base::StrCat({path_prefix, "."})) {
  target_pref_store_->AddObserver(this);
}

WrapWithPrefixPrefStore::~WrapWithPrefixPrefStore() {
  target_pref_store_->RemoveObserver(this);
}

bool WrapWithPrefixPrefStore::GetValue(std::string_view key,
                                       const base::Value** value) const {
  return target_pref_store_->GetValue(AddDottedPrefix(key), value);
}

base::Value::Dict WrapWithPrefixPrefStore::GetValues() const {
  base::Value::Dict values = target_pref_store_->GetValues();
  std::string_view prefix(dotted_prefix_.c_str(), dotted_prefix_.size() - 1);
  if (base::Value::Dict* values_with_prefix =
          values.FindDictByDottedPath(prefix)) {
    return std::move(*values_with_prefix);
  }
  return {};
}

bool WrapWithPrefixPrefStore::GetMutableValue(std::string_view key,
                                              base::Value** value) {
  return target_pref_store_->GetMutableValue(AddDottedPrefix(key), value);
}

void WrapWithPrefixPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void WrapWithPrefixPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool WrapWithPrefixPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool WrapWithPrefixPrefStore::IsInitializationComplete() const {
  return target_pref_store_->IsInitializationComplete();
}

void WrapWithPrefixPrefStore::SetValue(std::string_view key,
                                       base::Value value,
                                       uint32_t flags) {
  target_pref_store_->SetValue(AddDottedPrefix(key), std::move(value), flags);
}

void WrapWithPrefixPrefStore::SetValueSilently(std::string_view key,
                                               base::Value value,
                                               uint32_t flags) {
  target_pref_store_->SetValueSilently(AddDottedPrefix(key), std::move(value),
                                       flags);
}

void WrapWithPrefixPrefStore::RemoveValue(std::string_view key,
                                          uint32_t flags) {
  target_pref_store_->RemoveValue(AddDottedPrefix(key), flags);
}

void WrapWithPrefixPrefStore::RemoveValuesByPrefixSilently(
    std::string_view prefix) {
  target_pref_store_->RemoveValuesByPrefixSilently(AddDottedPrefix(prefix));
}

bool WrapWithPrefixPrefStore::ReadOnly() const {
  return target_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError WrapWithPrefixPrefStore::GetReadError()
    const {
  return target_pref_store_->GetReadError();
}

PersistentPrefStore::PrefReadError WrapWithPrefixPrefStore::ReadPrefs() {
  // The target pref store should have been initialized prior to calling
  // ReadPrefs() on this store.
  CHECK(target_pref_store_->IsInitializationComplete() ||
        // To catch case where target pref store initialization failed.
        target_pref_store_->GetReadError() !=
            PersistentPrefStore::PREF_READ_ERROR_NONE);
  return target_pref_store_->GetReadError();
}

void WrapWithPrefixPrefStore::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate) {
  // The target pref store should either have been initialized or should have an
  // ongoing read.
  CHECK(IsInitializationComplete() ||
        // To catch case where target pref store initialization failed.
        GetReadError() != PersistentPrefStore::PREF_READ_ERROR_NONE ||
        // ReadPrefsAsync() was called but it's still ongoing.
        target_pref_store_->HasReadErrorDelegate());
  read_error_delegate_.emplace(error_delegate);
  if (PersistentPrefStore::PrefReadError read_error = GetReadError();
      read_error != PersistentPrefStore::PREF_READ_ERROR_NONE &&
      error_delegate) {
    error_delegate->OnError(read_error);
  }
}

void WrapWithPrefixPrefStore::SchedulePendingLossyWrites() {
  // This store is only a wrapper and relies on the target pref store being
  // independently notified of this.
}

void WrapWithPrefixPrefStore::OnStoreDeletionFromDisk() {
  // This store is only a wrapper and relies on the target pref store being
  // independently notified of this.
}

void WrapWithPrefixPrefStore::ReportValueChanged(std::string_view key,
                                                 uint32_t flags) {
  return target_pref_store_->ReportValueChanged(AddDottedPrefix(key), flags);
}

void WrapWithPrefixPrefStore::OnPrefValueChanged(std::string_view key) {
  if (!HasDottedPrefix(key)) {
    return;
  }
  std::string_view original_key(RemoveDottedPrefix(key));
  for (PrefStore::Observer& observer : observers_) {
    observer.OnPrefValueChanged(original_key);
  }
}

void WrapWithPrefixPrefStore::OnInitializationCompleted(bool succeeded) {
  if (PersistentPrefStore::PrefReadError read_error = GetReadError();
      read_error != PersistentPrefStore::PREF_READ_ERROR_NONE &&
      read_error_delegate_.has_value() && read_error_delegate_.value()) {
    read_error_delegate_.value()->OnError(read_error);
  }
  for (PrefStore::Observer& observer : observers_) {
    observer.OnInitializationCompleted(succeeded);
  }
}

std::string WrapWithPrefixPrefStore::AddDottedPrefix(
    std::string_view path) const {
  return base::StrCat({dotted_prefix_, path});
}

std::string_view WrapWithPrefixPrefStore::RemoveDottedPrefix(
    std::string_view path) const {
  CHECK(HasDottedPrefix(path));
  path.remove_prefix(dotted_prefix_.size());
  return path;
}

bool WrapWithPrefixPrefStore::HasDottedPrefix(std::string_view path) const {
  return base::StartsWith(path, dotted_prefix_);
}

bool WrapWithPrefixPrefStore::HasReadErrorDelegate() const {
  return read_error_delegate_.has_value();
}
