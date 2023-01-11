// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/change_requests_state_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

const int64_t kRequestId1 = 42;
const int64_t kRequestId2 = 43;
const int64_t kRequestId3 = 44;

const ClientId kClientId1("bookmark", "1234");
const ClientId kClientId2("async", "5678");

class ChangeRequestsStateTaskTest : public RequestQueueTaskTestBase {
 public:
  ~ChangeRequestsStateTaskTest() override = default;

  void AddItemsToStore();
  void ChangeRequestsStateCallback(UpdateRequestsResult result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  void InitializeStoreDone(bool success);
  void AddRequestDone(AddRequestResult result);

  std::unique_ptr<UpdateRequestsResult> result_;
};

void ChangeRequestsStateTaskTest::AddItemsToStore() {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request_1(kRequestId1, GURL("http://example.com"), kClientId1,
                            creation_time, true);
  store_.AddRequest(request_1, RequestQueue::AddOptions(),
                    base::BindOnce(&ChangeRequestsStateTaskTest::AddRequestDone,
                                   base::Unretained(this)));
  SavePageRequest request_2(kRequestId2, GURL("http://another-example.com"),
                            kClientId2, creation_time, true);
  store_.AddRequest(request_2, RequestQueue::AddOptions(),
                    base::BindOnce(&ChangeRequestsStateTaskTest::AddRequestDone,
                                   base::Unretained(this)));
  PumpLoop();
}

void ChangeRequestsStateTaskTest::ChangeRequestsStateCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}

void ChangeRequestsStateTaskTest::InitializeStoreDone(bool success) {
  ASSERT_TRUE(success);
}

void ChangeRequestsStateTaskTest::AddRequestDone(AddRequestResult result) {
  ASSERT_EQ(AddRequestResult::SUCCESS, result);
}

TEST_F(ChangeRequestsStateTaskTest, UpdateWhenStoreEmpty) {
  InitializeStore();

  std::vector<int64_t> request_ids{kRequestId1};
  ChangeRequestsStateTask task(
      &store_, request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateSingleItem) {
  InitializeStore();
  AddItemsToStore();

  std::vector<int64_t> request_ids{kRequestId1};
  ChangeRequestsStateTask task(
      &store_, request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(0).request_state());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateMultipleItems) {
  InitializeStore();
  AddItemsToStore();

  std::vector<int64_t> request_ids{kRequestId1, kRequestId2};
  ChangeRequestsStateTask task(
      &store_, request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  ASSERT_EQ(2UL, last_result()->item_statuses.size());

  // Calculating the position of the items in the vector here, because it does
  // not matter, and might be platform dependent.
  // |index_id_1| is expected to correspond to |kRequestId1|.
  int index_id_1 =
      last_result()->item_statuses.at(0).first == kRequestId1 ? 0 : 1;
  // |index_id_2| is expected to correspond to |kRequestId2|.
  int index_id_2 = 1 - index_id_1;

  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(index_id_1).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(index_id_1).second);
  EXPECT_EQ(kRequestId2, last_result()->item_statuses.at(index_id_2).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(index_id_2).second);
  ASSERT_EQ(2UL, last_result()->updated_items.size());
  EXPECT_EQ(kRequestId1,
            last_result()->updated_items.at(index_id_1).request_id());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(index_id_1).request_state());
  EXPECT_EQ(kRequestId2,
            last_result()->updated_items.at(index_id_2).request_id());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(index_id_2).request_state());
}

TEST_F(ChangeRequestsStateTaskTest, EmptyRequestsList) {
  InitializeStore();

  std::vector<int64_t> request_ids;
  ChangeRequestsStateTask task(
      &store_, request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(0UL, last_result()->item_statuses.size());
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateMissingItem) {
  InitializeStore();
  AddItemsToStore();

  std::vector<int64_t> request_ids{kRequestId1, kRequestId3};
  ChangeRequestsStateTask task(
      &store_, request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  ASSERT_EQ(2UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(kRequestId3, last_result()->item_statuses.at(1).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(1).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(0).request_state());
}

}  // namespace
}  // namespace offline_pages
