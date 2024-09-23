// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_manager.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Property;

namespace autofill {
namespace {

const char kTestText[] = "abcd1234";

auto JsonHasText(std::string_view text) {
  return testing::ResultOf(
      [](const base::Value::Dict& dict) {
        const std::string* value = dict.FindString("value");
        return value ? *value : "";
      },
      Eq(text));
}

class MockLogReceiver : public autofill::LogReceiver {
 public:
  MockLogReceiver() = default;

  MockLogReceiver(const MockLogReceiver&) = delete;
  MockLogReceiver& operator=(const MockLogReceiver&) = delete;

  MOCK_METHOD(void, LogEntry, (const base::Value::Dict&), (override));
};

class MockNotifiedObject {
 public:
  MockNotifiedObject() = default;

  MockNotifiedObject(const MockNotifiedObject&) = delete;
  MockNotifiedObject& operator=(const MockNotifiedObject&) = delete;

  MOCK_METHOD(void, NotifyAboutLoggingActivity, (), ());
};

class LogManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    manager_ = LogManager::Create(
        &router_,
        base::BindRepeating(&MockNotifiedObject::NotifyAboutLoggingActivity,
                            base::Unretained(&notified_object_)));
    buffering_manager_ = LogManager::CreateBuffering();
  }

  void TearDown() override {
    manager_.reset();  // Destruct before LogRouter.
  }

 protected:
  testing::StrictMock<MockLogReceiver> receiver_;
  LogRouter router_;
  testing::StrictMock<MockNotifiedObject> notified_object_;
  std::unique_ptr<RoutingLogManager> manager_;
  std::unique_ptr<BufferingLogManager> buffering_manager_;
};

TEST_F(LogManagerTest, LogNoReceiver) {
  EXPECT_CALL(receiver_, LogEntry).Times(0);
  // Before attaching the receiver, no text should be passed.
  LOG_AF(*manager_) << kTestText;
  EXPECT_FALSE(manager_->IsLoggingActive());
}

TEST_F(LogManagerTest, LogAttachReceiver) {
  EXPECT_FALSE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.RegisterReceiver(&receiver_);
  EXPECT_TRUE(manager_->IsLoggingActive());
  // After attaching the logger, text should be passed.
  EXPECT_CALL(receiver_, LogEntry(JsonHasText(kTestText)));
  LOG_AF(*manager_) << kTestText;
  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.UnregisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());
}

TEST_F(LogManagerTest, LogDetachReceiver) {
  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.RegisterReceiver(&receiver_);
  EXPECT_TRUE(manager_->IsLoggingActive());
  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.UnregisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());

  // After detaching the logger, no text should be passed.
  EXPECT_CALL(receiver_, LogEntry).Times(0);
  LOG_AF(*manager_) << kTestText;
}

TEST_F(LogManagerTest, LogBufferingEntriesWhenFlushed) {
  LOG_AF(*buffering_manager_) << kTestText;
  LOG_AF(*buffering_manager_) << kTestText;
  EXPECT_FALSE(manager_->IsLoggingActive());
  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.RegisterReceiver(&receiver_);
  EXPECT_TRUE(manager_->IsLoggingActive());

  // After flushing the buffering log manager, text should be passed.
  EXPECT_CALL(receiver_, LogEntry(JsonHasText(kTestText))).Times(2);
  buffering_manager_->Flush(*manager_);

  // Flushing a second time has no effect because the buffer is now empty.
  EXPECT_CALL(receiver_, LogEntry(JsonHasText(kTestText))).Times(0);
  buffering_manager_->Flush(*manager_);

  // After logging to the buffering log manager and flushing it again, text
  // should be passed.
  LOG_AF(*buffering_manager_) << kTestText;
  LOG_AF(*buffering_manager_) << kTestText;
  EXPECT_CALL(receiver_, LogEntry(JsonHasText(kTestText))).Times(2);
  buffering_manager_->Flush(*manager_);

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.UnregisterReceiver(&receiver_);
}

TEST_F(LogManagerTest, NullCallbackWillNotCrash) {
  manager_ = LogManager::Create(&router_, base::NullCallback());
  router_.RegisterReceiver(&receiver_);
  router_.UnregisterReceiver(&receiver_);
}

TEST_F(LogManagerTest, SetSuspended_WithActiveLogging) {
  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.RegisterReceiver(&receiver_);
  EXPECT_TRUE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  manager_->SetSuspended(true);
  EXPECT_FALSE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  manager_->SetSuspended(false);
  EXPECT_TRUE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.UnregisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());
}

TEST_F(LogManagerTest, SetSuspended_WithInactiveLogging) {
  EXPECT_FALSE(manager_->IsLoggingActive());

  manager_->SetSuspended(true);
  EXPECT_FALSE(manager_->IsLoggingActive());

  manager_->SetSuspended(false);
  EXPECT_FALSE(manager_->IsLoggingActive());
}

TEST_F(LogManagerTest, InterleaveSuspendAndLoggingActivation_SuspendFirst) {
  manager_->SetSuspended(true);
  EXPECT_FALSE(manager_->IsLoggingActive());

  router_.RegisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  manager_->SetSuspended(false);
  EXPECT_TRUE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.UnregisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());
}

TEST_F(LogManagerTest, InterleaveSuspendAndLoggingActivation_ActiveFirst) {
  EXPECT_FALSE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  router_.RegisterReceiver(&receiver_);
  EXPECT_TRUE(manager_->IsLoggingActive());

  EXPECT_CALL(notified_object_, NotifyAboutLoggingActivity());
  manager_->SetSuspended(true);
  EXPECT_FALSE(manager_->IsLoggingActive());

  router_.UnregisterReceiver(&receiver_);
  EXPECT_FALSE(manager_->IsLoggingActive());

  manager_->SetSuspended(false);
  EXPECT_FALSE(manager_->IsLoggingActive());
}

}  // namespace
}  // namespace autofill
