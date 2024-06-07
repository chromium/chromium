// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_database.h"

#include <inttypes.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/download_slice_info.h"
#include "components/history/core/browser/history_types.h"
#include "sql/statement.h"

namespace history {

namespace {

#define DOWNLOADS_TABLE "downloads"
#define DOWNLOADS_SLICES_TABLE "downloads_slices"

// Reason for dropping a particular record. Used for UMA.
enum DroppedReason {
  DROPPED_REASON_BAD_STATE = 0,
  DROPPED_REASON_BAD_DANGER_TYPE = 1,
  DROPPED_REASON_BAD_ID = 2,
  DROPPED_REASON_DUPLICATE_ID = 3,
  DROPPED_REASON_MAX
};

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// Binds/reads the given file path to the given column of the given statement.
void BindFilePath(sql::Statement& statement,
                  const base::FilePath& path,
                  int col) {
  statement.BindString(col, path.value());
}
base::FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return base::FilePath(statement.ColumnString(col));
}

#else

// See above.
void BindFilePath(sql::Statement& statement,
                  const base::FilePath& path,
                  int col) {
  statement.BindString(col, path.AsUTF8Unsafe());
}
base::FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return base::FilePath::FromUTF8Unsafe(statement.ColumnString(col));
}

#endif

}  // namespace

DownloadDatabase::DownloadDatabase(
    DownloadInterruptReason download_interrupt_reason_none,
    DownloadInterruptReason download_interrupt_reason_crash)
    : owning_thread_set_(false),
      owning_thread_(0),
      in_progress_entry_cleanup_completed_(false),
      download_interrupt_reason_none_(download_interrupt_reason_none),
      download_interrupt_reason_crash_(download_interrupt_reason_crash) {
}

DownloadDatabase::~DownloadDatabase() {
}

bool DownloadDatabase::EnsureColumnExists(const std::string& name,
                                          const std::string& type) {
  return EnsureColumnExistsInTable(DOWNLOADS_TABLE, name, type);
}

bool DownloadDatabase::EnsureColumnExistsInTable(const std::string& table,
                                                 const std::string& name,
                                                 const std::string& type) {
  std::string add_col =
      base::StrCat({"ALTER TABLE ", table, " ADD COLUMN ", name, " ", type});
  return GetDB().DoesColumnExist(table, name) || GetDB().Execute(add_col);
}

bool DownloadDatabase::MigrateMimeType() {
  return EnsureColumnExists("mime_type", "VARCHAR(255) NOT NULL"
                            " DEFAULT \"\"") &&
         EnsureColumnExists("original_mime_type", "VARCHAR(255) NOT NULL"
                            " DEFAULT \"\"");
}

bool DownloadDatabase::MigrateDownloadsState() {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "UPDATE " DOWNLOADS_TABLE " SET state=? WHERE state=?"));
  statement.BindInt(0, DownloadStateToInt(DownloadState::INTERRUPTED));
  statement.BindInt(1, DownloadStateToInt(DownloadState::BUG_140687));
  return statement.Run();
}

bool DownloadDatabase::MigrateDownloadsReasonPathsAndDangerType() {
  // We need to rename the table and copy back from it because SQLite
  // provides no way to rename or delete a column.
  if (!GetDB().Execute("ALTER TABLE " DOWNLOADS_TABLE
                       " RENAME TO downloads_tmp")) {
    return false;
  }

  const constexpr char kReasonPathDangerSchema[] =
      "CREATE TABLE " DOWNLOADS_TABLE
      "("
      "id INTEGER PRIMARY KEY,"
      "current_path LONGVARCHAR NOT NULL,"
      "target_path LONGVARCHAR NOT NULL,"
      "start_time INTEGER NOT NULL,"
      "received_bytes INTEGER NOT NULL,"
      "total_bytes INTEGER NOT NULL,"
      "state INTEGER NOT NULL,"
      "danger_type INTEGER NOT NULL,"
      "interrupt_reason INTEGER NOT NULL,"
      "end_time INTEGER NOT NULL,"
      "opened INTEGER NOT NULL)";

  static constexpr char kReasonPathDangerUrlChainSchema[] =
      "CREATE TABLE downloads_url_chains ("
      "id INTEGER NOT NULL,"           // downloads.id.
      "chain_index INTEGER NOT NULL,"  // Index of url in chain
                                       // 0 is initial target,
                                       // MAX is target after redirects.
      "url LONGVARCHAR NOT NULL, "     // URL.
      "PRIMARY KEY (id, chain_index) )";

  // Recreate main table.
  if (!GetDB().Execute(kReasonPathDangerSchema)) {
    return false;
  }

  // Populate it.  As we do so, we transform the time values from time_t
  // (seconds since 1/1/1970 UTC), to our internal measure (microseconds
  // since the Windows Epoch).  Note that this is dependent on the
  // internal representation of base::Time and needs to change if that changes.
  sql::Statement statement_populate(GetDB().GetUniqueStatement(
      // clang-format: off
      "INSERT INTO " DOWNLOADS_TABLE
      "(id,current_path,target_path,start_time,received_bytes,"
      "total_bytes,state,danger_type,interrupt_reason,end_time,"
      "opened)"
      "SELECT id,full_path,full_path,"
      "CASE start_time WHEN 0 THEN 0 ELSE "
      "(start_time + 11644473600) * 1000000 END,"
      "received_bytes,total_bytes,state,?,?,"
      "CASE end_time WHEN 0 THEN 0 ELSE "
      "(end_time + 11644473600) * 1000000 END,"
      "opened "
      "FROM downloads_tmp"));
  // clang-format: on
  statement_populate.BindInt(
      0, DownloadInterruptReasonToInt(download_interrupt_reason_none_));
  statement_populate.BindInt(
      1, DownloadDangerTypeToInt(DownloadDangerType::NOT_DANGEROUS));
  if (!statement_populate.Run())
    return false;

  // Create new chain table and populate it.
  if (!GetDB().Execute(kReasonPathDangerUrlChainSchema))
    return false;

  if (!GetDB().Execute("INSERT INTO downloads_url_chains "
                       "  ( id, chain_index, url) "
                       "  SELECT id, 0, url from downloads_tmp"))
    return false;

  // Get rid of temporary table.
  if (!GetDB().Execute("DROP TABLE downloads_tmp"))
    return false;

  return true;
}

bool DownloadDatabase::MigrateReferrer() {
  return EnsureColumnExists("referrer", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::MigrateDownloadedByExtension() {
  return EnsureColumnExists("by_ext_id", "VARCHAR NOT NULL DEFAULT \"\"") &&
         EnsureColumnExists("by_ext_name", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::MigrateDownloadValidators() {
  return EnsureColumnExists("etag", "VARCHAR NOT NULL DEFAULT \"\"") &&
         EnsureColumnExists("last_modified", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::MigrateHashHttpMethodAndGenerateGuids() {
  if (!EnsureColumnExists("guid", "VARCHAR NOT NULL DEFAULT ''") ||
      !EnsureColumnExists("hash", "BLOB NOT NULL DEFAULT X''") ||
      !EnsureColumnExists("http_method", "VARCHAR NOT NULL DEFAULT ''"))
    return false;

  // Generate GUIDs for each download. GUIDs based on random data should conform
  // with RFC 4122 section 4.4. Given the following field layout (based on RFC
  // 4122):
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                          time_low                             |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |       time_mid                |         time_hi_and_version   |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                         node (2-5)                            |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // * Bits 4-7 of time_hi_and_version should be set to 0b0100 == 4
  // * Bits 6-7 of clk_seq_hi_res should be set to 0b10
  // * All other bits should be random or pseudorandom.
  //
  // We are going to take the liberty of setting time_low to the 32-bit download
  // ID. That will guarantee that no two randomly generated GUIDs will collide
  // even if the 90 bits of entropy doesn't save us.
  //
  // Translated to the canonical string representation, the GUID is generated
  // thusly:
  //
  //    XXXXXXXX-RRRR-4RRR-yRRR-RRRRRRRRRRRR
  //    \__  __/ \___________  ____________/
  //       \/                \/
  //       |          R = random hex digit.
  //       |          y = one of {'8','9','A','B'} selected randomly.
  //       |          4 = the character '4'.
  //       |
  //       Hex representation of 32-bit download ID.
  //
  // This GUID generation scheme is only used for migrated download rows and
  // assumes that the likelihood of a collision with a GUID generated via
  // base::Uuid::GenerateRandomV4().AsLowercaseString() will be vanishingly
  // small.
  //
  // A previous version of this code generated GUIDs that used random bits for
  // all but the first 32-bits. I.e. the scheme didn't respect the 6 fixed bits
  // as prescribed for type 4 GUIDs. The resulting GUIDs are not believed to
  // have an elevated risk of collision with GUIDs generated via
  // base::Uuid::GenerateRandomV4().AsLowercaseString() and are considered valid
  // by all known consumers. Hence no additional migration logic is being
  // introduced to fix those GUIDs.
  sql::Statement select(
      GetDB().GetUniqueStatement("SELECT id FROM " DOWNLOADS_TABLE));
  sql::Statement update(GetDB().GetUniqueStatement(
      "UPDATE " DOWNLOADS_TABLE " SET guid = ? WHERE id = ?"));
  while (select.Step()) {
    int id = select.ColumnInt(0);
    uint64_t r1 = base::RandUint64();
    uint64_t r2 = base::RandUint64();
    std::string guid = base::StringPrintf(
        "%08" PRIX32 "-%04" PRIX64 "-4%03" PRIX64 "-%04" PRIX64 "-%012" PRIX64,
        id, r1 >> 48,
        (r1 >> 36) & 0xfff,
        ((8 | ((r1 >> 34) & 3)) << 12) | ((r1 >> 22) & 0xfff),
        r2 & 0xffffffffffff);
    update.BindString(0, guid);
    update.BindInt(1, id);
    if (!update.Run())
      return false;
    update.Reset(true);
  }
  return true;
}

bool DownloadDatabase::MigrateDownloadTabUrl() {
  return EnsureColumnExists("tab_url", "VARCHAR NOT NULL DEFAULT ''") &&
         EnsureColumnExists("tab_referrer_url", "VARCHAR NOT NULL DEFAULT ''");
}

bool DownloadDatabase::MigrateDownloadSiteInstanceUrl() {
  return EnsureColumnExists("site_url", "VARCHAR NOT NULL DEFAULT ''");
}

bool DownloadDatabase::MigrateEmbedderDownloadData() {
  return EnsureColumnExists("embedder_download_data",
                            "VARCHAR NOT NULL DEFAULT ''");
}

bool DownloadDatabase::MigrateDownloadLastAccessTime() {
  return EnsureColumnExists("last_access_time", "INTEGER NOT NULL DEFAULT 0");
}

bool DownloadDatabase::MigrateDownloadTransient() {
  return EnsureColumnExists("transient", "INTEGER NOT NULL DEFAULT 0");
}

bool DownloadDatabase::MigrateDownloadSliceFinished() {
  return EnsureColumnExistsInTable(DOWNLOADS_SLICES_TABLE, "finished",
                                   "INTEGER NOT NULL DEFAULT 0");
}

bool DownloadDatabase::MigrateDownloadByWebApp() {
  return EnsureColumnExists("by_web_app_id", "VARCHAR NOT NULL DEFAULT ''");
}

bool DownloadDatabase::InitDownloadTable() {
  static constexpr char kSchema[] =
      "CREATE TABLE " DOWNLOADS_TABLE
      " ("
      "id INTEGER PRIMARY KEY,"             // Primary key.
      "guid VARCHAR NOT NULL,"              // GUID.
      "current_path LONGVARCHAR NOT NULL,"  // Current disk location
      "target_path LONGVARCHAR NOT NULL,"   // Final disk location
      "start_time INTEGER NOT NULL,"        // When the download was started.
      "received_bytes INTEGER NOT NULL,"    // Total size downloaded.
      "total_bytes INTEGER NOT NULL,"       // Total size of the download.
      "state INTEGER NOT NULL,"             // 1=complete, 4=interrupted
      "danger_type INTEGER NOT NULL,"       // Danger type, validated.
      "interrupt_reason INTEGER NOT NULL,"  // DownloadInterruptReason
      "hash BLOB NOT NULL,"                 // Raw SHA-256 hash of contents.
      "end_time INTEGER NOT NULL,"          // When the download completed.
      "opened INTEGER NOT NULL,"            // 1 if it has ever been opened
                                            // else 0
      "last_access_time INTEGER NOT NULL,"  // The last time it was
                                            // accessed.
      "transient INTEGER NOT NULL,"         // 1 if it is transient, else 0.
      "referrer VARCHAR NOT NULL,"          // HTTP Referrer
      "site_url VARCHAR NOT NULL,"          // Site URL for initiating site
                                            // instance.
      "embedder_download_data VARCHAR NOT NULL,"  // Serialized proto for
                                                  // embedder data pertaining to
                                                  // the initiating site
                                                  // instance.
      "tab_url VARCHAR NOT NULL,"                 // Tab URL for initiator.
      "tab_referrer_url VARCHAR NOT NULL,"        // Tag referrer URL for
                                                  // initiator.
      "http_method VARCHAR NOT NULL,"             // HTTP method.
      "by_ext_id VARCHAR NOT NULL,"       // ID of extension that started the
                                          // download
      "by_ext_name VARCHAR NOT NULL,"     // name of extension
      "by_web_app_id VARCHAR NOT NULL,"   // ID of web app that started the
                                          // download.
      "etag VARCHAR NOT NULL,"            // ETag
      "last_modified VARCHAR NOT NULL,"   // Last-Modified header
      "mime_type VARCHAR(255) NOT NULL,"  // MIME type.
      "original_mime_type VARCHAR(255) NOT NULL)"  // Original MIME type.
      ;

  static constexpr char kUrlChainSchema[] =
      "CREATE TABLE downloads_url_chains ("
      "id INTEGER NOT NULL,"           // downloads.id.
      "chain_index INTEGER NOT NULL,"  // Index of url in chain
                                       // 0 is initial target,
                                       // MAX is target after redirects.
      "url LONGVARCHAR NOT NULL, "     // URL.
      "PRIMARY KEY (id, chain_index) )";

  static constexpr char kSlicesSchema[] =
      "CREATE TABLE " DOWNLOADS_SLICES_TABLE
      " ("
      "download_id INTEGER NOT NULL,"         // downloads.id.
      "offset INTEGER NOT NULL,"              // Offset in the target file.
      "received_bytes INTEGER NOT NULL,"      // Total bytes downloaded.
      "finished INTEGER NOT NULL DEFAULT 0,"  // If the slice is finished.
      "PRIMARY KEY (download_id, offset) )";

  bool ret = true;
  if (GetDB().DoesTableExist(DOWNLOADS_TABLE)) {
    // If the "downloads" table exists, "downloads_url_chain" might not be there
    // as it is introduced in version 24. A migration function will be run to
    // create it later.
    ret = EnsureColumnExists("end_time", "INTEGER NOT NULL DEFAULT 0") &&
        EnsureColumnExists("opened", "INTEGER NOT NULL DEFAULT 0");
  } else {
    // If the "downloads" table doesn't exist, neither should the
    // "downloads_url_chain" table.
    ret = !GetDB().DoesTableExist("downloads_url_chain");
    // Recreate the "downloads" and "downloads_url_chain" table.
    ret = ret && GetDB().Execute(kSchema) && GetDB().Execute(kUrlChainSchema);
  }

  // The "downloads_reroute_info" table is introduced in version 46 but needs
  // to be dropped as part of the removal of the FileSystem Connector code.
  if (GetDB().DoesTableExist("downloads_reroute_info")) {
    ret = ret && GetDB().Execute("DROP TABLE downloads_reroute_info");
  }

  // Making sure these tables introduced in later versions are created or
  // dropped as needed. They don't require migration of existing tables.
  // The "downloads_slices" table is introduced in version 33.
  return ret && (GetDB().DoesTableExist(DOWNLOADS_SLICES_TABLE) ||
                 GetDB().Execute(kSlicesSchema));
}

uint32_t DownloadDatabase::GetNextDownloadId() {
  sql::Statement select_max_id(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT max(id) FROM " DOWNLOADS_TABLE));
  bool result = select_max_id.Step();
  DCHECK(result);
  // If there are zero records in the downloads table, then max(id) will
  // return 0 = kInvalidDownloadId, so GetNextDownloadId() will set
  // *id = kInvalidDownloadId + 1.
  //
  // If there is at least one record but all of the `id`s are
  // <= kInvalidDownloadId, then max(id) will return <= kInvalidDownloadId,
  // so GetNextDownloadId() should return kInvalidDownloadId + 1.
  //
  // Note that any records with `id <= kInvalidDownloadId` will be dropped in
  // QueryDownloads().
  //
  // SQLITE doesn't have unsigned integers.
  return 1 + static_cast<uint32_t>(
                 std::max(static_cast<int64_t>(kInvalidDownloadId),
                          select_max_id.ColumnInt64(0)));
}

bool DownloadDatabase::DropDownloadTable() {
  return GetDB().Execute("DROP TABLE " DOWNLOADS_TABLE);
}

void DownloadDatabase::QueryDownloads(std::vector<DownloadRow>* results) {
  EnsureInProgressEntriesCleanedUp();

  results->clear();
  std::set<uint32_t> ids;

  DownloadRowMap info_map;

  sql::Statement statement_main(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, guid, current_path, target_path, mime_type, "
      "original_mime_type, start_time, received_bytes, total_bytes, state, "
      "danger_type, interrupt_reason, hash, end_time, opened, "
      "last_access_time, transient, referrer, site_url, "
      "embedder_download_data, tab_url, tab_referrer_url, http_method, "
      "by_ext_id, by_ext_name, by_web_app_id, etag, last_modified "
      "FROM " DOWNLOADS_TABLE));

  while (statement_main.Step()) {
    std::unique_ptr<DownloadRow> info(new DownloadRow());
    int column = 0;

    // SQLITE does not have unsigned integers, so explicitly handle negative
    // `id`s instead of casting them to very large uint32s, which would break
    // the max(id) logic in GetNextDownloadId().
    int64_t signed_id = statement_main.ColumnInt64(column++);
    bool valid = ConvertIntToDownloadId(signed_id, &(info->id));
    info->guid = statement_main.ColumnString(column++);
    info->current_path = ColumnFilePath(statement_main, column++);
    info->target_path = ColumnFilePath(statement_main, column++);
    info->mime_type = statement_main.ColumnString(column++);
    info->original_mime_type = statement_main.ColumnString(column++);
    info->start_time =
        base::Time::FromInternalValue(statement_main.ColumnInt64(column++));
    info->received_bytes = statement_main.ColumnInt64(column++);
    info->total_bytes = statement_main.ColumnInt64(column++);
    int state = statement_main.ColumnInt(column++);
    info->state = IntToDownloadState(state);
    info->danger_type =
        IntToDownloadDangerType(statement_main.ColumnInt(column++));
    info->interrupt_reason =
        IntToDownloadInterruptReason(statement_main.ColumnInt(column++));
    statement_main.ColumnBlobAsString(column++, &info->hash);
    info->end_time =
        base::Time::FromInternalValue(statement_main.ColumnInt64(column++));
    info->opened = statement_main.ColumnInt(column++) != 0;
    info->last_access_time =
        base::Time::FromInternalValue(statement_main.ColumnInt64(column++));
    info->transient = statement_main.ColumnInt(column++) != 0;
    info->referrer_url = GURL(statement_main.ColumnString(column++));
    info->site_url = GURL(statement_main.ColumnString(column++));
    info->embedder_download_data = statement_main.ColumnString(column++);
    info->tab_url = GURL(statement_main.ColumnString(column++));
    info->tab_referrer_url = GURL(statement_main.ColumnString(column++));
    info->http_method = statement_main.ColumnString(column++);
    info->by_ext_id = statement_main.ColumnString(column++);
    info->by_ext_name = statement_main.ColumnString(column++);
    info->by_web_app_id = statement_main.ColumnString(column++);
    info->etag = statement_main.ColumnString(column++);
    info->last_modified = statement_main.ColumnString(column++);

    // If the record is corrupted, note that and drop it.
    // http://crbug.com/251269
    DroppedReason dropped_reason = DROPPED_REASON_MAX;
    if (!valid) {
      // SQLITE doesn't have unsigned integers.
      dropped_reason = DROPPED_REASON_BAD_ID;
    } else if (!ids.insert(info->id).second) {
      dropped_reason = DROPPED_REASON_DUPLICATE_ID;
      NOTREACHED_IN_MIGRATION() << info->id;
    } else if (info->state == DownloadState::INVALID) {
      dropped_reason = DROPPED_REASON_BAD_STATE;
    } else if (info->danger_type == DownloadDangerType::INVALID) {
      dropped_reason = DROPPED_REASON_BAD_DANGER_TYPE;
    }
    if (dropped_reason == DROPPED_REASON_MAX) {
      DCHECK(!base::Contains(info_map, info->id));
      uint32_t id = info->id;
      info_map[id] = info.release();
    }
  }

  sql::Statement statement_chain(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, chain_index, url FROM downloads_url_chains "
      "ORDER BY id, chain_index"));

  while (statement_chain.Step()) {
    int column = 0;
    // See the comment above about SQLITE lacking unsigned integers.
    int64_t signed_id = statement_chain.ColumnInt64(column++);
    int chain_index = statement_chain.ColumnInt(column++);

    DownloadId id = kInvalidDownloadId;
    if (!ConvertIntToDownloadId(signed_id, &id))
      continue;

    // Confirm the id has already been seen--if it hasn't, discard the
    // record.
    if (!base::Contains(info_map, id))
      continue;

    // Confirm all previous URLs in the chain have already been seen;
    // if not, fill in with null or discard record.
    int current_chain_size = static_cast<int>(info_map[id]->url_chain.size());
    std::vector<GURL>* url_chain(&info_map[id]->url_chain);
    // Note that this DCHECK may trip as a result of corrupted databases.
    // It exists because in debug builds the chances are higher there's
    // an actual bug than that the database is corrupt. The DB corruption
    // case in handled in production code.
    DCHECK_EQ(chain_index, current_chain_size);
    while (current_chain_size < chain_index) {
      url_chain->push_back(GURL());
      current_chain_size++;
    }
    if (current_chain_size > chain_index)
      continue;

    // Save the record.
    url_chain->push_back(GURL(statement_chain.ColumnString(2)));
  }

  QueryDownloadSlices(&info_map);

  for (auto it = info_map.begin(); it != info_map.end(); ++it) {
    DownloadRow* row = it->second;
    bool empty_url_chain = row->url_chain.empty();
    UMA_HISTOGRAM_BOOLEAN("Download.DatabaseEmptyUrlChain", empty_url_chain);
    if (empty_url_chain) {
      RemoveDownload(row->id);
    } else {
      // Copy the contents of the stored info.
      results->push_back(*row);
    }
    delete row;
    it->second = NULL;
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadRow& data) {
  // UpdateDownload() is called fairly frequently.
  EnsureInProgressEntriesCleanedUp();

  DCHECK_NE(kInvalidDownloadId, data.id);
  if (data.state == DownloadState::INVALID) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  if (data.danger_type == DownloadDangerType::INVALID) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE " DOWNLOADS_TABLE
                     " SET current_path=?, target_path=?, "
                     "mime_type=?, original_mime_type=?, "
                     "received_bytes=?, state=?, "
                     "danger_type=?, interrupt_reason=?, hash=?, "
                     "end_time=?, total_bytes=?, "
                     "opened=?, last_access_time=?, transient=?, "
                     "by_ext_id=?, by_ext_name=?, by_web_app_id=?, "
                     "etag=?, last_modified=? WHERE id=?"));
  int column = 0;
  BindFilePath(statement, data.current_path, column++);
  BindFilePath(statement, data.target_path, column++);
  statement.BindString(column++, data.mime_type);
  statement.BindString(column++, data.original_mime_type);
  statement.BindInt64(column++, data.received_bytes);
  statement.BindInt(column++, DownloadStateToInt(data.state));
  statement.BindInt(column++, DownloadDangerTypeToInt(data.danger_type));
  statement.BindInt(column++,
                    DownloadInterruptReasonToInt(data.interrupt_reason));
  statement.BindBlob(column++, data.hash);
  statement.BindTime(column++, data.end_time);
  statement.BindInt64(column++, data.total_bytes);
  statement.BindInt(column++, (data.opened ? 1 : 0));
  statement.BindTime(column++, data.last_access_time);
  statement.BindInt(column++, (data.transient ? 1 : 0));
  statement.BindString(column++, data.by_ext_id);
  statement.BindString(column++, data.by_ext_name);
  statement.BindString(column++, data.by_web_app_id);
  statement.BindString(column++, data.etag);
  statement.BindString(column++, data.last_modified);
  statement.BindInt64(column++, DownloadIdToInt(data.id));

  if (!statement.Run())
    return false;

  if (data.download_slice_info.size() == 0) {
    RemoveDownloadSlices(data.id);
  } else {
    for (size_t i = 0; i < data.download_slice_info.size(); ++i) {
      if (!CreateOrUpdateDownloadSlice(data.download_slice_info[i]))
        return false;
    }
  }

  return true;
}

void DownloadDatabase::EnsureInProgressEntriesCleanedUp() {
  if (in_progress_entry_cleanup_completed_)
    return;

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE " DOWNLOADS_TABLE
                     " SET state=?, interrupt_reason=? WHERE state=?"));
  statement.BindInt(0, DownloadStateToInt(DownloadState::INTERRUPTED));
  statement.BindInt(
      1, DownloadInterruptReasonToInt(download_interrupt_reason_crash_));
  statement.BindInt(2, DownloadStateToInt(DownloadState::IN_PROGRESS));

  statement.Run();
  in_progress_entry_cleanup_completed_ = true;
}

bool DownloadDatabase::CreateDownload(const DownloadRow& info) {
  DCHECK_NE(kInvalidDownloadId, info.id);
  DCHECK(!info.guid.empty());
  EnsureInProgressEntriesCleanedUp();

  if (info.url_chain.empty())
    return false;

  if (info.state == DownloadState::INVALID)
    return false;

  if (info.danger_type == DownloadDangerType::INVALID)
    return false;

  {
    sql::Statement statement_insert(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO " DOWNLOADS_TABLE
        "(id,guid,current_path,target_path,mime_type,"
        "original_mime_type,start_time,received_bytes,total_bytes,"
        "state,danger_type,interrupt_reason,hash,end_time,opened,"
        "last_access_time,transient,referrer,site_url,"
        "embedder_download_data,tab_url,tab_referrer_url,http_method,"
        "by_ext_id,by_ext_name,by_web_app_id,etag,last_modified) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));

    int column = 0;
    statement_insert.BindInt64(column++, DownloadIdToInt(info.id));
    statement_insert.BindString(column++, info.guid);
    BindFilePath(statement_insert, info.current_path, column++);
    BindFilePath(statement_insert, info.target_path, column++);
    statement_insert.BindString(column++, info.mime_type);
    statement_insert.BindString(column++, info.original_mime_type);
    statement_insert.BindTime(column++, info.start_time);
    statement_insert.BindInt64(column++, info.received_bytes);
    statement_insert.BindInt64(column++, info.total_bytes);
    statement_insert.BindInt(column++, DownloadStateToInt(info.state));
    statement_insert.BindInt(column++,
                             DownloadDangerTypeToInt(info.danger_type));
    statement_insert.BindInt(
        column++, DownloadInterruptReasonToInt(info.interrupt_reason));
    statement_insert.BindBlob(column++, info.hash);
    statement_insert.BindTime(column++, info.end_time);
    statement_insert.BindInt(column++, info.opened ? 1 : 0);
    statement_insert.BindTime(column++, info.last_access_time);
    statement_insert.BindInt(column++, info.transient ? 1 : 0);
    statement_insert.BindString(column++, info.referrer_url.spec());
    statement_insert.BindString(column++, info.site_url.spec());
    statement_insert.BindString(column++, info.embedder_download_data);
    statement_insert.BindString(column++, info.tab_url.spec());
    statement_insert.BindString(column++, info.tab_referrer_url.spec());
    statement_insert.BindString(column++, info.http_method);
    statement_insert.BindString(column++, info.by_ext_id);
    statement_insert.BindString(column++, info.by_ext_name);
    statement_insert.BindString(column++, info.by_web_app_id);
    statement_insert.BindString(column++, info.etag);
    statement_insert.BindString(column++, info.last_modified);
    if (!statement_insert.Run()) {
      // GetErrorCode() returns a bitmask where the lower byte is a more general
      // code and the upper byte is a more specific code. In order to save
      // memory, take the general code, of which there are fewer than 50. See
      // also sql/connection.cc
      // http://www.sqlite.org/c3ref/c_abort_rollback.html
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      return false;
    }
  }

  {
    sql::Statement count_urls(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "SELECT count(*) FROM downloads_url_chains WHERE id=?"));
    count_urls.BindInt64(0, info.id);
    if (count_urls.Step()) {
      bool corrupt_urls = count_urls.ColumnInt(0) > 0;
      if (corrupt_urls) {
        // There should not be any URLs in downloads_url_chains for this
        // info.id.  If there are, we don't want them to interfere with
        // inserting the correct URLs, so just remove them.
        RemoveDownloadURLs(info.id);
      }
    }
  }

  sql::Statement statement_insert_chain(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO downloads_url_chains "
                                 "(id, chain_index, url) "
                                 "VALUES (?, ?, ?)"));
  for (size_t i = 0; i < info.url_chain.size(); ++i) {
    statement_insert_chain.BindInt64(0, info.id);
    statement_insert_chain.BindInt(1, static_cast<int>(i));
    statement_insert_chain.BindString(2, info.url_chain[i].spec());
    if (!statement_insert_chain.Run()) {
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      RemoveDownload(info.id);
      return false;
    }
    statement_insert_chain.Reset(true);
  }

  for (size_t i = 0; i < info.download_slice_info.size(); ++i) {
    if (!CreateOrUpdateDownloadSlice(info.download_slice_info[i])) {
      RemoveDownload(info.id);
      return false;
    }
  }

  return true;
}

void DownloadDatabase::RemoveDownload(DownloadId id) {
  EnsureInProgressEntriesCleanedUp();

  sql::Statement downloads_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM " DOWNLOADS_TABLE " WHERE id=?"));
  downloads_statement.BindInt64(0, id);
  if (!downloads_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
    return;
  }
  RemoveDownloadURLs(id);
  RemoveDownloadSlices(id);
}

void DownloadDatabase::RemoveDownloadURLs(DownloadId id) {
  sql::Statement urlchain_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads_url_chains WHERE id=?"));
  urlchain_statement.BindInt64(0, id);
  if (!urlchain_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
  }
}

size_t DownloadDatabase::CountDownloads() {
  EnsureInProgressEntriesCleanedUp();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT count(*) from " DOWNLOADS_TABLE));
  statement.Step();
  return statement.ColumnInt(0);
}

bool DownloadDatabase::CreateOrUpdateDownloadSlice(
    const DownloadSliceInfo& info) {
  // If the slice has no data, there is no need to insert it into the db. Note
  // that for each slice, `received_bytes` can only go up. So if a slice is
  // already in the db, its `received_bytes` should always be larger than 0.
  if (info.received_bytes == 0)
    return true;
  sql::Statement statement_replace(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "REPLACE INTO " DOWNLOADS_SLICES_TABLE
                     "(download_id, offset, received_bytes, finished) "
                     "VALUES (?,?,?,?)"));
  int column = 0;
  statement_replace.BindInt64(column++, info.download_id);
  statement_replace.BindInt64(column++, info.offset);
  statement_replace.BindInt64(column++, info.received_bytes);
  statement_replace.BindInt64(column++, (info.finished ? 1 : 0));
  return statement_replace.Run();
}

void DownloadDatabase::RemoveDownloadSlices(DownloadId id) {
  sql::Statement statement_delete(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM " DOWNLOADS_SLICES_TABLE " WHERE download_id=?"));
  statement_delete.BindInt64(0, id);
  statement_delete.Run();
}

void DownloadDatabase::QueryDownloadSlices(DownloadRowMap* download_row_map) {
  DCHECK(download_row_map);
  sql::Statement statement_query(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT download_id, offset, received_bytes, finished "
      "FROM " DOWNLOADS_SLICES_TABLE " ORDER BY download_id, offset"));

  while (statement_query.Step()) {
    int column = 0;
    DownloadSliceInfo info;
    // Convert signed integer from sqlite to unsigned DownloadId.
    int64_t signed_id = statement_query.ColumnInt64(column++);
    bool success = ConvertIntToDownloadId(signed_id, &info.download_id);
    DCHECK(success) << "Invalid download ID found in " << DOWNLOADS_SLICES_TABLE
                    << " table " << signed_id;
    info.offset = statement_query.ColumnInt64(column++);
    info.received_bytes = statement_query.ColumnInt64(column++);
    info.finished = static_cast<bool>(statement_query.ColumnInt64(column++));

    // Confirm the download_id has already been seen--if it hasn't, discard the
    // record.
    auto it = download_row_map->find(info.download_id);
    bool found = (it != download_row_map->end());
    if (!found) {
      RemoveDownloadSlices(info.download_id);
      continue;
    }
    it->second->download_slice_info.push_back(info);
  }
}

}  // namespace history
