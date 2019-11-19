// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char* kExternalPolicyDataURLs[] = {
    "http://localhost/data_1",
    "http://localhost/data_2"
};

const int64_t kExternalPolicyDataMaxSize = 20;

const char* kExternalPolicyDataPayload = "External policy data";
const char* kExternalPolicyDataOverflowPayload = "External policy data+++++++";

}  // namespace

class ExternalPolicyDataFetcherTest : public testing::Test {
 protected:
  ExternalPolicyDataFetcherTest();
  ~ExternalPolicyDataFetcherTest() override;

  // testing::Test:
  void SetUp() override;

  void StartJob(int index);
  void CancelJob(int index);

  void OnJobFinished(int job_index,
                     ExternalPolicyDataFetcher::Result result,
                     std::unique_ptr<std::string> data);
  int GetAndResetCallbackCount();

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> owner_task_runner_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ExternalPolicyDataFetcher> fetcher_;

  std::map<int, ExternalPolicyDataFetcher::Job*> jobs_;  // Not owned.

  int callback_count_;
  int callback_job_index_;
  ExternalPolicyDataFetcher::Result callback_result_;
  std::unique_ptr<std::string> callback_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalPolicyDataFetcherTest);
};

ExternalPolicyDataFetcherTest::ExternalPolicyDataFetcherTest()
    : callback_count_(0) {}

ExternalPolicyDataFetcherTest::~ExternalPolicyDataFetcherTest() {
}

void ExternalPolicyDataFetcherTest::SetUp() {
  auto url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  owner_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  fetcher_ = std::make_unique<ExternalPolicyDataFetcher>(
      std::move(url_loader_factory), owner_task_runner_);
}

void ExternalPolicyDataFetcherTest::StartJob(int index) {
  jobs_[index] = fetcher_->StartJob(
      GURL(kExternalPolicyDataURLs[index]),
      kExternalPolicyDataMaxSize,
      base::Bind(&ExternalPolicyDataFetcherTest::OnJobFinished,
                 base::Unretained(this), index));
  base::RunLoop().RunUntilIdle();
}

void ExternalPolicyDataFetcherTest::CancelJob(int index) {
  auto it = jobs_.find(index);
  ASSERT_TRUE(it != jobs_.end());
  ExternalPolicyDataFetcher::Job* job = it->second;
  jobs_.erase(it);
  fetcher_->CancelJob(job);
}

void ExternalPolicyDataFetcherTest::OnJobFinished(
    int job_index,
    ExternalPolicyDataFetcher::Result result,
    std::unique_ptr<std::string> data) {
  ++callback_count_;
  callback_job_index_ = job_index;
  callback_result_ = result;
  callback_data_ = std::move(data);
  jobs_.erase(job_index);
}

int ExternalPolicyDataFetcherTest::GetAndResetCallbackCount() {
  const int callback_count = callback_count_;
  callback_count_ = 0;
  return callback_count;
}

TEST_F(ExternalPolicyDataFetcherTest, Success) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, MaxSizeExceeded) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch with more data than allowed.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataOverflowPayload);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::MAX_SIZE_EXCEEDED, callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, ConnectionInterrupted) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail due to an interrupted connection.
  test_url_loader_factory_.AddResponse(
      GURL(kExternalPolicyDataURLs[0]), network::mojom::URLResponseHead::New(),
      std::string(),
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::CONNECTION_INTERRUPTED,
            callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, NetworkError) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail due to a network error.
  test_url_loader_factory_.AddResponse(
      GURL(kExternalPolicyDataURLs[0]), network::mojom::URLResponseHead::New(),
      std::string(),
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::NETWORK_ERROR, callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, ServerError) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail with a server error.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(),
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SERVER_ERROR, callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, ClientError) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail with a client error.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       std::string(), net::HTTP_BAD_REQUEST);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::CLIENT_ERROR, callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, HTTPError) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Make the fetch fail with an HTTP error.
  test_url_loader_factory_.AddResponse(
      kExternalPolicyDataURLs[0], std::string(), net::HTTP_MULTIPLE_CHOICES);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the correct error code.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::HTTP_ERROR, callback_result_);
  EXPECT_FALSE(callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, Canceled) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Cancel the fetch job.
  CancelJob(0);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is not invoked.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, GetAndResetCallbackCount());
}

TEST_F(ExternalPolicyDataFetcherTest, SuccessfulCanceled) {
  // Start a fetch job.
  StartJob(0);

  // Verify that the fetch has been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Verify that the fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Cancel the fetch job before the successful fetch result has arrived from
  // the job.
  CancelJob(0);

  // Verify that the callback is not invoked.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, GetAndResetCallbackCount());
}

TEST_F(ExternalPolicyDataFetcherTest, ParallelJobs) {
  // Start two fetch jobs.
  StartJob(0);
  StartJob(1);

  // Verify that the first and second fetches have been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the first fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Verify that the first fetch is no longer running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);

  // Verify that the second fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Verify that the second fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(1, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, ParallelJobsFinishingOutOfOrder) {
  // Start two fetch jobs.
  StartJob(0);
  StartJob(1);

  // Verify that the first and second fetches have been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Verify that the second fetch is no longer running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(1, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);

  // Verify that the first fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));

  // Complete the first fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[0],
                                       kExternalPolicyDataPayload);

  // Verify that the first fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(0, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);
}

TEST_F(ExternalPolicyDataFetcherTest, ParallelJobsWithCancel) {
  // Start two fetch jobs.
  StartJob(0);
  StartJob(1);

  // Verify that the first and second fetches have been started.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[0]));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Cancel the first fetch job.
  CancelJob(0);

  // Verify that the first fetch is no longer running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Verify that the callback is not invoked.
  EXPECT_EQ(0, GetAndResetCallbackCount());

  // Verify that the second fetch is still running.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kExternalPolicyDataURLs[1]));

  // Complete the second fetch.
  test_url_loader_factory_.AddResponse(kExternalPolicyDataURLs[1],
                                       kExternalPolicyDataPayload);

  // Verify that the second fetch is no longer running.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Verify that the callback is invoked with the retrieved data.
  owner_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, GetAndResetCallbackCount());
  EXPECT_EQ(1, callback_job_index_);
  EXPECT_EQ(ExternalPolicyDataFetcher::SUCCESS, callback_result_);
  ASSERT_TRUE(callback_data_);
  EXPECT_EQ(kExternalPolicyDataPayload, *callback_data_);
}

}  // namespace policy
