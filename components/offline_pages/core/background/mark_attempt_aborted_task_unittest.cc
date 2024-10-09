// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_aborted_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/change_requests_state_task.h"
#include "components/offline_pages/core/background/mark_attempt_started_task.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/offline_clock.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

const int64_t kRequestId1 = 42;
const int64_t kRequestId2 = 44;

const ClientId kClientId1("download", "1234");

class MarkAttemptAbortedTaskTest : public RequestQueueTaskTestBase {
 public:
  MarkAttemptAbortedTaskTest() = default;
  ~MarkAttemptAbortedTaskTest() override = default;

  void AddItemToStore(RequestQueueStore* store);
  void ChangeRequestsStateCallback(UpdateRequestsResult result);

  void ClearResults();

  UpdateRequestsResult* last_result() const { return result_.get(); }

 protected:
  void InitializeStoreDone(bool success);
  void AddRequestDone(AddRequestResult result);

  std::unique_ptr<UpdateRequestsResult> result_;
};

void MarkAttemptAbortedTaskTest::AddItemToStore(RequestQueueStore* store) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request_1(kRequestId1, GURL("http://example.com"), kClientId1,
                            creation_time, true);
  store->AddRequest(request_1, RequestQueue::AddOptions(),
                    base::BindOnce(&MarkAttemptAbortedTaskTest::AddRequestDone,
                                   base::Unretained(this)));
  PumpLoop();
}

void MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}

void MarkAttemptAbortedTaskTest::ClearResults() {
  result_.reset(nullptr);
}

void MarkAttemptAbortedTaskTest::InitializeStoreDone(bool success) {
  ASSERT_TRUE(success);
}

void MarkAttemptAbortedTaskTest::AddRequestDone(AddRequestResult result) {
  ASSERT_EQ(AddRequestResult::SUCCESS, result);
}

TEST_F(MarkAttemptAbortedTaskTest, MarkAttemptAbortedWhenStoreEmpty) {
  InitializeStore();

  MarkAttemptAbortedTask task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
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

TEST_F(MarkAttemptAbortedTaskTest, MarkAttemptAbortedWhenExists) {
  InitializeStore();
  AddItemToStore(&store_);

  // First mark attempt started.
  MarkAttemptStartedTask start_request_task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  start_request_task.Execute(base::DoNothing());
  PumpLoop();
  ClearResults();

  MarkAttemptAbortedTask task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));

  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            last_result()->updated_items.at(0).request_state());
}

TEST_F(MarkAttemptAbortedTaskTest, MarkAttemptAbortedWhenItemMissing) {
  InitializeStore();
  AddItemToStore(&store_);

  MarkAttemptAbortedTask task(
      &store_, kRequestId2,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId2, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(MarkAttemptAbortedTaskTest, MarkAttemptAbortedWhenPaused) {
  InitializeStore();
  AddItemToStore(&store_);

  // First mark attempt started.
  MarkAttemptStartedTask start_request_task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  start_request_task.Execute(base::DoNothing());
  PumpLoop();
  ClearResults();

  // Mark the attempt as PAUSED, so we test the PAUSED to PAUSED transition.
    std::vector<int64_t> requests;
  requests.push_back(kRequestId1);
  ChangeRequestsStateTask pauseTask(
      &store_, requests, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));
  pauseTask.Execute(base::DoNothing());
  PumpLoop();

  // Abort the task, the state should not change from PAUSED.
  MarkAttemptAbortedTask abortTask(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptAbortedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));

  abortTask.Execute(base::DoNothing());
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

}  // namespace
}  // namespace offline_pages
