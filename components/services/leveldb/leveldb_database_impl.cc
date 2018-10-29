// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/leveldb/leveldb_database_impl.h"

#include <inttypes.h>
#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/services/leveldb/env_mojo.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb {
namespace {

template <typename FunctionType>
leveldb::Status ForEachWithPrefix(leveldb::DB* db,
                                  const leveldb::Slice& key_prefix,
                                  FunctionType function) {
  leveldb::ReadOptions read_options;
  // Disable filling the cache for bulk scans. Either this is for deletion
  // (where caching those blocks doesn't make sense) or a mass-read, which the
  // user "should" be caching / only needing once.
  read_options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> it(db->NewIterator(read_options));
  it->Seek(key_prefix);
  for (; it->Valid(); it->Next()) {
    if (!it->key().starts_with(key_prefix))
      break;
    function(it->key(), it->value());
  }
  return it->status();
}

}  // namespace

LevelDBDatabaseImpl::LevelDBDatabaseImpl(
    std::unique_ptr<Env> environment,
    std::unique_ptr<DB> db,
    std::unique_ptr<Cache> cache,
    const leveldb_env::Options& options,
    const std::string& name,
    base::Optional<base::trace_event::MemoryAllocatorDumpGuid> memory_dump_id)
    : environment_(std::move(environment)),
      cache_(std::move(cache)),
      db_(std::move(db)),
      options_(options),
      name_(name),
      memory_dump_id_(memory_dump_id) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "MojoLevelDB", base::SequencedTaskRunnerHandle::Get(),
          MemoryDumpProvider::Options());
}

LevelDBDatabaseImpl::~LevelDBDatabaseImpl() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  for (auto& p : iterator_map_)
    delete p.second;
  for (auto& p : snapshot_map_)
    db_->ReleaseSnapshot(p.second);
}

void LevelDBDatabaseImpl::Put(const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& value,
                              PutCallback callback) {
  Status status =
      db_->Put(leveldb::WriteOptions(), GetSliceFor(key), GetSliceFor(value));
  std::move(callback).Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Delete(const std::vector<uint8_t>& key,
                                 DeleteCallback callback) {
  Status status = db_->Delete(leveldb::WriteOptions(), GetSliceFor(key));
  std::move(callback).Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                                         DeletePrefixedCallback callback) {
  WriteBatch batch;
  Status status = DeletePrefixedHelper(GetSliceFor(key_prefix), &batch);
  if (status.ok())
    status = db_->Write(leveldb::WriteOptions(), &batch);
  std::move(callback).Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::RewriteDB(RewriteDBCallback callback) {
  Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  std::move(callback).Run(LeveldbStatusToError(status));
  if (!db_ && close_binding_) {
    // There is no point in existence without a db_.
    std::move(close_binding_).Run();
  }
}

void LevelDBDatabaseImpl::SetCloseBindingClosure(
    base::OnceClosure close_binding) {
  close_binding_ = std::move(close_binding);
}

void LevelDBDatabaseImpl::Write(
    std::vector<mojom::BatchedOperationPtr> operations,
    WriteCallback callback) {
  WriteBatch batch;

  for (size_t i = 0; i < operations.size(); ++i) {
    switch (operations[i]->type) {
      case mojom::BatchOperationType::PUT_KEY: {
        if (operations[i]->value) {
          batch.Put(GetSliceFor(operations[i]->key),
                    GetSliceFor(*(operations[i]->value)));
        } else {
          batch.Put(GetSliceFor(operations[i]->key), Slice());
        }
        break;
      }
      case mojom::BatchOperationType::DELETE_KEY: {
        batch.Delete(GetSliceFor(operations[i]->key));
        break;
      }
      case mojom::BatchOperationType::DELETE_PREFIXED_KEY: {
        DeletePrefixedHelper(GetSliceFor(operations[i]->key), &batch);
        break;
      }
      case mojom::BatchOperationType::COPY_PREFIXED_KEY: {
        if (!operations[i]->value) {
          mojo::ReportBadMessage(
              "COPY_PREFIXED_KEY operation must provide a value.");
          std::move(callback).Run(mojom::DatabaseError::INVALID_ARGUMENT);
          return;
        }
        CopyPrefixedHelper(operations[i]->key, *(operations[i]->value), &batch);
        break;
      }
    }
  }

  Status status = db_->Write(leveldb::WriteOptions(), &batch);
  std::move(callback).Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::Get(const std::vector<uint8_t>& key,
                              GetCallback callback) {
  std::string value;
  Status status = db_->Get(leveldb::ReadOptions(), GetSliceFor(key), &value);
  std::move(callback).Run(LeveldbStatusToError(status),
                          StdStringToUint8Vector(value));
}

void LevelDBDatabaseImpl::GetPrefixed(const std::vector<uint8_t>& key_prefix,
                                      GetPrefixedCallback callback) {
  std::vector<mojom::KeyValuePtr> data;
  Status status =
      ForEachWithPrefix(db_.get(), GetSliceFor(key_prefix),
                        [&data](const Slice& key, const Slice& value) {
                          mojom::KeyValuePtr kv = mojom::KeyValue::New();
                          kv->key = GetVectorFor(key);
                          kv->value = GetVectorFor(value);
                          data.push_back(std::move(kv));
                        });
  std::move(callback).Run(LeveldbStatusToError(status), std::move(data));
}

void LevelDBDatabaseImpl::CopyPrefixed(
    const std::vector<uint8_t>& source_key_prefix,
    const std::vector<uint8_t>& destination_key_prefix,
    CopyPrefixedCallback callback) {
  WriteBatch batch;
  Status status =
      CopyPrefixedHelper(source_key_prefix, destination_key_prefix, &batch);
  if (status.ok())
    status = db_->Write(leveldb::WriteOptions(), &batch);
  std::move(callback).Run(LeveldbStatusToError(status));
}

void LevelDBDatabaseImpl::GetSnapshot(GetSnapshotCallback callback) {
  const Snapshot* s = db_->GetSnapshot();
  base::UnguessableToken token = base::UnguessableToken::Create();
  snapshot_map_.insert(std::make_pair(token, s));
  std::move(callback).Run(token);
}

void LevelDBDatabaseImpl::ReleaseSnapshot(
    const base::UnguessableToken& snapshot) {
  auto it = snapshot_map_.find(snapshot);
  if (it != snapshot_map_.end()) {
    db_->ReleaseSnapshot(it->second);
    snapshot_map_.erase(it);
  }
}

void LevelDBDatabaseImpl::GetFromSnapshot(
    const base::UnguessableToken& snapshot,
    const std::vector<uint8_t>& key,
    GetCallback callback) {
  // If the snapshot id is invalid, send back invalid argument
  auto it = snapshot_map_.find(snapshot);
  if (it == snapshot_map_.end()) {
    std::move(callback).Run(mojom::DatabaseError::INVALID_ARGUMENT,
                            std::vector<uint8_t>());
    return;
  }

  std::string value;
  leveldb::ReadOptions options;
  options.snapshot = it->second;
  Status status = db_->Get(options, GetSliceFor(key), &value);
  std::move(callback).Run(LeveldbStatusToError(status),
                          StdStringToUint8Vector(value));
}

void LevelDBDatabaseImpl::NewIterator(NewIteratorCallback callback) {
  Iterator* iterator = db_->NewIterator(leveldb::ReadOptions());
  base::UnguessableToken token = base::UnguessableToken::Create();
  iterator_map_.insert(std::make_pair(token, iterator));
  std::move(callback).Run(token);
}

void LevelDBDatabaseImpl::NewIteratorFromSnapshot(
    const base::UnguessableToken& snapshot,
    NewIteratorFromSnapshotCallback callback) {
  // If the snapshot id is invalid, send back invalid argument
  auto it = snapshot_map_.find(snapshot);
  if (it == snapshot_map_.end()) {
    std::move(callback).Run(base::Optional<base::UnguessableToken>());
    return;
  }

  leveldb::ReadOptions options;
  options.snapshot = it->second;

  Iterator* iterator = db_->NewIterator(options);
  base::UnguessableToken new_token = base::UnguessableToken::Create();
  iterator_map_.insert(std::make_pair(new_token, iterator));
  std::move(callback).Run(new_token);
}

void LevelDBDatabaseImpl::ReleaseIterator(
    const base::UnguessableToken& iterator) {
  auto it = iterator_map_.find(iterator);
  if (it != iterator_map_.end()) {
    delete it->second;
    iterator_map_.erase(it);
  }
}

void LevelDBDatabaseImpl::IteratorSeekToFirst(
    const base::UnguessableToken& iterator,
    IteratorSeekToFirstCallback callback) {
  auto it = iterator_map_.find(iterator);
  if (it == iterator_map_.end()) {
    std::move(callback).Run(false, mojom::DatabaseError::INVALID_ARGUMENT,
                            base::nullopt, base::nullopt);
    return;
  }

  it->second->SeekToFirst();

  ReplyToIteratorMessage(it->second, std::move(callback));
}

void LevelDBDatabaseImpl::IteratorSeekToLast(
    const base::UnguessableToken& iterator,
    IteratorSeekToLastCallback callback) {
  auto it = iterator_map_.find(iterator);
  if (it == iterator_map_.end()) {
    std::move(callback).Run(false, mojom::DatabaseError::INVALID_ARGUMENT,
                            base::nullopt, base::nullopt);
    return;
  }

  it->second->SeekToLast();

  ReplyToIteratorMessage(it->second, std::move(callback));
}

void LevelDBDatabaseImpl::IteratorSeek(const base::UnguessableToken& iterator,
                                       const std::vector<uint8_t>& target,
                                       IteratorSeekToLastCallback callback) {
  auto it = iterator_map_.find(iterator);
  if (it == iterator_map_.end()) {
    std::move(callback).Run(false, mojom::DatabaseError::INVALID_ARGUMENT,
                            base::nullopt, base::nullopt);
    return;
  }

  it->second->Seek(GetSliceFor(target));

  ReplyToIteratorMessage(it->second, std::move(callback));
}

void LevelDBDatabaseImpl::IteratorNext(const base::UnguessableToken& iterator,
                                       IteratorNextCallback callback) {
  auto it = iterator_map_.find(iterator);
  if (it == iterator_map_.end()) {
    std::move(callback).Run(false, mojom::DatabaseError::INVALID_ARGUMENT,
                            base::nullopt, base::nullopt);
    return;
  }

  it->second->Next();

  ReplyToIteratorMessage(it->second, std::move(callback));
}

void LevelDBDatabaseImpl::IteratorPrev(const base::UnguessableToken& iterator,
                                       IteratorPrevCallback callback) {
  auto it = iterator_map_.find(iterator);
  if (it == iterator_map_.end()) {
    std::move(callback).Run(false, mojom::DatabaseError::INVALID_ARGUMENT,
                            base::nullopt, base::nullopt);
    return;
  }

  it->second->Prev();

  ReplyToIteratorMessage(it->second, std::move(callback));
}

bool LevelDBDatabaseImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!dump)
    return true;
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(*memory_dump_id_);
  pmd->AddOwnershipEdge(global_dump->guid(), dump->guid());
  // Add size to global dump to propagate the size of the database to the
  // client's dump.
  global_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         dump->GetSizeInternal());
  return true;
}

void LevelDBDatabaseImpl::ReplyToIteratorMessage(
    leveldb::Iterator* it,
    IteratorSeekToFirstCallback callback) {
  if (!it->Valid()) {
    std::move(callback).Run(false, LeveldbStatusToError(it->status()),
                            base::nullopt, base::nullopt);
    return;
  }

  std::move(callback).Run(true, LeveldbStatusToError(it->status()),
                          GetVectorFor(it->key()), GetVectorFor(it->value()));
}

Status LevelDBDatabaseImpl::DeletePrefixedHelper(const Slice& key_prefix,
                                                 WriteBatch* batch) {
  Status status = ForEachWithPrefix(
      db_.get(), key_prefix,
      [batch](const Slice& key, const Slice& value) { batch->Delete(key); });
  return status;
}

Status LevelDBDatabaseImpl::CopyPrefixedHelper(
    const std::vector<uint8_t>& source_key_prefix,
    const std::vector<uint8_t>& destination_key_prefix,
    leveldb::WriteBatch* batch) {
  std::vector<uint8_t> new_prefix = destination_key_prefix;
  size_t source_key_prefix_size = source_key_prefix.size();
  size_t destination_key_prefix_size = destination_key_prefix.size();
  Status status = ForEachWithPrefix(
      db_.get(), GetSliceFor(source_key_prefix),
      [&batch, &new_prefix, source_key_prefix_size,
       destination_key_prefix_size](const Slice& key, const Slice& value) {
        size_t excess_key = key.size() - source_key_prefix_size;
        new_prefix.resize(destination_key_prefix_size + excess_key);
        std::copy(key.data() + source_key_prefix_size, key.data() + key.size(),
                  new_prefix.begin() + destination_key_prefix_size);
        batch->Put(GetSliceFor(new_prefix), value);
      });
  return status;
}

}  // namespace leveldb
