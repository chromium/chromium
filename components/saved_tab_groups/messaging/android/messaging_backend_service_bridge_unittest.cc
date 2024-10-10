// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/android/messaging_backend_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/messaging/activity_log.h"
#include "components/saved_tab_groups/messaging/android/messaging_backend_service_bridge.h"
#include "components/saved_tab_groups/messaging/message.h"
#include "components/saved_tab_groups/messaging/messaging_backend_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/saved_tab_groups/messaging/android/native_j_unittests_jni_headers/MessagingBackendServiceBridgeUnitTestCompanion_jni.h"

using testing::_;
using testing::Eq;
using testing::Return;

namespace tab_groups::messaging::android {
namespace {

MATCHER_P(ActivityLogQueryParamsEq, expected, "") {
  return arg.collaboration_id == expected.collaboration_id;
}

}  // namespace

class MockMessagingBackendService : public MessagingBackendService {
 public:
  MockMessagingBackendService() = default;
  ~MockMessagingBackendService() override = default;

  // MessagingBackendService implementation.
  MOCK_METHOD(void, SetInstantMessageDelegate, (InstantMessageDelegate*));
  MOCK_METHOD(void, AddPersistentMessageObserver, (PersistentMessageObserver*));
  MOCK_METHOD(void,
              RemovePersistentMessageObserver,
              (PersistentMessageObserver*));
  MOCK_METHOD(bool, IsInitialized, ());
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForTab,
              (EitherTabID, std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForGroup,
              (EitherGroupID, std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessages,
              (std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<ActivityLogItem>,
              GetActivityLog,
              (const ActivityLogQueryParams&));
};

class MessagingBackendServiceBridgeTest : public testing::Test {
 public:
  MessagingBackendServiceBridgeTest() = default;
  ~MessagingBackendServiceBridgeTest() override = default;

  void SetUp() override {
    EXPECT_CALL(service(), AddPersistentMessageObserver(_)).Times(1);
    EXPECT_CALL(service(), SetInstantMessageDelegate(_)).Times(1);
    bridge_ = MessagingBackendServiceBridge::CreateForTest(&service_);

    j_service_ = bridge_->GetJavaObject();
    j_companion_ =
        Java_MessagingBackendServiceBridgeUnitTestCompanion_Constructor(
            base::android::AttachCurrentThread(), j_service_);
  }

  void TearDown() override {
    EXPECT_CALL(service_, SetInstantMessageDelegate(nullptr));
    EXPECT_CALL(service_, RemovePersistentMessageObserver(bridge()));
    success_callback_invocation_count_ = 0;
  }

  // API wrapper methods, since they are intentionaly private in the bridge.
  void OnMessagingBackendServiceInitialized() {
    bridge()->OnMessagingBackendServiceInitialized();
  }

  void DisplayPersistentMessage(PersistentMessage message) {
    bridge()->DisplayPersistentMessage(message);
  }

  void HidePersistentMessage(PersistentMessage message) {
    bridge()->HidePersistentMessage(message);
  }

  void DisplayInstantaneousMessage(InstantMessage message,
                                   bool expected_success_value) {
    bridge()->DisplayInstantaneousMessage(
        message,
        base::BindOnce(
            &MessagingBackendServiceBridgeTest::OnInstantMessageCallbackResult,
            base::Unretained(this), expected_success_value));
  }

  // Member accessors.
  MessagingBackendServiceBridge* bridge() { return bridge_.get(); }
  MockMessagingBackendService& service() { return service_; }
  base::android::ScopedJavaGlobalRef<jobject> j_service() { return j_service_; }
  base::android::ScopedJavaGlobalRef<jobject> j_companion() {
    return j_companion_;
  }
  uint64_t success_callback_invocation_count() {
    return success_callback_invocation_count_;
  }

 private:
  void OnInstantMessageCallbackResult(bool expected, bool actual) {
    EXPECT_EQ(expected, actual);
    ++success_callback_invocation_count_;
  }

  uint64_t success_callback_invocation_count_ = 0;
  MockMessagingBackendService service_;
  std::unique_ptr<MessagingBackendServiceBridge> bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_companion_;
};

TEST_F(MessagingBackendServiceBridgeTest, TestInitializationStatus) {
  EXPECT_CALL(service(), IsInitialized).WillOnce(Return(false));
  EXPECT_FALSE(
      Java_MessagingBackendServiceBridgeUnitTestCompanion_isInitialized(
          base::android::AttachCurrentThread(), j_companion()));

  EXPECT_CALL(service(), IsInitialized).WillOnce(Return(true));
  EXPECT_TRUE(Java_MessagingBackendServiceBridgeUnitTestCompanion_isInitialized(
      base::android::AttachCurrentThread(), j_companion()));
}

TEST_F(MessagingBackendServiceBridgeTest, TestPersistentMessageObservation) {
  // Add Java observer.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_addPersistentMessageObserver(
      base::android::AttachCurrentThread(), j_companion());

  // Verify Java observer is called on init.
  OnMessagingBackendServiceInitialized();
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyOnInitializedCalled(
      base::android::AttachCurrentThread(), j_companion(), 1);

  // Remove Java observer.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_removePersistentMessageObserver(
      base::android::AttachCurrentThread(), j_companion());

  // Verify Java observer is not called again (since it should be removed), so
  // the total call count should still be 1.
  OnMessagingBackendServiceInitialized();
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyOnInitializedCalled(
      base::android::AttachCurrentThread(), j_companion(), 1);
}

TEST_F(MessagingBackendServiceBridgeTest, TestDisplayingInstantMessageSuccess) {
  // Set up the delegate for instant messages in Java.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_setInstantMessageDelegate(
      base::android::AttachCurrentThread(), j_companion());

  // Create and display an instant message.
  InstantMessage message;
  message.level = InstantNotificationLevel::SYSTEM;
  message.type = InstantNotificationType::CONFLICT_TAB_REMOVED;
  message.action = UserAction::TAB_REMOVED;
  DisplayInstantaneousMessage(message, /*success=*/true);

  // Ensure that the message was received on the Java side with correct data.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyInstantMessage(
      base::android::AttachCurrentThread(), j_companion());

  // Verify that the callback has been invoked with the correct success value.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeInstantMessageSuccessCallback(
      base::android::AttachCurrentThread(), j_companion(), /*success=*/true);
  EXPECT_EQ(1U, success_callback_invocation_count());
}

TEST_F(MessagingBackendServiceBridgeTest, TestDisplayingInstantMessageFailure) {
  // Set up the delegate for instant messages in Java.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_setInstantMessageDelegate(
      base::android::AttachCurrentThread(), j_companion());

  // Create and display an instant message.
  InstantMessage message;
  message.level = InstantNotificationLevel::SYSTEM;
  message.type = InstantNotificationType::CONFLICT_TAB_REMOVED;
  message.action = UserAction::TAB_REMOVED;
  DisplayInstantaneousMessage(message, /*success=*/false);

  // Ensure that the message was received on the Java side with correct data.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyInstantMessage(
      base::android::AttachCurrentThread(), j_companion());

  // Verify that the callback has been invoked with the correct success value.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeInstantMessageSuccessCallback(
      base::android::AttachCurrentThread(), j_companion(), /*success=*/false);
  EXPECT_EQ(1U, success_callback_invocation_count());
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetActivityLog) {
  // Create two activity log items.
  ActivityLogItem activity_log_item1;
  activity_log_item1.user_action_type = UserAction::TAB_NAVIGATED;
  activity_log_item1.title_text = "title 1";
  activity_log_item1.description_text = "description 1";
  activity_log_item1.timestamp_text = "timestamp 1";

  ActivityLogItem activity_log_item2;
  activity_log_item2.user_action_type = UserAction::COLLABORATION_USER_JOINED;
  activity_log_item2.title_text = "title 2";
  activity_log_item2.description_text = "description 2";
  activity_log_item2.timestamp_text = "timestamp 2";

  std::vector<ActivityLogItem> activity_log_items;
  activity_log_items.emplace_back(activity_log_item1);
  activity_log_items.emplace_back(activity_log_item2);

  ActivityLogQueryParams params;
  params.collaboration_id = data_sharing::GroupId("collaboration1");
  EXPECT_CALL(service(), GetActivityLog(ActivityLogQueryParamsEq(params)))
      .WillRepeatedly(Return(activity_log_items));

  params.collaboration_id = data_sharing::GroupId("collaboration2");
  EXPECT_CALL(service(), GetActivityLog(ActivityLogQueryParamsEq(params)))
      .WillOnce(Return(std::vector<ActivityLogItem>()));

  // Invoke GetActivityLog from Java and verify.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetActivityLogAndVerify(
      base::android::AttachCurrentThread(), j_companion());
}

}  // namespace tab_groups::messaging::android
