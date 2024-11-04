// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/messaging/messaging_backend_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/types/strong_alias.h"
#include "components/collaboration/internal/android/messaging/messaging_backend_service_bridge.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/messaging_native_j_unittests_jni_headers/MessagingBackendServiceBridgeUnitTestCompanion_jni.h"

using testing::_;
using testing::Eq;
using testing::Return;

namespace collaboration::messaging::android {
namespace {

MATCHER_P(ActivityLogQueryParamsEq, expected, "") {
  return arg.collaboration_id == expected.collaboration_id;
}

std::vector<PersistentMessage> GetDefaultPersistentMessages() {
  std::vector<PersistentMessage> messages;
  PersistentMessage message1;
  message1.collaboration_event = CollaborationEvent::TAB_ADDED;
  messages.emplace_back(message1);
  PersistentMessage message2;
  message2.collaboration_event = CollaborationEvent::TAB_UPDATED;
  messages.emplace_back(message2);
  PersistentMessage message3;
  message3.collaboration_event = CollaborationEvent::TAB_REMOVED;
  messages.emplace_back(message3);
  return messages;
}

base::android::ScopedJavaLocalRef<jintArray>
PersistentMessagesToCollaborationEventArray(
    JNIEnv* env,
    std::vector<collaboration::messaging::PersistentMessage> messages) {
  std::vector<int32_t> ints;
  for (const auto& message : messages) {
    ints.push_back(static_cast<int32_t>(message.collaboration_event));
  }
  return base::android::ToJavaIntArray(env, ints);
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
              (tab_groups::EitherTabID,
               std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForGroup,
              (tab_groups::EitherGroupID,
               std::optional<PersistentNotificationType>));
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

InstantMessage CreateInstantMessage() {
  InstantMessage message;
  message.level = InstantNotificationLevel::SYSTEM;
  message.type = InstantNotificationType::CONFLICT_TAB_REMOVED;
  message.collaboration_event = CollaborationEvent::TAB_REMOVED;

  // Attribution.
  message.attribution.collaboration_id = data_sharing::GroupId("my group");
  // GroupMember has its own conversion utils, so only check a single field.
  message.attribution.affected_user = data_sharing::GroupMember();
  message.attribution.affected_user->gaia_id = "affected";
  message.attribution.triggering_user = data_sharing::GroupMember();
  message.attribution.triggering_user->gaia_id = "triggering";

  // TabGroupMessageMetadata.
  message.attribution.tab_group_metadata = TabGroupMessageMetadata();
  message.attribution.tab_group_metadata->local_tab_group_id =
      tab_groups::LocalTabGroupID(
          base::Token(2748937106984275893, 588177993057108452));
  message.attribution.tab_group_metadata->sync_tab_group_id =
      base::Uuid::ParseLowercase("a1b2c3d4-e5f6-7890-1234-567890abcdef");
  message.attribution.tab_group_metadata->last_known_title =
      "last known group title";
  message.attribution.tab_group_metadata->last_known_color =
      tab_groups::TabGroupColorId::kOrange;

  // TabMessageMetadata.
  message.attribution.tab_metadata = TabMessageMetadata();
  message.attribution.tab_metadata->local_tab_id =
      std::make_optional(499897179);
  message.attribution.tab_metadata->sync_tab_id =
      base::Uuid::ParseLowercase("fedcba09-8765-4321-0987-6f5e4d3c2b1a");
  message.attribution.tab_metadata->last_known_url = "https://example.com/";
  message.attribution.tab_metadata->last_known_title = "last known tab title";

  return message;
}

TEST_F(MessagingBackendServiceBridgeTest, TestDisplayingInstantMessageSuccess) {
  // Set up the delegate for instant messages in Java.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_setInstantMessageDelegate(
      base::android::AttachCurrentThread(), j_companion());

  // Create and display an instant message.
  InstantMessage message = CreateInstantMessage();
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
  InstantMessage message = CreateInstantMessage();
  DisplayInstantaneousMessage(message, /*success=*/false);

  // Ensure that the message was received on the Java side with correct data.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyInstantMessage(
      base::android::AttachCurrentThread(), j_companion());

  // Verify that the callback has been invoked with the correct success value.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeInstantMessageSuccessCallback(
      base::android::AttachCurrentThread(), j_companion(), /*success=*/false);
  EXPECT_EQ(1U, success_callback_invocation_count());
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetMessages) {
  std::vector<PersistentMessage> messages = GetDefaultPersistentMessages();

  // First -- All types.
  // The call should arrive to the service with no type.
  std::optional<PersistentNotificationType> all_types = std::nullopt;
  EXPECT_CALL(service(), GetMessages(all_types)).WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesAndVerify(
      env, j_companion(),
      /*notification_type=*/-1,
      PersistentMessagesToCollaborationEventArray(env, messages));

  // Next up -- Specific type.
  // The call should arrive to the service with the correct type.
  EXPECT_CALL(service(),
              GetMessages(std::make_optional(PersistentNotificationType::CHIP)))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesAndVerify(
      env, j_companion(),
      /*notification_type=*/static_cast<jint>(PersistentNotificationType::CHIP),
      PersistentMessagesToCollaborationEventArray(env, messages));
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetMessagesForGroup_LocalID) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<PersistentMessage> messages = GetDefaultPersistentMessages();

  // First -- All types
  // Create a new random local tab group ID for this query.
  tab_groups::LocalTabGroupID local_tab_group_id = base::Token::CreateRandom();
  auto j_local_tab_group_id =
      tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, std::make_optional(local_tab_group_id));

  // The call should arrive to the service with no type.
  std::optional<PersistentNotificationType> all_types = std::nullopt;
  tab_groups::EitherGroupID group_id(local_tab_group_id);
  EXPECT_CALL(service(), GetMessagesForGroup(group_id, all_types))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForGroupAndVerify(
      env, j_companion(), j_local_tab_group_id, nullptr,
      /*notification_type=*/-1,
      PersistentMessagesToCollaborationEventArray(env, messages));

  // Next up -- Specific type.
  // The call should arrive to the service with the correct type.
  PersistentNotificationType notification_type =
      PersistentNotificationType::CHIP;
  EXPECT_CALL(service(), GetMessagesForGroup(
                             group_id, std::make_optional(notification_type)))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForGroupAndVerify(
      env, j_companion(), j_local_tab_group_id, nullptr,
      /*notification_type=*/static_cast<jint>(notification_type),
      PersistentMessagesToCollaborationEventArray(env, messages));
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetMessagesForGroup_SyncId) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<PersistentMessage> messages = GetDefaultPersistentMessages();

  // First -- All types
  // Create a random sync tab group ID for this query.
  base::Uuid sync_tab_group_id = base::Uuid::GenerateRandomV4();
  ScopedJavaLocalRef<jstring> j_sync_tab_group_id =
      base::android::ConvertUTF8ToJavaString(
          env, sync_tab_group_id.AsLowercaseString());

  // The call should arrive to the service with no type.
  std::optional<PersistentNotificationType> all_types = std::nullopt;
  tab_groups::EitherGroupID group_id(sync_tab_group_id);
  EXPECT_CALL(service(), GetMessagesForGroup(group_id, all_types))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForGroupAndVerify(
      env, j_companion(), nullptr, j_sync_tab_group_id,
      /*notification_type=*/-1,
      PersistentMessagesToCollaborationEventArray(env, messages));

  // Next up -- Specific type.
  // The call should arrive to the service with the correct type.
  PersistentNotificationType notification_type =
      PersistentNotificationType::CHIP;
  EXPECT_CALL(service(), GetMessagesForGroup(
                             group_id, std::make_optional(notification_type)))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForGroupAndVerify(
      env, j_companion(), nullptr, j_sync_tab_group_id,
      /*notification_type=*/static_cast<jint>(notification_type),
      PersistentMessagesToCollaborationEventArray(env, messages));
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetMessagesForTab_LocalID) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<PersistentMessage> messages = GetDefaultPersistentMessages();

  // First -- All types
  std::optional<tab_groups::LocalTabID> local_tab_id = std::make_optional(14);
  auto j_local_tab_id = tab_groups::ToJavaTabId(local_tab_id);

  // The call should arrive to the service with no type.
  std::optional<PersistentNotificationType> all_types = std::nullopt;
  tab_groups::EitherTabID tab_id(local_tab_id.value());
  EXPECT_CALL(service(), GetMessagesForTab(tab_id, all_types))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForTabAndVerify(
      env, j_companion(), j_local_tab_id, nullptr,
      /*notification_type=*/-1,
      PersistentMessagesToCollaborationEventArray(env, messages));

  // Next up -- Specific type.
  // The call should arrive to the service with the correct type.
  PersistentNotificationType notification_type =
      PersistentNotificationType::CHIP;
  EXPECT_CALL(service(),
              GetMessagesForTab(tab_id, std::make_optional(notification_type)))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForTabAndVerify(
      env, j_companion(), j_local_tab_id, nullptr,
      /*notification_type=*/static_cast<jint>(notification_type),
      PersistentMessagesToCollaborationEventArray(env, messages));
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetMessagesForTab_SyncID) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<PersistentMessage> messages = GetDefaultPersistentMessages();

  // First -- All types
  // Create a random sync tab ID for this query.
  base::Uuid sync_tab_id = base::Uuid::GenerateRandomV4();
  ScopedJavaLocalRef<jstring> j_sync_tab_id =
      base::android::ConvertUTF8ToJavaString(env,
                                             sync_tab_id.AsLowercaseString());

  // The call should arrive to the service with no type.
  std::optional<PersistentNotificationType> all_types = std::nullopt;
  tab_groups::EitherTabID tab_id(sync_tab_id);
  EXPECT_CALL(service(), GetMessagesForTab(tab_id, all_types))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForTabAndVerify(
      env, j_companion(), -1, j_sync_tab_id,
      /*notification_type=*/-1,
      PersistentMessagesToCollaborationEventArray(env, messages));

  // Next up -- Specific type.
  // The call should arrive to the service with the correct type.
  PersistentNotificationType notification_type =
      PersistentNotificationType::CHIP;
  EXPECT_CALL(service(),
              GetMessagesForTab(tab_id, std::make_optional(notification_type)))
      .WillOnce(Return(messages));

  // Verify that the order of messages stays the same.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_invokeGetMessagesForTabAndVerify(
      env, j_companion(), -1, j_sync_tab_id,
      /*notification_type=*/static_cast<jint>(notification_type),
      PersistentMessagesToCollaborationEventArray(env, messages));
}

TEST_F(MessagingBackendServiceBridgeTest, TestGetActivityLog) {
  // Create two activity log items.
  ActivityLogItem activity_log_item1;
  activity_log_item1.collaboration_event = CollaborationEvent::TAB_UPDATED;
  activity_log_item1.title_text = "title 1";
  activity_log_item1.description_text = "description 1";
  activity_log_item1.timestamp_text = "timestamp 1";

  ActivityLogItem activity_log_item2;
  activity_log_item2.collaboration_event =
      CollaborationEvent::COLLABORATION_MEMBER_ADDED;
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

}  // namespace collaboration::messaging::android
