// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/fake_dom_storage_database.h"

#include <utility>

#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"

namespace storage {

FakeDomStorageDatabase::FakeDomStorageDatabase(DbStatus open_status)
    : open_status_(std::move(open_status)) {
  // After a successful Open(), SessionStorageImpl calls ReadAllMetadata() and
  // passes the result to SessionStorageMetadata::Initialize(), which accesses
  // next_map_id.value() unconditionally. Real databases always populate
  // next_map_id, so we do the same to avoid a crash.
  Metadata metadata;
  metadata.next_map_id = 0;
  read_all_metadata_result_ = std::move(metadata);
}

FakeDomStorageDatabase::~FakeDomStorageDatabase() = default;

void FakeDomStorageDatabase::SetReadAllMetadataResult(
    StatusOr<Metadata> result) {
  read_all_metadata_result_ = std::move(result);
}

DbStatus FakeDomStorageDatabase::Open(
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  return std::move(open_status_);
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
FakeDomStorageDatabase::ReadMapKeyValues(MapLocator map_locator) {
  return std::map<Key, Value>();
}

void FakeDomStorageDatabase::SetUpdateMapsStatus(DbStatus status) {
  update_maps_status_ = std::move(status);
}

DbStatus FakeDomStorageDatabase::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  return update_maps_status_;
}

DbStatus FakeDomStorageDatabase::CloneMap(MapLocator source_map,
                                          MapLocator target_map) {
  return DbStatus::OK();
}

StatusOr<DomStorageDatabase::Metadata>
FakeDomStorageDatabase::ReadAllMetadata() {
  // Return a copy so this can be called multiple times.
  if (!read_all_metadata_result_.has_value()) {
    return base::unexpected(read_all_metadata_result_.error());
  }
  return CloneMetadata(read_all_metadata_result_.value());
}

DbStatus FakeDomStorageDatabase::PutMetadata(Metadata metadata) {
  return DbStatus::OK();
}

DbStatus FakeDomStorageDatabase::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  return DbStatus::OK();
}

DbStatus FakeDomStorageDatabase::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  return DbStatus::OK();
}

DbStatus FakeDomStorageDatabase::PurgeOrigins(std::set<url::Origin> origins) {
  return DbStatus::OK();
}

DbStatus FakeDomStorageDatabase::CleanUpStaleData() {
  return DbStatus::OK();
}

DbStatus FakeDomStorageDatabase::PutVersionForTesting(int64_t version) {
  return DbStatus::OK();
}

void FakeDomStorageDatabase::MakeAllCommitsFailForTesting() {}

void FakeDomStorageDatabase::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {}

}  // namespace storage
