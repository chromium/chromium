// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_completed_task.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

class MarkAttemptCompletedTaskTest : public RequestQueueTaskTestBase {
 public:
  MarkAttemptCompletedTaskTest() = default;
  ~MarkAttemptCompletedTaskTest() override = default;

  void AddStartedItemToStore();
  void ChangeRequestsStateCallback(UpdateRequestsResult result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  static void AddRequestDone(AddRequestResult result) {
    ASSERT_EQ(AddRequestResult::SUCCESS, result);
  }

  std::unique_ptr<UpdateRequestsResult> result_;
};

void MarkAttemptCompletedTaskTest::AddStartedItemToStore() {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request_1(kRequestId1, GURL("http://example.com"), kClientId1,
                            creation_time, true);
  request_1.MarkAttemptStarted(OfflineTimeNow());
  store_.AddRequest(
      request_1, RequestQueue::AddOptions(),
      base::BindOnce(&MarkAttemptCompletedTaskTest::AddRequestDone));
  PumpLoop();
}

void MarkAttemptCompletedTaskTest::ChangeRequestsStateCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}


TEST_F(MarkAttemptCompletedTaskTest, MarkAttemptCompletedWhenExists) {
  InitializeStore();
  AddStartedItemToStore();

  MarkAttemptCompletedTask task(
      &store_, kRequestId1, FailState::CANNOT_DOWNLOAD,
      base::BindOnce(&MarkAttemptCompletedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));

  task.Execute(base::DoNothing());
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(1, last_result()->updated_items.at(0).completed_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            last_result()->updated_items.at(0).request_state());
}

TEST_F(MarkAttemptCompletedTaskTest, MarkAttemptCompletedWhenItemMissing) {
  InitializeStore();
  // Add request 1 to the store.
  AddStartedItemToStore();
  // Try to mark request 2 (not in the store).
  MarkAttemptCompletedTask task(
      &store_, kRequestId2, FailState::CANNOT_DOWNLOAD,
      base::BindOnce(&MarkAttemptCompletedTaskTest::ChangeRequestsStateCallback,
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
