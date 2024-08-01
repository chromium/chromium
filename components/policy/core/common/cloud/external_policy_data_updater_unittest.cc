// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/cloud/external_policy_data_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Mock;
using testing::Return;

namespace policy {

namespace {

const char* kExternalPolicyDataKeys[] = {"external_policy_data_1",
                                         "external_policy_data_2",
                                         "external_policy_data_3"};
const char* kExternalPolicyDataURLs[] = {"http://example.com/data_1",
                                         "http://example.com/data_2",
                                         "http://example.com/data_3"};

const int64_t kExternalPolicyDataMaxSize = 20;

const char* kExternalPolicyDataPayload = "External policy data";
const char* kExternalPolicyDataOverflowPayload = "External policy data+++++++";

class MockFetchSuccessCallbackListener {
 public:
  MOCK_METHOD2(OnFetchSuccess, bool(const std::string&, const std::string&));

  ExternalPolicyDataUpdater::FetchSuccessCallback CreateCallback(
      const std::string& key);
};

ExternalPolicyDataUpdater::FetchSuccessCallback
    MockFetchSuccessCallbackListener::CreateCallback(const std::string& key) {
  return base::BindRepeating(&MockFetchSuccessCallbackListener::OnFetchSuccess,
                             base::Unretained(this), key);
}

}  // namespace

class ExternalPolicyDataUpdaterTest : public testing::Test {
 protected:
  void SetUp() override;

  void CreateUpdater(size_t max_parallel_fetches);
  ExternalPolicyDataUpdater::Request CreateRequest(
      const std::string& url) const;
  void RequestExternalDataFetchAndWait(int index);
  void RequestExternalDataFetchAndWait(int key_index, int url_index);
  void RequestExternalDataFetch(int index);
  void RequestExternalDataFetch(int key_index, int url_index);
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  MockFetchSuccessCallbackListener callback_listener_;
  scoped_refptr<base::TestSimpleTaskRunner> backend_task_runner_;
  std::unique_ptr<ExternalPolicyDataUpdater> updater_;
};

void ExternalPolicyDataUpdaterTest::SetUp() {
  backend_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
}

void ExternalPolicyDataUpdaterTest::CreateUpdater(size_t max_parallel_fetches) {
  auto url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  updater_ = std::make_unique<ExternalPolicyDataUpdater>(
      backend_task_runner_,
      std::make_unique<ExternalPolicyDataFetcher>(std::move(url_loader_factory),
                                                  backend_task_runner_),
      max_parallel_fetches);
}

void ExternalPolicyDataUpdaterTest::RequestExternalDataFetchAndWait(int index) {
  RequestExternalDataFetchAndWait(index, index);
}

void ExternalPolicyDataUpdaterTest::RequestExternalDataFetchAndWait(
    int key_index,
    int url_index) {
  RequestExternalDataFetch(key_index, url_index);

  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
}

void ExternalPolicyDataUpdaterTest::RequestExternalDataFetch(int index) {
  RequestExternalDataFetch(index, index);
}

void ExternalPolicyDataUpdaterTest::RequestExternalDataFetch(int key_index,
                                                             int url_index) {
  updater_->FetchExternalData(
      kExternalPolicyDataKeys[key_index],
      CreateRequest(kExternalPolicyDataURLs[url_index]),
      callback_listener_.CreateCallback(kExternalPolicyDataKeys[key_index]));
}

ExternalPolicyDataUpdater::Request
    ExternalPolicyDataUpdaterTest::CreateRequest(const std::string& url) const {
  return ExternalPolicyDataUpdater::Request(
      url,
      crypto::SHA256HashString(kExternalPolicyDataPayload),
      kExternalPolicyDataMaxSize);
}

TEST_F(ExternalPolicyDataUpdaterTest, FetchSuccess) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the first fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[0],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the second fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, PayloadSizeExceedsLimit) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch with more data than allowed.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataOverflowPayload);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that first fetch is no longer running and the second fetch has been
  // started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that a retry has been scheduled for the first fetch.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());
}

TEST_F(ExternalPolicyDataUpdaterTest, FetchFailure) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the first fetch fail due to an interrupted connection.
  test_url_loader_factory_.AddResponse(
      GURL(kExternalPolicyDataURLs[0]), network::mojom::URLResponseHead::New(),
      std::string(),
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the first fetch is no longer running and the second fetch has
  // been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that a retry has been scheduled for the first fetch.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());
}

TEST_F(ExternalPolicyDataUpdaterTest, ServerFailure) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the first fetch fail with a server error.
  test_url_loader_factory_.AddResponse(
      GURL(kExternalPolicyDataURLs[0]), network::mojom::URLResponseHead::New(),
      std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_INTERNAL_SERVER_ERROR));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the first fetch is no longer running and the second fetch has
  // been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that a retry has been scheduled for the first fetch.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());
}

TEST_F(ExternalPolicyDataUpdaterTest, RetryLimit) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  // Verify that client failures cause the fetch to be retried three times.
  for (int i = 0; i < 3; ++i) {
    // Verify that the fetch has been (re)started.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());
    EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

    // Make the fetch fail with a client error.
    test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                         std::string(), net::HTTP_BAD_REQUEST);
    base::RunLoop().RunUntilIdle();
    backend_task_runner_->RunPendingTasks();
    test_url_loader_factory_.ClearResponses();

    // Verify that the fetch is no longer running.
    ASSERT_EQ(0, test_url_loader_factory_.NumPending());

    // Verify that a retry has been scheduled.
    EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

    // Fast-forward time to the scheduled retry.
    backend_task_runner_->RunPendingTasks();
    EXPECT_FALSE(backend_task_runner_->HasPendingTask());
  }

  // Verify that the fetch has been restarted.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail once more.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(), net::HTTP_BAD_REQUEST);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that no further retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, RetryWithBackoff) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  base::TimeDelta expected_delay = base::Seconds(15);
  const base::TimeDelta delay_cap = base::Hours(12);

  // The backoff delay is capped at 12 hours, which is reached after 12 retries:
  // 15 * 2^12 == 61440 > 43200 == 12 * 60 * 60
  for (int i = 0; i < 20; ++i) {
    // Verify that the fetch has been (re)started.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());
    EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

    // Make the fetch fail with a server error.
    test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                         std::string(),
                                         net::HTTP_INTERNAL_SERVER_ERROR);
    base::RunLoop().RunUntilIdle();
    backend_task_runner_->RunPendingTasks();
    test_url_loader_factory_.ClearResponses();

    // Verify that the fetch is no longer running.
    EXPECT_EQ(0, test_url_loader_factory_.NumPending());

    // Verify that a retry has been scheduled.
    EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

    // Verify that the retry delay has been doubled, with random jitter from 80%
    // to 100%.
    base::TimeDelta delay = backend_task_runner_->NextPendingTaskDelay();
    EXPECT_GT(delay,
              base::Milliseconds(0.799 * expected_delay.InMilliseconds()));
    EXPECT_LE(delay, expected_delay);

    if (i < 12) {
      // The delay cap has not been reached yet.
      EXPECT_LT(expected_delay, delay_cap);
      expected_delay *= 2;

      if (i == 11) {
        // The last doubling reached the cap.
        EXPECT_GT(expected_delay, delay_cap);
        expected_delay = delay_cap;
      }
    }

    // Fast-forward time to the scheduled retry.
    backend_task_runner_->RunPendingTasks();
    EXPECT_FALSE(backend_task_runner_->HasPendingTask());
  }
}

TEST_F(ExternalPolicyDataUpdaterTest, CancelDelayedTsaks) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  // Verify that the fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail with a server error.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(),
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  test_url_loader_factory_.ClearResponses();

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that a retry has been scheduled.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

  // Make second fetch request.
  RequestExternalDataFetch(0);

  // Verify that the fetch has been started and retry is still scheduled.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

  // Fast-forward time to the scheduled retry.
  backend_task_runner_->RunPendingTasks();

  // Verify that retry has been canceled and no additional requests started.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Complete the fetch successfully.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);
  EXPECT_CALL(callback_listener_, OnFetchSuccess(kExternalPolicyDataKeys[0],
                                                 kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that all tasks are completed.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(ExternalPolicyDataUpdaterTest, HashInvalid) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the first fetch retrieve data whose hash does not match the expected
  // value.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       "Invalid data");
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the first fetch is no longer running and the second fetch has
  // been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that a retry has been scheduled for the first fetch.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());
}

TEST_F(ExternalPolicyDataUpdaterTest, DataRejectedByCallback) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Reject the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[0],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(false));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that a retry has been scheduled.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

  // Fast-forward time to the scheduled retry.
  test_url_loader_factory_.ClearResponses();
  backend_task_runner_->RunPendingTasks();
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());

  // Verify that the fetch has been restarted.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked this time.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[0],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, URLChanged) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make another fetch request with the same key but an updated URL.
  RequestExternalDataFetchAndWait(0, 1);

  // Verify that the original fetch is no longer running and a new fetch has
  // been started with the updated URL.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, JobInvalidated) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that only the first fetch has been started.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make another fetch request with the same key as the second request but an
  // updated URL.
  RequestExternalDataFetchAndWait(1, 2);

  // Verify that the first fetch is still the only one running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the first fetch fail with a server error.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(),
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the second fetch was invalidated and the third fetch has been
  // started instead.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));
}

TEST_F(ExternalPolicyDataUpdaterTest, FetchCanceled) {
  // Create an updater that runs one fetch at a time.
  CreateUpdater(1);

  // Make a fetch request.
  RequestExternalDataFetchAndWait(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Cancel the fetch request.
  updater_->CancelExternalDataFetch(kExternalPolicyDataKeys[0]);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, ParallelJobs) {
  // Create an updater that runs up to two fetches in parallel.
  CreateUpdater(2);

  // Make three fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);
  RequestExternalDataFetchAndWait(2);

  // Verify that only the first and second fetches have been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the first fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[0],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the second fetch is still running and the third has started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[1],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that only the third fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the third fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[2],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[2],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the third fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, ParallelJobsFinishingOutOfOrder) {
  // Create an updater that runs up to two fetches in parallel.
  CreateUpdater(2);

  // Make three fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);
  RequestExternalDataFetchAndWait(2);

  // Verify that only the first and second fetches have been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[1],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the first fetch is still running and the third has started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the first fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[0],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that only the third fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the third fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[2],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[2],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the third fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, ParallelJobsWithRetry) {
  // Create an updater that runs up to two fetches in parallel.
  CreateUpdater(2);

  // Make three fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);
  RequestExternalDataFetchAndWait(2);

  // Verify that only the first and second fetches have been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Make the first fetch fail with a client error.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(), net::HTTP_BAD_REQUEST);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  test_url_loader_factory_.ClearResponses();

  // Verify that the first fetch is no longer running and the third fetch has
  // been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Verify that a retry has been scheduled for the first fetch.
  EXPECT_EQ(1u, backend_task_runner_->NumPendingTasks());

  // Fast-forward time to the scheduled retry.
  backend_task_runner_->RunPendingTasks();
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());

  // Verify that the first fetch has not been restarted yet.
  ASSERT_EQ(2, test_url_loader_factory_.NumPending());

  // Complete the third fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[2],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[2],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the second fetch is still running and the first fetch has been
  // restarted.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Verify that no further retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, ParallelJobsWithCancel) {
  // Create an updater that runs up to two fetches in parallel.
  CreateUpdater(2);

  // Make three fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);
  RequestExternalDataFetchAndWait(2);

  // Verify that only the first and second fetches have been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Cancel the fetch request.
  updater_->CancelExternalDataFetch(kExternalPolicyDataKeys[0]);
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();

  // Verify that the second fetch is still running and the third request has
  // been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[1],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that only the third fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));

  // Complete the third fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[2],
                                       kExternalPolicyDataPayload);

  // Accept the data when the callback is invoked.
  EXPECT_CALL(callback_listener_,
              OnFetchSuccess(kExternalPolicyDataKeys[2],
                             kExternalPolicyDataPayload))
      .Times(1)
      .WillOnce(Return(true));
  base::RunLoop().RunUntilIdle();
  backend_task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&callback_listener_);

  // Verify that the third fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that no retries have been scheduled.
  EXPECT_FALSE(backend_task_runner_->HasPendingTask());
}

TEST_F(ExternalPolicyDataUpdaterTest, ParallelJobsWithInvalidatedJob) {
  // Create an updater that runs up to two fetches in parallel.
  CreateUpdater(2);

  // Make two fetch requests.
  RequestExternalDataFetchAndWait(0);
  RequestExternalDataFetchAndWait(1);

  // Verify that the first and second fetches has been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Make another fetch request with the same key as the second request but an
  // updated URL.
  RequestExternalDataFetchAndWait(1, 2);

  // Verify that the first fetch is still running, the second has been canceled
  // and a third fetch has been started.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[2]));
}

}  // namespace policy
