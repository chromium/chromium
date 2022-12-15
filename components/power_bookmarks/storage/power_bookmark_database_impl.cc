// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/power_bookmarks/core/powers/search_params.h"
#include "components/power_bookmarks/metrics/power_bookmark_metrics.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace power_bookmarks {

namespace {

// `kCurrentVersionNumber` and `kCompatibleVersionNumber` are used for DB
// migrations. Update both accordingly when changing the schema.
const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

static constexpr char kSaveTableName[] = "saves";
static constexpr char kBlobTableName[] = "blobs";

std::unique_ptr<Power> CreatePowerFromSpecifics(
    const sync_pb::PowerBookmarkSpecifics& specifics) {
  switch (specifics.power_type()) {
    case sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED:
    case sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK:
    case sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE:
      return std::make_unique<Power>(specifics);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool CheckIfPowerWithIdExists(sql::Database* db, const base::GUID& guid) {
  if (guid == base::GUID())
    return false;

  static constexpr char kCheckIfPowerWithIdExistsSql[] =
      // clang-format off
      "SELECT COUNT(*) FROM saves WHERE id=?";
  // clang-format on
  DCHECK(db->IsSQLValid(kCheckIfPowerWithIdExistsSql));

  sql::Statement count_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kCheckIfPowerWithIdExistsSql));
  if (!count_statement.is_valid())
    return false;

  count_statement.BindString(0, guid.AsLowercaseString());
  if (!count_statement.Step())
    return false;

  size_t count = count_statement.ColumnInt(0);
  DCHECK(count == 0 || count == 1);
  return count > 0;
}

bool MatchesSearchParams(const sync_pb::PowerBookmarkSpecifics& specifics,
                         const SearchParams& search_params) {
  if (search_params.query.empty())
    return true;
  std::string pattern = base::StrCat({"*", search_params.query, "*"});
  if (base::MatchPattern(specifics.url(), pattern))
    return true;

  // A note can be matched by its contents.
  switch (specifics.power_type()) {
    case sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE:
      if (base::MatchPattern(
              specifics.power_entity().note_entity().plain_text(), pattern)) {
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

}  // namespace

PowerBookmarkDatabaseImpl::PowerBookmarkDatabaseImpl(
    const base::FilePath& database_dir)
    : db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 128}),
      database_path_(database_dir.Append(kDatabaseName)) {
  sync_db_ =
      std::make_unique<PowerBookmarkSyncMetadataDatabase>(&db_, &meta_table_);
}

PowerBookmarkDatabaseImpl::~PowerBookmarkDatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool PowerBookmarkDatabaseImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return true;
  }

  // Use of Unretained is safe as sql::Database will only run the callback while
  // it's alive. As PowerBookmarkDatabaseImpl instance owns the sql::Database
  // it's guaranteed that the PowerBookmarkDatabaseImpl will be alive when the
  // callback is run.
  db_.set_error_callback(
      base::BindRepeating(&PowerBookmarkDatabaseImpl::DatabaseErrorCallback,
                          base::Unretained(this)));
  db_.set_histogram_tag("PowerBookmarks");

  const base::FilePath dir = database_path_.DirName();
  bool dir_exists = base::DirectoryExists(dir);
  if (!dir_exists && !base::CreateDirectory(dir)) {
    DLOG(ERROR) << "Failed to create directory for power bookmarks database";
    return false;
  }

  if (!db_.Open(database_path_)) {
    DLOG(ERROR) << "Failed to open power bookmarks database: "
                << db_.GetErrorMessage();
    return false;
  }

  if (!InitSchema()) {
    DLOG(ERROR) << "Failed to create schema for power bookmarks database: "
                << db_.GetErrorMessage();
    db_.Close();
    return false;
  }

  if (!sync_db_->Init()) {
    DLOG(ERROR) << "Failed to initialize sync metadata db: "
                << db_.GetErrorMessage();
    db_.Close();
    return false;
  }

  // Directory will always exist here, but check to be safe.
  if (dir_exists) {
    int64_t file_size_bytes = base::ComputeDirectorySize(dir);
    metrics::RecordDatabaseSizeAtStartup(file_size_bytes);
  }

  return true;
}

bool PowerBookmarkDatabaseImpl::IsOpen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.is_open();
}

void PowerBookmarkDatabaseImpl::DatabaseErrorCallback(int error,
                                                      sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics::RecordDatabaseError(error);

  if (!sql::IsErrorCatastrophic(error))
    return;

  // Ignore repeated callbacks.
  db_.reset_error_callback();

  // After this call, the `db_` handle is poisoned so that future calls will
  // return errors until the handle is re-opened.
  db_.RazeAndClose();
}

bool PowerBookmarkDatabaseImpl::InitSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool has_metatable = meta_table_.DoesTableExist(&db_);
  bool has_schema =
      db_.DoesTableExist(kSaveTableName) && db_.DoesTableExist(kBlobTableName);

  if (!has_metatable && has_schema) {
    // Existing DB with no meta table. Cannot determine DB version.
    db_.Raze();
  }

  // Create the meta table if it doesn't exist.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  // If DB and meta table already existed and current version is not compatible
  // with DB then it should fail.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    return false;
  }
  if (!has_schema && !CreateSchema()) {
    return false;
  }

  meta_table_.SetVersionNumber(kCurrentVersionNumber);
  meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber);
  return true;
}

bool PowerBookmarkDatabaseImpl::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `id` is the primary key of the table, corresponds to a base::GUID.
  // `url` The URL of the target page.
  // `origin` The URL origin of the target page.
  // `power_type` The type of target this power.
  // `time_added` The date and time in seconds when the row was created.
  // `time_modified` The date and time in seconds when the row was last
  //  modified.
  static constexpr char kCreateSaveSchemaSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS saves("
          "id TEXT PRIMARY KEY NOT NULL,"
          "url TEXT NOT NULL,"
          "origin TEXT NOT NULL,"
          "power_type INTEGER NOT NULL,"
          "time_added INTEGER NOT NULL,"
          "time_modified INTEGER NOT NULL)"
          "WITHOUT ROWID";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreateSaveSchemaSql));
  if (!db_.Execute(kCreateSaveSchemaSql))
    return false;

  // `id` is the primary key of the table, corresponds to a base::GUID.
  // `specifics` The serialized specifics of the save. This is split into a
  // separate table because SQLite reads the whole row into memory when
  // querying. Having a separate table for blobs increases query performance
  // and also take take advantage of the "WITHOUT ROWID" optimization.
  static constexpr char kCreateBlobSchemaSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS blobs("
          "id TEXT PRIMARY KEY NOT NULL,"
          "specifics TEXT NOT NULL)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreateBlobSchemaSql));
  return db_.Execute(kCreateBlobSchemaSql);

  // TODO(crbug.com/1376612): Create indexes for searching capabilities.
}

std::vector<std::unique_ptr<Power>> PowerBookmarkDatabaseImpl::GetPowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kGetPowersForURLSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics, saves.url "
          "FROM blobs JOIN saves ON blobs.id=saves.id "
          "WHERE (url=?) AND (power_type=? OR ?=?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kGetPowersForURLSql));

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetPowersForURLSql));
  statement.BindString(0, url.spec());
  statement.BindInt(1, power_type);
  statement.BindInt(2, power_type);
  statement.BindInt(3, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED);

  std::vector<std::unique_ptr<Power>> powers;
  while (statement.Step()) {
    DCHECK_EQ(3, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;

    powers.emplace_back(CreatePowerFromSpecifics(specifics.value()));
  }

  return powers;
}

std::vector<std::unique_ptr<PowerOverview>>
PowerBookmarkDatabaseImpl::GetPowerOverviewsForType(
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1382855): Optimize this query to avoid SCAN TABLE.
  static constexpr char kGetPowerOverviewsForTypeSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics, COUNT(blobs.id) FROM blobs "
          "JOIN saves ON blobs.id=saves.id "
          "WHERE saves.power_type=? "
          "GROUP BY saves.url "
          "ORDER BY COUNT(saves.url) DESC";
  // clang-format on
  DCHECK(db_.IsSQLValid(kGetPowerOverviewsForTypeSql));

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetPowerOverviewsForTypeSql));
  statement.BindInt(0, power_type);

  std::vector<std::unique_ptr<PowerOverview>> power_overviews;
  while (statement.Step()) {
    DCHECK_EQ(3, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;

    power_overviews.emplace_back(std::make_unique<PowerOverview>(
        CreatePowerFromSpecifics(specifics.value()), statement.ColumnInt(2)));
  }

  return power_overviews;
}

std::vector<std::unique_ptr<Power>>
PowerBookmarkDatabaseImpl::GetPowersForSearchParams(
    const SearchParams& search_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1382855): Optimize this query to avoid SCAN TABLE.
  static constexpr char kGetPowersForSearchParamsSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics "
          "FROM blobs JOIN saves ON blobs.id=saves.id "
          "ORDER BY url ASC";
  // clang-format on
  DCHECK(db_.IsSQLValid(kGetPowersForSearchParamsSql));

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetPowersForSearchParamsSql));

  std::vector<std::unique_ptr<Power>> search_results;
  while (statement.Step()) {
    DCHECK_EQ(2, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;
    if (!MatchesSearchParams(specifics.value(), search_params))
      continue;

    search_results.emplace_back(CreatePowerFromSpecifics(specifics.value()));
  }

  return search_results;
}

bool PowerBookmarkDatabaseImpl::CreatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (CheckIfPowerWithIdExists(&db_, power->guid())) {
    DLOG(ERROR)
        << "Failed to create power because the current power already exists.";
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kCreatePowerSaveSql[] =
      // clang-format off
      "INSERT INTO saves("
          "id, url, origin, power_type, "
          "time_added, time_modified)"
          "VALUES(?,?,?,?,?,?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreatePowerSaveSql));

  sql::Statement save_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kCreatePowerSaveSql));
  save_statement.BindString(0, power->guid().AsLowercaseString());
  save_statement.BindString(1, power->url().spec());
  save_statement.BindString(2, url::Origin::Create(power->url()).Serialize());
  save_statement.BindInt(3, power->power_type());
  save_statement.BindTime(4, power->time_added());
  save_statement.BindTime(5, power->time_modified());
  if (!save_statement.Run())
    return false;

  static constexpr char kCreatePowerBlobSql[] =
      // clang-format off
      "INSERT INTO blobs(id, specifics) VALUES(?, ?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreatePowerBlobSql));

  sql::Statement blob_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kCreatePowerBlobSql));
  blob_statement.BindString(0, power->guid().AsLowercaseString());

  std::string data;
  sync_pb::PowerBookmarkSpecifics specifics;
  power->ToPowerBookmarkSpecifics(&specifics);
  specifics.SerializeToString(&data);
  blob_statement.BindString(1, data);
  if (!blob_statement.Run())
    return false;

  return transaction.Commit();
}

bool PowerBookmarkDatabaseImpl::UpdatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto existing_power = GetPowerForGUID(power->guid().AsLowercaseString());
  if (!existing_power) {
    DLOG(ERROR)
        << "Failed to update power because the current power does not exist.";
    return false;
  }
  existing_power->Merge(*power);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kUpdatePowerSaveSql[] =
      // clang-format off
      "UPDATE saves SET "
          "url=?, origin=?, power_type=?, time_added=?, "
          "time_modified=?"
          "WHERE id=?";
  // clang-format on
  DCHECK(db_.IsSQLValid(kUpdatePowerSaveSql));

  sql::Statement save_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdatePowerSaveSql));

  save_statement.BindString(0, existing_power->url().spec());
  save_statement.BindString(
      1, url::Origin::Create(existing_power->url()).Serialize());
  save_statement.BindInt(2, existing_power->power_type());
  save_statement.BindTime(3, existing_power->time_added());
  save_statement.BindTime(4, existing_power->time_modified());
  if (!save_statement.Run())
    return false;

  static constexpr char kUpdatePowerBlobSql[] =
      // clang-format off
      "UPDATE blobs SET specifics=? WHERE id=?";
  // clang-format on
  DCHECK(db_.IsSQLValid(kUpdatePowerBlobSql));

  sql::Statement blob_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdatePowerBlobSql));

  std::string data;
  sync_pb::PowerBookmarkSpecifics specifics;
  existing_power->ToPowerBookmarkSpecifics(&specifics);
  bool success = specifics.SerializeToString(&data);
  DCHECK(success);
  blob_statement.BindBlob(0, data);
  blob_statement.BindString(1, existing_power->guid().AsLowercaseString());
  if (!blob_statement.Run())
    return false;

  return transaction.Commit();
}

bool PowerBookmarkDatabaseImpl::DeletePower(const base::GUID& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CheckIfPowerWithIdExists(&db_, guid)) {
    return true;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeletePowerSaveSql[] =
      // clang-format off
      "DELETE FROM saves WHERE id=?";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeletePowerSaveSql));

  sql::Statement save_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeletePowerSaveSql));
  save_statement.BindString(0, guid.AsLowercaseString());
  if (!save_statement.Run())
    return false;

  static constexpr char kDeletePowerBlobSql[] =
      // clang-format off
      "DELETE FROM blobs WHERE id=?";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeletePowerBlobSql));

  sql::Statement blob_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeletePowerBlobSql));
  blob_statement.BindString(0, guid.AsLowercaseString());
  if (!blob_statement.Run())
    return false;

  return transaction.Commit();
}

bool PowerBookmarkDatabaseImpl::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeletePowersBlobsForURLSql[] =
      // clang-format off
      "DELETE FROM blobs WHERE id="
      "(SELECT id FROM saves WHERE url=? AND (power_type=? OR ?=?))";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeletePowersBlobsForURLSql));

  sql::Statement blob_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeletePowersBlobsForURLSql));
  blob_statement.BindString(0, url.spec());
  blob_statement.BindInt(1, power_type);
  blob_statement.BindInt(2, power_type);
  blob_statement.BindInt(
      3, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED);

  if (!blob_statement.Run())
    return false;

  static constexpr char kDeletePowersSavesForURLSql[] =
      // clang-format off
      "DELETE FROM saves WHERE url=? AND (power_type=? OR ?=?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeletePowersSavesForURLSql));

  sql::Statement save_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeletePowersSavesForURLSql));
  save_statement.BindString(0, url.spec());
  save_statement.BindInt(1, power_type);
  save_statement.BindInt(2, power_type);
  save_statement.BindInt(
      3, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED);
  if (!save_statement.Run())
    return false;

  return transaction.Commit();
}

std::vector<std::unique_ptr<Power>>
PowerBookmarkDatabaseImpl::GetPowersForGUIDs(
    const std::vector<std::string>& guids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& guid : guids) {
    DCHECK(base::IsValidGUID(guid));
  }
  static constexpr char kGetPowersForGUIDsSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics, saves.url "
          "FROM blobs JOIN saves ON blobs.id=saves.id "
          "WHERE saves.id IN ('";
  // clang-format on
  std::string sql_string = (std::string(kGetPowersForGUIDsSql) +
                            base::JoinString(guids, "','") + "')");
  db_.IsSQLValid(sql_string.c_str());
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, sql_string.c_str()));
  std::vector<std::unique_ptr<Power>> powers;
  while (statement.Step()) {
    DCHECK_EQ(3, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;

    powers.emplace_back(CreatePowerFromSpecifics(specifics.value()));
  }

  return powers;
}

std::vector<std::unique_ptr<Power>> PowerBookmarkDatabaseImpl::GetAllPowers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kGetPowersSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics, saves.url "
          "FROM blobs JOIN saves ON blobs.id=saves.id";
  // clang-format on
  DCHECK(db_.IsSQLValid(kGetPowersSql));

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetPowersSql));
  std::vector<std::unique_ptr<Power>> powers;
  while (statement.Step()) {
    DCHECK_EQ(3, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;

    powers.emplace_back(CreatePowerFromSpecifics(specifics.value()));
  }

  return powers;
}

std::unique_ptr<Power> PowerBookmarkDatabaseImpl::GetPowerForGUID(
    const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(base::IsValidGUID(guid));
  static constexpr char kGetPowerForGUIDSql[] =
      // clang-format off
      "SELECT blobs.id, blobs.specifics, saves.url "
          "FROM blobs JOIN saves ON blobs.id=saves.id "
          "WHERE saves.id=?";
  // clang-format on
  DCHECK(db_.IsSQLValid(kGetPowerForGUIDSql));

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetPowerForGUIDSql));
  statement.BindString(0, guid);
  std::vector<std::unique_ptr<Power>> powers;
  while (statement.Step()) {
    DCHECK_EQ(3, statement.ColumnCount());

    absl::optional<sync_pb::PowerBookmarkSpecifics> specifics =
        DeserializeOrDelete(
            statement.ColumnString(1),
            base::GUID::ParseLowercase(statement.ColumnString(0)));
    if (!specifics.has_value())
      continue;

    return CreatePowerFromSpecifics(specifics.value());
  }

  return nullptr;
}

absl::optional<sync_pb::PowerBookmarkSpecifics>
PowerBookmarkDatabaseImpl::DeserializeOrDelete(const std::string& data,
                                               const base::GUID& id) {
  sync_pb::PowerBookmarkSpecifics specifics;
  bool parse_success = specifics.ParseFromString(data);

  if (parse_success)
    return specifics;

  bool delete_success = DeletePower(id);
  DCHECK(delete_success);
  return absl::nullopt;
}

}  // namespace power_bookmarks
