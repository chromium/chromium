// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/local_page_entities_metadata_provider.h"

#include "components/optimization_guide/core/entity_metadata.h"

namespace optimization_guide {

namespace {

// The amount of data to build up in memory before converting to a sorted on-
// disk file.
constexpr size_t kDatabaseWriteBufferSizeBytes = 128 * 1024;

}  // namespace

LocalPageEntitiesMetadataProvider::LocalPageEntitiesMetadataProvider() =
    default;
LocalPageEntitiesMetadataProvider::~LocalPageEntitiesMetadataProvider() =
    default;

void LocalPageEntitiesMetadataProvider::Initialize(
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  background_task_runner_ = std::move(background_task_runner);
  database_ = database_provider->GetDB<proto::EntityMetadataStorage>(
      leveldb_proto::ProtoDbType::PAGE_ENTITY_METADATA_STORE, database_dir,
      background_task_runner_);

  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  database_->Init(
      options,
      base::BindOnce(&LocalPageEntitiesMetadataProvider::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalPageEntitiesMetadataProvider::InitializeForTesting(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::EntityMetadataStorage>>
        database,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  database_ = std::move(database);
  background_task_runner_ = std::move(background_task_runner);
}

void LocalPageEntitiesMetadataProvider::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    database_.reset();
    return;
  }
}

void LocalPageEntitiesMetadataProvider::GetMetadataForEntityId(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!database_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  database_->GetEntry(
      entity_id, base::BindOnce(&LocalPageEntitiesMetadataProvider::OnGotEntry,
                                weak_ptr_factory_.GetWeakPtr(), entity_id,
                                std::move(callback)));
}

void LocalPageEntitiesMetadataProvider::OnGotEntry(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback,
    bool success,
    std::unique_ptr<proto::EntityMetadataStorage> entry) {
  if (!success || !entry) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  EntityMetadata md;
  md.entity_id = entity_id;
  md.human_readable_name = entry->entity_name();

  std::move(callback).Run(md);
}

}  // namespace optimization_guide