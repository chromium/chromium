// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/pick_request_task.h"

#include <memory>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/device_conditions.h"
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
const int kAttemptCount = 1;
const int kMaxStartedTries = 5;
const int kMaxCompletedTries = 1;

// Constants for policy values - These settings represent the default values.
const bool kPreferUntried = false;
const bool kPreferEarlier = true;
const bool kPreferRetryCount = true;
const int kBackgroundProcessingTimeBudgetSeconds = 170;

// Default request
SavePageRequest EmptyRequest() {
  return SavePageRequest(0UL, GURL(""), ClientId("", ""), base::Time(), true);
}

// Helper class needed by the PickRequestTask
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

class PickRequestTaskTest : public RequestQueueTaskTestBase {
 public:
  PickRequestTaskTest() = default;
  ~PickRequestTaskTest() override = default;

  void SetUp() override;

  void RequestPicked(
      const SavePageRequest& request,
      const std::unique_ptr<std::vector<SavePageRequest>> available_requests,
      bool cleanup_needed);

  void RequestNotPicked(const bool non_user_requested_tasks_remaining,
                        bool cleanup_needed,
                        base::Time available_time);

  void QueueRequests(const SavePageRequest& request1,
                     const SavePageRequest& request2);

  // Reset the factory and the task using the current policy.
  void MakePickRequestTask();

  RequestNotifierStub* GetNotifier() { return notifier_.get(); }

  PickRequestTask* task() { return task_.get(); }

  base::OnceClosure TaskCompletionCallback() {
    return base::BindOnce(&PickRequestTaskTest::OnTaskComplete,
                          base::Unretained(this));
  }

 protected:
  void OnTaskComplete() { task_complete_called_ = true; }

  std::unique_ptr<RequestNotifierStub> notifier_;
  std::unique_ptr<SavePageRequest> last_picked_;
  std::unique_ptr<OfflinerPolicy> policy_;
  RequestCoordinatorEventLogger event_logger_;
  std::set<int64_t> disabled_requests_;
  base::circular_deque<int64_t> prioritized_requests_;
  std::unique_ptr<PickRequestTask> task_;
  bool request_queue_not_picked_called_;
  bool cleanup_needed_;
  bool task_complete_called_;
};

void PickRequestTaskTest::SetUp() {
  DeviceConditions conditions;
  policy_ = std::make_unique<OfflinerPolicy>();
  notifier_ = std::make_unique<RequestNotifierStub>();
  MakePickRequestTask();
  request_queue_not_picked_called_ = false;
  task_complete_called_ = false;
  last_picked_.reset();
  cleanup_needed_ = false;

  InitializeStore();
  PumpLoop();
}

void PickRequestTaskTest::RequestPicked(
    const SavePageRequest& request,
    std::unique_ptr<std::vector<SavePageRequest>> available_requests,
    const bool cleanup_needed) {
  last_picked_ = std::make_unique<SavePageRequest>(request);
  cleanup_needed_ = cleanup_needed;
}

void PickRequestTaskTest::RequestNotPicked(
    const bool non_user_requested_tasks_remaining,
    const bool cleanup_needed,
    base::Time available_time) {
  request_queue_not_picked_called_ = true;
}

// Test helper to queue the two given requests.
void PickRequestTaskTest::QueueRequests(const SavePageRequest& request1,
                                        const SavePageRequest& request2) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
  // Add test requests on the Queue.
  store_.AddRequest(request1, RequestQueue::AddOptions(), base::DoNothing());
  store_.AddRequest(request2, RequestQueue::AddOptions(), base::DoNothing());

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();
}

void PickRequestTaskTest::MakePickRequestTask() {
  DeviceConditions conditions;
  task_ = std::make_unique<PickRequestTask>(
      &store_, policy_.get(),
      base::BindOnce(&PickRequestTaskTest::RequestPicked,
                     base::Unretained(this)),
      base::BindOnce(&PickRequestTaskTest::RequestNotPicked,
                     base::Unretained(this)),
      conditions, disabled_requests_, &prioritized_requests_);
}

TEST_F(PickRequestTaskTest, PickFromEmptyQueue) {
  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "QueueEmpty"
  // callback.
  PumpLoop();

  EXPECT_TRUE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithHigherRetryCount) {
  // Set up policy to prefer higher retry count.
  policy_ = std::make_unique<OfflinerPolicy>(
      kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds);
  MakePickRequestTask();

  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithSameRetryCountButEarlier) {
  base::Time creation_time1 = OfflineTimeNow() - base::Seconds(10);
  base::Time creation_time2 = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time1, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time2, kUserRequested);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseEarlierRequest) {
  // We need a custom policy object prefering recency to retry count.
  policy_ = std::make_unique<OfflinerPolicy>(
      kPreferUntried, kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries, kBackgroundProcessingTimeBudgetSeconds);
  MakePickRequestTask();

  base::Time creation_time1 = OfflineTimeNow() - base::Seconds(10);
  base::Time creation_time2 = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time1, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time2, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseSameTimeRequestWithHigherRetryCount) {
  // We need a custom policy object preferring recency to retry count.
  policy_ = std::make_unique<OfflinerPolicy>(
      kPreferUntried, kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds);
  MakePickRequestTask();

  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithLowerRetryCount) {
  // We need a custom policy object preferring lower retry count.
  policy_ = std::make_unique<OfflinerPolicy>(
      !kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds);
  MakePickRequestTask();

  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseLaterRequest) {
  // We need a custom policy preferring recency over retry, and later requests.
  policy_ = std::make_unique<OfflinerPolicy>(
      kPreferUntried, !kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries, kBackgroundProcessingTimeBudgetSeconds);
  MakePickRequestTask();

  base::Time creation_time1 = OfflineTimeNow() - base::Seconds(10);
  base::Time creation_time2 = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time1, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time2, kUserRequested);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseNonExpiredRequest) {
  base::Time creation_time = OfflineTimeNow();
  base::Time expired_time =
      creation_time -
      base::Seconds(policy_->GetRequestExpirationTimeInSeconds() + 60);
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           expired_time, kUserRequested);

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatHasNotExceededStartLimit) {
  base::Time creation_time1 = OfflineTimeNow() - base::Seconds(1);
  base::Time creation_time2 = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time1, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time2, kUserRequested);

  // With default policy settings, we should choose the earlier request.
  // However, we will make the earlier reqeust exceed the limit.
  request1.set_started_attempt_count(policy_->GetMaxStartedTries());

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatHasNotExceededCompletionLimit) {
  base::Time creation_time1 = OfflineTimeNow() - base::Seconds(1);
  base::Time creation_time2 = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time1, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time2, kUserRequested);

  // With default policy settings, we should choose the earlier request.
  // However, we will make the earlier reqeust exceed the limit.
  request1.set_completed_attempt_count(policy_->GetMaxCompletedTries());

  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatIsNotDisabled) {
  policy_ = std::make_unique<OfflinerPolicy>(
      kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds);

  // put request 2 on disabled list, ensure request1 picked instead,
  // even though policy would prefer 2.
  disabled_requests_.insert(kRequestId2);
  MakePickRequestTask();

  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChoosePrioritizedRequests) {
  prioritized_requests_.push_back(kRequestId2);
  MakePickRequestTask();

  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  // Since default policy prefer untried requests, make request1 the favorable
  // pick if no prioritized requests. But request2 is prioritized so it should
  // be picked.
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_EQ(1UL, prioritized_requests_.size());
}

TEST_F(PickRequestTaskTest, ChooseFromTwoPrioritizedRequests) {
  // Make two prioritized requests, the second one should be picked because
  // higher priority requests are later on the list.
  prioritized_requests_.push_back(kRequestId1);
  prioritized_requests_.push_back(kRequestId2);
  MakePickRequestTask();

  // Making request 1 more attractive to be picked not considering the
  // prioritizing issues with older creation time, fewer attempt count and it's
  // earlier in the request queue.
  base::Time creation_time = OfflineTimeNow();
  base::Time older_creation_time = creation_time - base::Minutes(10);
  SavePageRequest request1(kRequestId1, GURL("https://google.com"), kClientId1,
                           older_creation_time, kUserRequested);
  SavePageRequest request2(kRequestId2, GURL("http://nytimes.com"), kClientId2,
                           creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Execute(TaskCompletionCallback());
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_EQ(2UL, prioritized_requests_.size());
}

}  // namespace
}  // namespace offline_pages
