// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_FACTORY_H_

#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class DB;
}

namespace content {
class LevelDBState;

// Factory class used to open leveldb databases, and stores all necessary
// objects in a LevelDBState. This interface exists so that it can be mocked out
// for tests.
class LevelDBFactory {
 public:
  virtual ~LevelDBFactory() = default;

  // Creates an in-memory database.
  virtual std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>
  OpenInMemoryDB(leveldb::Env* in_memory_env) = 0;

  // Opens a leveldb database with the given name. If |create_if_missing| is
  // true, then the database will be created if it does not exist.
  virtual std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const std::string& name,
      bool create_if_missing,
      size_t write_buffer_size) = 0;

  // Opens a leveldb database and returns it inside of a LevelDBState. If the
  // |file_name| is empty, then the database will be in-memory. If
  // |create_if_missing| is false and the database doesn't exist, then the
  // return tuple will be {nullptr, leveldb::Status::NotFound("", ""), false}.
  virtual std::tuple<scoped_refptr<LevelDBState>,
                     leveldb::Status,
                     /* is_disk_full= */ bool>
  OpenLevelDBState(const base::FilePath& file_name,
                   bool create_if_missing,
                   size_t write_buffer_size) = 0;

  // Assumes that there is no leveldb database currently running for this path.
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& path) = 0;
};

class DefaultLevelDBFactory : public LevelDBFactory {
 public:
  // The |database_options| are used to open any database. The
  // |create_if_missing| is supplied by Open*() calls, and |write_buffer_size|
  // is calculated during the Open* calls as well. |in_memory_db_name| is used
  // for memory profiling reporting in the in-memory database environment.
  DefaultLevelDBFactory(leveldb_env::Options database_options,
                        const std::string& in_memory_db_name);
  ~DefaultLevelDBFactory() override;

  std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenInMemoryDB(
      leveldb::Env* in_memory_env) override;

  std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const std::string& name,
      bool create_if_missing,
      size_t write_buffer_size) override;

  std::tuple<scoped_refptr<LevelDBState>,
             leveldb::Status,
             /* is_disk_full= */ bool>
  OpenLevelDBState(const base::FilePath& file_name,
                   bool create_if_missing,
                   size_t write_buffer_size) override;

  leveldb::Status DestroyLevelDB(const base::FilePath& path) override;

 protected:
  leveldb_env::Options options_;
  std::string in_memory_db_name_;

  DISALLOW_COPY_AND_ASSIGN(DefaultLevelDBFactory);
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_FACTORY_H_
