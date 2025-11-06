// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"

namespace storage {

SessionStorageLevelDB::SessionStorageLevelDB(PassKey) {}

SessionStorageLevelDB::~SessionStorageLevelDB() = default;

DbStatus SessionStorageLevelDB::Open(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  ASSIGN_OR_RETURN(leveldb_, DomStorageDatabaseLevelDB::Open(directory, name,
                                                             memory_dump_id));
  return DbStatus::OK();
}

DomStorageDatabaseLevelDB& SessionStorageLevelDB::GetLevelDB() {
  return *leveldb_;
}

StatusOr<DomStorageDatabase::Metadata>
SessionStorageLevelDB::ReadAllMetadata() {
  // TODO(crbug.com/377242771): Implement `DomStorageDatabase` for session
  // storage to make backend swappable for SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus SessionStorageLevelDB::RewriteDB() {
  return leveldb_->RewriteDB();
}

bool SessionStorageLevelDB::ShouldFailAllCommits() {
  return leveldb_->ShouldFailAllCommits();
}

void SessionStorageLevelDB::MakeAllCommitsFailForTesting() {
  leveldb_->MakeAllCommitsFailForTesting();
}

void SessionStorageLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  leveldb_->SetDestructionCallbackForTesting(std::move(callback));
}

}  // namespace storage
