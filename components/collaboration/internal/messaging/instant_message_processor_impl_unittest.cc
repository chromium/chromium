// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
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
    attribution.id =
        base::Uuid::ParseLowercase("cf07d904-88d4-4bc9-989d-57a9ab9e17a7");
    attribution.collaboration_id = data_sharing::GroupId("my group");
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
    attribution.tab_group_metadata->last_known_title = "last known group title";
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

    message.attribution = CreateAttribution1();
    return message;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockInstantMessageDelegate mock_instant_message_delegate_;
  MockMessagingBackendService messaging_backend_service_;
  std::unique_ptr<InstantMessageProcessorImpl> processor_;
};

TEST_F(InstantMessageProcessorImplTest, DisplayInstantMessageWithSuccess) {
  SetupInstantMessageDelegate();
  InstantMessage message = CreateInstantMessage();

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
  EXPECT_EQ(message.collaboration_event, actual_message.collaboration_event);
  EXPECT_EQ(message.level, actual_message.level);
  EXPECT_EQ(message.type, actual_message.type);
  EXPECT_EQ(message.attribution.has_value(),
            actual_message.attribution.has_value());
  EXPECT_EQ(message.attribution->id, actual_message.attribution->id);

  // Invoke callback and verify that backend clears out the message from DB.
  EXPECT_CALL(
      messaging_backend_service_,
      ClearPersistentMessage(Eq(message.attribution->id.value()),
                             Eq(PersistentNotificationType::INSTANT_MESSAGE)))
      .Times(1);
  std::move(success_callback).Run(true);
}

}  // namespace collaboration::messaging
