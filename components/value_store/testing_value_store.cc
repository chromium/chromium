// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/testing_value_store.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/notreached.h"

namespace value_store {

namespace {

const char kGenericErrorMessage[] = "TestingValueStore configured to error";

// Having this utility function allows ValueStore::Status to not have a copy
// constructor.
ValueStore::Status CreateStatusCopy(const ValueStore::Status& status) {
  return ValueStore::Status(status.code, status.restore_status, status.message);
}

}  // namespace

TestingValueStore::TestingValueStore() = default;
TestingValueStore::~TestingValueStore() = default;

void TestingValueStore::set_status_code(StatusCode status_code) {
  status_ = ValueStore::Status(status_code, kGenericErrorMessage);
}

size_t TestingValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

ValueStore::ReadResult TestingValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult TestingValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  if (!status_.ok())
    return ReadResult(CreateStatusCopy(status_));

  base::Value::Dict settings;
  for (const auto& key : keys) {
    base::Value* value = storage_.Find(key);
    if (value) {
      settings.Set(key, value->Clone());
    }
  }
  return ReadResult(std::move(settings), CreateStatusCopy(status_));
}

ValueStore::ReadResult TestingValueStore::Get() {
  read_count_++;
  if (!status_.ok())
    return ReadResult(CreateStatusCopy(status_));
  return ReadResult(storage_.Clone(), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Set(WriteOptions options,
                                               const std::string& key,
                                               const base::Value& value) {
  base::Value::Dict settings;
  settings.Set(key, value.Clone());
  return Set(options, settings);
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options,
    const base::Value::Dict& settings) {
  write_count_++;
  if (!status_.ok())
    return WriteResult(CreateStatusCopy(status_));

  ValueStoreChangeList changes;
  for (const auto [key, value] : settings) {
    base::Value* old_value = storage_.Find(key);
    if (!old_value || *old_value != value) {
      changes.emplace_back(key,
                           old_value
                               ? std::optional<base::Value>(old_value->Clone())
                               : std::nullopt,
                           value.Clone());
      storage_.Set(key, value.Clone());
    }
  }
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Remove(const std::string& key) {
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult TestingValueStore::Remove(
    const std::vector<std::string>& keys) {
  write_count_++;
  if (!status_.ok())
    return WriteResult(CreateStatusCopy(status_));

  ValueStoreChangeList changes;
  for (auto const& key : keys) {
    std::optional<base::Value> old_value = storage_.Extract(key);
    if (old_value.has_value()) {
      changes.emplace_back(key, std::move(*old_value), std::nullopt);
    }
  }
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Clear() {
  std::vector<std::string> keys;
  for (const auto [key, value] : storage_) {
    keys.push_back(key);
  }
  return Remove(keys);
}

}  // namespace value_store
