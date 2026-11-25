// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/testing_value_store.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"

namespace value_store {

namespace {

const char kGenericErrorMessage[] = "TestingValueStore configured to error";

// Having this utility function allows ValueStore::Status to not have a copy
// constructor.
ValueStore::Status CreateStatusCopy(const ValueStore::Status& status) {
  return ValueStore::Status(status.code, status.restore_status, status.message);
}

size_t JsonValueByteSize(const base::Value& value) {
  return base::WriteJson(value).value_or("").size();
}

}  // namespace

TestingValueStore::TestingValueStore() = default;
TestingValueStore::~TestingValueStore() = default;

void TestingValueStore::set_status_code(StatusCode status_code) {
  status_ = ValueStore::Status(status_code, kGenericErrorMessage);
}

ValueStore::ReadResult TestingValueStore::GetKeys() {
  read_count_++;
  if (!status_.ok()) {
    return ReadResult(CreateStatusCopy(status_));
  }

  base::DictValue settings;
  for (auto entry : storage_) {
    settings.Set(entry.first, base::Value());
  }
  return ReadResult(std::move(settings), CreateStatusCopy(status_));
}

ValueStore::ReadResult TestingValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult TestingValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  if (!status_.ok())
    return ReadResult(CreateStatusCopy(status_));

  base::DictValue settings;
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
  ScopedQuotaTransaction quota_transaction(this);
  if (Status quota_status = quota_transaction.Stage(options, key, value);
      !quota_status.ok()) {
    return WriteResult(std::move(quota_status));
  }

  write_count_++;
  if (!status_.ok()) {
    return WriteResult(CreateStatusCopy(status_));
  }

  ValueStoreChangeList changes;
  if (!(options & NO_GENERATE_CHANGES)) {
    base::Value* old_value = storage_.Find(key);
    if (!old_value || *old_value != value) {
      changes.emplace_back(key,
                           old_value
                               ? std::optional<base::Value>(old_value->Clone())
                               : std::nullopt,
                           value.Clone());
      storage_.Set(key, value.Clone());
    }
  } else {
    storage_.Set(key, value.Clone());
  }

  quota_transaction.Commit();
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options,
    const base::DictValue& settings) {
  ScopedQuotaTransaction quota_transaction(this);
  if (Status quota_status = quota_transaction.Stage(options, settings);
      !quota_status.ok()) {
    return WriteResult(std::move(quota_status));
  }

  write_count_++;
  if (!status_.ok()) {
    return WriteResult(CreateStatusCopy(status_));
  }

  ValueStoreChangeList changes;
  for (const auto [key, value] : settings) {
    if (!(options & NO_GENERATE_CHANGES)) {
      base::Value* old_value = storage_.Find(key);
      if (!old_value || *old_value != value) {
        changes.emplace_back(
            key,
            old_value ? std::optional<base::Value>(old_value->Clone())
                      : std::nullopt,
            value.Clone());
        storage_.Set(key, value.Clone());
      }
    } else {
      storage_.Set(key, value.Clone());
    }
  }

  quota_transaction.Commit();
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
    FreeQuotaUsage(key);
  }
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Clear() {
  std::vector<std::string> keys;
  for (const auto [key, value] : storage_) {
    keys.push_back(key);
  }
  WriteResult result = Remove(keys);
  if (result.status().ok()) {
    ResetQuotaUsage();
  }
  return result;
}

bool TestingValueStore::CalculateUsage() {
  if (!status_.ok()) {
    return false;
  }

  for (const auto [key, value] : storage_) {
    RecordStoredItemSize(key, key.size() + JsonValueByteSize(value));
  }
  return true;
}

}  // namespace value_store
