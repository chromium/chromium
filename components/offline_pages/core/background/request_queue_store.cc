// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_queue_store.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_item_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

template class StoreUpdateResult<SavePageRequest>;

namespace {

using SuccessCallback = base::OnceCallback<void(bool)>;

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define REQUEST_QUEUE_TABLE_NAME "request_queue_v1"
// The full set of fields, used in several SQL statements.
#define REQUEST_QUEUE_FIELDS                                                \
  "request_id, creation_time, activation_time,"                             \
  " last_attempt_time, started_attempt_count, completed_attempt_count,"     \
  " state, url, client_namespace, client_id, original_url, request_origin," \
  " fail_state, auto_fetch_notification_state"

const bool kUserRequested = true;

bool CreateRequestQueueTable(sql::Database* db) {
  static const char kSql[] =
      "CREATE TABLE IF NOT EXISTS " REQUEST_QUEUE_TABLE_NAME
      " (request_id INTEGER PRIMARY KEY NOT NULL,"
      " creation_time INTEGER NOT NULL,"
      " activation_time INTEGER NOT NULL DEFAULT 0,"
      " last_attempt_time INTEGER NOT NULL DEFAULT 0,"
      " started_attempt_count INTEGER NOT NULL,"
      " completed_attempt_count INTEGER NOT NULL,"
      " state INTEGER NOT NULL DEFAULT 0,"
      " url VARCHAR NOT NULL,"
      " client_namespace VARCHAR NOT NULL,"
      " client_id VARCHAR NOT NULL,"
      " original_url VARCHAR NOT NULL DEFAULT '',"
      " request_origin VARCHAR NOT NULL DEFAULT '',"
      " fail_state INTEGER NOT NULL DEFAULT 0,"
      " auto_fetch_notification_state INTEGER NOT NULL DEFAULT 0"
      ")";
  return db->Execute(kSql);
}

// Upgrades an old version of the request queue table to the new version.
//
// The upgrade is done by renaming the existing table to
// 'temp_request_queue_v1', reinserting data from the temporary table back into
// 'request_queue_v1', and finally dropping the temporary table.
//
// |upgrade_sql| is the SQL statement that copies data from the temporary
// table back into the primary table.
bool UpgradeWithQuery(sql::Database* db, const base::cstring_view upgrade_sql) {
  if (!db->Execute("ALTER TABLE " REQUEST_QUEUE_TABLE_NAME
                   " RENAME TO temp_" REQUEST_QUEUE_TABLE_NAME)) {
    return false;
  }
  if (!CreateRequestQueueTable(db))
    return false;
  if (!db->Execute(upgrade_sql))
    return false;
  return db->Execute("DROP TABLE IF EXISTS temp_" REQUEST_QUEUE_TABLE_NAME);
}

bool UpgradeFrom57(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id) "
      "SELECT "
      "request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id "
      "FROM temp_" REQUEST_QUEUE_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom58(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url) "
      "SELECT "
      "request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url "
      "FROM temp_" REQUEST_QUEUE_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom61(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url, request_origin) "
      "SELECT "
      "request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url, request_origin "
      "FROM temp_" REQUEST_QUEUE_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom72(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url, request_origin, fail_state) "
      "SELECT "
      "request_id, creation_time, activation_time, last_attempt_time, "
      "started_attempt_count, completed_attempt_count, state, url, "
      "client_namespace, client_id, original_url, request_origin, fail_state "
      "FROM temp_" REQUEST_QUEUE_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool CreateSchemaSync(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!db->DoesTableExist(REQUEST_QUEUE_TABLE_NAME)) {
    if (!CreateRequestQueueTable(db))
      return false;
  }

  // If there is not already a state column, we need to drop the old table. We
  // are choosing to drop instead of upgrade since the feature is not yet
  // released, so we don't try to migrate it.
  if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "state")) {
    if (!db->Execute("DROP TABLE IF EXISTS " REQUEST_QUEUE_TABLE_NAME))
      return false;
  }

  if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "original_url")) {
    if (!UpgradeFrom57(db))
      return false;
  } else if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "request_origin")) {
    if (!UpgradeFrom58(db))
      return false;
  } else if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "fail_state")) {
    if (!UpgradeFrom61(db))
      return false;
  } else if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME,
                                  "auto_fetch_notification_state")) {
    if (!UpgradeFrom72(db))
      return false;
  }

  // This would be a great place to add indices when we need them.
  return transaction.Commit();
}

// Enum conversion code. Database corruption is possible, so make sure enum
// values are in the domain. Because corruption is rare, there is not robust
// error handling.

SavePageRequest::AutoFetchNotificationState AutoFetchNotificationStateFromInt(
    int value) {
  switch (static_cast<SavePageRequest::AutoFetchNotificationState>(value)) {
    case SavePageRequest::AutoFetchNotificationState::kUnknown:
    case SavePageRequest::AutoFetchNotificationState::kShown:
      return static_cast<SavePageRequest::AutoFetchNotificationState>(value);
  }
  DLOG(ERROR) << "Invalid AutoFetchNotificationState value: " << value;
  return SavePageRequest::AutoFetchNotificationState::kUnknown;
}

SavePageRequest::RequestState ToRequestState(int value) {
  switch (static_cast<SavePageRequest::RequestState>(value)) {
    case SavePageRequest::RequestState::AVAILABLE:
    case SavePageRequest::RequestState::PAUSED:
    case SavePageRequest::RequestState::OFFLINING:
      return static_cast<SavePageRequest::RequestState>(value);
  }
  DLOG(ERROR) << "Invalid RequestState value: " << value;
  return SavePageRequest::RequestState::AVAILABLE;
}

offline_items_collection::FailState ToFailState(int value) {
  offline_items_collection::FailState state = FailState::NO_FAILURE;
  if (!offline_items_collection::ToFailState(value, &state)) {
    DLOG(ERROR) << "Invalid FailState: " << value;
  }

  return state;
}

// Create a save page request from the first row of an SQL result. The result
// must have the exact columns from the |REQUEST_QUEUE_FIELDS| macro.
std::unique_ptr<SavePageRequest> MakeSavePageRequest(
    sql::Statement& statement) {
  const int64_t id = statement.ColumnInt64(0);
  const base::Time creation_time = statement.ColumnTime(1);
  const base::Time last_attempt_time = statement.ColumnTime(3);
  const int64_t started_attempt_count = statement.ColumnInt64(4);
  const int64_t completed_attempt_count = statement.ColumnInt64(5);
  const SavePageRequest::RequestState state =
      ToRequestState(statement.ColumnInt64(6));
  const GURL url(statement.ColumnString(7));
  ClientId client_id(statement.ColumnString(8), statement.ColumnString(9));
  GURL original_url(statement.ColumnString(10));
  std::string request_origin(statement.ColumnString(11));

  DVLOG(2) << "making save page request - id " << id << " url " << url
           << " client_id " << client_id.name_space << "-" << client_id.id
           << " creation time " << creation_time << " user requested "
           << kUserRequested << " original_url " << original_url
           << " request_origin " << request_origin;

  std::unique_ptr<SavePageRequest> request(new SavePageRequest(
      id, std::move(url), std::move(client_id), creation_time, kUserRequested));
  request->set_last_attempt_time(last_attempt_time);
  request->set_started_attempt_count(started_attempt_count);
  request->set_completed_attempt_count(completed_attempt_count);
  request->set_request_state(state);
  request->set_original_url(std::move(original_url));
  request->set_request_origin(std::move(request_origin));
  request->set_fail_state(ToFailState(statement.ColumnInt64(12)));
  request->set_auto_fetch_notification_state(
      AutoFetchNotificationStateFromInt(statement.ColumnInt(13)));
  return request;
}

// Get a request for a specific id.
std::unique_ptr<SavePageRequest> GetOneRequestSync(sql::Database* db,
                                                   const int64_t request_id) {
  static const char kSql[] =
      "SELECT " REQUEST_QUEUE_FIELDS " FROM " REQUEST_QUEUE_TABLE_NAME
      " WHERE request_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);

  if (statement.Step())
    return MakeSavePageRequest(statement);
  return {};
}

ItemActionStatus DeleteRequestByIdSync(sql::Database* db, int64_t request_id) {
  static const char kSql[] =
      "DELETE FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);
  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::NOT_FOUND;
  return ItemActionStatus::SUCCESS;
}

AddRequestResult InsertSync(sql::Database* db, const SavePageRequest& request) {
  static const char kSql[] = "INSERT OR IGNORE INTO " REQUEST_QUEUE_TABLE_NAME
                             " (" REQUEST_QUEUE_FIELDS
                             ") VALUES"
                             " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request.request_id());
  statement.BindTime(1, request.creation_time());
  statement.BindInt64(2, 0);
  statement.BindTime(3, request.last_attempt_time());
  statement.BindInt64(4, request.started_attempt_count());
  statement.BindInt64(5, request.completed_attempt_count());
  statement.BindInt64(6, static_cast<int64_t>(request.request_state()));
  statement.BindString(7, request.url().spec());
  statement.BindString(8, request.client_id().name_space);
  statement.BindString(9, request.client_id().id);
  statement.BindString(10, request.original_url().spec());
  statement.BindString(11, request.request_origin());
  statement.BindInt64(12, static_cast<int64_t>(request.fail_state()));
  statement.BindInt64(
      13, static_cast<int64_t>(request.auto_fetch_notification_state()));

  if (!statement.Run())
    return AddRequestResult::STORE_FAILURE;
  if (db->GetLastChangeCount() == 0)
    return AddRequestResult::ALREADY_EXISTS;
  return AddRequestResult::SUCCESS;
}

ItemActionStatus UpdateSync(sql::Database* db, const SavePageRequest& request) {
  static const char kSql[] =
      "UPDATE OR IGNORE " REQUEST_QUEUE_TABLE_NAME
      " SET creation_time = ?, activation_time = ?, last_attempt_time = ?,"
      " started_attempt_count = ?, completed_attempt_count = ?, state = ?,"
      " url = ?, client_namespace = ?, client_id = ?, original_url = ?,"
      " request_origin = ?, fail_state = ?, auto_fetch_notification_state = ?"
      " WHERE request_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  // SET columns:
  statement.BindTime(0, request.creation_time());
  statement.BindInt64(1, 0);
  statement.BindTime(2, request.last_attempt_time());
  statement.BindInt64(3, request.started_attempt_count());
  statement.BindInt64(4, request.completed_attempt_count());
  statement.BindInt64(5, static_cast<int64_t>(request.request_state()));
  statement.BindString(6, request.url().spec());
  statement.BindString(7, request.client_id().name_space);
  statement.BindString(8, request.client_id().id);
  statement.BindString(9, request.original_url().spec());
  statement.BindString(10, request.request_origin());
  statement.BindInt64(11, static_cast<int64_t>(request.fail_state()));
  statement.BindInt64(
      12, static_cast<int64_t>(request.auto_fetch_notification_state()));
  // WHERE:
  statement.BindInt64(13, request.request_id());

  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::NOT_FOUND;
  return ItemActionStatus::SUCCESS;
}

UpdateRequestsResult StoreUpdateResultForIds(
    StoreState store_state,
    const std::vector<int64_t>& item_ids,
    ItemActionStatus action_status) {
  UpdateRequestsResult result(store_state);
  for (const auto& item_id : item_ids)
    result.item_statuses.emplace_back(item_id, action_status);
  return result;
}

UpdateRequestsResult StoreErrorForAllRequests(
    const std::vector<SavePageRequest>& items) {
  std::vector<int64_t> item_ids;
  for (const auto& item : items)
    item_ids.push_back(item.request_id());
  return StoreUpdateResultForIds(StoreState::LOADED, item_ids,
                                 ItemActionStatus::STORE_ERROR);
}

UpdateRequestsResult StoreErrorForAllIds(const std::vector<int64_t>& item_ids) {
  return StoreUpdateResultForIds(StoreState::LOADED, item_ids,
                                 ItemActionStatus::STORE_ERROR);
}

bool InitDatabaseSync(sql::Database* db, const base::FilePath& path) {
  db->set_histogram_tag("BackgroundRequestQueue");

  if (path.empty()) {
    if (!db->OpenInMemory())
      return false;
  } else {
    base::File::Error err;
    if (!base::CreateDirectoryAndGetError(path.DirName(), &err))
      return false;
    if (!db->Open(path))
      return false;
  }
  db->Preload();

  return CreateSchemaSync(db);
}

std::optional<std::vector<std::unique_ptr<SavePageRequest>>> GetAllRequestsSync(
    sql::Database* db) {
  static const char kSql[] =
      "SELECT " REQUEST_QUEUE_FIELDS " FROM " REQUEST_QUEUE_TABLE_NAME;
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  std::vector<std::unique_ptr<SavePageRequest>> requests;
  while (statement.Step())
    requests.push_back(MakeSavePageRequest(statement));
  if (!statement.Succeeded())
    return std::nullopt;
  return requests;
}

// Calls |callback| with the result of |requests|.
void InvokeGetRequestsCallback(
    RequestQueueStore::GetRequestsCallback callback,
    std::optional<std::vector<std::unique_ptr<SavePageRequest>>> requests) {
  if (requests) {
    std::move(callback).Run(true, std::move(requests).value());
  } else {
    std::move(callback).Run(false, {});
  }
}

UpdateRequestsResult GetRequestsByIdsSync(
    sql::Database* db,
    const std::vector<int64_t>& request_ids) {
  UpdateRequestsResult result(StoreState::LOADED);

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return StoreErrorForAllIds(request_ids);

  // Make sure not to include the same request multiple times, preserving the
  // order of non-duplicated IDs in the result.
  std::unordered_set<int64_t> processed_ids;
  for (int64_t request_id : request_ids) {
    if (!processed_ids.insert(request_id).second)
      continue;
    std::unique_ptr<SavePageRequest> request =
        GetOneRequestSync(db, request_id);
    if (request)
      result.updated_items.push_back(*request);
    ItemActionStatus status =
        request ? ItemActionStatus::SUCCESS : ItemActionStatus::NOT_FOUND;
    result.item_statuses.emplace_back(request_id, status);
  }

  if (!transaction.Commit())
    return StoreErrorForAllIds(request_ids);

  return result;
}

AddRequestResult AddRequestSync(sql::Database* db,
                                const SavePageRequest& request,
                                const RequestQueueStore::AddOptions& options) {
  // If we need to check preconditions, read the set of active requests and
  // check preconditions.
  if (options.maximum_in_flight_requests_for_namespace > 0 ||
      options.disallow_duplicate_requests) {
    std::optional<std::vector<std::unique_ptr<SavePageRequest>>> requests =
        GetAllRequestsSync(db);
    if (!requests)
      return AddRequestResult::STORE_FAILURE;

    if (options.maximum_in_flight_requests_for_namespace > 0) {
      int existing_requests = 0;
      for (const std::unique_ptr<SavePageRequest>& existing_request :
           requests.value()) {
        if (existing_request->client_id().name_space ==
            request.client_id().name_space)
          ++existing_requests;
      }
      if (existing_requests >= options.maximum_in_flight_requests_for_namespace)
        return AddRequestResult::REQUEST_QUOTA_HIT;
    }

    if (options.disallow_duplicate_requests) {
      for (const std::unique_ptr<SavePageRequest>& existing_request :
           requests.value()) {
        if (existing_request->client_id().name_space ==
                request.client_id().name_space &&
            EqualsIgnoringFragment(existing_request->url(), request.url()))
          return AddRequestResult::DUPLICATE_URL;
      }
    }
  }
  return InsertSync(db, request);
}

UpdateRequestsResult UpdateRequestsSync(
    sql::Database* db,
    const std::vector<SavePageRequest>& requests) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return StoreErrorForAllRequests(requests);

  UpdateRequestsResult result(StoreState::LOADED);
  for (const auto& request : requests) {
    ItemActionStatus status = UpdateSync(db, request);
    result.item_statuses.emplace_back(request.request_id(), status);
    if (status == ItemActionStatus::SUCCESS)
      result.updated_items.push_back(request);
  }

  if (!transaction.Commit())
    return StoreErrorForAllRequests(requests);

  return result;
}

UpdateRequestsResult RemoveRequestsSync(
    sql::Database* db,
    const std::vector<int64_t>& request_ids) {
  UpdateRequestsResult result(StoreState::LOADED);

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return StoreErrorForAllIds(request_ids);
  }

  // Read the request before we delete it, and if the delete worked, put it on
  // the queue of requests that got deleted.
  for (int64_t request_id : request_ids) {
    std::unique_ptr<SavePageRequest> request =
        GetOneRequestSync(db, request_id);
    ItemActionStatus status = DeleteRequestByIdSync(db, request_id);
    result.item_statuses.push_back(std::make_pair(request_id, status));
    if (status == ItemActionStatus::SUCCESS)
      result.updated_items.push_back(*request);
  }

  if (!transaction.Commit()) {
    return StoreErrorForAllIds(request_ids);
  }

  return result;
}

bool ResetSync(sql::Database* db, const base::FilePath& db_file_path) {
  // This method deletes the content of the whole store and reinitializes it.
  bool success = true;
  if (db) {
    success = db->Raze();
    db->Close();
  }
  return base::DeletePathRecursively(db_file_path) && success;
}

bool SetAutoFetchNotificationStateSync(
    sql::Database* db,
    int64_t request_id,
    SavePageRequest::AutoFetchNotificationState state) {
  std::unique_ptr<SavePageRequest> request = GetOneRequestSync(db, request_id);
  if (!request)
    return false;

  request->set_auto_fetch_notification_state(state);
  return UpdateSync(db, *request) == ItemActionStatus::SUCCESS;
}

UpdateRequestsResult RemoveRequestsIfSync(
    sql::Database* db,
    const base::RepeatingCallback<bool(const SavePageRequest&)>&
        remove_predicate) {
  std::optional<std::vector<std::unique_ptr<SavePageRequest>>> requests =
      GetAllRequestsSync(db);
  if (!requests)
    return UpdateRequestsResult(StoreState::LOADED);

  std::vector<int64_t> ids_to_remove;
  for (const std::unique_ptr<SavePageRequest>& request : requests.value()) {
    if (remove_predicate.Run(*request))
      ids_to_remove.push_back(request->request_id());
  }
  return RemoveRequestsSync(db, ids_to_remove);
}

}  // anonymous namespace

RequestQueueStore::RequestQueueStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)),
      state_(StoreState::NOT_LOADED) {}

RequestQueueStore::RequestQueueStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : RequestQueueStore(background_task_runner) {
  DCHECK(!path.empty());
  db_file_path_ = path.AppendASCII("RequestQueue.db");
}

RequestQueueStore::~RequestQueueStore() {
  if (db_)
    background_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

void RequestQueueStore::Initialize(InitializeCallback callback) {
  DCHECK(!db_);
  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions{.page_size = 4096, .cache_size = 500});

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&InitDatabaseSync, db_.get(), db_file_path_),
      base::BindOnce(&RequestQueueStore::OnOpenConnectionDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RequestQueueStore::GetRequests(GetRequestsCallback callback) {
  DCHECK(db_);
  if (!CheckDb()) {
    std::vector<std::unique_ptr<SavePageRequest>> requests;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::move(requests)));
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetAllRequestsSync, db_.get()),
      base::BindOnce(&InvokeGetRequestsCallback, std::move(callback)));
}

void RequestQueueStore::GetRequestsByIds(
    const std::vector<int64_t>& request_ids,
    UpdateCallback callback) {
  if (!CheckDb()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       StoreUpdateResultForIds(StoreState::LOADED, request_ids,
                                               ItemActionStatus::STORE_ERROR)));
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetRequestsByIdsSync, db_.get(), request_ids),
      std::move(callback));
}

void RequestQueueStore::AddRequest(const SavePageRequest& request,
                                   AddOptions options,
                                   AddCallback callback) {
  if (!CheckDb()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), AddRequestResult::STORE_FAILURE));
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&AddRequestSync, db_.get(), request, options),
      std::move(callback));
}

void RequestQueueStore::UpdateRequests(
    const std::vector<SavePageRequest>& requests,
    UpdateCallback callback) {
  if (!CheckDb()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  StoreErrorForAllRequests(requests)));
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&UpdateRequestsSync, db_.get(), requests),
      std::move(callback));
}

void RequestQueueStore::RemoveRequests(const std::vector<int64_t>& request_ids,
                                       UpdateCallback callback) {
  if (!CheckDb()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       StoreUpdateResultForIds(StoreState::LOADED, request_ids,
                                               ItemActionStatus::STORE_ERROR)));
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&RemoveRequestsSync, db_.get(), request_ids),
      std::move(callback));
}

void RequestQueueStore::RemoveRequestsIf(
    const base::RepeatingCallback<bool(const SavePageRequest&)>&
        remove_predicate,
    UpdateCallback callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(RemoveRequestsIfSync, db_.get(), remove_predicate),
      std::move(callback));
}

void RequestQueueStore::SetAutoFetchNotificationState(
    int64_t request_id,
    SavePageRequest::AutoFetchNotificationState state,
    base::OnceCallback<void(bool updated)> callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(SetAutoFetchNotificationStateSync, db_.get(), request_id,
                     state),
      std::move(callback));
}

void RequestQueueStore::Reset(ResetCallback callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(ResetSync, db_.get(), db_file_path_),
      base::BindOnce(&RequestQueueStore::OnResetDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

StoreState RequestQueueStore::state() const {
  return state_;
}

void RequestQueueStore::SetStateForTesting(StoreState state, bool reset_db) {
  state_ = state;
  if (reset_db)
    db_.reset(nullptr);
}

void RequestQueueStore::OnOpenConnectionDone(InitializeCallback callback,
                                             bool success) {
  DCHECK(db_);
  state_ = success ? StoreState::LOADED : StoreState::FAILED_LOADING;
  std::move(callback).Run(success);
}

void RequestQueueStore::OnResetDone(ResetCallback callback, bool success) {
  state_ = success ? StoreState::NOT_LOADED : StoreState::FAILED_RESET;
  db_.reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

bool RequestQueueStore::CheckDb() const {
  return db_ && state_ == StoreState::LOADED;
}

}  // namespace offline_pages
