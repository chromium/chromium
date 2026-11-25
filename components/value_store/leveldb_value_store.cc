// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/leveldb_value_store.h"

#include <inttypes.h>
#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/value_store/value_store.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

const char kInvalidJson[] = "Invalid JSON";
const char kCannotSerialize[] = "Cannot serialize value to JSON";
const char kCannotDestroyDb[] = "Cannot destroy database";

}  // namespace

namespace value_store {

LeveldbValueStore::LeveldbValueStore(const std::string& uma_client_name,
                                     const base::FilePath& db_path)
    : LazyLevelDb(uma_client_name, db_path) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "LeveldbValueStore",
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::trace_event::MemoryDumpProvider::Options());
}

LeveldbValueStore::~LeveldbValueStore() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

ValueStore::ReadResult LeveldbValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult LeveldbValueStore::GetKeys() {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    return ReadResult(std::move(status));
  }

  base::DictValue settings;

  std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    settings.Set(key, base::Value());
  }

  if (!it->status().ok()) {
    status.Merge(ToValueStoreError(it->status()));
    return ReadResult(std::move(status));
  }

  return ReadResult(std::move(settings), std::move(status));
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return ReadResult(std::move(status));

  base::DictValue settings;

  for (const std::string& key : keys) {
    std::optional<base::Value> setting;
    status.Merge(Read(key, &setting));
    if (!status.ok())
      return ReadResult(std::move(status));
    if (setting)
      settings.Set(key, std::move(*setting));
  }

  return ReadResult(std::move(settings), std::move(status));
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return ReadResult(std::move(status));

  base::DictValue settings;

  std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    std::optional<base::Value> value = base::JSONReader::Read(
        std::string_view(it->value().data(), it->value().size()),
        base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!value) {
      ResetQuotaUsage();
      return ReadResult(Status(CORRUPTION,
                               Delete(key).ok() ? VALUE_RESTORE_DELETE_SUCCESS
                                                : VALUE_RESTORE_DELETE_FAILURE,
                               kInvalidJson));
    }
    settings.Set(key, std::move(*value));
  }

  if (!it->status().ok()) {
    status.Merge(ToValueStoreError(it->status()));
    return ReadResult(std::move(status));
  }

  return ReadResult(std::move(settings), std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Set(WriteOptions options,
                                               const std::string& key,
                                               const base::Value& value) {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  ScopedQuotaTransaction quota_transaction(this);
  std::optional<std::string> value_as_json;
  if (Status quota_status =
          quota_transaction.Stage(options, key, value, &value_as_json);
      !quota_status.ok()) {
    return WriteResult(std::move(quota_status));
  }

  leveldb::WriteBatch batch;
  ValueStoreChangeList changes;
  status.Merge(
      AddToBatch(options, key, value, value_as_json, &batch, &changes));
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  status.Merge(WriteToDb(&batch));
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  quota_transaction.Commit();
  return WriteResult(std::move(changes), std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options,
    const base::DictValue& settings) {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  ScopedQuotaTransaction quota_transaction(this);
  std::vector<std::string> serialized_values;
  if (Status quota_status =
          quota_transaction.Stage(options, settings, &serialized_values);
      !quota_status.ok()) {
    return WriteResult(std::move(quota_status));
  }

  leveldb::WriteBatch batch;
  ValueStoreChangeList changes;
  size_t index = 0;
  for (const auto [key, value] : settings) {
    std::optional<std::string_view> pre_serialized =
        index < serialized_values.size()
            ? std::optional<std::string_view>(serialized_values[index++])
            : std::nullopt;
    status.Merge(
        AddToBatch(options, key, value, pre_serialized, &batch, &changes));
    if (!status.ok()) {
      return WriteResult(std::move(status));
    }
  }

  status.Merge(WriteToDb(&batch));
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  quota_transaction.Commit();
  return WriteResult(std::move(changes), std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Remove(const std::string& key) {
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return WriteResult(std::move(status));

  leveldb::WriteBatch batch;
  ValueStoreChangeList changes;

  for (const std::string& key : keys) {
    std::optional<base::Value> old_value;
    status.Merge(Read(key, &old_value));
    if (!status.ok())
      return WriteResult(std::move(status));

    if (old_value) {
      changes.emplace_back(key, std::move(old_value), std::nullopt);
      batch.Delete(key);
    }
  }

  leveldb::Status ldb_status;
  ldb_status = db()->Write(leveldb::WriteOptions(), &batch);
  if (!ldb_status.ok() && !ldb_status.IsNotFound()) {
    status.Merge(ToValueStoreError(ldb_status));
    return WriteResult(std::move(status));
  }

  for (const std::string& key : keys) {
    FreeQuotaUsage(key);
  }
  return WriteResult(std::move(changes), std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    return WriteResult(std::move(status));
  }

  ValueStoreChangeList changes;
  bool has_corrupt_keys = false;
  {
    // Must be destroyed before DeleteDbFile() closes db().
    std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::optional<base::Value> value =
          base::JSONReader::Read(leveldb_env::MakeStringView(it->value()),
                                 base::JSON_PARSE_CHROMIUM_EXTENSIONS);
      if (value) {
        changes.emplace_back(it->key().ToString(), std::move(*value),
                             std::nullopt);
      } else {
        has_corrupt_keys = true;
      }
    }

    if (!it->status().ok()) {
      status.Merge(ToValueStoreError(it->status()));
    }
  }

  if (!DeleteDbFile()) {
    status.Merge({CORRUPTION, DB_RESTORE_DELETE_FAILURE, kCannotDestroyDb});
  } else if (status.IsCorrupted()) {
    status.Merge({OK, DB_RESTORE_DELETE_SUCCESS, ""});
  } else if (has_corrupt_keys) {
    status.Merge({OK, VALUE_RESTORE_DELETE_SUCCESS, kInvalidJson});
  }
  ResetQuotaUsage();
  return status.ok() ? WriteResult(std::move(changes), std::move(status))
                     : WriteResult(std::move(status));
}

bool LeveldbValueStore::WriteToDbForTest(leveldb::WriteBatch* batch) {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    return false;
  }
  ResetQuotaUsage();
  return WriteToDb(batch).ok();
}

bool LeveldbValueStore::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // Return true so that the provider is not disabled.
  if (!db())
    return true;

  // All leveldb databases are already dumped by leveldb_env::DBTracker. Add
  // an edge to the existing dump.
  auto* tracker_dump =
      leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db());
  if (!tracker_dump)
    return true;

  auto* dump = pmd->CreateAllocatorDump(base::StringPrintf(
      "extensions/value_store/%s/0x%" PRIXPTR, open_histogram_name(),
      reinterpret_cast<uintptr_t>(this)));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  tracker_dump->GetSizeInternal());
  pmd->AddOwnershipEdge(dump->guid(), tracker_dump->guid());

  return true;
}

bool LeveldbValueStore::CalculateUsage() {
  Status status = EnsureDbIsOpen();
  if (!status.ok()) {
    LOG(WARNING) << "Failed to open settings db for quota: " << status.message;
    return false;
  }

  std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    RecordStoredItemSize(it->key().ToString(),
                         it->key().size() + it->value().size());
  }

  if (!it->status().ok()) {
    LOG(WARNING) << "Failed to iterate settings for quota: "
                 << it->status().ToString();
    return false;
  }

  return true;
}

ValueStore::Status LeveldbValueStore::EnsureDbIsOpen() {
  Status status = LazyLevelDb::EnsureDbIsOpen();
  if (status.restore_status != RESTORE_NONE) {
    // Restoration means that an unknown amount, possibly all, of the data was
    // lost from the database. Reset our counters - they will be lazily
    // recalculated if/when needed.
    ResetQuotaUsage();
  }
  return status;
}

ValueStore::Status LeveldbValueStore::Read(const std::string& key,
                                           std::optional<base::Value>* value) {
  Status status = LazyLevelDb::Read(key, value);
  if (status.restore_status != RESTORE_NONE) {
    // Restoration means that an unknown amount, possibly all, of the data was
    // lost from the database. Reset our counters - they will be lazily
    // recalculated if/when needed.
    ResetQuotaUsage();
  }
  return status;
}

ValueStore::Status LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    std::optional<std::string_view> value_as_json,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  bool write_new_value = true;

  if (!(options & NO_GENERATE_CHANGES)) {
    std::optional<base::Value> old_value;
    Status status = Read(key, &old_value);
    if (!status.ok()) {
      return status;
    }
    if (!old_value || *old_value != value) {
      changes->push_back(
          ValueStoreChange(key, std::move(old_value), value.Clone()));
    } else {
      write_new_value = false;
    }
  }

  if (write_new_value) {
    if (value_as_json.has_value()) {
      batch->Put(key,
                 leveldb::Slice(value_as_json->data(), value_as_json->size()));
    } else {
      std::optional<std::string> serialized = base::WriteJson(value);
      if (!serialized) {
        return Status(OTHER_ERROR, kCannotSerialize);
      }
      batch->Put(key, *serialized);
    }
  }

  return Status();
}

ValueStore::Status LeveldbValueStore::WriteToDb(leveldb::WriteBatch* batch) {
  return ToValueStoreError(db()->Write(write_options(), batch));
}

}  // namespace value_store
