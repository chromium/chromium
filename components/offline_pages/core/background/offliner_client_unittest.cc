// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/offliner_client.h"

#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using ::testing::_;

using MockCompleteCallback =
    base::MockCallback<base::RepeatingCallback<void(const SavePageRequest&,
                                                    Offliner::RequestStatus)>>;

using MockProgressCallback = base::MockCallback<
    base::RepeatingCallback<void(const SavePageRequest&, int64_t)>>;

const base::TimeDelta kOneMinute = base::TimeDelta::FromMinutes(1);

SavePageRequest TestRequest() {
  return SavePageRequest(123, GURL("http://test.com"),
                         ClientId("test-namespace", "test-id"), base::Time(),
                         /*user_requested=*/true);
}

class OfflinerClientTest : public testing::Test {
 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_{
      new base::TestMockTimeTaskRunner};
  base::ThreadTaskRunnerHandle task_runner_handle_{task_runner_};

  MockProgressCallback progress_callback_;
  OfflinerStub* offliner_ = new OfflinerStub;
  OfflinerClient client_{std::unique_ptr<OfflinerStub>(offliner_),
                         progress_callback_.Get()};
};

TEST_F(OfflinerClientTest, NoRequests) {
  EXPECT_TRUE(client_.Ready());
  EXPECT_FALSE(client_.Active());
  EXPECT_EQ(nullptr, client_.ActiveRequest());
}

// Call Stop() when the offliner is not active. It should do nothing.
TEST_F(OfflinerClientTest, StopWithNoRequest) {
  client_.Stop(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED);

  EXPECT_FALSE(client_.Active());
}

TEST_F(OfflinerClientTest, LoadAndSave) {
  offliner_->enable_callback(true);

  // Load a page. Completion callback and progress callback should be called.
  MockCompleteCallback complete_callback;
  EXPECT_CALL(complete_callback, Run(_, Offliner::RequestStatus::SAVED));
  EXPECT_CALL(progress_callback_, Run(_, _));
  ASSERT_TRUE(
      client_.LoadAndSave(TestRequest(), kOneMinute, complete_callback.Get()));
  EXPECT_TRUE(client_.Active());

  task_runner_->RunUntilIdle();
  EXPECT_FALSE(client_.Active());
}

TEST_F(OfflinerClientTest, LoadAndSaveTimeout) {
  MockCompleteCallback complete_callback;
  EXPECT_CALL(complete_callback,
              Run(_, Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT));
  ASSERT_TRUE(
      client_.LoadAndSave(TestRequest(), kOneMinute, complete_callback.Get()));
  EXPECT_TRUE(client_.Active());

  task_runner_->FastForwardBy(kOneMinute);
  EXPECT_FALSE(client_.Active());
}

TEST_F(OfflinerClientTest, StopInFlight) {
  MockCompleteCallback complete_callback;
  // Simulate loading in progress.
  ASSERT_TRUE(
      client_.LoadAndSave(TestRequest(), kOneMinute, complete_callback.Get()));
  task_runner_->FastForwardBy(kOneMinute / 2);

  // Call Stop(), and verify the corrrect status is returned.
  EXPECT_CALL(complete_callback,
              Run(_, Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED));
  client_.Stop(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED);
  EXPECT_TRUE(client_.Active());  // Cancellation isn't yet complete.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(client_.Active());
}

TEST_F(OfflinerClientTest, LoadAndSaveFailsWhileAlreadyActive) {
  MockCompleteCallback complete_callback1;
  ASSERT_TRUE(
      client_.LoadAndSave(TestRequest(), kOneMinute, complete_callback1.Get()));
  EXPECT_CALL(complete_callback1, Run(_, _)).Times(1);

  MockCompleteCallback complete_callback2;
  EXPECT_CALL(complete_callback2, Run(_, _)).Times(0);
  EXPECT_FALSE(
      client_.LoadAndSave(TestRequest(), kOneMinute, complete_callback2.Get()));

  task_runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace offline_pages
