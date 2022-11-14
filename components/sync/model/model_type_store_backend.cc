// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_backend.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
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

namespace {

void LogDbStatusByCallingSiteIfNeeded(const std::string& calling_site,
                                      leveldb::Status status) {
  if (status.ok()) {
    return;
  }
  const std::string histogram_name =
      "Sync.ModelTypeStoreBackendError." + calling_site;
  base::UmaHistogramEnumeration(histogram_name,
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
}

}  // namespace

ModelTypeStoreBackend::CustomOnTaskRunnerDeleter::CustomOnTaskRunnerDeleter(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

ModelTypeStoreBackend::CustomOnTaskRunnerDeleter::CustomOnTaskRunnerDeleter(
    CustomOnTaskRunnerDeleter&&) = default;

ModelTypeStoreBackend::CustomOnTaskRunnerDeleter&
ModelTypeStoreBackend::CustomOnTaskRunnerDeleter::operator=(
    CustomOnTaskRunnerDeleter&&) = default;

ModelTypeStoreBackend::CustomOnTaskRunnerDeleter::~CustomOnTaskRunnerDeleter() =
    default;

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

  absl::optional<ModelError> error = backend->Init(path);
  DCHECK(!error);
  return backend;
}

// static
scoped_refptr<ModelTypeStoreBackend>
ModelTypeStoreBackend::CreateUninitialized() {
  return new ModelTypeStoreBackend(/*env=*/nullptr);
}

// This is a refcounted class and the destructor is safe on any sequence and
// hence DCHECK_CALLED_ON_VALID_SEQUENCE is omitted. Note that blocking
// operations in leveldb's DBImpl::~DBImpl are posted to the backend sequence
// due to the custom deleter used for |db_|.
ModelTypeStoreBackend::~ModelTypeStoreBackend() = default;

absl::optional<ModelError> ModelTypeStoreBackend::Init(
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
  }
  LogDbStatusByCallingSiteIfNeeded("Init", status);
  if (!status.ok()) {
    DCHECK(db_ == nullptr);
    return ModelError(FROM_HERE, status.ToString());
  }

  int64_t current_version = GetStoreVersion();
  if (current_version == kInvalidSchemaVersion) {
    return ModelError(FROM_HERE, "Invalid schema descriptor");
  }

  if (current_version != kLatestSchemaVersion) {
    absl::optional<ModelError> error =
        Migrate(current_version, kLatestSchemaVersion);
    if (error) {
      return error;
    }
  }
  return absl::nullopt;
}

bool ModelTypeStoreBackend::IsInitialized() const {
  return db_ != nullptr;
}

ModelTypeStoreBackend::ModelTypeStoreBackend(std::unique_ptr<leveldb::Env> env)
    : env_(std::move(env)), db_(nullptr, CustomOnTaskRunnerDeleter(nullptr)) {
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

  std::unique_ptr<leveldb::DB> tmp_db;
  const leveldb::Status status = leveldb_env::OpenDB(options, path, &tmp_db);
  // Make sure that the database is destroyed on the same sequence where it was
  // created.
  db_ = std::unique_ptr<leveldb::DB, CustomOnTaskRunnerDeleter>(
      tmp_db.release(), CustomOnTaskRunnerDeleter(
                            base::SequencedTaskRunner::GetCurrentDefault()));
  return status;
}

leveldb::Status ModelTypeStoreBackend::DestroyDatabase(const std::string& path,
                                                       leveldb::Env* env) {
  leveldb_env::Options options;
  if (env)
    options.env = env;
  return leveldb::DestroyDB(path, options);
}

absl::optional<ModelError> ModelTypeStoreBackend::ReadRecordsWithPrefix(
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
    LogDbStatusByCallingSiteIfNeeded("ReadRecords", status);
    if (status.ok()) {
      record_list->emplace_back(id, value);
    } else if (status.IsNotFound()) {
      missing_id_list->push_back(id);
    } else {
      return ModelError(FROM_HERE, status.ToString());
    }
  }
  return absl::nullopt;
}

absl::optional<ModelError> ModelTypeStoreBackend::ReadAllRecordsWithPrefix(
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
  LogDbStatusByCallingSiteIfNeeded("ReadAllRecords", iter->status());
  return iter->status().ok() ? absl::nullopt
                             : absl::optional<ModelError>(
                                   {FROM_HERE, iter->status().ToString()});
}

absl::optional<ModelError> ModelTypeStoreBackend::WriteModifications(
    std::unique_ptr<leveldb::WriteBatch> write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  leveldb::Status status =
      db_->Write(leveldb::WriteOptions(), write_batch.get());
  LogDbStatusByCallingSiteIfNeeded("WriteModifications", status);
  return status.ok()
             ? absl::nullopt
             : absl::optional<ModelError>({FROM_HERE, status.ToString()});
}

absl::optional<ModelError>
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
  LogDbStatusByCallingSiteIfNeeded("DeleteData", status);
  return status.ok()
             ? absl::nullopt
             : absl::optional<ModelError>({FROM_HERE, status.ToString()});
}

absl::optional<ModelError> ModelTypeStoreBackend::MigrateForTest(
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
    LogDbStatusByCallingSiteIfNeeded("GetStoreVersion", status);
    return kInvalidSchemaVersion;
  }
  return schema_descriptor.version_number();
}

absl::optional<ModelError> ModelTypeStoreBackend::Migrate(
    int64_t current_version,
    int64_t desired_version) {
  DCHECK(db_);
  if (current_version == 0) {
    if (Migrate0To1()) {
      current_version = 1;
    }
  }
  if (current_version == desired_version) {
    return absl::nullopt;
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

}  // namespace syncer
