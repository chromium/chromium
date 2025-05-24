// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_started_task.h"

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
const int64_t kRequestId2 = 44;
const ClientId kClientId1("download", "1234");

class MarkAttemptStartedTaskTest : public RequestQueueTaskTestBase {
 public:
  MarkAttemptStartedTaskTest() = default;
  ~MarkAttemptStartedTaskTest() override = default;

  void AddItemToStore();
  void ChangeRequestsStateCallback(UpdateRequestsResult result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  static void AddRequestDone(AddRequestResult result) {
    ASSERT_EQ(AddRequestResult::SUCCESS, result);
  }

  std::unique_ptr<UpdateRequestsResult> result_;
};

void MarkAttemptStartedTaskTest::AddItemToStore() {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request_1(kRequestId1, GURL("http://example.com"), kClientId1,
                            creation_time, true);
  store_.AddRequest(
      request_1, RequestQueue::AddOptions(),
      base::BindOnce(&MarkAttemptStartedTaskTest::AddRequestDone));
  PumpLoop();
}

void MarkAttemptStartedTaskTest::ChangeRequestsStateCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}


TEST_F(MarkAttemptStartedTaskTest, MarkAttemptStartedWhenStoreEmpty) {
  InitializeStore();

  MarkAttemptStartedTask task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptStartedTaskTest::ChangeRequestsStateCallback,
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

TEST_F(MarkAttemptStartedTaskTest, MarkAttemptStartedWhenExists) {
  InitializeStore();
  AddItemToStore();

  MarkAttemptStartedTask task(
      &store_, kRequestId1,
      base::BindOnce(&MarkAttemptStartedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));

  // Current time for verification.
  base::Time before_time = OfflineTimeNow();
  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_LE(before_time,
            last_result()->updated_items.at(0).last_attempt_time());
  EXPECT_GE(OfflineTimeNow(),
            last_result()->updated_items.at(0).last_attempt_time());
  EXPECT_EQ(1, last_result()->updated_items.at(0).started_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING,
            last_result()->updated_items.at(0).request_state());
}

TEST_F(MarkAttemptStartedTaskTest, MarkAttemptStartedWhenItemMissing) {
  InitializeStore();
  AddItemToStore();

  MarkAttemptStartedTask task(
      &store_, kRequestId2,
      base::BindOnce(&MarkAttemptStartedTaskTest::ChangeRequestsStateCallback,
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

}  // namespace
}  // namespace offline_pages
