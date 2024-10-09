// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/reconcile_task.h"

#include <memory>
#include <set>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

// Data for request 1.
const int64_t kRequestId1 = 17;
const ClientId kClientId1("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 42;
const ClientId kClientId2("bookmark", "5678");
const bool kUserRequested = true;

class ReconcileTaskTest : public RequestQueueTaskTestBase {
 public:
  ReconcileTaskTest() = default;
  ~ReconcileTaskTest() override = default;

  void SetUp() override;

  static void AddRequestDone(AddRequestResult result) {
    ASSERT_EQ(AddRequestResult::SUCCESS, result);
  }

  void GetRequestsCallback(
      bool success,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  void ReconcileCallback(UpdateRequestsResult result);

  void QueueRequests(const SavePageRequest& request1,
                     const SavePageRequest& request2);

  // Reset the factory and the task using the current policy.
  void MakeTask();

  ReconcileTask* task() { return task_.get(); }
  std::vector<std::unique_ptr<SavePageRequest>>& found_requests() {
    return found_requests_;
  }

 protected:

  std::unique_ptr<ReconcileTask> task_;
  std::vector<std::unique_ptr<SavePageRequest>> found_requests_;
  bool reconcile_called_ = false;
};

void ReconcileTaskTest::SetUp() {
  DeviceConditions conditions;
  MakeTask();

  InitializeStore();
}

void ReconcileTaskTest::GetRequestsCallback(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  found_requests_ = std::move(requests);
}

void ReconcileTaskTest::ReconcileCallback(UpdateRequestsResult result) {
  reconcile_called_ = true;
  // Make sure the item in the callback is now AVAILABLE.
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            result.updated_items.at(0).request_state());
}

// Test helper to queue the two given requests.
void ReconcileTaskTest::QueueRequests(const SavePageRequest& request1,
                                      const SavePageRequest& request2) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
  // Add test requests on the Queue.
  store_.AddRequest(request1, RequestQueue::AddOptions(),
                    base::BindOnce(&ReconcileTaskTest::AddRequestDone));
  store_.AddRequest(request2, RequestQueue::AddOptions(),
                    base::BindOnce(&ReconcileTaskTest::AddRequestDone));

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();
}

void ReconcileTaskTest::MakeTask() {
  task_ = std::make_unique<ReconcileTask>(
      &store_, base::BindOnce(&ReconcileTaskTest::ReconcileCallback,
                              base::Unretained(this)));
}

TEST_F(ReconcileTaskTest, Reconcile) {
  base::Time creation_time = OfflineTimeNow();
  // Request2 will be expired, request1 will be current.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  request1.set_request_state(SavePageRequest::RequestState::PAUSED);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_request_state(SavePageRequest::RequestState::OFFLINING);
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, should be just the other request.
  store_.GetRequests(base::BindOnce(&ReconcileTaskTest::GetRequestsCallback,
                                    base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(2UL, found_requests().size());

  // in case requests come back in a different order, check which is where.
  int request1_index = 0;
  int request2_index = 1;
  if (found_requests().at(0)->request_id() != kRequestId1) {
    request1_index = 1;
    request2_index = 0;
  }
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            found_requests().at(request1_index)->request_state());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            found_requests().at(request2_index)->request_state());
  EXPECT_TRUE(reconcile_called_);
}

TEST_F(ReconcileTaskTest, NothingToReconcile) {
  base::Time creation_time = OfflineTimeNow();
  // Request2 will be expired, request1 will be current.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  request1.set_request_state(SavePageRequest::RequestState::PAUSED);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_request_state(SavePageRequest::RequestState::AVAILABLE);
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, should be just the other request.
  store_.GetRequests(base::BindOnce(&ReconcileTaskTest::GetRequestsCallback,
                                    base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(2UL, found_requests().size());

  // in case requests come back in a different order, check which is where.
  int request1_index = 0;
  int request2_index = 1;
  if (found_requests().at(0)->request_id() != kRequestId1) {
    request1_index = 1;
    request2_index = 0;
  }
  // Requests should still be in their starting states.
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            found_requests().at(request1_index)->request_state());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            found_requests().at(request2_index)->request_state());
  // In this case, we do not expect the reconcile callback to be called.
  EXPECT_FALSE(reconcile_called_);
}

}  // namespace
}  // namespace offline_pages
