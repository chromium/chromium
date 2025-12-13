// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"

#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/collaboration/test_support/mock_messaging_backend_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SaveArg;

namespace collaboration::messaging {

class MockInstantMessageDelegate
    : public MessagingBackendService::InstantMessageDelegate {
 public:
  MOCK_METHOD(void,
              DisplayInstantaneousMessage,
              (InstantMessage message, SuccessCallback success_callback),
              (override));
  MOCK_METHOD(void,
              HideInstantaneousMessage,
              (const std::set<base::Uuid>& message_ids),
              (override));
};

class InstantMessageProcessorImplTest : public testing::Test {
 public:
  InstantMessageProcessorImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~InstantMessageProcessorImplTest() override = default;

  void SetUp() override {
    processor_ = std::make_unique<InstantMessageProcessorImpl>();
    processor_->SetMessagingBackendService(&messaging_backend_service_);
  }

  void TearDown() override {}

  void SetupInstantMessageDelegate() {
    EXPECT_FALSE(processor_->IsEnabled());
    processor_->SetInstantMessageDelegate(&mock_instant_message_delegate_);
    EXPECT_TRUE(processor_->IsEnabled());
  }

  MessageAttribution CreateAttribution1() {
    MessageAttribution attribution;
    attribution.id = msg_id1_;
    attribution.collaboration_id = data_sharing::GroupId("foo group");
    // GroupMember has its own conversion utils, so only check a single field.
    attribution.affected_user = data_sharing::GroupMember();
    attribution.affected_user->gaia_id = GaiaId("affected");
    attribution.triggering_user = data_sharing::GroupMember();
    attribution.triggering_user->gaia_id = GaiaId("triggering");

    // TabGroupMessageMetadata.
    attribution.tab_group_metadata = TabGroupMessageMetadata();
    attribution.tab_group_metadata->local_tab_group_id =
        tab_groups::test::GenerateRandomTabGroupID();
    attribution.tab_group_metadata->sync_tab_group_id =
        base::Uuid::ParseLowercase("a1b2c3d4-e5f6-7890-1234-567890abcdef");
    attribution.tab_group_metadata->last_known_title = "My Awesome Group";
    attribution.tab_group_metadata->last_known_color =
        tab_groups::TabGroupColorId::kOrange;

    // TabMessageMetadata.
    attribution.tab_metadata = TabMessageMetadata();
    attribution.tab_metadata->local_tab_id = std::make_optional(499897179);
    attribution.tab_metadata->sync_tab_id =
        base::Uuid::ParseLowercase("fedcba09-8765-4321-0987-6f5e4d3c2b1a");
    attribution.tab_metadata->last_known_url = "https://example.com/";
    attribution.tab_metadata->last_known_title = "last known tab title";

    return attribution;
  }

  InstantMessage CreateInstantMessage() {
    InstantMessage message;
    message.level = InstantNotificationLevel::SYSTEM;
    message.type = InstantNotificationType::CONFLICT_TAB_REMOVED;
    message.collaboration_event = CollaborationEvent::TAB_REMOVED;

    message.attributions.emplace_back(CreateAttribution1());
    return message;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockInstantMessageDelegate mock_instant_message_delegate_;
  MockMessagingBackendService messaging_backend_service_;
  std::unique_ptr<InstantMessageProcessorImpl> processor_;
  base::Uuid msg_id1_ =
      base::Uuid::ParseLowercase("cf07d904-88d4-4bc9-989d-57a9ab9e17a7");
  base::Uuid msg_id2_ =
      base::Uuid::ParseLowercase("fedcba09-8765-4321-0987-6f5e4d3c2b1a");
  base::Uuid msg_id3_ =
      base::Uuid::ParseLowercase("1b687a61-8a17-4f98-bf9d-74d2b50abf3e");
};

TEST_F(InstantMessageProcessorImplTest, SingleMemberAddedMessage) {
  SetupInstantMessageDelegate();
  InstantMessage message = CreateInstantMessage();
  message.attributions[0].id = msg_id1_;
  message.collaboration_event = CollaborationEvent::COLLABORATION_MEMBER_ADDED;
  message.attributions[0].affected_user = data_sharing::GroupMember();
  message.attributions[0].affected_user->gaia_id = GaiaId("affected1");
  message.localized_message = u"Random message title";

  // Save the last invocation of calls to the InstantMessageDelegate.
  // Dispatch instant message.
  InstantMessage actual_message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&actual_message), MoveArg<1>(&success_callback)));
  processor_->DisplayInstantMessage(message);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify instant message.
  EXPECT_TRUE(IsSingleMessage(actual_message));
  EXPECT_EQ(message.localized_message, actual_message.localized_message);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            actual_message.collaboration_event);
  EXPECT_EQ(message.level, actual_message.level);
  EXPECT_EQ(message.type, actual_message.type);
  EXPECT_EQ(msg_id1_, actual_message.attributions[0].id);

  // Invoke callback and verify that backend clears out the message from DB.
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id1_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  std::move(success_callback).Run(true);
}

TEST_F(InstantMessageProcessorImplTest, AggregateMemberAddedMessage) {
  SetupInstantMessageDelegate();

  // Create 3 messages of COLLABORATION_MEMBER_ADDED type.
  InstantMessage message1 = CreateInstantMessage();
  message1.collaboration_event = CollaborationEvent::COLLABORATION_MEMBER_ADDED;
  message1.attributions[0].id = msg_id1_;

  InstantMessage message2 = message1;
  message2.attributions[0].affected_user = data_sharing::GroupMember();
  message2.attributions[0].affected_user->gaia_id = GaiaId("affected2");
  message2.attributions[0].id = msg_id2_;

  InstantMessage actual_message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&actual_message), MoveArg<1>(&success_callback)));
  processor_->DisplayInstantMessage(message1);
  processor_->DisplayInstantMessage(message2);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify the aggregated instant message.
  EXPECT_FALSE(IsSingleMessage(actual_message));
  EXPECT_EQ(u"2 members joined \"My Awesome Group\" tab group",
            actual_message.localized_message);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            actual_message.collaboration_event);
  EXPECT_EQ(message1.level, actual_message.level);
  EXPECT_EQ(message1.type, actual_message.type);
  EXPECT_EQ(msg_id1_, actual_message.attributions[0].id);
  EXPECT_EQ(msg_id2_, actual_message.attributions[1].id);

  // Invoke callback and verify that backend clears out the individual messages
  // from DB.
  testing::InSequence sequence;
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id1_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id2_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  std::move(success_callback).Run(true);
}

TEST_F(InstantMessageProcessorImplTest, AggregateTabGroupRemovedMessage) {
  SetupInstantMessageDelegate();

  // Create 3 messages of TAB_GROUP_REMOVED type.
  InstantMessage message1 = CreateInstantMessage();
  message1.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
  message1.attributions[0].id = msg_id1_;
  InstantMessage message2 = message1;
  message2.attributions[0].id = msg_id2_;
  InstantMessage message3 = message1;
  message3.attributions[0].id = msg_id3_;

  InstantMessage actual_message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&actual_message), MoveArg<1>(&success_callback)));
  processor_->DisplayInstantMessage(message1);
  processor_->DisplayInstantMessage(message2);
  processor_->DisplayInstantMessage(message3);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify the aggregated instant message.
  EXPECT_FALSE(IsSingleMessage(actual_message));
  EXPECT_EQ(u"3 tab groups no longer available",
            actual_message.localized_message);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED,
            actual_message.collaboration_event);
  EXPECT_EQ(message1.level, actual_message.level);
  EXPECT_EQ(message1.type, actual_message.type);
  EXPECT_EQ(msg_id1_, actual_message.attributions[0].id);
  EXPECT_EQ(msg_id2_, actual_message.attributions[1].id);
  EXPECT_EQ(msg_id3_, actual_message.attributions[2].id);

  // Invoke callback and verify that backend clears out the individual messages
  // from DB.
  testing::InSequence sequence;
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id1_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id2_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id3_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  std::move(success_callback).Run(true);
}

TEST_F(InstantMessageProcessorImplTest, ShouldNotAggregateTabRemovedMessage) {
  SetupInstantMessageDelegate();

  // Create 3 messages of TAB_REMOVED type.
  InstantMessage message1 = CreateInstantMessage();
  message1.collaboration_event = CollaborationEvent::TAB_REMOVED;
  message1.attributions[0].id = msg_id1_;
  message1.localized_message = u"Message text 1";

  InstantMessage message2 = message1;
  message2.attributions[0].id = msg_id2_;
  message2.localized_message = u"Message text 2";

  std::vector<InstantMessage> actual_messages;
  std::vector<MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      success_callbacks;

  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(
          [&](InstantMessage message,
              MessagingBackendService::InstantMessageDelegate::SuccessCallback
                  success_callback) {
            actual_messages.emplace_back(message);
            success_callbacks.emplace_back(std::move(success_callback));
          });

  processor_->DisplayInstantMessage(message1);
  processor_->DisplayInstantMessage(message2);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify the individual instant messages.
  EXPECT_EQ(2u, actual_messages.size());
  EXPECT_TRUE(IsSingleMessage(actual_messages[0]));
  EXPECT_EQ(message1.collaboration_event,
            actual_messages[0].collaboration_event);
  EXPECT_EQ(message1.localized_message, actual_messages[0].localized_message);
  EXPECT_EQ(message1.level, actual_messages[0].level);
  EXPECT_EQ(message1.type, actual_messages[0].type);
  EXPECT_EQ(msg_id1_, actual_messages[0].attributions[0].id);

  EXPECT_TRUE(IsSingleMessage(actual_messages[1]));
  EXPECT_EQ(message2.collaboration_event,
            actual_messages[1].collaboration_event);
  EXPECT_EQ(message2.localized_message, actual_messages[1].localized_message);
  EXPECT_EQ(msg_id2_, actual_messages[1].attributions[0].id);

  // Invoke callback and verify that backend clears out the individual messages
  // from DB.
  testing::InSequence sequence;
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id1_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(msg_id2_),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  std::move(success_callbacks[0]).Run(true);
  std::move(success_callbacks[1]).Run(true);
}

TEST_F(InstantMessageProcessorImplTest,
       TwoMessagesOfSimilarTypeWithOneOfDifferentType) {
  SetupInstantMessageDelegate();

  // Create 2 messages of TAB_GROUP_REMOVED type and the third one of
  // COLLOABORATION_MEMBER_ADDED type.
  InstantMessage message1 = CreateInstantMessage();
  message1.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
  message1.attributions[0].id = msg_id1_;
  InstantMessage message2 = message1;
  message2.attributions[0].id = msg_id2_;

  InstantMessage message3 = message1;
  message3.attributions[0].id = msg_id3_;
  message3.localized_message = u"Some message";
  message3.collaboration_event = CollaborationEvent::COLLABORATION_MEMBER_ADDED;

  // Save arguments of invocation.
  std::vector<InstantMessage> actual_messages;
  std::vector<MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      success_callbacks;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(
          [&](InstantMessage message,
              MessagingBackendService::InstantMessageDelegate::SuccessCallback
                  success_callback) {
            actual_messages.emplace_back(message);
            success_callbacks.emplace_back(std::move(success_callback));
          });

  processor_->DisplayInstantMessage(message1);
  processor_->DisplayInstantMessage(message2);
  processor_->DisplayInstantMessage(message3);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify the aggregated instant message.
  EXPECT_EQ(2u, actual_messages.size());
  EXPECT_FALSE(IsSingleMessage(actual_messages[0]));
  EXPECT_EQ(u"2 tab groups no longer available",
            actual_messages[0].localized_message);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED,
            actual_messages[0].collaboration_event);
  EXPECT_EQ(message1.level, actual_messages[0].level);
  EXPECT_EQ(message1.type, actual_messages[0].type);
  EXPECT_EQ(msg_id1_, actual_messages[0].attributions[0].id);
  EXPECT_EQ(msg_id2_, actual_messages[0].attributions[1].id);

  // Verify the single non-aggregated message.
  EXPECT_TRUE(IsSingleMessage(actual_messages[1]));
  EXPECT_EQ(message3.collaboration_event,
            actual_messages[1].collaboration_event);
  EXPECT_EQ(message3.localized_message, actual_messages[1].localized_message);
  EXPECT_EQ(msg_id3_, actual_messages[1].attributions[0].id);
}

TEST_F(InstantMessageProcessorImplTest,
       DontAggregateMessageswithDifferentInstantNotificationLevel) {
  SetupInstantMessageDelegate();

  // Create 2 messages of TAB_GROUP_REMOVED type, but with different levels.
  InstantMessage message1 = CreateInstantMessage();
  message1.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
  message1.attributions[0].id = msg_id1_;
  message1.level = InstantNotificationLevel::SYSTEM;

  InstantMessage message2 = message1;
  message2.attributions[0].id = msg_id2_;
  message2.level = InstantNotificationLevel::BROWSER;

  std::vector<InstantMessage> actual_messages;
  std::vector<MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      success_callbacks;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(
          [&](InstantMessage message,
              MessagingBackendService::InstantMessageDelegate::SuccessCallback
                  success_callback) {
            actual_messages.emplace_back(message);
            success_callbacks.emplace_back(std::move(success_callback));
          });

  processor_->DisplayInstantMessage(message1);
  processor_->DisplayInstantMessage(message2);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify that the messages are not aggregated.
  EXPECT_EQ(2u, actual_messages.size());
  EXPECT_EQ(msg_id1_, actual_messages[1].attributions[0].id);
  EXPECT_EQ(msg_id2_, actual_messages[0].attributions[0].id);
  EXPECT_EQ(message1.level, actual_messages[1].level);
  EXPECT_EQ(message2.level, actual_messages[0].level);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED,
            actual_messages[1].collaboration_event);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED,
            actual_messages[0].collaboration_event);
}

// Test for HideInstantMessage when the message is in the queue.
TEST_F(InstantMessageProcessorImplTest, HideInstantMessage_MessageInQueue) {
  SetupInstantMessageDelegate();
  InstantMessage message_to_hide = CreateInstantMessage();
  message_to_hide.attributions[0].id = msg_id1_;
  // Use a non-aggregatable type to simplify verification.
  message_to_hide.collaboration_event = CollaborationEvent::TAB_REMOVED;
  message_to_hide.localized_message = u"Message to be hidden";

  InstantMessage message_to_keep = CreateInstantMessage();
  message_to_keep.attributions[0].id = msg_id2_;
  message_to_keep.collaboration_event = CollaborationEvent::TAB_REMOVED;
  message_to_keep.localized_message = u"Message to be kept";

  // Add both messages to the queue.
  processor_->DisplayInstantMessage(message_to_hide);
  processor_->DisplayInstantMessage(message_to_keep);

  std::set<base::Uuid> ids_to_hide = {msg_id1_};

  // Expect the delegate to be asked to hide the first message.
  EXPECT_CALL(mock_instant_message_delegate_,
              HideInstantaneousMessage(Eq(ids_to_hide)))
      .Times(1);

  // Expect DisplayInstantaneousMessage to be called only for the message
  // that was not hidden.
  InstantMessage displayed_message;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .WillOnce(SaveArg<0>(&displayed_message));

  // Hide the first message. This should happen before ProcessQueue runs.
  processor_->HideInstantMessage(ids_to_hide);

  // Fast forward time to allow ProcessQueue to run.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify that the correct message was displayed.
  ASSERT_FALSE(displayed_message.attributions.empty());
  EXPECT_EQ(msg_id2_, displayed_message.attributions[0].id);
  EXPECT_EQ(message_to_keep.localized_message,
            displayed_message.localized_message);
}

// Test for HideInstantMessage when the message is NOT in the queue.
TEST_F(InstantMessageProcessorImplTest, HideInstantMessage_NoMessageInQueue) {
  SetupInstantMessageDelegate();

  InstantMessage message_in_queue = CreateInstantMessage();
  message_in_queue.attributions[0].id = msg_id2_;
  message_in_queue.collaboration_event = CollaborationEvent::TAB_REMOVED;
  message_in_queue.localized_message = u"Message in queue";

  // Add one message to the queue.
  processor_->DisplayInstantMessage(message_in_queue);

  // Attempt to hide a different message (msg_id1_) that is not in the queue.
  std::set<base::Uuid> ids_to_hide = {msg_id1_};

  // Expect the delegate to be asked to hide msg_id1_, even if it's not in
  // the queue.
  EXPECT_CALL(mock_instant_message_delegate_,
              HideInstantaneousMessage(Eq(ids_to_hide)))
      .Times(1);

  // Expect DisplayInstantaneousMessage to be called for the message
  // that was in the queue and not targeted by HideInstantMessage.
  InstantMessage displayed_message;
  EXPECT_CALL(mock_instant_message_delegate_, DisplayInstantaneousMessage(_, _))
      .WillOnce(SaveArg<0>(&displayed_message));

  processor_->HideInstantMessage(ids_to_hide);

  // Fast forward time to allow ProcessQueue to run.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify that the message originally in the queue was displayed.
  ASSERT_FALSE(displayed_message.attributions.empty());
  EXPECT_EQ(msg_id2_, displayed_message.attributions[0].id);
  EXPECT_EQ(message_in_queue.localized_message,
            displayed_message.localized_message);
}

}  // namespace collaboration::messaging
