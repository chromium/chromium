// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_router.h"

#include "base/macros.h"
#include "base/values.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public LogReceiver {
 public:
  MockLogReceiver() = default;

  MOCK_METHOD1(LogEntry, void(const base::Value&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLogReceiver);
};

class MockLogManager : public StubLogManager {
 public:
  MockLogManager() = default;

  MOCK_METHOD1(OnLogRouterAvailabilityChanged, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLogManager);
};

}  // namespace

class LogRouterTest : public testing::Test {
 protected:
  testing::StrictMock<MockLogReceiver> receiver_;
  testing::StrictMock<MockLogReceiver> receiver2_;
  testing::StrictMock<MockLogManager> manager_;
};

TEST_F(LogRouterTest, ProcessLog_NoReceiver) {
  LogRouter router;
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  base::Value log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver_);
  // Without receivers, accumulated logs should not have been kept. That means
  // that on the registration of the first receiver, none are returned.
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_OneReceiver) {
  LogRouter router;
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  // Check that logs generated after activation are passed.
  base::Value log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversAccumulatedLogsPassed) {
  LogRouter router;
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));

  // Log something with only the first receiver, to accumulate some logs.
  base::Value log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  EXPECT_CALL(receiver2_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(0);
  router.ProcessLog(kTestText);
  // Accumulated logs get passed on registration.
  std::vector<base::Value> expected_logs;
  expected_logs.emplace_back(log_entry.Clone());
  EXPECT_EQ(expected_logs, router.RegisterReceiver(&receiver2_));
  router.UnregisterReceiver(&receiver_);
  router.UnregisterReceiver(&receiver2_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversBothUpdated) {
  LogRouter router;
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver2_));

  // Check that both receivers get log updates.
  base::Value log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  EXPECT_CALL(receiver2_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver2_);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversNoUpdateAfterUnregistering) {
  LogRouter router;
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver2_));

  // Check that no logs are passed to an unregistered receiver.
  router.UnregisterReceiver(&receiver_);
  EXPECT_CALL(receiver_, LogEntry(_)).Times(0);
  base::Value log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver2_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver2_);
}

TEST_F(LogRouterTest, RegisterManager_NoReceivers) {
  LogRouter router;
  EXPECT_FALSE(router.RegisterManager(&manager_));
  router.UnregisterManager(&manager_);
}

TEST_F(LogRouterTest, RegisterManager_OneReceiverBeforeManager) {
  LogRouter router;
  // First register a receiver.
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  // The manager should be told the LogRouter has some receivers.
  EXPECT_TRUE(router.RegisterManager(&manager_));
  // Now unregister the receiver. The manager should be told the LogRouter has
  // no receivers.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(false));
  router.UnregisterReceiver(&receiver_);
  router.UnregisterManager(&manager_);
}

TEST_F(LogRouterTest, RegisterManager_OneManagerBeforeReceiver) {
  LogRouter router;
  // First register a manager; the manager should be told the LogRouter has no
  // receivers.
  EXPECT_FALSE(router.RegisterManager(&manager_));
  // Now register the receiver. The manager should be notified.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(true));
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  // Now unregister the manager.
  router.UnregisterManager(&manager_);
  // Now unregister the receiver. The manager should not hear about it.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(_)).Times(0);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, RegisterManager_OneManagerTwoReceivers) {
  LogRouter router;
  // First register a manager; the manager should be told the LogRouter has no
  // receivers.
  EXPECT_FALSE(router.RegisterManager(&manager_));
  // Now register the 1st receiver. The manager should be notified.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(true));
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver_));
  // Now register the 2nd receiver. The manager should not be notified.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(true)).Times(0);
  EXPECT_EQ(std::vector<base::Value>(), router.RegisterReceiver(&receiver2_));
  // Now unregister the 1st receiver. The manager should not hear about it.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(false)).Times(0);
  router.UnregisterReceiver(&receiver_);
  // Now unregister the 2nd receiver. The manager should hear about it.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(false));
  router.UnregisterReceiver(&receiver2_);
  // Now unregister the manager.
  router.UnregisterManager(&manager_);
}

}  // namespace autofill
