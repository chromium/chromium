// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/db/allowlist_checker_client.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using base::TimeDelta;
using testing::_;
using testing::Return;
using testing::SaveArg;

using BoolCallback = base::Callback<void(bool /* did_match_allowlist */)>;
using MockBoolCallback = testing::StrictMock<base::MockCallback<BoolCallback>>;

namespace {
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(CancelCheck, void(SafeBrowsingDatabaseManager::Client*));

  MOCK_METHOD2(CheckCsdWhitelistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

  MOCK_METHOD2(CheckUrlForHighConfidenceAllowlist,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};
}  // namespace

class AllowlistCheckerClientTest : public testing::Test {
 public:
  AllowlistCheckerClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        target_url_("https://example.test") {}

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
  content::BrowserTaskEnvironment task_environment_;

  GURL target_url_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
};

TEST_F(AllowlistCheckerClientTest, TestCsdListMatch) {
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::MATCH));
  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(true /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
}

TEST_F(AllowlistCheckerClientTest, TestCsdListNoMatch) {
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::NO_MATCH));
  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
}

TEST_F(AllowlistCheckerClientTest, TestCsdListAsyncNoMatch) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
  // Callback should not be called yet.

  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  // The self-owned client deletes itself here.
  client->OnCheckWhitelistUrlResult(false);
}

TEST_F(AllowlistCheckerClientTest, TestCsdListAsyncTimeout) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));
  EXPECT_CALL(*database_manager_, CancelCheck(_)).Times(1);

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  // No callback yet.

  EXPECT_CALL(callback, Run(true /* did_match_allowlist */));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
}

TEST_F(AllowlistCheckerClientTest, TestHighConfidenceListMatch) {
  EXPECT_CALL(*database_manager_,
              CheckUrlForHighConfidenceAllowlist(target_url_, _))
      .WillOnce(Return(AsyncMatch::MATCH));

  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(true /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
      database_manager_, target_url_, callback.Get());
}

TEST_F(AllowlistCheckerClientTest, TestHighConfidenceListNoMatch) {
  EXPECT_CALL(*database_manager_,
              CheckUrlForHighConfidenceAllowlist(target_url_, _))
      .WillOnce(Return(AsyncMatch::NO_MATCH));

  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
      database_manager_, target_url_, callback.Get());
}

TEST_F(AllowlistCheckerClientTest, TestHighConfidenceListAsyncNoMatch) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_,
              CheckUrlForHighConfidenceAllowlist(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
      database_manager_, target_url_, callback.Get());
  // Callback should not be called yet.

  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  // The self-owned client deletes itself here.
  client->OnCheckWhitelistUrlResult(false);
}

TEST_F(AllowlistCheckerClientTest, TestHighConfidenceListAsyncTimeout) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_,
              CheckUrlForHighConfidenceAllowlist(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));
  EXPECT_CALL(*database_manager_, CancelCheck(_)).Times(1);

  MockBoolCallback callback;
  AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
      database_manager_, target_url_, callback.Get());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  // No callback yet.

  EXPECT_CALL(callback, Run(false /* did_match_allowlist */));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
}

}  // namespace safe_browsing
