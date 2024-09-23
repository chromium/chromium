// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_PROTO_PROTO_TABLE_MANAGER_H_
#define COMPONENTS_SQLITE_PROTO_PROTO_TABLE_MANAGER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sqlite_proto/table_manager.h"

namespace sqlite_proto {

// ProtoTableManager is the instance of TableManager intended to be used
// in the common case of storing string-to-proto tables (alongside
// KeyValueData<T> and KeyValueTable<T>).
//
// End-to-end use:
// 1. Initialize a database with sql::Database::Open.
// 2. Initialize a ProtoTableManager on top of this database.
// 3. For each table, construct a pair of KeyValueTable and KeyValueData
// on the main sequence; initialize the KeyValueData on the database sequence.
// 4. Now, initialization is finished and the KeyValueData objects can be
// used (on the main thread) for get/put/delete operations against the database.
//
// TODO(crbug.com/40671040): This interface is a bit complex and could do with
// a refactor.
class ProtoTableManager : public TableManager {
 public:
  // Constructor. |db_task_runner| must run tasks on the
  // sequence that subsequently calls |InitializeOnDbSequence|.
  explicit ProtoTableManager(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  // Initialization:
  // - |table_names| specifies the names of the tables to initialize.
  // These must be unique.
  // - |db| must be null or point to an open sql::Database.
  // - |schema_version|, which must be positive, will be compared to the version
  // stored on disk if any; in case of a mismatch (either there's no version
  // stored, or |schema_version| != the stored version), clears and
  // reinitializes the tables and writes the new version. It generally makes
  // sense to change the version whenever the database's layout changes in a
  // backwards-incompatible manner, for instance when removing or renaming
  // tables or columns.
  //
  // If |db| is null or initialization fails, subsequent calls to
  // ExecuteTaskOnDBThread will silently no-op. (When this class provides the
  // backing store for a collection of KeyValueData/KeyValueTable, this leads to
  // reasonable fallback behavior of the KeyValueData objects caching data in
  // memory.)
  void InitializeOnDbSequence(sql::Database* db,
                              base::span<const std::string> table_names,
                              int schema_version);

  void WillShutdown();

 protected:
  ~ProtoTableManager() override;

 private:
  // TableManager implementation.
  void CreateOrClearTablesIfNecessary() override;

  // Currently unused.
  void LogDatabaseStats() override {}

  std::vector<std::string> table_names_;
  int schema_version_;
};

}  // namespace sqlite_proto

#endif  // COMPONENTS_SQLITE_PROTO_PROTO_TABLE_MANAGER_H_
