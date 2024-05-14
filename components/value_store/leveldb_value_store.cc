// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/leveldb_value_store.h"

#include <inttypes.h>
#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

const char kInvalidJson[] = "Invalid JSON";
const char kCannotSerialize[] = "Cannot serialize value to JSON";

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

size_t LeveldbValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

size_t LeveldbValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

size_t LeveldbValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return 0;
}

ValueStore::ReadResult LeveldbValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return ReadResult(std::move(status));

  base::Value::Dict settings;

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

  base::Value::Dict settings;

  std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    std::optional<base::Value> value = base::JSONReader::Read(
        std::string_view(it->value().data(), it->value().size()));
    if (!value) {
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
  if (!status.ok())
    return WriteResult(std::move(status));

  leveldb::WriteBatch batch;
  ValueStoreChangeList changes;
  status.Merge(AddToBatch(options, key, value, &batch, &changes));
  if (!status.ok())
    return WriteResult(std::move(status));

  status.Merge(WriteToDb(&batch));
  return status.ok() ? WriteResult(std::move(changes), std::move(status))
                     : WriteResult(std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options,
    const base::Value::Dict& settings) {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return WriteResult(std::move(status));

  leveldb::WriteBatch batch;
  ValueStoreChangeList changes;

  for (const auto [key, value] : settings) {
    status.Merge(AddToBatch(options, key, value, &batch, &changes));
    if (!status.ok())
      return WriteResult(std::move(status));
  }

  status.Merge(WriteToDb(&batch));
  return status.ok() ? WriteResult(std::move(changes), std::move(status))
                     : WriteResult(std::move(status));
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
  return WriteResult(std::move(changes), std::move(status));
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  ValueStoreChangeList changes;

  ReadResult read_result = Get();
  if (!read_result.status().ok())
    return WriteResult(read_result.PassStatus());

  base::Value::Dict& whole_db = read_result.settings();
  while (!whole_db.empty()) {
    std::string next_key = whole_db.begin()->first;
    std::optional<base::Value> next_value = whole_db.Extract(next_key);
    changes.emplace_back(next_key, std::move(*next_value), std::nullopt);
  }

  DeleteDbFile();
  return WriteResult(std::move(changes), read_result.PassStatus());
}

bool LeveldbValueStore::WriteToDbForTest(leveldb::WriteBatch* batch) {
  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return false;
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

ValueStore::Status LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  bool write_new_value = true;

  if (!(options & NO_GENERATE_CHANGES)) {
    std::optional<base::Value> old_value;
    Status status = Read(key, &old_value);
    if (!status.ok())
      return status;
    if (!old_value || *old_value != value) {
      changes->push_back(
          ValueStoreChange(key, std::move(old_value), value.Clone()));
    } else {
      write_new_value = false;
    }
  }

  if (write_new_value) {
    std::string value_as_json;
    if (!base::JSONWriter::Write(value, &value_as_json))
      return Status(OTHER_ERROR, kCannotSerialize);
    batch->Put(key, value_as_json);
  }

  return Status();
}

ValueStore::Status LeveldbValueStore::WriteToDb(leveldb::WriteBatch* batch) {
  return ToValueStoreError(db()->Write(write_options(), batch));
}

}  // namespace value_store
