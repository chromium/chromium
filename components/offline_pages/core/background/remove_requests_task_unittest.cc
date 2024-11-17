// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/remove_requests_task.h"

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

class RemoveRequestsTaskTest : public RequestQueueTaskTestBase {
 public:
  RemoveRequestsTaskTest() = default;
  ~RemoveRequestsTaskTest() override = default;

  void PumpLoop();

  void AddRequestsToStore();
  void RemoveRequestsCallback(UpdateRequestsResult result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  static void AddRequestDone(AddRequestResult result) {
    ASSERT_EQ(AddRequestResult::SUCCESS, result);
  }

  std::unique_ptr<UpdateRequestsResult> result_;
};

void RemoveRequestsTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RemoveRequestsTaskTest::AddRequestsToStore() {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request_1(kRequestId1, GURL("http://example.com"), kClientId1,
                            creation_time, true);
  store_.AddRequest(request_1, RequestQueue::AddOptions(),
                    base::BindOnce(&RemoveRequestsTaskTest::AddRequestDone));
  SavePageRequest request_2(kRequestId2, GURL("http://another-example.com"),
                            kClientId2, creation_time, true);
  store_.AddRequest(request_2, RequestQueue::AddOptions(),
                    base::BindOnce(&RemoveRequestsTaskTest::AddRequestDone));
  PumpLoop();
}

void RemoveRequestsTaskTest::RemoveRequestsCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}

TEST_F(RemoveRequestsTaskTest, RemoveWhenStoreEmpty) {
  InitializeStore();

  std::vector<int64_t> request_ids{kRequestId1};
  RemoveRequestsTask task(
      &store_, request_ids,
      base::BindOnce(&RemoveRequestsTaskTest::RemoveRequestsCallback,
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

TEST_F(RemoveRequestsTaskTest, RemoveSingleItem) {
  InitializeStore();
  AddRequestsToStore();

  std::vector<int64_t> request_ids{kRequestId1};
  RemoveRequestsTask task(
      &store_, request_ids,
      base::BindOnce(&RemoveRequestsTaskTest::RemoveRequestsCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(kRequestId1, last_result()->updated_items.at(0).request_id());
}

TEST_F(RemoveRequestsTaskTest, RemoveMultipleItems) {
  InitializeStore();
  AddRequestsToStore();

  std::vector<int64_t> request_ids{kRequestId1, kRequestId2};
  RemoveRequestsTask task(
      &store_, request_ids,
      base::BindOnce(&RemoveRequestsTaskTest::RemoveRequestsCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(2UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(kRequestId2, last_result()->item_statuses.at(1).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(1).second);
  EXPECT_EQ(2UL, last_result()->updated_items.size());
  EXPECT_EQ(kRequestId1, last_result()->updated_items.at(0).request_id());
  EXPECT_EQ(kRequestId2, last_result()->updated_items.at(1).request_id());
}

TEST_F(RemoveRequestsTaskTest, DeleteWithEmptyIdList) {
  InitializeStore();

  std::vector<int64_t> request_ids;
  RemoveRequestsTask task(
      &store_, request_ids,
      base::BindOnce(&RemoveRequestsTaskTest::RemoveRequestsCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(0UL, last_result()->item_statuses.size());
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(RemoveRequestsTaskTest, RemoveMissingItem) {
  InitializeStore();
  AddRequestsToStore();

  std::vector<int64_t> request_ids{kRequestId1, kRequestId3};
  RemoveRequestsTask task(
      &store_, request_ids,
      base::BindOnce(&RemoveRequestsTaskTest::RemoveRequestsCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(2UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(kRequestId3, last_result()->item_statuses.at(1).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(1).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(kRequestId1, last_result()->updated_items.at(0).request_id());
}

}  // namespace
}  // namespace offline_pages
