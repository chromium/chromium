// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/testing_pref_store.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TestingPrefStore::TestingPrefStore()
    : read_only_(true),
      read_success_(true),
      read_error_(PersistentPrefStore::PREF_READ_ERROR_NONE),
      block_async_read_(false),
      pending_async_read_(false),
      init_complete_(false),
      committed_(true) {}

bool TestingPrefStore::GetValue(base::StringPiece key,
                                const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

base::Value::Dict TestingPrefStore::GetValues() const {
  return prefs_.AsDict();
}

bool TestingPrefStore::GetMutableValue(const std::string& key,
                                       base::Value** value) {
  return prefs_.GetValue(key, value);
}

void TestingPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void TestingPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TestingPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool TestingPrefStore::IsInitializationComplete() const {
  return init_complete_;
}

void TestingPrefStore::SetValue(const std::string& key,
                                base::Value value,
                                uint32_t flags) {
  if (prefs_.SetValue(key, std::move(value))) {
    committed_ = false;
    NotifyPrefValueChanged(key);
  }
}

void TestingPrefStore::SetValueSilently(const std::string& key,
                                        base::Value value,
                                        uint32_t flags) {
  CheckPrefIsSerializable(key, value);
  if (prefs_.SetValue(key, std::move(value)))
    committed_ = false;
}

void TestingPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  if (prefs_.RemoveValue(key)) {
    committed_ = false;
    NotifyPrefValueChanged(key);
  }
}

void TestingPrefStore::RemoveValuesByPrefixSilently(const std::string& prefix) {
  prefs_.ClearWithPrefix(prefix);
}

bool TestingPrefStore::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError TestingPrefStore::GetReadError() const {
  return read_error_;
}

PersistentPrefStore::PrefReadError TestingPrefStore::ReadPrefs() {
  NotifyInitializationCompleted();
  return read_error_;
}

void TestingPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  DCHECK(!pending_async_read_);
  error_delegate_.reset(error_delegate);
  if (block_async_read_)
    pending_async_read_ = true;
  else
    NotifyInitializationCompleted();
}

void TestingPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  committed_ = true;
  PersistentPrefStore::CommitPendingWrite(std::move(reply_callback),
                                          std::move(synchronous_done_callback));
}

void TestingPrefStore::SchedulePendingLossyWrites() {}

void TestingPrefStore::SetInitializationCompleted() {
  NotifyInitializationCompleted();
}

void TestingPrefStore::NotifyPrefValueChanged(const std::string& key) {
  for (Observer& observer : observers_)
    observer.OnPrefValueChanged(key);
}

void TestingPrefStore::NotifyInitializationCompleted() {
  DCHECK(!init_complete_);
  init_complete_ = true;
  if (read_success_ && read_error_ != PREF_READ_ERROR_NONE && error_delegate_)
    error_delegate_->OnError(read_error_);
  for (Observer& observer : observers_)
    observer.OnInitializationCompleted(read_success_);
}

void TestingPrefStore::ReportValueChanged(const std::string& key,
                                          uint32_t flags) {
  const base::Value* value = nullptr;
  if (prefs_.GetValue(key, &value))
    CheckPrefIsSerializable(key, *value);

  for (Observer& observer : observers_)
    observer.OnPrefValueChanged(key);
}

void TestingPrefStore::SetString(const std::string& key,
                                 const std::string& value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

void TestingPrefStore::SetInteger(const std::string& key, int value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

void TestingPrefStore::SetBoolean(const std::string& key, bool value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

bool TestingPrefStore::GetString(const std::string& key,
                                 std::string* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value)
    return false;

  if (value && stored_value->is_string()) {
    *value = stored_value->GetString();
    return true;
  }
  return stored_value->is_string();
}

bool TestingPrefStore::GetInteger(const std::string& key, int* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value)
    return false;

  if (value && stored_value->is_int()) {
    *value = stored_value->GetInt();
    return true;
  }
  return stored_value->is_int();
}

bool TestingPrefStore::GetBoolean(const std::string& key, bool* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value)
    return false;

  if (value && stored_value->is_bool()) {
    *value = stored_value->GetBool();
    return true;
  }
  return stored_value->is_bool();
}

void TestingPrefStore::SetBlockAsyncRead(bool block_async_read) {
  DCHECK(!init_complete_);
  block_async_read_ = block_async_read;
  if (pending_async_read_ && !block_async_read_)
    NotifyInitializationCompleted();
}

void TestingPrefStore::OnStoreDeletionFromDisk() {}

void TestingPrefStore::set_read_only(bool read_only) {
  read_only_ = read_only;
}

void TestingPrefStore::set_read_success(bool read_success) {
  DCHECK(!init_complete_);
  read_success_ = read_success;
}

void TestingPrefStore::set_read_error(
    PersistentPrefStore::PrefReadError read_error) {
  DCHECK(!init_complete_);
  read_error_ = read_error;
}

TestingPrefStore::~TestingPrefStore() {
  for (auto& pref : prefs_)
    CheckPrefIsSerializable(pref.first, pref.second);
}

void TestingPrefStore::CheckPrefIsSerializable(const std::string& key,
                                               const base::Value& value) {
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(value, &json))
      << "Pref \"" << key << "\" is not serializable as JSON.";
}
