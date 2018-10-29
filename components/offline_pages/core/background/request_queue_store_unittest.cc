// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_queue_store.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

#define REQUEST_QUEUE_TABLE_NAME "request_queue_v1"

using UpdateStatus = RequestQueueStore::UpdateStatus;

namespace {
const int64_t kRequestId = 42;
const int64_t kRequestId2 = 44;
const int64_t kRequestId3 = 47;
const GURL kUrl("http://example.com");
const GURL kUrl2("http://another-example.com");
const ClientId kClientId("bookmark", "1234");
const ClientId kClientId2("async", "5678");
const bool kUserRequested = true;
const std::string kRequestOrigin = "abc.xyz";

enum class LastResult {
  RESULT_NONE,
  RESULT_FALSE,
  RESULT_TRUE,
};

void BuildTestStoreWithSchemaFromM57(const base::FilePath& file) {
  sql::Database connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("RequestQueue.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(
      connection.Execute("CREATE TABLE " REQUEST_QUEUE_TABLE_NAME
                         " (request_id INTEGER PRIMARY KEY NOT NULL,"
                         " creation_time INTEGER NOT NULL,"
                         " activation_time INTEGER NOT NULL DEFAULT 0,"
                         " last_attempt_time INTEGER NOT NULL DEFAULT 0,"
                         " started_attempt_count INTEGER NOT NULL,"
                         " completed_attempt_count INTEGER NOT NULL,"
                         " state INTEGER NOT NULL DEFAULT 0,"
                         " url VARCHAR NOT NULL,"
                         " client_namespace VARCHAR NOT NULL,"
                         " client_id VARCHAR NOT NULL"
                         ")"));

  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT OR IGNORE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, kRequestId);
  statement.BindInt64(1, 0);
  statement.BindInt64(2, 0);
  statement.BindInt64(3, 0);
  statement.BindInt64(4, 0);
  statement.BindInt64(5, 0);
  statement.BindInt64(6, 0);
  statement.BindString(7, kUrl.spec());
  statement.BindString(8, kClientId.name_space);
  statement.BindString(9, kClientId.id);
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(REQUEST_QUEUE_TABLE_NAME));
  ASSERT_FALSE(
      connection.DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "original_url"));
}

void BuildTestStoreWithSchemaFromM58(const base::FilePath& file) {
  sql::Database connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("RequestQueue.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(
      connection.Execute("CREATE TABLE " REQUEST_QUEUE_TABLE_NAME
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
                         " original_url VARCHAR NOT NULL"
                         ")"));

  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT OR IGNORE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id, original_url)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, kRequestId);
  statement.BindInt64(1, 0);
  statement.BindInt64(2, 0);
  statement.BindInt64(3, 0);
  statement.BindInt64(4, 0);
  statement.BindInt64(5, 0);
  statement.BindInt64(6, 0);
  statement.BindString(7, kUrl.spec());
  statement.BindString(8, kClientId.name_space);
  statement.BindString(9, kClientId.id);
  statement.BindString(10, kUrl2.spec());
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(REQUEST_QUEUE_TABLE_NAME));
  ASSERT_FALSE(
      connection.DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "request_origin"));
}

void BuildTestStoreWithSchemaFromM61(const base::FilePath& file) {
  sql::Database connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("RequestQueue.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(
      connection.Execute("CREATE TABLE " REQUEST_QUEUE_TABLE_NAME
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
                         " request_origin VARCHAR NOT NULL DEFAULT ''"
                         ")"));

  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT OR IGNORE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id, original_url, request_origin)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, kRequestId);
  statement.BindInt64(1, 0);
  statement.BindInt64(2, 0);
  statement.BindInt64(3, 0);
  statement.BindInt64(4, 0);
  statement.BindInt64(5, 0);
  statement.BindInt64(6, 0);
  statement.BindString(7, kUrl.spec());
  statement.BindString(8, kClientId.name_space);
  statement.BindString(9, kClientId.id);
  statement.BindString(10, kUrl2.spec());
  statement.BindString(11, kRequestOrigin);
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(REQUEST_QUEUE_TABLE_NAME));
  ASSERT_FALSE(
      connection.DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "fail_state"));
}

// Class that serves as a base for testing different implementations of the
// |RequestQueueStore|. Specific implementations extend the templatized version
// of this class and provide appropriate store factory.
class RequestQueueStoreTestBase : public testing::Test {
 public:
  RequestQueueStoreTestBase();

  // Test overrides.
  void TearDown() override;

  void PumpLoop();
  void ClearResults();
  void InitializeStore(RequestQueueStore* store);

  void InitializeCallback(bool success);
  // Callback used for get requests.
  void GetRequestsDone(bool result,
                       std::vector<std::unique_ptr<SavePageRequest>> requests);
  // Callback used for add/update request.
  void AddOrUpdateDone(UpdateStatus result);
  void AddRequestDone(ItemActionStatus status);
  void UpdateRequestDone(UpdateRequestsResult result);
  // Callback used for reset.
  void ResetDone(bool result);

  LastResult last_result() const { return last_result_; }
  UpdateStatus last_update_status() const { return last_update_status_; }
  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return last_requests_;
  }
  ItemActionStatus last_add_status() const { return last_add_status_; }

  UpdateRequestsResult* last_update_result() const {
    return last_update_result_.get();
  }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  LastResult last_result_;
  UpdateStatus last_update_status_;
  ItemActionStatus last_add_status_;
  std::unique_ptr<UpdateRequestsResult> last_update_result_;
  std::vector<std::unique_ptr<SavePageRequest>> last_requests_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueStoreTestBase::RequestQueueStoreTestBase()
    : last_result_(LastResult::RESULT_NONE),
      last_update_status_(UpdateStatus::FAILED),
      last_add_status_(ItemActionStatus::NOT_FOUND),
      task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

void RequestQueueStoreTestBase::TearDown() {
  // Wait for all the pieces of the store to delete itself properly.
  PumpLoop();
}

void RequestQueueStoreTestBase::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueStoreTestBase::ClearResults() {
  last_result_ = LastResult::RESULT_NONE;
  last_update_status_ = UpdateStatus::FAILED;
  last_add_status_ = ItemActionStatus::NOT_FOUND;
  last_requests_.clear();
  last_update_result_.reset(nullptr);
}

void RequestQueueStoreTestBase::InitializeStore(RequestQueueStore* store) {
  store->Initialize(base::BindOnce(
      &RequestQueueStoreTestBase::InitializeCallback, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ClearResults();
}

void RequestQueueStoreTestBase::InitializeCallback(bool success) {
  last_result_ = success ? LastResult::RESULT_TRUE : LastResult::RESULT_FALSE;
}

void RequestQueueStoreTestBase::GetRequestsDone(
    bool result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_result_ = result ? LastResult::RESULT_TRUE : LastResult::RESULT_FALSE;
  last_requests_ = std::move(requests);
}

void RequestQueueStoreTestBase::AddOrUpdateDone(UpdateStatus status) {
  last_update_status_ = status;
}

void RequestQueueStoreTestBase::AddRequestDone(ItemActionStatus status) {
  last_add_status_ = status;
}

void RequestQueueStoreTestBase::UpdateRequestDone(UpdateRequestsResult result) {
  last_update_result_ =
      std::make_unique<UpdateRequestsResult>(std::move(result));
}

void RequestQueueStoreTestBase::ResetDone(bool result) {
  last_result_ = result ? LastResult::RESULT_TRUE : LastResult::RESULT_FALSE;
}

// Defines a store test fixture templatized by the store factory.
class RequestQueueStoreTest : public RequestQueueStoreTestBase {
 public:
  std::unique_ptr<RequestQueueStore> BuildStore() {
    return std::make_unique<RequestQueueStore>(
        base::ThreadTaskRunnerHandle::Get(), temp_directory_.GetPath());
  }
  std::unique_ptr<RequestQueueStore> BuildStoreWithOldSchema(int version) {
    if (version == 57) {
      BuildTestStoreWithSchemaFromM57(temp_directory_.GetPath());
    } else if (version == 58) {
      BuildTestStoreWithSchemaFromM58(temp_directory_.GetPath());
    } else if (version == 61) {
      BuildTestStoreWithSchemaFromM61(temp_directory_.GetPath());
    }

    return std::make_unique<RequestQueueStore>(
        base::ThreadTaskRunnerHandle::Get(), temp_directory_.GetPath());
  }

 protected:
};

// This portion causes test fixtures to be defined.
// Notice that in the store we are using "this->" to refer to the methods
// defined on the |RequestQuieueStoreBaseTest| class. That's by design.

TEST_F(RequestQueueStoreTest, UpgradeFromVersion57Store) {
  std::unique_ptr<RequestQueueStore> store = BuildStoreWithOldSchema(57);
  // In-memory store does not support upgrading.
  if (!store)
    return;
  this->InitializeStore(store.get());

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1u, this->last_requests().size());
  EXPECT_EQ(kRequestId, this->last_requests()[0]->request_id());
  EXPECT_EQ(kUrl, this->last_requests()[0]->url());
  EXPECT_EQ(GURL(), this->last_requests()[0]->original_url());
}

TEST_F(RequestQueueStoreTest, UpgradeFromVersion58Store) {
  std::unique_ptr<RequestQueueStore> store(BuildStoreWithOldSchema(58));
  // In-memory store does not support upgrading.
  if (!store)
    return;
  this->InitializeStore(store.get());

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1u, this->last_requests().size());
  EXPECT_EQ(kRequestId, this->last_requests()[0]->request_id());
  EXPECT_EQ(kUrl, this->last_requests()[0]->url());
  EXPECT_EQ(kUrl2, this->last_requests()[0]->original_url());
  EXPECT_EQ("", this->last_requests()[0]->request_origin());
}

TEST_F(RequestQueueStoreTest, UpgradeFromVersion61Store) {
  std::unique_ptr<RequestQueueStore> store(BuildStoreWithOldSchema(61));
  // In-memory store does not support upgrading.
  if (!store)
    return;
  this->InitializeStore(store.get());

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1u, this->last_requests().size());
  EXPECT_EQ(kRequestId, this->last_requests()[0]->request_id());
  EXPECT_EQ(kUrl, this->last_requests()[0]->url());
  EXPECT_EQ(kUrl2, this->last_requests()[0]->original_url());
  EXPECT_EQ(kRequestOrigin, this->last_requests()[0]->request_origin());
  EXPECT_EQ(0, static_cast<int>(this->last_requests()[0]->fail_state()));
}

TEST_F(RequestQueueStoreTest, GetRequestsEmpty) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
}

TEST_F(RequestQueueStoreTest, GetRequestsByIds) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId, kUrl, kClientId, creation_time,
                           kUserRequested);
  store->AddRequest(request1,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  store->AddRequest(request2,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  std::vector<int64_t> request_ids{kRequestId, kRequestId2};
  store->GetRequestsByIds(
      request_ids, base::BindOnce(&RequestQueueStoreTestBase::UpdateRequestDone,
                                  base::Unretained(this)));

  ASSERT_FALSE(this->last_update_result());
  this->PumpLoop();
  ASSERT_TRUE(this->last_update_result());
  EXPECT_EQ(2UL, this->last_update_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, this->last_update_result()->item_statuses[0].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[0].second);
  EXPECT_EQ(kRequestId2, this->last_update_result()->item_statuses[1].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[1].second);
  EXPECT_EQ(2UL, this->last_update_result()->updated_items.size());
  EXPECT_EQ(request1, this->last_update_result()->updated_items.at(0));
  EXPECT_EQ(request2, this->last_update_result()->updated_items.at(1));
  this->ClearResults();

  request_ids.clear();
  request_ids.push_back(kRequestId);
  request_ids.push_back(kRequestId3);
  request_ids.push_back(kRequestId);

  store->GetRequestsByIds(
      request_ids, base::BindOnce(&RequestQueueStoreTestBase::UpdateRequestDone,
                                  base::Unretained(this)));

  ASSERT_FALSE(this->last_update_result());
  this->PumpLoop();
  ASSERT_TRUE(this->last_update_result());
  EXPECT_EQ(2UL, this->last_update_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, this->last_update_result()->item_statuses[0].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[0].second);
  EXPECT_EQ(kRequestId3, this->last_update_result()->item_statuses[1].first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            this->last_update_result()->item_statuses[1].second);
  EXPECT_EQ(1UL, this->last_update_result()->updated_items.size());
  EXPECT_EQ(request1, this->last_update_result()->updated_items.at(0));
}

TEST_F(RequestQueueStoreTest, AddRequest) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_original_url(kUrl2);

  store->AddRequest(request,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  ASSERT_EQ(ItemActionStatus::NOT_FOUND, this->last_add_status());
  this->PumpLoop();
  ASSERT_EQ(ItemActionStatus::SUCCESS, this->last_add_status());

  // Verifying get reqeust results after a request was added.
  this->ClearResults();
  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_EQ(request, *(this->last_requests()[0].get()));

  // Verify it is not possible to add the same request twice.
  this->ClearResults();
  store->AddRequest(request,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  ASSERT_EQ(ItemActionStatus::NOT_FOUND, this->last_add_status());
  this->PumpLoop();
  ASSERT_EQ(ItemActionStatus::ALREADY_EXISTS, this->last_add_status());

  // Check that there is still only one item in the store.
  this->ClearResults();
  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
}

TEST_F(RequestQueueStoreTest, UpdateRequest) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time,
                                   kUserRequested);
  store->AddRequest(original_request,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  base::Time new_creation_time =
      creation_time + base::TimeDelta::FromMinutes(1);
  // Try updating an existing request.
  SavePageRequest updated_request(kRequestId, kUrl, kClientId,
                                  new_creation_time, kUserRequested);
  updated_request.set_original_url(kUrl2);
  updated_request.set_request_origin(kRequestOrigin);
  // Try to update a non-existing request.
  SavePageRequest updated_request2(kRequestId2, kUrl, kClientId,
                                   new_creation_time, kUserRequested);
  std::vector<SavePageRequest> requests_to_update{updated_request,
                                                  updated_request2};
  store->UpdateRequests(
      requests_to_update,
      base::BindOnce(&RequestQueueStoreTestBase::UpdateRequestDone,
                     base::Unretained(this)));
  ASSERT_FALSE(this->last_update_result());
  this->PumpLoop();
  ASSERT_TRUE(this->last_update_result());
  EXPECT_EQ(2UL, this->last_update_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, this->last_update_result()->item_statuses[0].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[0].second);
  EXPECT_EQ(kRequestId2, this->last_update_result()->item_statuses[1].first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            this->last_update_result()->item_statuses[1].second);
  EXPECT_EQ(1UL, this->last_update_result()->updated_items.size());
  EXPECT_EQ(updated_request,
            *(this->last_update_result()->updated_items.begin()));

  // Verifying get reqeust results after a request was updated.
  this->ClearResults();
  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_EQ(updated_request, *(this->last_requests()[0].get()));
}

TEST_F(RequestQueueStoreTest, RemoveRequests) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId, kUrl, kClientId, creation_time,
                           kUserRequested);
  store->AddRequest(request1,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  store->AddRequest(request2,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  std::vector<int64_t> request_ids{kRequestId, kRequestId2};
  store->RemoveRequests(
      request_ids, base::BindOnce(&RequestQueueStoreTestBase::UpdateRequestDone,
                                  base::Unretained(this)));

  ASSERT_FALSE(this->last_update_result());
  this->PumpLoop();
  ASSERT_TRUE(this->last_update_result());
  EXPECT_EQ(2UL, this->last_update_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, this->last_update_result()->item_statuses[0].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[0].second);
  EXPECT_EQ(kRequestId2, this->last_update_result()->item_statuses[1].first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            this->last_update_result()->item_statuses[1].second);
  EXPECT_EQ(2UL, this->last_update_result()->updated_items.size());
  EXPECT_EQ(request1, this->last_update_result()->updated_items.at(0));
  EXPECT_EQ(request2, this->last_update_result()->updated_items.at(1));
  this->ClearResults();

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
  this->ClearResults();

  // Try to remove a request that is not in the queue.
  store->RemoveRequests(
      request_ids, base::BindOnce(&RequestQueueStoreTestBase::UpdateRequestDone,
                                  base::Unretained(this)));
  ASSERT_FALSE(this->last_update_result());
  this->PumpLoop();
  ASSERT_TRUE(this->last_update_result());
  // When requests are missing, we expect the results to say so, but since they
  // are missing, no requests should have been returned.
  EXPECT_EQ(2UL, this->last_update_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, this->last_update_result()->item_statuses[0].first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            this->last_update_result()->item_statuses[0].second);
  EXPECT_EQ(kRequestId2, this->last_update_result()->item_statuses[1].first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            this->last_update_result()->item_statuses[1].second);
  EXPECT_EQ(0UL, this->last_update_result()->updated_items.size());
}

TEST_F(RequestQueueStoreTest, ResetStore) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time,
                                   kUserRequested);
  store->AddRequest(original_request,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  store->Reset(base::BindOnce(&RequestQueueStoreTestBase::ResetDone,
                              base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  this->ClearResults();

  this->InitializeStore(store.get());
  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
}

// Makes sure that persistent DB is actually persisting requests across store
// restarts.
TEST_F(RequestQueueStoreTest, SaveCloseReopenRead) {
  std::unique_ptr<RequestQueueStore> store(BuildStore());
  this->InitializeStore(store.get());

  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time,
                                   kUserRequested);
  store->AddRequest(original_request,
                    base::BindOnce(&RequestQueueStoreTestBase::AddRequestDone,
                                   base::Unretained(this)));
  PumpLoop();
  ClearResults();

  // Resets the store, using the same temp directory. The contents should be
  // intact. First reset is done separately to release DB lock.
  store.reset();
  store = BuildStore();
  this->InitializeStore(store.get());

  store->GetRequests(base::BindOnce(&RequestQueueStoreTestBase::GetRequestsDone,
                                    base::Unretained(this)));
  ASSERT_EQ(LastResult::RESULT_NONE, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::RESULT_TRUE, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_TRUE(original_request == *(this->last_requests().at(0).get()));
}

}  // namespace
}  // namespace offline_pages
