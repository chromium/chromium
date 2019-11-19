// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_sync_metadata_database.h"

#include <memory>

#include "base/big_endian.h"
#include "base/logging.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace history {

namespace {

// Key in sql::MetaTable, the value will be Serialization of sync
// ModelTypeState, which is for tracking sync state of typed url datatype.
const char kTypedURLModelTypeStateKey[] = "typed_url_model_type_state";

}  // namespace

// Description of database table:
//
// typed_url_sync_metadata
//   storage_key      the rowid of an entry in urls table, used by service to
//                    look up native data with sync metadata records.
//   value            Serialize sync EntityMetadata, which is for tracking sync
//                    state of each typed url.

TypedURLSyncMetadataDatabase::TypedURLSyncMetadataDatabase() {}

TypedURLSyncMetadataDatabase::~TypedURLSyncMetadataDatabase() {}

bool TypedURLSyncMetadataDatabase::GetAllSyncMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);
  if (!GetAllSyncEntityMetadata(metadata_batch)) {
    return false;
  }

  sync_pb::ModelTypeState model_type_state;
  if (!GetModelTypeState(&model_type_state))
    return false;

  metadata_batch->SetModelTypeState(model_type_state);
  return true;
}

bool TypedURLSyncMetadataDatabase::UpdateSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK_EQ(model_type, syncer::TYPED_URLS)
      << "Only the TYPED_URLS model type is supported";

  sql::Statement s(GetDB().GetUniqueStatement(
      "INSERT OR REPLACE INTO typed_url_sync_metadata "
      "(storage_key, value) VALUES(?, ?)"));
  s.BindInt64(0, StorageKeyToURLID(storage_key));
  s.BindString(1, metadata.SerializeAsString());

  return s.Run();
}

bool TypedURLSyncMetadataDatabase::ClearSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key) {
  DCHECK_EQ(model_type, syncer::TYPED_URLS)
      << "Only the TYPED_URLS model type is supported";

  sql::Statement s(GetDB().GetUniqueStatement(
      "DELETE FROM typed_url_sync_metadata WHERE storage_key=?"));
  s.BindInt64(0, StorageKeyToURLID(storage_key));

  return s.Run();
}

bool TypedURLSyncMetadataDatabase::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK_EQ(model_type, syncer::TYPED_URLS)
      << "Only the TYPED_URLS model type is supported";
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);

  std::string serialized_state = model_type_state.SerializeAsString();
  return GetMetaTable().SetValue(kTypedURLModelTypeStateKey, serialized_state);
}

bool TypedURLSyncMetadataDatabase::ClearModelTypeState(
    syncer::ModelType model_type) {
  DCHECK_EQ(model_type, syncer::TYPED_URLS)
      << "Only the TYPED_URLS model type is supported";
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);
  return GetMetaTable().DeleteKey(kTypedURLModelTypeStateKey);
}

// static
URLID TypedURLSyncMetadataDatabase::StorageKeyToURLID(
    const std::string& storage_key) {
  URLID storage_key_int = 0;
  DCHECK_EQ(storage_key.size(), sizeof(storage_key_int));
  base::ReadBigEndian(storage_key.data(), &storage_key_int);
  // Make sure storage_key_int is set.
  DCHECK_NE(storage_key_int, 0);
  return storage_key_int;
}

bool TypedURLSyncMetadataDatabase::InitSyncTable() {
  if (!GetDB().DoesTableExist("typed_url_sync_metadata")) {
    if (!GetDB().Execute("CREATE TABLE typed_url_sync_metadata ("
                         "storage_key INTEGER PRIMARY KEY NOT NULL,"
                         "value BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool TypedURLSyncMetadataDatabase::
    CleanTypedURLOrphanedMetadataForMigrationToVersion40(
        const std::vector<URLID>& sorted_valid_rowids) {
  DCHECK(
      std::is_sorted(sorted_valid_rowids.begin(), sorted_valid_rowids.end()));
  std::vector<URLID> invalid_metadata_rowids;
  auto valid_rowids_iter = sorted_valid_rowids.begin();

  sql::Statement sorted_metadata_rowids(GetDB().GetUniqueStatement(
      "SELECT storage_key FROM typed_url_sync_metadata ORDER BY storage_key"));
  while (sorted_metadata_rowids.Step()) {
    URLID metadata_rowid = sorted_metadata_rowids.ColumnInt64(0);
    // Both collections are sorted, we check whether |metadata_rowid| is valid
    // by iterating both at the same time.

    // First, skip all valid IDs that are omitted in |sorted_metadata_rowids|.
    while (valid_rowids_iter != sorted_valid_rowids.end() &&
           *valid_rowids_iter < metadata_rowid) {
      valid_rowids_iter++;
    }
    // Now, is |metadata_rowid| invalid?
    if (valid_rowids_iter == sorted_valid_rowids.end() ||
        *valid_rowids_iter != metadata_rowid) {
      invalid_metadata_rowids.push_back(metadata_rowid);
    }
  }

  if (!sorted_metadata_rowids.Succeeded()) {
    return false;
  }

  for (const URLID& rowid : invalid_metadata_rowids) {
    sql::Statement del(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM typed_url_sync_metadata WHERE storage_key=?"));
    del.BindInt64(0, rowid);
    if (!del.Run())
      return false;
  }

  return true;
}

bool TypedURLSyncMetadataDatabase::GetAllSyncEntityMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);
  sql::Statement s(GetDB().GetUniqueStatement(
      "SELECT storage_key, value FROM typed_url_sync_metadata"));

  while (s.Step()) {
    std::string storage_key(sizeof(URLID), 0);
    base::WriteBigEndian<URLID>(&storage_key[0], s.ColumnInt64(0));
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize TYPED_URLS model type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
  }
  return true;
}

bool TypedURLSyncMetadataDatabase::GetModelTypeState(
    sync_pb::ModelTypeState* state) {
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);
  std::string serialized_state;
  if (!GetMetaTable().GetValue(kTypedURLModelTypeStateKey, &serialized_state)) {
    return true;
  }

  return state->ParseFromString(serialized_state);
}

}  // namespace history
