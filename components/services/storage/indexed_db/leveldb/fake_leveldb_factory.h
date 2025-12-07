// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content::indexed_db {
class LevelDBState;

class FakeLevelDBFactory {
 public:
  FakeLevelDBFactory() = delete;

  struct FlakePoint {
    int calls_before_flake;
    leveldb::Status flake_status;
    std::string replaced_get_result;
  };

  // The returned callback will trigger the database to be broken, and forever
  // return the given status.
  static std::unique_ptr<leveldb::DB> CreateFlakyDB(
      std::unique_ptr<leveldb::DB> db,
      std::queue<FlakePoint> flake_points);

  static scoped_refptr<LevelDBState> GetBrokenLevelDB(
      leveldb::Status error_to_return,
      const base::FilePath& reported_file_path);

  // The returned callback will trigger the database to be broken, and forever
  // return the given status.
  static std::pair<std::unique_ptr<leveldb::DB>,
                   base::OnceCallback<void(leveldb::Status)>>
  CreateBreakableDB(std::unique_ptr<leveldb::DB> db);
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
