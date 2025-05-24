// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/cleanup_task.h"

#include <memory>
#include <set>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
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

// Default request
SavePageRequest EmptyRequest() {
  return SavePageRequest(0UL, GURL(""), ClientId("", ""), base::Time(), true);
}

// TODO: Refactor this stub class into its own file, use in Pick Request Task
// Test too.
// Helper class needed by the CleanupTask
class RequestNotifierStub : public RequestNotifier {
 public:
  RequestNotifierStub()
      : last_expired_request_(EmptyRequest()), total_expired_requests_(0) {}

  void NotifyAdded(const SavePageRequest& request) override {}
  void NotifyChanged(const SavePageRequest& request) override {}

  void NotifyCompleted(const SavePageRequest& request,
                       BackgroundSavePageResult status) override {
    last_expired_request_ = request;
    last_request_expiration_status_ = status;
    total_expired_requests_++;
  }

  void NotifyNetworkProgress(const SavePageRequest& request,
                             int64_t received_bytes) override {}

  const SavePageRequest& last_expired_request() {
    return last_expired_request_;
  }

  RequestCoordinator::BackgroundSavePageResult
  last_request_expiration_status() {
    return last_request_expiration_status_;
  }

  int32_t total_expired_requests() { return total_expired_requests_; }

 private:
  BackgroundSavePageResult last_request_expiration_status_;
  SavePageRequest last_expired_request_;
  int32_t total_expired_requests_;
};

class CleanupTaskTest : public RequestQueueTaskTestBase {
 public:
  CleanupTaskTest() = default;
  ~CleanupTaskTest() override = default;

  void SetUp() override;

  static void AddRequestDone(AddRequestResult result) {
    ASSERT_EQ(AddRequestResult::SUCCESS, result);
  }

  void GetRequestsCallback(
      bool success,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  void QueueRequests(const SavePageRequest& request1,
                     const SavePageRequest& request2);

  // Reset the factory and the task using the current policy.
  void MakeFactoryAndTask();

  RequestNotifierStub* GetNotifier() { return notifier_.get(); }

  CleanupTask* task() { return task_.get(); }
  RequestQueueStore* store() { return &store_; }
  OfflinerPolicy* policy() { return policy_.get(); }
  std::vector<std::unique_ptr<SavePageRequest>>& found_requests() {
    return found_requests_;
  }

 protected:

  std::unique_ptr<RequestNotifierStub> notifier_;
  std::unique_ptr<SavePageRequest> last_picked_;
  std::unique_ptr<OfflinerPolicy> policy_;
  RequestCoordinatorEventLogger event_logger_;
  std::unique_ptr<CleanupTaskFactory> factory_;
  std::unique_ptr<CleanupTask> task_;
  std::vector<std::unique_ptr<SavePageRequest>> found_requests_;
};

void CleanupTaskTest::SetUp() {
  DeviceConditions conditions;
  policy_ = std::make_unique<OfflinerPolicy>();
  notifier_ = std::make_unique<RequestNotifierStub>();
  MakeFactoryAndTask();

  InitializeStore();
}

void CleanupTaskTest::GetRequestsCallback(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  found_requests_ = std::move(requests);
}

// Test helper to queue the two given requests.
void CleanupTaskTest::QueueRequests(const SavePageRequest& request1,
                                    const SavePageRequest& request2) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
  // Add test requests on the Queue.
  store_.AddRequest(request1, RequestQueue::AddOptions(),
                    base::BindOnce(&CleanupTaskTest::AddRequestDone));
  store_.AddRequest(request2, RequestQueue::AddOptions(),
                    base::BindOnce(&CleanupTaskTest::AddRequestDone));

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();
}

void CleanupTaskTest::MakeFactoryAndTask() {
  factory_ = std::make_unique<CleanupTaskFactory>(
      policy_.get(), notifier_.get(), &event_logger_);
  DeviceConditions conditions;
  task_ = factory_->CreateCleanupTask(&store_);
}

TEST_F(CleanupTaskTest, CleanupExpiredRequest) {
  base::Time creation_time = OfflineTimeNow();
  base::Time expired_time =
      creation_time -
      base::Seconds(policy()->GetRequestExpirationTimeInSeconds() + 10);
  // Request2 will be expired, request1 will be current.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           expired_time, kUserRequested);
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, should be just the other request.
  store()->GetRequests(base::BindOnce(&CleanupTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1UL, found_requests().size());
  EXPECT_EQ(kRequestId1, found_requests().at(0)->request_id());
}

TEST_F(CleanupTaskTest, CleanupStartCountExceededRequest) {
  base::Time creation_time = OfflineTimeNow();
  // Request2 will have an exceeded start count.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_started_attempt_count(policy()->GetMaxStartedTries());
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, should be just the other request.
  store()->GetRequests(base::BindOnce(&CleanupTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1UL, found_requests().size());
  EXPECT_EQ(kRequestId1, found_requests().at(0)->request_id());
}

TEST_F(CleanupTaskTest, CleanupCompletionCountExceededRequest) {
  base::Time creation_time = OfflineTimeNow();
  // Request2 will have an exceeded completion count.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(policy()->GetMaxCompletedTries());
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, should be just the other request.
  store()->GetRequests(base::BindOnce(&CleanupTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1UL, found_requests().size());
  EXPECT_EQ(kRequestId1, found_requests().at(0)->request_id());
}

TEST_F(CleanupTaskTest, IgnoreRequestInProgress) {
  base::Time creation_time = OfflineTimeNow();
  // Both requests will have an exceeded completion count.
  // The first request will be marked as started.
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  request1.set_completed_attempt_count(policy()->GetMaxCompletedTries());
  request1.MarkAttemptStarted(creation_time);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(policy()->GetMaxCompletedTries());
  QueueRequests(request1, request2);

  // Initiate cleanup.
  task()->Execute(base::DoNothing());
  PumpLoop();

  // See what is left in the queue, request1 should be left in the queue even
  // though it is expired because it was listed as in-progress while cleaning.
  // Request2 should have been cleaned out of the queue.
  store()->GetRequests(base::BindOnce(&CleanupTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1UL, found_requests().size());
  EXPECT_EQ(kRequestId1, found_requests().at(0)->request_id());
}

}  // namespace
}  // namespace offline_pages
