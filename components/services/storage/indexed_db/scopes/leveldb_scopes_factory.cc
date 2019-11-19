// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_factory.h"

#include <utility>

#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

namespace content {

LevelDBScopesOptions::LevelDBScopesOptions() = default;
LevelDBScopesOptions::~LevelDBScopesOptions() = default;
LevelDBScopesOptions::LevelDBScopesOptions(LevelDBScopesOptions&&) noexcept =
    default;
LevelDBScopesOptions& LevelDBScopesOptions::operator=(
    LevelDBScopesOptions&&) noexcept = default;

std::tuple<std::unique_ptr<LevelDBScopes>, leveldb::Status>
DefaultLevelDBScopesFactory::CreateAndInitializeLevelDBScopes(
    LevelDBScopesOptions options,
    scoped_refptr<LevelDBState> level_db) {
  std::unique_ptr<LevelDBScopes> scopes = std::make_unique<LevelDBScopes>(
      std::move(options.metadata_key_prefix), options.max_write_batch_size,
      std::move(level_db), options.lock_manager,
      std::move(options.failure_callback));
  leveldb::Status s = scopes->Initialize();
  return {s.ok() ? std::move(scopes) : nullptr, s};
}

}  // namespace content
