// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_database.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace collaboration::messaging {
namespace {

const char kCollaborationId1[] = "TEST_COLLAB_ID";
const char kMemberId1[] = "MEMBER_1";
const char kMemberId2[] = "MEMBER_2";

}  // namespace

class MockMessagingBackendDatabase : public MessagingBackendDatabase {
 public:
  MockMessagingBackendDatabase() = default;
  ~MockMessagingBackendDatabase() override = default;

  void Initialize(DBLoadedCallback db_loaded_callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(db_loaded_callback), true,
                       std::map<std::string, collaboration_pb::Message>()));
  }

  MOCK_METHOD(void,
              Update,
              (const collaboration_pb::Message& message),
              (override));
  MOCK_METHOD(void,
              Delete,
              (const std::vector<std::string>& message_uuids),
              (override));
  MOCK_METHOD(void, DeleteAllData, (), (override));
};

class MessagingBackendStoreTest : public testing::Test {
 public:
  void SetUp() override {
    auto database = std::make_unique<MockMessagingBackendDatabase>();
    unowned_database_ = database.get();
    store_ = std::make_unique<MessagingBackendStoreImpl>(std::move(database));

    base::RunLoop run_loop;
    store_->Initialize(base::BindOnce(
        [](base::RunLoop* run_loop, bool success) { run_loop->Quit(); },
        &run_loop));
    run_loop.Run();
  }

 protected:
  collaboration_pb::Message CreateMessage(
      collaboration_pb::EventType event_type,
      const std::string& collaboration_id = "TEST_COLLAB_ID",
      const std::string& member_id = "MEMBER_1") {
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
      message.set_affected_user_gaia_id(member_id);
    }
    return message;
  }

  collaboration_pb::Message CreateUngroupedMessage(
      collaboration_pb::EventType event_type) {
    collaboration_pb::Message message;
    message.set_event_type(event_type);

    message.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
    message.set_dirty(static_cast<int>(DirtyType::kAll));
    message.set_event_timestamp(base::Time::Now().ToTimeT());
    return message;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<MessagingBackendStoreImpl> store_;

  raw_ptr<MockMessagingBackendDatabase> unowned_database_;
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
                                collaboration_id, kMemberId1);
  auto message7 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                collaboration_id, kMemberId1);
  auto message8 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                collaboration_id, kMemberId1);
  auto message9 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                collaboration_id, kMemberId2);
  auto message10 =
      CreateUngroupedMessage(collaboration_pb::VERSION_OUT_OF_DATE);

  EXPECT_CALL(*unowned_database_, Update(_)).Times(10);
  EXPECT_CALL(*unowned_database_, Delete(_)).Times(4);

  store_->AddMessage(message1);
  store_->AddMessage(message2);  // Message 2 will replace message 1.
  store_->AddMessage(message3);
  store_->AddMessage(message4);
  store_->AddMessage(message5);  // Message 5 will replace message 4.
  store_->AddMessage(message6);
  store_->AddMessage(message7);  // Message 7 will replace message 6.
  store_->AddMessage(message8);  // Message 8 will replace message 7.
  store_->AddMessage(message9);
  store_->AddMessage(message10);

  std::optional<MessagesPerGroup*> messages_per_group =
      store_->GetMessagesPerGroupForTesting(
          data_sharing::GroupId(collaboration_id));
  ASSERT_TRUE(messages_per_group.has_value());
  MessagesPerGroup* messages = messages_per_group.value();
  EXPECT_EQ(1u, messages->tab_messages.size());
  EXPECT_EQ(2u, messages->tab_group_messages.size());
  EXPECT_EQ(2u, messages->collaboration_messages.size());

  // Count of all messages.
  EXPECT_EQ(6u, store_->GetDirtyMessages(std::nullopt).size());
  testing::Mock::VerifyAndClearExpectations(unowned_database_);

  // Try removing 3 messages where message 4 has already been overwritten.
  EXPECT_CALL(*unowned_database_, Delete).Times(1);
  store_->RemoveMessages(std::set<std::string>(
      {message3.uuid(), message4.uuid(), message5.uuid()}));
  EXPECT_EQ(4u, store_->GetDirtyMessages(std::nullopt).size());

  EXPECT_CALL(*unowned_database_, DeleteAllData).Times(1);
  store_->RemoveAllMessages();
  EXPECT_EQ(0u, store_->GetDirtyMessages(std::nullopt).size());
}

TEST_F(MessagingBackendStoreTest, HasAnyDirtyMessage) {
  EXPECT_FALSE(store_->HasAnyDirtyMessages(DirtyType::kAll));
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  store_->AddMessage(message);
  EXPECT_TRUE(store_->HasAnyDirtyMessages(DirtyType::kAll));
}

TEST_F(MessagingBackendStoreTest, TraverseMessageForVersionMessage) {
  EXPECT_FALSE(store_->HasAnyDirtyMessages(DirtyType::kAll));
  auto message1 = CreateUngroupedMessage(collaboration_pb::VERSION_OUT_OF_DATE);
  auto message2 = CreateUngroupedMessage(collaboration_pb::VERSION_OUT_OF_DATE);
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  // HasAnyDirtyMessages should internally traverse all messages including the
  // version message.
  EXPECT_TRUE(store_->HasAnyDirtyMessages(DirtyType::kAll));
  EXPECT_EQ(2u, store_->GetDirtyMessages(std::nullopt).size());

  store_->RemoveMessages({message1.uuid()});
  EXPECT_EQ(1u, store_->GetDirtyMessages(std::nullopt).size());

  store_->RemoveAllMessages();
  EXPECT_EQ(0u, store_->GetDirtyMessages(std::nullopt).size());
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

  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);

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

TEST_F(MessagingBackendStoreTest, ClearDirtyTabMessagesForGroup) {
  // Message 1 and message 2 are from different tabs and they are both dirty
  // with DirtyType::kAll.
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  auto message2 =
      CreateMessage(collaboration_pb::TAB_UPDATED, message1.collaboration_id());

  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);

  data_sharing::GroupId collaboration_id(message1.collaboration_id());
  EXPECT_EQ(
      0u,
      store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kDotAndChip)
          .size());
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  EXPECT_EQ(
      2u,
      store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kDotAndChip)
          .size());
  testing::Mock::VerifyAndClearExpectations(&unowned_database_);

  // Clear the dirty bits for the messages.
  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);
  auto cleared_messages =
      store_->ClearDirtyTabMessagesForGroup(collaboration_id);
  EXPECT_EQ(
      0u,
      store_->GetDirtyMessagesForGroup(collaboration_id, DirtyType::kDotAndChip)
          .size());
  EXPECT_EQ(2u, cleared_messages.size());
}

TEST_F(MessagingBackendStoreTest, ClearDirtyMessageById) {
  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);

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
  store_->AddMessage(CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                   kCollaborationId1, kMemberId1));
  auto non_dirty_message = CreateMessage(collaboration_pb::TAB_ADDED);
  non_dirty_message.set_dirty(static_cast<int>(DirtyType::kNone));
  store_->AddMessage(non_dirty_message);
  EXPECT_EQ(4u, store_->GetDirtyMessages(DirtyType::kAll).size());
  EXPECT_EQ(5u, store_->GetDirtyMessages(std::nullopt).size());
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

  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);
  EXPECT_CALL(*unowned_database_, Delete(_)).Times(1);

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
  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);
  EXPECT_CALL(*unowned_database_, Delete(_)).Times(1);

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

TEST_F(MessagingBackendStoreTest, KeepMostRecentCollaborationMessages) {
  EXPECT_CALL(*unowned_database_, Update(_)).Times(2);
  EXPECT_CALL(*unowned_database_, Delete(_)).Times(1);

  auto message1 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                kCollaborationId1, kMemberId1);
  // Message 2 has same timestamp. It will replace (delete) message 1.
  auto message2 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                kCollaborationId1, kMemberId1);
  // Message 3 is older than message2. It will be ignored (no delete / update).
  auto message3 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                kCollaborationId1, kMemberId1);
  message3.set_event_timestamp(message2.event_timestamp() - 1);

  data_sharing::GroupId group_id(kCollaborationId1);
  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);
  auto messages = store_->GetRecentMessagesForGroup(group_id);
  EXPECT_EQ(1u, messages.size());
  EXPECT_EQ(message2.uuid(), messages[0].uuid());  // latest is message2
}

TEST_F(MessagingBackendStoreTest, EnsureRecentActivityIsSorted) {
  data_sharing::GroupId group_id("Group ID");
  base::Time now = base::Time::Now();
  // Create three messages stored in an arbitrary order, but timestamp wise,
  // their order is: 3, 1, 4, 2, message 3 being the oldest.
  // This test verifies that they are received in the opposite order of this,
  // so 2, 4, 1, 3.
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED, group_id.value());
  message1.set_event_timestamp((now + base::Seconds(2)).ToTimeT());

  auto message2 =
      CreateMessage(collaboration_pb::TAB_REMOVED, group_id.value());
  message2.set_event_timestamp((now + base::Seconds(4)).ToTimeT());

  auto message3 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                group_id.value(), kMemberId1);
  message3.set_event_timestamp((now + base::Seconds(1)).ToTimeT());

  auto message4 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                group_id.value(), kMemberId2);
  message4.set_event_timestamp((now + base::Seconds(3)).ToTimeT());

  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);
  store_->AddMessage(message4);
  auto messages = store_->GetRecentMessagesForGroup(group_id);
  ASSERT_EQ(4u, messages.size());

  // Verify newest message is first, etc.
  EXPECT_EQ(message2.uuid(), messages[0].uuid());
  EXPECT_EQ(message4.uuid(), messages[1].uuid());
  EXPECT_EQ(message1.uuid(), messages[2].uuid());
  EXPECT_EQ(message3.uuid(), messages[3].uuid());
}

TEST_F(MessagingBackendStoreTest, EnsureExpiredMessagesAreDeleted) {
  // Create 6 messages.
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  auto message2 = CreateMessage(collaboration_pb::TAB_ADDED);
  auto message3 = CreateMessage(collaboration_pb::TAB_GROUP_NAME_UPDATED);
  auto message4 = CreateMessage(collaboration_pb::TAB_GROUP_COLOR_UPDATED);
  auto message5 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_ADDED,
                                kCollaborationId1, kMemberId1);
  auto message6 = CreateMessage(collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                                kCollaborationId1, kMemberId2);

  // Set message 2, 4 and 6 to be expired.
  base::Time now = base::Time::Now();
  message2.set_event_timestamp((now - base::Days(60)).ToTimeT());
  message4.set_event_timestamp((now - base::Days(60)).ToTimeT());
  message6.set_event_timestamp((now - base::Days(60)).ToTimeT());

  EXPECT_CALL(*unowned_database_, Update(_)).Times(6);

  store_->AddMessage(message1);
  store_->AddMessage(message2);
  store_->AddMessage(message3);
  store_->AddMessage(message4);
  store_->AddMessage(message5);
  store_->AddMessage(message6);

  // Trigger the clean up timer to run and ensure message 2, 4 and 6 are
  // deleted.
  std::vector<std::string> uuids = {message2.uuid(), message4.uuid(),
                                    message6.uuid()};
  EXPECT_CALL(*unowned_database_, Delete(uuids)).Times(1);
  task_environment_.FastForwardBy(base::Days(2));
}

}  // namespace collaboration::messaging
