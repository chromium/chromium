// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

using BoolCallback = base::OnceCallback<void(bool /* did_match_allowlist */)>;
using MockBoolCallback = testing::StrictMock<base::MockCallback<BoolCallback>>;

namespace {
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD1(CancelCheck, void(SafeBrowsingDatabaseManager::Client*));

  MOCK_METHOD2(CheckCsdAllowlistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}
};
}  // namespace

class AllowlistCheckerClientTest : public testing::Test {
 public:
  AllowlistCheckerClientTest() : target_url_("https://example.test") {}

  void SetUp() override {
    database_manager_ = new MockSafeBrowsingDatabaseManager;
  }

  void TearDown() override {
    database_manager_ = nullptr;
    base::RunLoop().RunUntilIdle();

    // Verify no callback is remaining.
    EXPECT_TRUE(task_environment_.MainThreadIsIdle());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  GURL target_url_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
};

TEST_F(AllowlistCheckerClientTest, TestCsdListMatch) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::MATCH));
  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(true /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, target_url_,
                                                 callback.Get());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSidePhishingDetection.AllowlistMatchResult",
      /*sample=*/AsyncMatch::MATCH, /*expected_bucket_count=*/1);
}

TEST_F(AllowlistCheckerClientTest, TestCsdListNoMatch) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::NO_MATCH));
  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, target_url_,
                                                 callback.Get());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSidePhishingDetection.AllowlistMatchResult",
      /*sample=*/AsyncMatch::NO_MATCH, /*expected_bucket_count=*/1);
}

TEST_F(AllowlistCheckerClientTest, TestCsdListAsyncNoMatch) {
  base::HistogramTester histogram_tester;
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, target_url_,
                                                 callback.Get());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSidePhishingDetection.AllowlistMatchResult",
      /*sample=*/AsyncMatch::ASYNC, /*expected_bucket_count=*/1);
  // Callback should not be called yet.

  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  // The self-owned client deletes itself here.
  client->OnCheckAllowlistUrlResult(false);
}

TEST_F(AllowlistCheckerClientTest, TestCsdListAsyncTimeout) {
  base::HistogramTester histogram_tester;
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));
  EXPECT_CALL(*database_manager_, CancelCheck(_)).Times(1);

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, target_url_,
                                                 callback.Get());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSidePhishingDetection.AllowlistMatchResult",
      /*sample=*/AsyncMatch::ASYNC, /*expected_bucket_count=*/1);
  task_environment_.FastForwardBy(base::Seconds(1));
  // No callback yet.

  EXPECT_CALL(callback, Run(true /* did_match_allowlist */));
  task_environment_.FastForwardBy(base::Seconds(5));
}

}  // namespace safe_browsing
