// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class LevelDBState;

// Used for unittests, this factory will only create in-memory leveldb
// databases, and will optionally allow the user to override the next
// LevelDBState returned with |EnqueueNextLevelDBState|.
class FakeLevelDBFactory : public DefaultLevelDBFactory {
 public:
  FakeLevelDBFactory(leveldb_env::Options database_options,
                     const std::string& in_memory_db_name);
  ~FakeLevelDBFactory() override;

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

  void EnqueueNextOpenDBResult(std::unique_ptr<leveldb::DB>,
                               leveldb::Status status);

  std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const std::string& name,
      bool create_if_missing,
      size_t write_buffer_size) override;

  void EnqueueNextOpenLevelDBStateResult(scoped_refptr<LevelDBState> state,
                                         leveldb::Status status,
                                         bool is_disk_full);

  std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /*disk_full*/>
  OpenLevelDBState(const base::FilePath& file_name,
                   bool create_if_missing,
                   size_t write_buffer_size) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::queue<std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>>
      next_dbs_;
  std::queue<std::tuple<scoped_refptr<LevelDBState>,
                        leveldb::Status,
                        bool /*disk_full*/>>
      next_leveldb_states_;
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
