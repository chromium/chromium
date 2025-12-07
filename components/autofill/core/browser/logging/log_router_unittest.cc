// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_router.h"

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

  MockLogReceiver(const MockLogReceiver&) = delete;
  MockLogReceiver& operator=(const MockLogReceiver&) = delete;

  MOCK_METHOD(void, LogEntry, (const base::Value::Dict&), (override));
};

class MockLogManager : public StubLogManager {
 public:
  MockLogManager() = default;

  MockLogManager(const MockLogManager&) = delete;
  MockLogManager& operator=(const MockLogManager&) = delete;

  MOCK_METHOD(void, OnLogRouterAvailabilityChanged, (bool), (override));
};

class LogRouterTest : public testing::Test {
 protected:
  testing::StrictMock<MockLogReceiver> receiver_;
  testing::StrictMock<MockLogReceiver> receiver2_;
  testing::StrictMock<MockLogManager> manager_;
};

TEST_F(LogRouterTest, ProcessLog_OneReceiver) {
  LogRouter router;
  router.RegisterReceiver(&receiver_);
  base::Value::Dict log_entry = LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver_, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversBothUpdated) {
  LogRouter router;
  router.RegisterReceiver(&receiver_);
  router.RegisterReceiver(&receiver2_);

  // Check that both receivers get log updates.
  base::Value::Dict log_entry = LogRouter::CreateEntryForText(kTestText);
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
  router.RegisterReceiver(&receiver_);
  router.RegisterReceiver(&receiver2_);

  // Check that no logs are passed to an unregistered receiver.
  router.UnregisterReceiver(&receiver_);
  EXPECT_CALL(receiver_, LogEntry(_)).Times(0);
  base::Value::Dict log_entry = LogRouter::CreateEntryForText(kTestText);
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
  router.RegisterReceiver(&receiver_);
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
  router.RegisterReceiver(&receiver_);
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
  router.RegisterReceiver(&receiver_);
  // Now register the 2nd receiver. The manager should not be notified.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(true)).Times(0);
  router.RegisterReceiver(&receiver2_);
  // Now unregister the 1st receiver. The manager should not hear about it.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(false)).Times(0);
  router.UnregisterReceiver(&receiver_);
  // Now unregister the 2nd receiver. The manager should hear about it.
  EXPECT_CALL(manager_, OnLogRouterAvailabilityChanged(false));
  router.UnregisterReceiver(&receiver2_);
  // Now unregister the manager.
  router.UnregisterManager(&manager_);
}

}  // namespace
}  // namespace autofill
