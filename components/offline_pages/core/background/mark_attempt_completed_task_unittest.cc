// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_completed_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const int64_t kRequestId1 = 42;
const int64_t kRequestId2 = 44;
const GURL kUrl1("http://example.com");
const ClientId kClientId1("download", "1234");

class MarkAttemptCompletedTaskTest : public RequestQueueTaskTestBase {
 public:
  MarkAttemptCompletedTaskTest() {}
  ~MarkAttemptCompletedTaskTest() override {}

  void AddStartedItemToStore();
  void ChangeRequestsStateCallback(UpdateRequestsResult result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  void AddRequestDone(ItemActionStatus status);

  std::unique_ptr<UpdateRequestsResult> result_;
};

void MarkAttemptCompletedTaskTest::AddStartedItemToStore() {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request_1(kRequestId1, kUrl1, kClientId1, creation_time,
                            true);
  request_1.MarkAttemptStarted(base::Time::Now());
  store_.AddRequest(
      request_1, base::BindOnce(&MarkAttemptCompletedTaskTest::AddRequestDone,
                                base::Unretained(this)));
  PumpLoop();
}

void MarkAttemptCompletedTaskTest::ChangeRequestsStateCallback(
    UpdateRequestsResult result) {
  result_ = std::make_unique<UpdateRequestsResult>(std::move(result));
}

void MarkAttemptCompletedTaskTest::AddRequestDone(ItemActionStatus status) {
  ASSERT_EQ(ItemActionStatus::SUCCESS, status);
}

TEST_F(MarkAttemptCompletedTaskTest, MarkAttemptCompletedWhenExists) {
  InitializeStore();
  AddStartedItemToStore();

  MarkAttemptCompletedTask task(
      &store_, kRequestId1, FailState::CANNOT_DOWNLOAD,
      base::BindOnce(&MarkAttemptCompletedTaskTest::ChangeRequestsStateCallback,
                     base::Unretained(this)));

  task.Run();
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
  task.Run();
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
