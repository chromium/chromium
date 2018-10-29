// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_backend.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/protocol/model_type_store_schema_descriptor.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using sync_pb::ModelTypeStoreSchemaDescriptor;

namespace syncer {

const int64_t kInvalidSchemaVersion = -1;
const int64_t ModelTypeStoreBackend::kLatestSchemaVersion = 1;
const char ModelTypeStoreBackend::kDBSchemaDescriptorRecordId[] =
    "_mts_schema_descriptor";
const char ModelTypeStoreBackend::kStoreInitResultHistogramName[] =
    "Sync.ModelTypeStoreInitResult";

namespace {

StoreInitResultForHistogram LevelDbStatusToStoreInitResult(
    const leveldb::Status& status) {
  if (status.ok())
    return STORE_INIT_RESULT_SUCCESS;
  if (status.IsNotFound())
    return STORE_INIT_RESULT_NOT_FOUND;
  if (status.IsCorruption())
    return STORE_INIT_RESULT_CORRUPTION;
  if (status.IsNotSupportedError())
    return STORE_INIT_RESULT_NOT_SUPPORTED;
  if (status.IsInvalidArgument())
    return STORE_INIT_RESULT_INVALID_ARGUMENT;
  if (status.IsIOError())
    return STORE_INIT_RESULT_IO_ERROR;
  return STORE_INIT_RESULT_UNKNOWN;
}

}  // namespace

// static
scoped_refptr<ModelTypeStoreBackend>
ModelTypeStoreBackend::CreateInMemoryForTest() {
  std::unique_ptr<leveldb::Env> env =
      leveldb_chrome::NewMemEnv("ModelTypeStore");

  std::string test_directory_str;
  env->GetTestDirectory(&test_directory_str);
  const base::FilePath path = base::FilePath::FromUTF8Unsafe(test_directory_str)
                                  .Append(FILE_PATH_LITERAL("in-memory"));

  scoped_refptr<ModelTypeStoreBackend> backend =
      new ModelTypeStoreBackend(std::move(env));

  base::Optional<ModelError> error = backend->Init(path);
  DCHECK(!error);
  return backend;
}

// static
scoped_refptr<ModelTypeStoreBackend>
ModelTypeStoreBackend::CreateUninitialized() {
  return new ModelTypeStoreBackend(/*env=*/nullptr);
}

ModelTypeStoreBackend::~ModelTypeStoreBackend() {}

base::Optional<ModelError> ModelTypeStoreBackend::Init(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsInitialized());
  const std::string path_str = path.AsUTF8Unsafe();

  leveldb::Status status = OpenDatabase(path_str, env_.get());
  if (status.IsCorruption()) {
    DCHECK(db_ == nullptr);
    status = DestroyDatabase(path_str, env_.get());
    if (status.ok())
      status = OpenDatabase(path_str, env_.get());
    if (status.ok())
      RecordStoreInitResultHistogram(
          STORE_INIT_RESULT_RECOVERED_AFTER_CORRUPTION);
  }
  if (!status.ok()) {
    DCHECK(db_ == nullptr);
    RecordStoreInitResultHistogram(LevelDbStatusToStoreInitResult(status));
    return ModelError(FROM_HERE, status.ToString());
  }

  int64_t current_version = GetStoreVersion();
  if (current_version == kInvalidSchemaVersion) {
    RecordStoreInitResultHistogram(STORE_INIT_RESULT_SCHEMA_DESCRIPTOR_ISSUE);
    return ModelError(FROM_HERE, "Invalid schema descriptor");
  }

  if (current_version != kLatestSchemaVersion) {
    base::Optional<ModelError> error =
        Migrate(current_version, kLatestSchemaVersion);
    if (error) {
      RecordStoreInitResultHistogram(STORE_INIT_RESULT_MIGRATION);
      return error;
    }
  }
  RecordStoreInitResultHistogram(STORE_INIT_RESULT_SUCCESS);
  return base::nullopt;
}

bool ModelTypeStoreBackend::IsInitialized() const {
  return db_ != nullptr;
}

ModelTypeStoreBackend::ModelTypeStoreBackend(std::unique_ptr<leveldb::Env> env)
    : env_(std::move(env)) {
  // It's OK to construct this class in a sequence and Init() it elsewhere.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

leveldb::Status ModelTypeStoreBackend::OpenDatabase(const std::string& path,
                                                    leveldb::Env* env) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.paranoid_checks = true;
  options.write_buffer_size = 512 * 1024;

  if (env)
    options.env = env;

  return leveldb_env::OpenDB(options, path, &db_);
}

leveldb::Status ModelTypeStoreBackend::DestroyDatabase(const std::string& path,
                                                       leveldb::Env* env) {
  leveldb_env::Options options;
  if (env)
    options.env = env;
  return leveldb::DestroyDB(path, options);
}

base::Optional<ModelError> ModelTypeStoreBackend::ReadRecordsWithPrefix(
    const std::string& prefix,
    const ModelTypeStore::IdList& id_list,
    ModelTypeStore::RecordList* record_list,
    ModelTypeStore::IdList* missing_id_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  record_list->reserve(id_list.size());
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  read_options.fill_cache = false;
  std::string key;
  std::string value;
  for (const std::string& id : id_list) {
    key = prefix + id;
    leveldb::Status status = db_->Get(read_options, key, &value);
    if (status.ok()) {
      record_list->emplace_back(id, value);
    } else if (status.IsNotFound()) {
      missing_id_list->push_back(id);
    } else {
      return ModelError(FROM_HERE, status.ToString());
    }
  }
  return base::nullopt;
}

base::Optional<ModelError> ModelTypeStoreBackend::ReadAllRecordsWithPrefix(
    const std::string& prefix,
    ModelTypeStore::RecordList* record_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  read_options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  const leveldb::Slice prefix_slice(prefix);
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    leveldb::Slice key = iter->key();
    if (!key.starts_with(prefix_slice))
      break;
    key.remove_prefix(prefix_slice.size());
    record_list->emplace_back(key.ToString(), iter->value().ToString());
  }
  return iter->status().ok() ? base::nullopt
                             : base::Optional<ModelError>(
                                   {FROM_HERE, iter->status().ToString()});
}

base::Optional<ModelError> ModelTypeStoreBackend::WriteModifications(
    std::unique_ptr<leveldb::WriteBatch> write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  leveldb::Status status =
      db_->Write(leveldb::WriteOptions(), write_batch.get());
  return status.ok()
             ? base::nullopt
             : base::Optional<ModelError>({FROM_HERE, status.ToString()});
}

base::Optional<ModelError>
ModelTypeStoreBackend::DeleteDataAndMetadataForPrefix(
    const std::string& prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  leveldb::WriteBatch write_batch;
  leveldb::ReadOptions read_options;
  read_options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  const leveldb::Slice prefix_slice(prefix);
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    leveldb::Slice key = iter->key();
    if (!key.starts_with(prefix_slice))
      break;
    write_batch.Delete(key);
  }
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &write_batch);
  return status.ok()
             ? base::nullopt
             : base::Optional<ModelError>({FROM_HERE, status.ToString()});
}

base::Optional<ModelError> ModelTypeStoreBackend::MigrateForTest(
    int64_t current_version,
    int64_t desired_version) {
  return Migrate(current_version, desired_version);
}

int64_t ModelTypeStoreBackend::GetStoreVersionForTest() {
  return GetStoreVersion();
}

int64_t ModelTypeStoreBackend::GetStoreVersion() {
  DCHECK(db_);
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  read_options.fill_cache = false;
  std::string value;
  ModelTypeStoreSchemaDescriptor schema_descriptor;
  leveldb::Status status =
      db_->Get(read_options, kDBSchemaDescriptorRecordId, &value);
  if (status.IsNotFound()) {
    return 0;
  } else if (!status.ok() || !schema_descriptor.ParseFromString(value)) {
    return kInvalidSchemaVersion;
  }
  return schema_descriptor.version_number();
}

base::Optional<ModelError> ModelTypeStoreBackend::Migrate(
    int64_t current_version,
    int64_t desired_version) {
  DCHECK(db_);
  if (current_version == 0) {
    if (Migrate0To1()) {
      current_version = 1;
    }
  }
  if (current_version == desired_version) {
    return base::nullopt;
  } else if (current_version > desired_version) {
    return ModelError(FROM_HERE, "Schema version too high");
  } else {
    return ModelError(FROM_HERE, "Schema upgrade failed");
  }
}

bool ModelTypeStoreBackend::Migrate0To1() {
  DCHECK(db_);
  ModelTypeStoreSchemaDescriptor schema_descriptor;
  schema_descriptor.set_version_number(1);
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), kDBSchemaDescriptorRecordId,
               schema_descriptor.SerializeAsString());
  return status.ok();
}

// static
void ModelTypeStoreBackend::RecordStoreInitResultHistogram(
    StoreInitResultForHistogram result) {
  UMA_HISTOGRAM_ENUMERATION(kStoreInitResultHistogramName, result,
                            STORE_INIT_RESULT_COUNT);
}

}  // namespace syncer
