// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"

#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::messaging {

class MessagingBackendStoreTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = std::make_unique<MessagingBackendStoreImpl>();
  }

 protected:
  collaboration_pb::Message CreateMessage(
      collaboration_pb::EventType event_type,
      const std::string& collaboration_id = "TEST_COLLAB_ID") {
    collaboration_pb::Message message;
    message.set_event_type(event_type);

    message.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
    message.set_collaboration_id(collaboration_id);
    message.set_dirty(static_cast<int>(DirtyType::kAll));
    message.set_event_timestamp(base::Time::Now().ToTimeT());

    MessageCategory category = GetMessageCategory(message);
    if (category == MessageCategory::kTab) {
      message.mutable_tab_data()->set_sync_tab_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
      message.mutable_tab_data()->set_sync_tab_group_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    } else if (category == MessageCategory::kTabGroup) {
      message.mutable_tab_group_data()->set_sync_tab_group_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    } else if (category == MessageCategory::kCollaboration) {
      *message.mutable_collaboration_data() =
          collaboration_pb::CollaborationData();
    }
    return message;
  }

  std::unique_ptr<MessagingBackendStoreImpl> store_;
};

TEST_F(MessagingBackendStoreTest, AddMessages) {
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  std::string collaboration_id = message1.collaboration_id();

  auto message2 =
      CreateMessage(collaboration_pb::TAB_UPDATED, collaboration_id);
  message2.mutable_tab_data()->set_sync_tab_id(
      message1.tab_data().sync_tab_id());

  auto message3 =
      CreateMessage(collaboration_pb::TAB_GROUP_NAME_UPDATED, collaboration_id);
  auto message4 = CreateMessage(collaboration_pb::TAB_GROUP_COLOR_UPDATED,
                                collaboration_id);
  auto message5 = CreateMessage(collaboration_pb::TAB_GROUP_COLOR_UPDATED,
                                collaboration_id);

  auto message6 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                collaboration_id);
  auto message7 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                collaboration_id);

  store_->AddMessage(message1);
  store_->AddMessage(message2);  // Message 2 will replace message 1.
  store_->AddMessage(message3);
  store_->AddMessage(message4);
  store_->AddMessage(message5);  // Message 5 will replace message 4.
  store_->AddMessage(message6);
  store_->AddMessage(message7);

  std::optional<MessagesPerGroup*> messages_per_group =
      store_->GetMessagesPerGroupForTesting(
          data_sharing::GroupId(collaboration_id));
  ASSERT_TRUE(messages_per_group.has_value());
  MessagesPerGroup* messages = messages_per_group.value();
  EXPECT_EQ(1u, messages->tab_messages.size());
  EXPECT_EQ(2u, messages->tab_group_messages.size());
  EXPECT_EQ(2u, messages->collaboration_messages.size());
}

TEST_F(MessagingBackendStoreTest, HasAnyDirtyMessage) {
  EXPECT_FALSE(store_->HasAnyDirtyMessages(DirtyType::kAll));
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  store_->AddMessage(message);
  EXPECT_TRUE(store_->HasAnyDirtyMessages(DirtyType::kAll));
}

TEST_F(MessagingBackendStoreTest, GetDirtyMessagesForGroup) {
  // Message 1 and message 2 are from different tabs and they are both dirty.
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  auto message2 =
      CreateMessage(collaboration_pb::TAB_UPDATED, message1.collaboration_id());

  data_sharing::GroupId collaboration_id(message1.collaboration_id());

  EXPECT_EQ(0u,
            store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kAll)
                .size());
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  EXPECT_EQ(2u,
            store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kAll)
                .size());
}

TEST_F(MessagingBackendStoreTest, GetDirtyMessageForTab) {
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  data_sharing::GroupId collaboration_id(message.collaboration_id());
  base::Uuid tab_id =
      base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());
  EXPECT_EQ(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kAll));
  store_->AddMessage(message);
  EXPECT_NE(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kAll));
}

TEST_F(MessagingBackendStoreTest, ClearDirtyMessageForTab) {
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  data_sharing::GroupId collaboration_id(message.collaboration_id());
  base::Uuid tab_id =
      base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());

  EXPECT_EQ(0u,
            store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kAll)
                .size());
  store_->AddMessage(message);
  EXPECT_EQ(1u,
            store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kAll)
                .size());
  store_->ClearDirtyMessageForTab(collaboration_id, tab_id, DirtyType::kAll);
  EXPECT_EQ(0u,
            store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kAll)
                .size());
}

TEST_F(MessagingBackendStoreTest, ClearDirtyMessageById) {
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  store_->AddMessage(message);
  EXPECT_TRUE(store_->HasAnyDirtyMessages(DirtyType::kAll));
  store_->ClearDirtyMessage(base::Uuid::ParseLowercase(message.uuid()),
                            DirtyType::kAll);
  EXPECT_FALSE(store_->HasAnyDirtyMessages(DirtyType::kAll));
}

TEST_F(MessagingBackendStoreTest, GetAllDirtyMessages) {
  store_->AddMessage(CreateMessage(collaboration_pb::TAB_ADDED));
  store_->AddMessage(CreateMessage(collaboration_pb::TAB_UPDATED));
  store_->AddMessage(CreateMessage(collaboration_pb::TAB_GROUP_COLOR_UPDATED));
  store_->AddMessage(
      CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED));
  EXPECT_EQ(4u, store_->GetDirtyMessages(DirtyType::kAll).size());
}

TEST_F(MessagingBackendStoreTest, GetRecentMessagesForGroup) {
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  auto message2 =
      CreateMessage(collaboration_pb::TAB_ADDED, message1.collaboration_id());
  auto message3 =
      CreateMessage(collaboration_pb::TAB_ADDED, message1.collaboration_id());
  message3.set_event_timestamp(
      (base::Time::Now() - store_->GetRecentMessageCutoffDuration()).ToTimeT() -
      1);

  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);

  data_sharing::GroupId group_id(message1.collaboration_id());
  EXPECT_EQ(2u, store_->GetRecentMessagesForGroup(group_id).size());
}

TEST_F(MessagingBackendStoreTest, SetAndClearDirtyTypes) {
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  message.set_dirty(static_cast<int>(DirtyType::kDot) |
                    static_cast<int>(DirtyType::kChip));
  store_->AddMessage(message);
  data_sharing::GroupId collaboration_id(message.collaboration_id());
  base::Uuid tab_id =
      base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());
  EXPECT_NE(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kAll));
  store_->ClearDirtyMessageForTab(collaboration_id, tab_id, DirtyType::kDot);
  EXPECT_EQ(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kDot));
  EXPECT_NE(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kAll));
  store_->ClearDirtyMessageForTab(collaboration_id, tab_id, DirtyType::kChip);
  EXPECT_EQ(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kChip));
  EXPECT_EQ(std::nullopt, store_->GetDirtyMessageForTab(
                              collaboration_id, tab_id, DirtyType::kAll));
}

TEST_F(MessagingBackendStoreTest, KeepMostRecentTabMessages) {
  // message3.event_timestamp < message1.event_timestamp <=
  // message2.event_timestamp; Keep message2 since it's the latest message.
  auto message1 = CreateMessage(collaboration_pb::TAB_UPDATED);
  auto message2 =
      CreateMessage(collaboration_pb::TAB_UPDATED, message1.collaboration_id());
  message2.mutable_tab_data()->set_sync_tab_id(
      message1.tab_data().sync_tab_id());
  auto message3 =
      CreateMessage(collaboration_pb::TAB_UPDATED, message1.collaboration_id());
  message3.mutable_tab_data()->set_sync_tab_id(
      message1.tab_data().sync_tab_id());
  message3.set_event_timestamp(message2.event_timestamp() - 1);
  data_sharing::GroupId group_id(message1.collaboration_id());
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);
  auto messages = store_->GetRecentMessagesForGroup(group_id);
  EXPECT_EQ(1u, messages.size());
  EXPECT_EQ(message2.uuid(), messages[0].uuid());
}

TEST_F(MessagingBackendStoreTest, KeepMostRecentTabGroupMessages) {
  // message3.event_timestamp < message1.event_timestamp <=
  // message2.event_timestamp; Keep message2 since it's the latest message.
  auto message1 = CreateMessage(collaboration_pb::TAB_GROUP_NAME_UPDATED);
  auto message2 = CreateMessage(collaboration_pb::TAB_GROUP_NAME_UPDATED,
                                message1.collaboration_id());
  auto message3 = CreateMessage(collaboration_pb::TAB_GROUP_NAME_UPDATED,
                                message1.collaboration_id());
  message3.set_event_timestamp(message2.event_timestamp() - 1);
  data_sharing::GroupId group_id(message1.collaboration_id());
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);
  auto messages = store_->GetRecentMessagesForGroup(group_id);
  EXPECT_EQ(1u, messages.size());
  EXPECT_EQ(message2.uuid(), messages[0].uuid());
}

}  // namespace collaboration::messaging
