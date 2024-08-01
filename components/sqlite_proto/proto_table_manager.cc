// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/proto_table_manager.h"

#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace sqlite_proto {

namespace {

const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";

}  // namespace

ProtoTableManager::ProtoTableManager(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : TableManager(db_task_runner) {}

ProtoTableManager::~ProtoTableManager() = default;

void ProtoTableManager::InitializeOnDbSequence(
    sql::Database* db,
    base::span<const std::string> table_names,
    int schema_version) {
  DCHECK(std::set<std::string>(table_names.begin(), table_names.end()).size() ==
         table_names.size());
  DCHECK(!db || db->is_open());
  table_names_.assign(table_names.begin(), table_names.end());
  schema_version_ = schema_version;
  Initialize(db);  // Superclass method.
}

void ProtoTableManager::CreateOrClearTablesIfNecessary() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (CantAccessDatabase())
    return;

  sql::Database* db = DB();  // Superclass method.

  // RazeIfIncompatible doesn't explicitly handle the case where no version
  // was previously written.
  if (!sql::MetaTable::DoesTableExist(db))
    db->Raze();
  if (sql::MetaTable::RazeIfIncompatible(
          db, /*lowest_supported_version=*/schema_version_,
          /*current_version=*/schema_version_) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    ResetDB();
    return;
  }

  sql::Transaction transaction(db);
  bool success = transaction.Begin();

  // No-ops if there's already a version stored.
  sql::MetaTable meta_table;
  success = success && meta_table.Init(db, schema_version_,
                                       /*compatible_version=*/schema_version_);

  for (const std::string& table_name : table_names_) {
    success = success &&
              (db->DoesTableExist(table_name.c_str()) ||
               db->Execute(base::StringPrintf(
                   kCreateProtoTableStatementTemplate, table_name.c_str())));
  }

  if (!success || !transaction.Commit())
    ResetDB();  // Resets our non-owning pointer; doesn't mutate the database
                // object.
}

void ProtoTableManager::WillShutdown() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  ResetDB();
}

}  // namespace sqlite_proto
