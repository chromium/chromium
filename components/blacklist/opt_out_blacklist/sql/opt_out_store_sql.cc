// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blacklist/opt_out_blacklist/sql/opt_out_store_sql.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace blacklist {

namespace {

// Command line switch to change the entry per host DB size.
const char kMaxRowsPerHost[] = "max-opt-out-rows-per-host";

// Command line switch to change the DB size.
const char kMaxRows[] = "max-opt-out-rows";

// Returns the maximum number of table rows allowed per host for the sql
// opt out store. This is enforced during insertion of new navigation entries.
int MaxRowsPerHostInOptOutDB() {
  std::string max_rows =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMaxRowsPerHost);
  int value;
  return base::StringToInt(max_rows, &value) ? value : 32;
}

// Returns the maximum number of table rows allowed for the blacklist opt out
// store. This is enforced during load time; thus the database can grow
// larger than this temporarily.
int MaxRowsInOptOutDB() {
  std::string max_rows =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kMaxRows);
  int value;
  return base::StringToInt(max_rows, &value) ? value : 3200;
}

// Table names use a macro instead of a const, so they can be used inline in
// other SQL statements below.

// The Opt Out table holds entries for hosts that should not use a specified
// type. Historically, this was named previews_v1.
#define OPT_OUT_TABLE_NAME "previews_v1"

// The Enabled types table hold the list of enabled types
// treatments with a version for that enabled treatment. If the version
// changes or the type becomes disabled, then any entries in the Opt Out
// table for that treatment type should be cleared. Historically, this was named
// enabled_previews_v1.
#define ENABLED_TYPES_TABLE_NAME "enabled_previews_v1"

void CreateSchema(sql::Database* db) {
  static const char kSqlCreateTable[] =
      "CREATE TABLE IF NOT EXISTS " OPT_OUT_TABLE_NAME
      " (host_name VARCHAR NOT NULL,"
      " time INTEGER NOT NULL,"
      " opt_out INTEGER NOT NULL,"
      " type INTEGER NOT NULL,"
      " PRIMARY KEY(host_name, time DESC, opt_out, type))";
  if (!db->Execute(kSqlCreateTable))
    return;

  static const char kSqlCreateEnabledTypeVersionTable[] =
      "CREATE TABLE IF NOT EXISTS " ENABLED_TYPES_TABLE_NAME
      " (type INTEGER NOT NULL,"
      " version INTEGER NOT NULL,"
      " PRIMARY KEY(type))";
  if (!db->Execute(kSqlCreateEnabledTypeVersionTable))
    return;
}

void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(WARNING) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }
}

void InitDatabase(sql::Database* db, base::FilePath path) {
  // The entry size should be between 11 and 10 + x bytes, where x is the the
  // length of the host name string in bytes.
  // The total number of entries per host is bounded at 32, and the total number
  // of hosts is currently unbounded (but typically expected to be under 100).
  // Assuming average of 100 bytes per entry, and 100 hosts, the total size will
  // be 4096 * 78. 250 allows room for extreme cases such as many host names
  // or very long host names.
  // The average case should be much smaller as users rarely visit hosts that
  // are not in their top 20 hosts. It should be closer to 32 * 100 * 20 for
  // most users, which is about 4096 * 15.
  // The total size of the database will be capped at 3200 entries.
  db->set_page_size(4096);
  db->set_cache_size(250);
  db->set_histogram_tag("OptOutBlacklist");
  db->set_exclusive_locking();

  db->set_error_callback(base::BindRepeating(&DatabaseErrorCallback, db, path));

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(path.DirName(), &err)) {
    return;
  }
  if (!db->Open(path)) {
    return;
  }

  CreateSchema(db);
}

// Adds a new OptOut entry to the data base.
void AddEntryToDataBase(sql::Database* db,
                        bool opt_out,
                        const std::string& host_name,
                        int type,
                        base::Time now) {
  // Adds the new entry.
  static const char kSqlInsert[] = "INSERT INTO " OPT_OUT_TABLE_NAME
                                   " (host_name, time, opt_out, type)"
                                   " VALUES "
                                   " (?, ?, ?, ?)";

  sql::Statement statement_insert(
      db->GetCachedStatement(SQL_FROM_HERE, kSqlInsert));
  statement_insert.BindString(0, host_name);
  statement_insert.BindInt64(1, (now - base::Time()).InMicroseconds());
  statement_insert.BindBool(2, opt_out);
  statement_insert.BindInt(3, type);
  statement_insert.Run();
}

// Removes OptOut entries for |host_name| if the per-host row limit is exceeded.
// Removes OptOut entries if per data base row limit is exceeded.
void MaybeEvictHostEntryFromDataBase(sql::Database* db,
                                     const std::string& host_name) {
  // Delete the oldest entries if there are more than |MaxRowsPerHostInOptOutDB|
  // for |host_name|.
  // DELETE ... LIMIT -1 OFFSET x means delete all but the first x entries.
  static const char kSqlDeleteByHost[] =
      "DELETE FROM " OPT_OUT_TABLE_NAME
      " WHERE ROWID IN"
      " (SELECT ROWID from " OPT_OUT_TABLE_NAME
      " WHERE host_name == ?"
      " ORDER BY time DESC"
      " LIMIT -1 OFFSET ?)";

  sql::Statement statement_delete_by_host(
      db->GetCachedStatement(SQL_FROM_HERE, kSqlDeleteByHost));
  statement_delete_by_host.BindString(0, host_name);
  statement_delete_by_host.BindInt(1, MaxRowsPerHostInOptOutDB());
  statement_delete_by_host.Run();
}

// Deletes every entry for |type|.
void ClearBlacklistForTypeInDataBase(sql::Database* db, int type) {
  static const char kSql[] =
      "DELETE FROM " OPT_OUT_TABLE_NAME " WHERE type == ?";
  sql::Statement statement(db->GetUniqueStatement(kSql));
  statement.BindInt(0, type);
  statement.Run();
}

// Retrieves the list of previously enabled types with their version from the
// Enabled table.
BlacklistData::AllowedTypesAndVersions GetStoredEntries(sql::Database* db) {
  static const char kSqlLoadEnabledTypesVersions[] =
      "SELECT type, version FROM " ENABLED_TYPES_TABLE_NAME;

  sql::Statement statement(
      db->GetUniqueStatement(kSqlLoadEnabledTypesVersions));

  BlacklistData::AllowedTypesAndVersions stored_entries;
  while (statement.Step()) {
    int type = statement.ColumnInt(0);
    int version = statement.ColumnInt(1);
    stored_entries.insert({type, version});
  }
  return stored_entries;
}

// Adds a newly enabled |type| with its |version| to the Enabled types table.
void InsertEnabledTypesInDataBase(sql::Database* db, int type, int version) {
  static const char kSqlInsert[] = "INSERT INTO " ENABLED_TYPES_TABLE_NAME
                                   " (type, version)"
                                   " VALUES "
                                   " (?, ?)";

  sql::Statement statement_insert(db->GetUniqueStatement(kSqlInsert));
  statement_insert.BindInt(0, type);
  statement_insert.BindInt(1, version);
  statement_insert.Run();
}

// Updates the |version| of an enabled |type| in the Enabled table.
void UpdateEnabledTypesInDataBase(sql::Database* db, int type, int version) {
  static const char kSqlUpdate[] = "UPDATE " ENABLED_TYPES_TABLE_NAME
                                   " SET version = ?"
                                   " WHERE type = ?";

  sql::Statement statement_update(
      db->GetCachedStatement(SQL_FROM_HERE, kSqlUpdate));
  statement_update.BindInt(0, version);
  statement_update.BindInt(1, type);
  statement_update.Run();
}

// Checks the current set of enabled types (with their current version)
// and where a type is now disabled or has a different version, cleans up
// any associated blacklist entries.
void CheckAndReconcileEnabledTypesWithDataBase(
    sql::Database* db,
    const BlacklistData::AllowedTypesAndVersions& allowed_types) {
  BlacklistData::AllowedTypesAndVersions stored_entries = GetStoredEntries(db);

  for (auto enabled_it : allowed_types) {
    int type = enabled_it.first;
    int current_version = enabled_it.second;
    auto stored_it = stored_entries.find(type);
    if (stored_it == stored_entries.end()) {
      InsertEnabledTypesInDataBase(db, type, current_version);
    } else {
      if (stored_it->second != current_version) {
        DCHECK_GE(current_version, stored_it->second);
        ClearBlacklistForTypeInDataBase(db, type);
        UpdateEnabledTypesInDataBase(db, type, current_version);
      }
    }
  }
  // Do not delete types that are not in |allowed_types|. They will get cleaned
  // up eventually when they expire if the type is truly gone. However, if the
  // type has been removed temporarily (like in a holdback experiment), then
  // it'll still be around for the next time it is used.
}

void LoadBlackListFromDataBase(
    sql::Database* db,
    std::unique_ptr<BlacklistData> blacklist_data,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    LoadBlackListCallback callback) {
  // First handle any update needed wrt enabled types and their versions.
  CheckAndReconcileEnabledTypesWithDataBase(db,
                                            blacklist_data->allowed_types());

  // Gets the table sorted by host and time. Limits the number of hosts using
  // most recent opt_out time as the limiting function. Sorting is free due to
  // the table structure, and it improves performance in the loop below.
  static const char kSql[] =
      "SELECT host_name, time, opt_out, type"
      " FROM " OPT_OUT_TABLE_NAME " ORDER BY host_name, time DESC";

  sql::Statement statement(db->GetUniqueStatement(kSql));

  int count = 0;
  while (statement.Step()) {
    ++count;
    blacklist_data->AddEntry(statement.ColumnString(0), statement.ColumnBool(2),
                             statement.ColumnInt64(3),
                             base::Time() + base::TimeDelta::FromMicroseconds(
                                                statement.ColumnInt64(1)),
                             true);
  }

  if (count > MaxRowsInOptOutDB()) {
    // Delete the oldest entries if there are more than |kMaxEntriesInDB|.
    // DELETE ... LIMIT -1 OFFSET x means delete all but the first x entries.
    static const char kSqlDeleteByDBSize[] =
        "DELETE FROM " OPT_OUT_TABLE_NAME
        " WHERE ROWID IN"
        " (SELECT ROWID from " OPT_OUT_TABLE_NAME
        " ORDER BY time DESC"
        " LIMIT -1 OFFSET ?)";

    sql::Statement statement_delete(
        db->GetCachedStatement(SQL_FROM_HERE, kSqlDeleteByDBSize));
    statement_delete.BindInt(0, MaxRowsInOptOutDB());
    statement_delete.Run();
  }

  runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback),
                                             std::move(blacklist_data)));
}

// Synchronous implementations, these are run on the background thread
// and actually do the work to access the SQL data base.
void LoadBlackListSync(sql::Database* db,
                       const base::FilePath& path,
                       std::unique_ptr<BlacklistData> blacklist_data,
                       scoped_refptr<base::SingleThreadTaskRunner> runner,
                       LoadBlackListCallback callback) {
  if (!db->is_open())
    InitDatabase(db, path);

  LoadBlackListFromDataBase(db, std::move(blacklist_data), runner,
                            std::move(callback));
}

// Deletes every row in the table that has entry time between |begin_time| and
// |end_time|.
void ClearBlackListSync(sql::Database* db,
                        base::Time begin_time,
                        base::Time end_time) {
  static const char kSql[] =
      "DELETE FROM " OPT_OUT_TABLE_NAME " WHERE time >= ? and time <= ?";

  sql::Statement statement(db->GetUniqueStatement(kSql));
  statement.BindInt64(0, (begin_time - base::Time()).InMicroseconds());
  statement.BindInt64(1, (end_time - base::Time()).InMicroseconds());
  statement.Run();
}

void AddEntrySync(bool opt_out,
                  const std::string& host_name,
                  int type,
                  base::Time now,
                  sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return;
  AddEntryToDataBase(db, opt_out, host_name, type, now);
  MaybeEvictHostEntryFromDataBase(db, host_name);
  transaction.Commit();
}

}  // namespace

OptOutStoreSQL::OptOutStoreSQL(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : io_task_runner_(io_task_runner),
      background_task_runner_(background_task_runner),
      db_file_path_(path) {}

OptOutStoreSQL::~OptOutStoreSQL() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (db_) {
    background_task_runner_->DeleteSoon(FROM_HERE, db_.release());
  }
}

void OptOutStoreSQL::AddEntry(bool opt_out,
                              const std::string& host_name,
                              int type,
                              base::Time now) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(db_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AddEntrySync, opt_out, host_name, type, now, db_.get()));
}

void OptOutStoreSQL::ClearBlackList(base::Time begin_time,
                                    base::Time end_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(db_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearBlackListSync, db_.get(), begin_time, end_time));
}

void OptOutStoreSQL::LoadBlackList(
    std::unique_ptr<BlacklistData> blacklist_data,
    LoadBlackListCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!db_)
    db_ = std::make_unique<sql::Database>();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadBlackListSync, db_.get(), db_file_path_,
                     std::move(blacklist_data),
                     base::ThreadTaskRunnerHandle::Get(), std::move(callback)));
}

}  // namespace blacklist
