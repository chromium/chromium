// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"

#include "base/system/sys_info.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content {

DefaultLevelDBFactory::DefaultLevelDBFactory(
    leveldb_env::Options database_options,
    const std::string& in_memory_db_name)
    : options_(database_options), in_memory_db_name_(in_memory_db_name) {}
DefaultLevelDBFactory::~DefaultLevelDBFactory() = default;

std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>
DefaultLevelDBFactory::OpenInMemoryDB(leveldb::Env* in_memory_env) {
  constexpr int64_t kBytesInOneMegabyte = 1024 * 1024;
  leveldb_env::Options in_memory_options = options_;
  in_memory_options.write_buffer_size =
      /* default of 4MB */ 4 * kBytesInOneMegabyte;
  in_memory_options.paranoid_checks = false;
  in_memory_options.env = in_memory_env;
  in_memory_options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s =
      leveldb_env::OpenDB(in_memory_options, std::string(), &db);
  return {std::move(db), s};
}

std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>
DefaultLevelDBFactory::OpenDB(const std::string& name,
                              bool create_if_missing,
                              size_t write_buffer_size) {
  std::unique_ptr<leveldb::DB> db;
  options_.create_if_missing = create_if_missing;
  options_.write_buffer_size = write_buffer_size;
  leveldb::Status s = leveldb_env::OpenDB(options_, name, &db);
  return {std::move(db), s};
}

std::tuple<scoped_refptr<LevelDBState>,
           leveldb::Status,
           /* is_disk_full= */ bool>
DefaultLevelDBFactory::OpenLevelDBState(const base::FilePath& file_name,
                                        bool create_if_missing,
                                        size_t write_buffer_size) {
  leveldb::Status status;
  std::unique_ptr<leveldb::DB> db;

  if (file_name.empty()) {
    if (!create_if_missing)
      return {nullptr, leveldb::Status::NotFound("", ""), false};

    std::unique_ptr<leveldb::Env> in_memory_env =
        leveldb_chrome::NewMemEnv(in_memory_db_name_, options_.env);
    std::tie(db, status) = OpenInMemoryDB(in_memory_env.get());
    if (UNLIKELY(!status.ok())) {
      LOG(ERROR) << "Failed to open in-memory LevelDB database: "
                 << status.ToString();
      return {nullptr, status, false};
    }

    return {LevelDBState::CreateForInMemoryDB(
                std::move(in_memory_env), options_.comparator, std::move(db),
                "in-memory-database"),
            status, false};
  }

  // ChromiumEnv assumes UTF8, converts back to FilePath before using.
  std::tie(db, status) =
      OpenDB(file_name.AsUTF8Unsafe(), create_if_missing, write_buffer_size);
  if (UNLIKELY(!status.ok())) {
    if (!create_if_missing && status.IsInvalidArgument())
      return {nullptr, leveldb::Status::NotFound("", ""), false};
    constexpr int64_t kBytesInOneKilobyte = 1024;
    int64_t free_disk_space_bytes =
        base::SysInfo::AmountOfFreeDiskSpace(file_name);
    bool below_100kb = free_disk_space_bytes != -1 &&
                       free_disk_space_bytes < 100 * kBytesInOneKilobyte;

    // Disks with <100k of free space almost never succeed in opening a
    // leveldb database.
    bool is_disk_full = below_100kb || leveldb_env::IndicatesDiskFull(status);

    LOG(ERROR) << "Failed to open LevelDB database from "
               << file_name.AsUTF8Unsafe() << "," << status.ToString();
    return {nullptr, status, is_disk_full};
  }

  return {LevelDBState::CreateForDiskDB(options_.comparator, std::move(db),
                                        std::move(file_name)),
          status, false};
}

leveldb::Status DefaultLevelDBFactory::DestroyLevelDB(
    const base::FilePath& path) {
  return leveldb::DestroyDB(path.AsUTF8Unsafe(), options_);
}

}  // namespace content
