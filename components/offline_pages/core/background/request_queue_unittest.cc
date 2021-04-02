// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using AddRequestResult = AddRequestResult;
using GetRequestsResult = GetRequestsResult;
using UpdateRequestResult = UpdateRequestResult;

namespace {
// Data for request 1.
const int64_t kRequestId = 42;
const ClientId kClientId("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 77;
const ClientId kClientId2("bookmark", "567");
const bool kUserRequested = true;
const int64_t kRequestId3 = 99;
const int kOneWeekInSeconds = 7 * 24 * 60 * 60;

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL Url1() {
  return GURL("http://example.com");
}
GURL Url2() {
  return GURL("http://test.com");
}

// Default request
SavePageRequest EmptyRequest() {
  return SavePageRequest(0UL, GURL(""), ClientId("", ""), base::Time(), true);
}

}  // namespace

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
                             int64_t received_bytes) override {
    // This has nothing to do with the queue (progress is ephemeral) - no test.
  }

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

class RequestQueueTest : public testing::Test {
 public:
  RequestQueueTest();
  ~RequestQueueTest() override;

  // Test overrides.
  void SetUp() override;
  void TearDown() override {
    store_->Close();
    PumpLoop();
  }

  void PumpLoop();

  // Callback for adding requests.
  void AddRequestDone(AddRequestResult result, const SavePageRequest& request);
  // Callback for getting requests.
  void GetRequestsDone(GetRequestsResult result,
                       std::vector<std::unique_ptr<SavePageRequest>> requests);

  void UpdateRequestDone(UpdateRequestResult result);
  void UpdateRequestsDone(UpdateRequestsResult result);

  void ClearResults();

  RequestQueue* queue() { return queue_.get(); }

  AddRequestResult last_add_result() const { return last_add_result_; }
  SavePageRequest* last_added_request() { return last_added_request_.get(); }

  UpdateRequestResult last_update_result() const { return last_update_result_; }

  GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return last_requests_;
  }

  UpdateRequestsResult* update_requests_result() const {
    return update_requests_result_.get();
  }

  void RequestPickedCallback(const SavePageRequest& request) {}
  void RequestNotPickedCallback(bool non_user_requested_tasks_remain) {}
  void RequestCountCallback(size_t total_count, size_t available_count) {}

 private:
  AddRequestResult last_add_result_;
  std::unique_ptr<SavePageRequest> last_added_request_;
  std::unique_ptr<UpdateRequestsResult> update_requests_result_;
  UpdateRequestResult last_update_result_;

  GetRequestsResult last_get_requests_result_;
  std::vector<std::unique_ptr<SavePageRequest>> last_requests_;

  std::unique_ptr<RequestQueue> queue_;
  TestRequestQueueStore* store_;  // Owned by queue_.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueTest::RequestQueueTest()
    : last_add_result_(AddRequestResult::STORE_FAILURE),
      last_update_result_(UpdateRequestResult::STORE_FAILURE),
      last_get_requests_result_(GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestQueueTest::~RequestQueueTest() {}

void RequestQueueTest::SetUp() {
  auto store = std::make_unique<TestRequestQueueStore>();
  store_ = store.get();
  queue_.reset(new RequestQueue(std::move(store)));
}

void RequestQueueTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueTest::AddRequestDone(AddRequestResult result,
                                      const SavePageRequest& request) {
  last_add_result_ = result;
  last_added_request_.reset(new SavePageRequest(request));
}

void RequestQueueTest::GetRequestsDone(
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_get_requests_result_ = result;
  last_requests_ = std::move(requests);
}

void RequestQueueTest::UpdateRequestDone(UpdateRequestResult result) {
  last_update_result_ = result;
}

void RequestQueueTest::UpdateRequestsDone(UpdateRequestsResult result) {
  update_requests_result_ =
      std::make_unique<UpdateRequestsResult>(std::move(result));
}

void RequestQueueTest::ClearResults() {
  last_add_result_ = AddRequestResult::STORE_FAILURE;
  last_update_result_ = UpdateRequestResult::STORE_FAILURE;
  last_get_requests_result_ = GetRequestsResult::STORE_FAILURE;
  last_added_request_.reset(nullptr);
  update_requests_result_.reset(nullptr);
  last_requests_.clear();
}

TEST_F(RequestQueueTest, GetRequestsEmpty) {
  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, AddRequest) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(AddRequestResult::SUCCESS, last_add_result());
  ASSERT_TRUE(last_added_request());
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
}

TEST_F(RequestQueueTest, RemoveRequest) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  std::vector<int64_t> remove_requests{kRequestId};
  queue()->RemoveRequests(remove_requests,
                          base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(request, update_requests_result()->updated_items.at(0));

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, RemoveSeveralRequests) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  SavePageRequest request2(kRequestId2, Url2(), kClientId2, creation_time,
                           kUserRequested);
  queue()->AddRequest(request2, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId2, last_added_request()->request_id());

  std::vector<int64_t> remove_requests;
  remove_requests.push_back(kRequestId);
  remove_requests.push_back(kRequestId2);
  remove_requests.push_back(kRequestId3);
  queue()->RemoveRequests(remove_requests,
                          base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(3ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(kRequestId2, update_requests_result()->item_statuses.at(1).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(1).second);
  ASSERT_EQ(kRequestId3, update_requests_result()->item_statuses.at(2).first);
  ASSERT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(2).second);
  EXPECT_EQ(2UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(request, update_requests_result()->updated_items.at(0));
  EXPECT_EQ(request2, update_requests_result()->updated_items.at(1));

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();

  // Verify both requests are no longer in the queue.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, PauseAndResume) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());

  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId);

  // Pause the request.
  queue()->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(1ul, update_requests_result()->updated_items.size());
  ASSERT_EQ(SavePageRequest::RequestState::PAUSED,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();

  // Verify the request is paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::PAUSED,
            last_requests().at(0)->request_state());

  // Resume the request.
  queue()->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::AVAILABLE,
      base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(1ul, update_requests_result()->updated_items.size());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();

  // Verify the request is no longer paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE,
            last_requests().at(0)->request_state());
}

// A longer test populating the request queue with more than one item, properly
// listing multiple items and removing the right item.
TEST_F(RequestQueueTest, MultipleRequestsAddGetRemove) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request1(kRequestId, Url1(), kClientId, creation_time,
                           kUserRequested);
  queue()->AddRequest(request1, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request1.request_id(), last_added_request()->request_id());
  SavePageRequest request2(kRequestId2, Url2(), kClientId2, creation_time,
                           kUserRequested);
  queue()->AddRequest(request2, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request2.request_id(), last_added_request()->request_id());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(2ul, last_requests().size());

  std::vector<int64_t> remove_requests;
  remove_requests.push_back(request1.request_id());
  queue()->RemoveRequests(remove_requests,
                          base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(request2.request_id(), last_requests().at(0)->request_id());
}

TEST_F(RequestQueueTest, MarkAttemptStarted) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();

  base::Time before_time = OfflineTimeNow();
  // Update the request, ensure it succeeded.
  queue()->MarkAttemptStarted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_LE(before_time,
            update_requests_result()->updated_items.at(0).last_attempt_time());
  EXPECT_GE(OfflineTimeNow(),
            update_requests_result()->updated_items.at(0).last_attempt_time());
  EXPECT_EQ(
      1, update_requests_result()->updated_items.at(0).started_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  EXPECT_EQ(update_requests_result()->updated_items.at(0),
            *last_requests().at(0));
}

TEST_F(RequestQueueTest, MarkAttempStartedRequestNotPresent) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = OfflineTimeNow();
  // This request is never put into the queue.
  SavePageRequest request1(kRequestId, Url1(), kClientId, creation_time,
                           kUserRequested);

  queue()->MarkAttemptStarted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(0ul, update_requests_result()->updated_items.size());
}

TEST_F(RequestQueueTest, MarkAttemptAborted) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();

  // Start request.
  queue()->MarkAttemptStarted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();
  ClearResults();

  queue()->MarkAttemptAborted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();

  ASSERT_TRUE(update_requests_result());
  EXPECT_EQ(1UL, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());
}

TEST_F(RequestQueueTest, MarkAttemptAbortedRequestNotPresent) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = OfflineTimeNow();
  // This request is never put into the queue.
  SavePageRequest request1(kRequestId, Url1(), kClientId, creation_time,
                           kUserRequested);

  queue()->MarkAttemptAborted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(0ul, update_requests_result()->updated_items.size());
}

TEST_F(RequestQueueTest, MarkAttemptCompleted) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  PumpLoop();

  // Start request.
  queue()->MarkAttemptStarted(
      kRequestId, base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                                 base::Unretained(this)));
  PumpLoop();
  ClearResults();

  queue()->MarkAttemptCompleted(
      kRequestId, FailState::CANNOT_DOWNLOAD,
      base::BindOnce(&RequestQueueTest::UpdateRequestsDone,
                     base::Unretained(this)));
  PumpLoop();

  ASSERT_TRUE(update_requests_result());
  EXPECT_EQ(1UL, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());
}

TEST_F(RequestQueueTest, CleanStaleRequests) {
  // Create a request that is already expired.
  base::Time creation_time =
      OfflineTimeNow() - base::TimeDelta::FromSeconds(2 * kOneWeekInSeconds);

  SavePageRequest original_request(kRequestId, Url1(), kClientId, creation_time,
                                   kUserRequested);
  queue()->AddRequest(original_request, RequestQueue::AddOptions(),
                      base::BindOnce(&RequestQueueTest::AddRequestDone,
                                     base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  // Set up a picker factory pointing to our fake notifier.
  OfflinerPolicy policy;
  RequestNotifierStub notifier;
  RequestCoordinatorEventLogger event_logger;
  std::unique_ptr<CleanupTaskFactory> cleanup_factory(
      new CleanupTaskFactory(&policy, &notifier, &event_logger));
  queue()->SetCleanupFactory(std::move(cleanup_factory));

  // Do a pick and clean operation, which will remove stale entries.
  DeviceConditions conditions;
  std::set<int64_t> disabled_list;
  queue()->CleanupRequestQueue();

  this->PumpLoop();

  // Notifier should have been notified that the request was removed.
  ASSERT_EQ(notifier.last_expired_request().request_id(), kRequestId);
  ASSERT_EQ(notifier.last_request_expiration_status(),
            RequestNotifier::BackgroundSavePageResult::EXPIRED);

  // Doing a get should show no entries left in the queue since the expired
  // request has been removed.
  queue()->GetRequests(base::BindOnce(&RequestQueueTest::GetRequestsDone,
                                      base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, this->last_get_requests_result());
  ASSERT_TRUE(this->last_requests().empty());
}

}  // namespace offline_pages
