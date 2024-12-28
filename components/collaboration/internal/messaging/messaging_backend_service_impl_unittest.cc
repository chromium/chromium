// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <ctime>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SaveArg;
using testing::Truly;

namespace collaboration::messaging {

namespace {
bool PersistentMessagesHaveSameTypeAndEvent(const PersistentMessage& a,
                                            const PersistentMessage& b) {
  return a.collaboration_event == b.collaboration_event && a.type == b.type;
}

MATCHER_P(PersistentMessageTypeAndEventEq, expected_message, "") {
  return PersistentMessagesHaveSameTypeAndEvent(arg, expected_message);
}

data_sharing::GroupMemberPartialData CreatePartialGroupMember(
    const GaiaId& gaia_id,
    const std::string& display_name,
    const std::string& given_name) {
  data_sharing::GroupMemberPartialData member;
  member.gaia_id = gaia_id;
  member.display_name = display_name;
  member.given_name = given_name;
  return member;
}

collaboration_pb::Message CreateStoredMessage(
    const data_sharing::GroupId& collaboration_group_id,
    collaboration_pb::EventType event_type,
    DirtyType dirty_type,
    const base::Time& event_time) {
  collaboration_pb::Message message;
  message.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  message.set_collaboration_id(collaboration_group_id.value());
  message.set_event_type(event_type);
  message.set_dirty(static_cast<int>(dirty_type));
  message.set_event_timestamp(event_time.ToTimeT());
  return message;
}

data_sharing::GroupMemberPartialData CreatePartialMember(
    const GaiaId& gaia_id,
    std::string email,
    std::string display_name,
    std::string given_name) {
  data_sharing::GroupMemberPartialData member;
  member.gaia_id = gaia_id;
  member.email = email;
  member.display_name = display_name;
  member.given_name = given_name;
  return member;
}

tab_groups::SavedTabGroup CreateSharedTabGroup(
    data_sharing::GroupId collaboration_group_id) {
  base::Uuid tab_group_sync_id = base::Uuid::GenerateRandomV4();

  std::vector<tab_groups::SavedTabGroupTab> tabs;
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    tab_group_sync_id, std::nullopt);
  tab_groups::SavedTabGroupTab tab2(GURL("https://www.example2.com/"), u"Tab 2",
                                    tab_group_sync_id, std::nullopt);
  tabs.emplace_back(tab1);
  tabs.emplace_back(tab2);

  tab_groups::SavedTabGroup tab_group(u"Tab Group Title",
                                      tab_groups::TabGroupColorId::kOrange,
                                      tabs, std::nullopt, tab_group_sync_id);
  tab_group.SetCollaborationId(
      tab_groups::CollaborationId(collaboration_group_id.value()));
  return tab_group;
}

class MockInstantMessageDelegate
    : public MessagingBackendService::InstantMessageDelegate {
 public:
  MOCK_METHOD(void,
              DisplayInstantaneousMessage,
              (InstantMessage message, SuccessCallback success_callback),
              (override));
};

class MockPersistentMessageObserver
    : public MessagingBackendService::PersistentMessageObserver {
 public:
  MOCK_METHOD(void, OnMessagingBackendServiceInitialized, (), (override));
  MOCK_METHOD(void,
              DisplayPersistentMessage,
              (PersistentMessage message),
              (override));
  MOCK_METHOD(void,
              HidePersistentMessage,
              (PersistentMessage message),
              (override));
};

}  // namespace

class MockTabGroupChangeNotifier : public TabGroupChangeNotifier {
 public:
  MockTabGroupChangeNotifier() = default;
  ~MockTabGroupChangeNotifier() override = default;

  MOCK_METHOD(void,
              AddObserver,
              (TabGroupChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (TabGroupChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
};

class MockMessagingBackendStore : public MessagingBackendStore {
 public:
  MockMessagingBackendStore() = default;
  ~MockMessagingBackendStore() override = default;

  MOCK_METHOD(void,
              Initialize,
              (base::OnceCallback<void(bool)> on_initialized_callback),
              (override));
  MOCK_METHOD(bool, HasAnyDirtyMessages, (DirtyType dirty_type), (override));
  MOCK_METHOD(void,
              ClearDirtyMessageForTab,
              (const data_sharing::GroupId& collaboration_id,
               const base::Uuid& tab_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(void,
              ClearDirtyMessage,
              (const base::Uuid uuid, DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetDirtyMessages,
              (DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetDirtyMessagesForGroup,
              (const data_sharing::GroupId& collaboration_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::optional<collaboration_pb::Message>,
              GetDirtyMessageForTab,
              (const data_sharing::GroupId& collaboration_id,
               const base::Uuid& tab_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetRecentMessagesForGroup,
              (const data_sharing::GroupId& collaboration_id),
              (override));
  MOCK_METHOD(void,
              AddMessage,
              (const collaboration_pb::Message& message),
              (override));
  MOCK_METHOD(base::TimeDelta, GetRecentMessageCutoffDuration, (), (override));
  MOCK_METHOD(void,
              SetRecentMessageCutoffDuration,
              (base::TimeDelta time_delta),
              (override));
};

class MockDataSharingChangeNotifier : public DataSharingChangeNotifier {
 public:
  MockDataSharingChangeNotifier() = default;
  ~MockDataSharingChangeNotifier() override = default;

  MOCK_METHOD(void,
              AddObserver,
              (DataSharingChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (DataSharingChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(FlushCallback, Initialize, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
};

class MessagingBackendServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    mock_tab_group_sync_service_ =
        std::make_unique<tab_groups::MockTabGroupSyncService>();
    mock_data_sharing_service_ =
        std::make_unique<data_sharing::MockDataSharingService>();
  }

  void TearDown() override {}

  void CreateService() {
    auto tab_group_change_notifier =
        std::make_unique<MockTabGroupChangeNotifier>();
    unowned_tab_group_change_notifier_ = tab_group_change_notifier.get();
    EXPECT_CALL(*unowned_tab_group_change_notifier_, AddObserver(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&tg_notifier_observer_));
    EXPECT_CALL(*unowned_tab_group_change_notifier_, Initialize());
    EXPECT_CALL(*unowned_tab_group_change_notifier_, RemoveObserver(_));

    auto data_sharing_change_notifier =
        std::make_unique<MockDataSharingChangeNotifier>();
    unowned_data_sharing_change_notifier_ = data_sharing_change_notifier.get();
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, AddObserver(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&ds_notifier_observer_));
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, Initialize())
        .WillOnce(Return(base::DoNothing()));
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, RemoveObserver(_));

    auto mock_messaging_backend_store =
        std::make_unique<MockMessagingBackendStore>();
    unowned_messaging_backend_store_ = mock_messaging_backend_store.get();
    EXPECT_CALL(*unowned_messaging_backend_store_, Initialize(_))
        .Times(1)
        .WillOnce(
            [&](base::OnceCallback<void(bool)> on_store_initialized_callback) {
              on_store_initialized_callback_ =
                  std::move(on_store_initialized_callback);
            });

    service_ = std::make_unique<MessagingBackendServiceImpl>(
        configuration, std::move(tab_group_change_notifier),
        std::move(data_sharing_change_notifier),
        std::move(mock_messaging_backend_store),
        mock_tab_group_sync_service_.get(), mock_data_sharing_service_.get());
  }

  void InitializeService() {
    std::move(on_store_initialized_callback_).Run(/*success=*/true);
    ds_notifier_observer_->OnDataSharingChangeNotifierInitialized();
    tg_notifier_observer_->OnTabGroupChangeNotifierInitialized();
  }

  void CreateAndInitializeService() {
    CreateService();
    InitializeService();
  }

  void SetupInstantMessageDelegate() {
    mock_instant_message_delegate_ =
        std::make_unique<MockInstantMessageDelegate>();
    service_->SetInstantMessageDelegate(mock_instant_message_delegate_.get());
  }

  void AddPersistentMessageObserver() {
    service_->AddPersistentMessageObserver(&mock_persistent_message_observer_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Use default configuration unless we specify something else.
  MessagingBackendConfiguration configuration;
  base::OnceCallback<void(bool)> on_store_initialized_callback_;
  std::unique_ptr<data_sharing::MockDataSharingService>
      mock_data_sharing_service_;
  std::unique_ptr<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  std::unique_ptr<MockInstantMessageDelegate> mock_instant_message_delegate_;
  MockPersistentMessageObserver mock_persistent_message_observer_;
  std::unique_ptr<MessagingBackendServiceImpl> service_;
  raw_ptr<MockTabGroupChangeNotifier> unowned_tab_group_change_notifier_;
  raw_ptr<MockDataSharingChangeNotifier> unowned_data_sharing_change_notifier_;
  raw_ptr<MockMessagingBackendStore> unowned_messaging_backend_store_;
  raw_ptr<TabGroupChangeNotifier::Observer> tg_notifier_observer_;
  raw_ptr<DataSharingChangeNotifier::Observer> ds_notifier_observer_;
};

TEST_F(MessagingBackendServiceImplTest, TestInitialization) {
  CreateService();
  EXPECT_FALSE(service_->IsInitialized());
  std::move(on_store_initialized_callback_).Run(/*success=*/true);
  EXPECT_FALSE(service_->IsInitialized());
  ds_notifier_observer_->OnDataSharingChangeNotifierInitialized();
  EXPECT_FALSE(service_->IsInitialized());
  tg_notifier_observer_->OnTabGroupChangeNotifierInitialized();
  EXPECT_TRUE(service_->IsInitialized());
}

void VerifyGenericMessageData(const collaboration_pb::Message& message,
                              const std::string& collaboration_id,
                              collaboration_pb::EventType event_type,
                              DirtyType dirty_type,
                              time_t event_timestamp) {
  EXPECT_NE("", message.uuid());
  EXPECT_EQ(message.event_timestamp(), message.event_timestamp());
  EXPECT_EQ(message.collaboration_id(), collaboration_id);
  EXPECT_EQ(message.event_type(), event_type);
  EXPECT_EQ(message.dirty(), static_cast<int>(dirty_type));
}

TEST_F(MessagingBackendServiceImplTest, TestStoringCollaborationEvents) {
  CreateAndInitializeService();

  data_sharing::GroupData group_data;
  group_data.group_token.group_id = data_sharing::GroupId("my group id");
  data_sharing::GroupMember member;
  member.gaia_id = GaiaId("abc");
  member.display_name = "First Last";
  member.given_name = "First";
  group_data.members.emplace_back(member);

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(group_data.group_token.group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Always refers to the latest message that has been added to storage.
  collaboration_pb::Message message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&message));

  EXPECT_CALL(*mock_data_sharing_service_, GetPossiblyRemovedGroupMember(_, _))
      .WillRepeatedly(Return(std::nullopt));

  base::Time time = base::Time::Now();
  ds_notifier_observer_->OnGroupAdded(group_data.group_token.group_id,
                                      group_data, time);
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::COLLABORATION_ADDED,
                           DirtyType::kNone, time.ToTimeT());

  // Move time forward so it is unique.
  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupRemoved(group_data.group_token.group_id,
                                        group_data, time);
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::COLLABORATION_REMOVED,
                           DirtyType::kMessageOnly, time.ToTimeT());

  time += base::Seconds(1);
  GaiaId gaia_id("abc");
  ds_notifier_observer_->OnGroupMemberAdded(group_data, gaia_id, time);
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ("abc", message.affected_user_gaia_id());
  EXPECT_EQ("First", message.collaboration_data().affected_user_name());

  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupMemberRemoved(group_data, gaia_id, time);
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::COLLABORATION_MEMBER_REMOVED,
                           DirtyType::kNone, time.ToTimeT());
  EXPECT_EQ("abc", message.affected_user_gaia_id());
  EXPECT_EQ("First", message.collaboration_data().affected_user_name());
}

TEST_F(MessagingBackendServiceImplTest,
       TestLookingUpMemberNameForCollaborationEventsForStorage) {
  CreateAndInitializeService();

  data_sharing::GroupData group_data;
  data_sharing::GroupId group_id("my group id");
  group_data.group_token.group_id = group_id;
  data_sharing::GroupMember member1;
  member1.gaia_id = GaiaId("abc");
  member1.display_name = "Provided Diplay Name 1";
  member1.given_name = "Provided Given Name 1";
  group_data.members.emplace_back(member1);

  data_sharing::GroupMember member2;
  member2.gaia_id = GaiaId("def");
  member2.display_name = "Provided Display Name 2";
  member2.given_name = "";  // No given name available.
  group_data.members.emplace_back(member2);

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(group_data.group_token.group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Always refers to the latest message that has been added to storage.
  collaboration_pb::Message message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&message));

  // Current given name should be first priority.
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(group_id), Eq(member1.gaia_id)))
      .WillOnce(Return(std::make_optional(CreatePartialGroupMember(
          member1.gaia_id, "Live Display Name 1", "Live Given Name 1"))));
  base::Time time = base::Time::Now();
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member1.gaia_id, time);
  VerifyGenericMessageData(message, group_id.value(),
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ(member1.gaia_id, message.affected_user_gaia_id());
  EXPECT_EQ("Live Given Name 1",
            message.collaboration_data().affected_user_name());

  // Given name from provided data should be second priority.
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(group_id), Eq(member1.gaia_id)))
      .WillOnce(Return(std::nullopt));
  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member1.gaia_id, time);
  VerifyGenericMessageData(message, group_id.value(),
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ(member1.gaia_id, message.affected_user_gaia_id());
  EXPECT_EQ("Provided Given Name 1",
            message.collaboration_data().affected_user_name());

  // Current display name should be next.
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(group_id), Eq(member2.gaia_id)))
      .WillOnce(Return(std::make_optional(CreatePartialGroupMember(
          member2.gaia_id, "Live Display Name 2", /*given_name=*/""))));
  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member2.gaia_id, time);
  VerifyGenericMessageData(message, group_id.value(),
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ(member2.gaia_id, message.affected_user_gaia_id());
  EXPECT_EQ("Live Display Name 2",
            message.collaboration_data().affected_user_name());

  // Provided display name should be next.
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(group_id), Eq(member2.gaia_id)))
      .WillOnce(Return(std::nullopt));
  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member2.gaia_id, time);
  VerifyGenericMessageData(message, group_id.value(),
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ(member2.gaia_id, message.affected_user_gaia_id());
  EXPECT_EQ("Provided Display Name 2",
            message.collaboration_data().affected_user_name());
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogWithNoEvents) {
  CreateAndInitializeService();

  std::vector<collaboration_pb::Message> messages;

  EXPECT_CALL(*unowned_messaging_backend_store_, GetRecentMessagesForGroup(_))
      .WillOnce(Return(messages));

  ActivityLogQueryParams params;
  params.collaboration_id = data_sharing::GroupId("my group id");
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  EXPECT_EQ(0u, activity_log.size());
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogAcceptsMaxLength) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  base::Time now = base::Time::Now();
  std::vector<collaboration_pb::Message> messages;
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED, DirtyType::kNone,
      now + base::Seconds(4));
  messages.emplace_back(message1);
  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_REMOVED,
      DirtyType::kNone, now + base::Seconds(3));
  messages.emplace_back(message2);
  collaboration_pb::Message message3 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kNone, now + base::Seconds(2));
  messages.emplace_back(message3);
  // COLLABORATION_ADDED should never be returned.
  collaboration_pb::Message message4 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::COLLABORATION_ADDED,
      DirtyType::kNone, now + base::Seconds(1));
  messages.emplace_back(message4);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetRecentMessagesForGroup(Eq(collaboration_group_id)))
      .WillRepeatedly(Return(messages));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  params.result_length = 2;  // We only want max 2 items.
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(2u, activity_log.size());
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_REMOVED,
            activity_log[1].collaboration_event);

  // We want max 5 items, but the log has 3, it should still return what it has.
  params.result_length = 5;
  activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(3u, activity_log.size());
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_REMOVED,
            activity_log[1].collaboration_event);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, activity_log[2].collaboration_event);

  // Setting result length to 0 should return all items.
  params.result_length = 0;
  activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(3u, activity_log.size());
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogCollaborationEvents) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  base::Time now = base::Time::Now();
  std::vector<collaboration_pb::Message> messages;
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED, DirtyType::kNone,
      now);
  message1.set_affected_user_gaia_id("gaia_1");
  message1.mutable_collaboration_data()->set_affected_user_name("gaia_1 name");
  messages.emplace_back(message1);

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_REMOVED,
      DirtyType::kNone, now);
  message2.set_affected_user_gaia_id("gaia_2");
  message2.mutable_collaboration_data()->set_affected_user_name("gaia_2 name");
  messages.emplace_back(message2);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetRecentMessagesForGroup(Eq(collaboration_group_id)))
      .WillOnce(Return(messages));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_2"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_2"), "gaia2@gmail.com",
                                     "Display Name", "Given Name 2")));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(2u, activity_log.size());
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_REMOVED,
            activity_log[1].collaboration_event);

  // Use name from DB and no email. This is not intended to happen, because we
  // should always have the member data, but this tests that we still work
  // without it.
  EXPECT_EQ("gaia_1 name", activity_log[0].user_display_name);
  EXPECT_EQ(u"", activity_log[0].description);
  // Use name and email from DataSharingService.
  EXPECT_EQ("Given Name 2", activity_log[1].user_display_name);
  EXPECT_EQ(u"gaia2@gmail.com", activity_log[1].description);
  // We should also fill in the MessageAttribution.
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[1].activity_metadata.affected_user->email);
}

TEST_F(MessagingBackendServiceImplTest, TestStoringTabGroupEvents) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  // Always refers to the latest message that has been added to storage.
  collaboration_pb::Message message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetCreatedByAttribution(gaia1);
  tab_group.SetUpdatedByAttribution(gaia2);

  tg_notifier_observer_->OnTabGroupAdded(tab_group);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_ADDED, DirtyType::kNone,
                           now.ToTimeT());
  EXPECT_EQ(gaia1, message.triggering_user_gaia_id());

  tg_notifier_observer_->OnTabGroupRemoved(tab_group);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_REMOVED,
                           DirtyType::kNone, now.ToTimeT());
  EXPECT_EQ(gaia2, message.triggering_user_gaia_id());

  tg_notifier_observer_->OnTabGroupNameUpdated(tab_group);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_NAME_UPDATED,
                           DirtyType::kNone, now.ToTimeT());
  EXPECT_EQ(gaia2, message.triggering_user_gaia_id());

  tg_notifier_observer_->OnTabGroupColorUpdated(tab_group);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_COLOR_UPDATED,
                           DirtyType::kNone, now.ToTimeT());
  EXPECT_EQ(gaia2, message.triggering_user_gaia_id());
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogTabGroupEvents) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  GaiaId gaia1("gaia_1");
  GaiaId gaia2("gaia_2");

  base::Time now = base::Time::Now();
  std::vector<collaboration_pb::Message> messages;
  // Adding and removing tab groups should not be part of activity log.
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_GROUP_ADDED,
      DirtyType::kNone, now);
  message1.set_triggering_user_gaia_id("gaia_1");
  messages.emplace_back(message1);

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_GROUP_REMOVED,
      DirtyType::kNone, now);
  message2.set_triggering_user_gaia_id("gaia_2");
  messages.emplace_back(message2);

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetCreatedByAttribution(gaia1);
  tab_group.SetUpdatedByAttribution(gaia2);
  collaboration_pb::Message message3 =
      CreateStoredMessage(collaboration_group_id,
                          collaboration_pb::EventType::TAB_GROUP_NAME_UPDATED,
                          DirtyType::kNone, now);
  message3.mutable_tab_group_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  message3.set_triggering_user_gaia_id("gaia_2");
  messages.emplace_back(message3);

  collaboration_pb::Message message4 =
      CreateStoredMessage(collaboration_group_id,
                          collaboration_pb::EventType::TAB_GROUP_COLOR_UPDATED,
                          DirtyType::kNone, now);
  message4.set_triggering_user_gaia_id("gaia_2");
  messages.emplace_back(message4);

  // Add support for looking up GAIA IDs.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetRecentMessagesForGroup(Eq(collaboration_group_id)))
      .WillOnce(Return(messages));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_2"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_2"), "gaia2@gmail.com",
                                     "Display Name", "Given Name 2")));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillOnce(Return(tab_group));

  // Query for all itemms, which should only be name and color updates.
  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(2u, activity_log.size());
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_NAME_UPDATED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(tab_group.title(), activity_log[0].description);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_COLOR_UPDATED,
            activity_log[1].collaboration_event);
}

TEST_F(MessagingBackendServiceImplTest, TestReceivingTabEvents) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");
  PersistentMessage expected_message_chip;
  expected_message_chip.type = PersistentNotificationType::CHIP;
  PersistentMessage expected_message_dot;
  expected_message_dot.type = PersistentNotificationType::DIRTY_TAB;
  PersistentMessage expected_message_dot_tab_group;
  expected_message_dot_tab_group.type =
      PersistentNotificationType::DIRTY_TAB_GROUP;
  expected_message_dot_tab_group.collaboration_event =
      CollaborationEvent::UNDEFINED;

  // Enables support for dirty and non-dirty tab groups.
  std::vector<collaboration_pb::Message> db_messages_dirty;
  collaboration_pb::Message db_message;
  db_messages_dirty.emplace_back(db_message);
  std::vector<collaboration_pb::Message> db_messages_empty;

  // Always refers to the latest message that has been added to storage.
  collaboration_pb::Message message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  // Make creation and update GaiaId unique.
  tab1->SetCreatedByAttribution(gaia1);
  tab1->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab1->SetUpdateTimeWindowsEpochMicros(now + base::Seconds(1));

  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  // Make creation and update GaiaId unique.
  tab2->SetCreatedByAttribution(gaia1);
  tab2->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab2->SetUpdateTimeWindowsEpochMicros(now + base::Seconds(1));

  // Create a third tab to check for removal.
  base::Uuid tab3_sync_id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab3(GURL("https://www.example3.com/"), u"Tab 3",
                                    tab_group.saved_guid(), std::nullopt,
                                    tab3_sync_id);
  tab3.SetCreatedByAttribution(gaia1);
  tab3.SetUpdatedByAttribution(gaia2);

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Temporary storage of observer invocations.
  PersistentMessage last_persistent_message_chip;
  PersistentMessage last_persistent_message_dot;
  PersistentMessage last_persistent_message_dot_tab_group;

  // SCENARIO 1: Add a new tab.
  // Should receive stored messages about tab added, and also there should be a
  // dirty dot and chip for the tab, and a dirty dot for the group.
  expected_message_chip.collaboration_event = CollaborationEvent::TAB_ADDED;
  expected_message_dot.collaboration_event = CollaborationEvent::TAB_ADDED;
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages_dirty));
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabAdded(*tab1);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_ADDED, DirtyType::kDotAndChip,
                           now.ToTimeT());
  EXPECT_EQ(gaia1, message.triggering_user_gaia_id());
  EXPECT_EQ(tab1->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab1->creation_time_windows_epoch_micros().ToTimeT(),
            message.event_timestamp());
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);

  // SCENARIO 2: Update a tab.
  // Should receive stored messages about tab updated, and also there should be
  // a dirty dot and chip for the tab, and a dirty dot for the group.
  expected_message_chip.collaboration_event = CollaborationEvent::TAB_UPDATED;
  expected_message_dot.collaboration_event = CollaborationEvent::TAB_UPDATED;
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages_dirty));
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabUpdated(*tab2);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_UPDATED,
                           DirtyType::kDotAndChip, now.ToTimeT());
  EXPECT_EQ(gaia2, message.triggering_user_gaia_id());
  EXPECT_EQ(tab2->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab2->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab2->update_time_windows_epoch_micros().ToTimeT(),
            message.event_timestamp());
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);

  // SCENARIO 3: Remove a tab.
  // Should receive stored messages about tab removed, and we should also remove
  // the dirty dot and chip for the tab, as well as the group. In this
  // particular scenario we are faking that there are no more dirty dots on tabs
  // in the group, so the dot for the group should also go away.
  expected_message_chip.collaboration_event = CollaborationEvent::TAB_REMOVED;
  expected_message_dot.collaboration_event = CollaborationEvent::TAB_REMOVED;
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  // No more dirty dots for the tab group.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages_empty));
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabRemoved(tab3);
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_REMOVED, DirtyType::kNone,
                           now.ToTimeT());
  EXPECT_EQ(gaia2, message.triggering_user_gaia_id());
  EXPECT_EQ(tab3.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab3.update_time_windows_epoch_micros().ToTimeT(),
            message.event_timestamp());
  EXPECT_EQ(tab3.url().spec(), message.tab_data().last_url());
  EXPECT_EQ(tab3_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab3_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogTabEvents) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetCreatedByAttribution(gaia1);
  tab_group.SetUpdatedByAttribution(gaia2);

  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  tab1->SetCreatedByAttribution(gaia1);
  tab1->SetUpdatedByAttribution(gaia2);

  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  tab2->SetCreatedByAttribution(gaia1);
  tab2->SetUpdatedByAttribution(gaia2);

  // Create a third tab to check for removal.
  tab_groups::SavedTabGroupTab tab3(GURL("https://www.example3.com/"), u"Tab 3",
                                    tab_group.saved_guid(), std::nullopt);

  std::vector<collaboration_pb::Message> messages;

  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  message1.set_triggering_user_gaia_id("gaia_1");
  message1.mutable_tab_data()->set_sync_tab_id(
      tab1->saved_tab_guid().AsLowercaseString());
  message1.mutable_tab_data()->set_sync_tab_group_id(
      tab1->saved_group_guid().AsLowercaseString());
  messages.emplace_back(message1);

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, now);
  message2.set_triggering_user_gaia_id("gaia_2");
  message2.mutable_tab_data()->set_sync_tab_id(
      tab2->saved_tab_guid().AsLowercaseString());
  message2.mutable_tab_data()->set_sync_tab_group_id(
      tab2->saved_group_guid().AsLowercaseString());
  messages.emplace_back(message2);

  collaboration_pb::Message message3 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_REMOVED,
      DirtyType::kNone, now);
  message3.set_triggering_user_gaia_id("gaia_2");
  message3.mutable_tab_data()->set_sync_tab_id(
      tab3.saved_tab_guid().AsLowercaseString());
  message3.mutable_tab_data()->set_sync_tab_group_id(
      tab3.saved_group_guid().AsLowercaseString());
  message3.mutable_tab_data()->set_last_url(tab3.url().spec());
  messages.emplace_back(message3);

  // Add support for looking up GAIA IDs.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetRecentMessagesForGroup(Eq(collaboration_group_id)))
      .WillOnce(Return(messages));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_1"), "gaia1@gmail.com",
                                     "Display Name 1", "Given Name 1")));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_2"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_2"), "gaia2@gmail.com",
                                     "Display Name 1", "Given Name 2")));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(3u, activity_log.size());

  // Verify tab 1.
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, activity_log[0].collaboration_event);
  EXPECT_EQ(tab1->url().spec(),
            *activity_log[0].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example.com", activity_log[0].description);
  EXPECT_EQ("Given Name 1", activity_log[0].user_display_name);
  EXPECT_EQ("gaia1@gmail.com",
            activity_log[0].activity_metadata.triggering_user->email);

  // Verify tab 2.
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
            activity_log[1].collaboration_event);
  EXPECT_EQ(tab2->url().spec(),
            *activity_log[1].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example2.com", activity_log[1].description);
  EXPECT_EQ("Given Name 2", activity_log[1].user_display_name);
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[1].activity_metadata.triggering_user->email);

  // Verify tab 3.
  EXPECT_EQ(CollaborationEvent::TAB_REMOVED,
            activity_log[2].collaboration_event);
  EXPECT_EQ(tab3.url().spec(),
            *activity_log[2].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example3.com", activity_log[2].description);
  EXPECT_EQ("Given Name 2", activity_log[2].user_display_name);
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[2].activity_metadata.triggering_user->email);
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesNoMessages) {
  CreateAndInitializeService();

  std::vector<collaboration_pb::Message> db_messages;

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesOneMessage) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<collaboration_pb::Message> db_messages;

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  db_messages.emplace_back(message);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  // Our service will need to also query for dirty dot messages for a group.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillOnce(Return(db_messages));
  messages = service_->GetMessages(std::nullopt);
  // Should become two PersistentMessages for the tab, and one for the tab
  // group.
  ASSERT_EQ(3u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
  EXPECT_EQ(CollaborationEvent::UNDEFINED, messages.at(2).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesTwoMessages) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<collaboration_pb::Message> db_messages;

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, now);
  db_messages.emplace_back(message1);
  db_messages.emplace_back(message2);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillRepeatedly(Return(db_messages));
  // Our service will need to also query for dirty dot messages for a group.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages));
  messages = service_->GetMessages(std::nullopt);
  // Should become two PersistentMessages for each tab, and one for the tab
  // group.
  ASSERT_EQ(5u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
  EXPECT_EQ(CollaborationEvent::UNDEFINED, messages.at(2).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
            messages.at(3).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(3).type);
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
            messages.at(4).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(4).type);
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesForGroup) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<collaboration_pb::Message> db_messages;

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  db_messages.emplace_back(message);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kAll))
      .WillOnce(Return(db_messages));
  // Our service will need to query for dirty dot messages for a group.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillOnce(Return(db_messages));

  // The query should come for the given tab group.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  EXPECT_CALL(*mock_tab_group_sync_service_,
              GetGroup(tab_groups::EitherGroupID(tab_group.saved_guid())))
      .WillOnce(Return(tab_group));

  messages = service_->GetMessagesForGroup(
      tab_groups::EitherGroupID(tab_group.saved_guid()), std::nullopt);
  // Should become two PersistentMessages for the tab, and one for the tab
  // group.
  ASSERT_EQ(3u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
  EXPECT_EQ(CollaborationEvent::UNDEFINED, messages.at(2).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesForTab) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<collaboration_pb::Message> db_messages;

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessages(DirtyType::kAll))
      .WillOnce(Return(db_messages));
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  db_messages.emplace_back(message);

  // The query should come for the given tab's tab group.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();

  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                    DirtyType::kAll))
      .WillRepeatedly(Return(db_messages.at(0)));
  // Our service will need to query for dirty dot messages for a group.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages));

  messages = service_->GetMessagesForTab(tab_groups::EitherTabID(tab1_sync_id),
                                         std::nullopt);
  // Should become two PersistentMessages for the tab, but nothing from the
  // group.
  ASSERT_EQ(2u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
}

TEST_F(MessagingBackendServiceImplTest, TestSelectedTabGetsRemoved) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Store all stored messages to this field
  collaboration_pb::Message db_message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&db_message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);

  tg_notifier_observer_->OnTabSelected(*tab1);

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      succes_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&succes_callback)));

  // Removing the currently selected tab should inform the delegate.
  tg_notifier_observer_->OnTabRemoved(*tab1);

  // We should have received a stored message about the removed tab.
  EXPECT_NE("", db_message.uuid());
  base::Uuid db_message_id = base::Uuid::ParseLowercase(db_message.uuid());

  EXPECT_EQ(CollaborationEvent::TAB_REMOVED, message.collaboration_event);
  EXPECT_EQ(InstantNotificationType::CONFLICT_TAB_REMOVED, message.type);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessage(db_message_id, DirtyType::kMessageOnly))
      .Times(1);
  std::move(succes_callback).Run(true);
}

TEST_F(MessagingBackendServiceImplTest, TestSelectedTabAtStartupGetsRemoved) {
  CreateService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);

  // This will make tab1 be selected at startup.
  EXPECT_CALL(*mock_tab_group_sync_service_, GetCurrentlySelectedTabID())
      .WillOnce(Return(std::pair(tab_group.saved_guid(), tab1_sync_id)));

  InitializeService();
  SetupInstantMessageDelegate();

  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      succes_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&succes_callback)));
  tg_notifier_observer_->OnTabRemoved(*tab1);

  EXPECT_EQ(CollaborationEvent::TAB_REMOVED, message.collaboration_event);
  EXPECT_EQ(InstantNotificationType::CONFLICT_TAB_REMOVED, message.type);
}

TEST_F(MessagingBackendServiceImplTest, TestUnselectedTabGetsRemoved) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Store all stored messages to this field
  collaboration_pb::Message db_message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&db_message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);

  tg_notifier_observer_->OnTabSelected(*tab1);

  // Removing tab 2 should not invoke the delegate.
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .Times(0);
  tg_notifier_observer_->OnTabRemoved(*tab2);
}

TEST_F(MessagingBackendServiceImplTest, TestTabGroupRemovedInstantMessage) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Store all stored messages to this field
  collaboration_pb::Message db_message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&db_message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      succes_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&succes_callback)));

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group);

  // We should have received a stored message about the removed tab group.
  EXPECT_NE("", db_message.uuid());
  base::Uuid db_message_id = base::Uuid::ParseLowercase(db_message.uuid());

  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED, message.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message.attribution.tab_group_metadata->sync_tab_group_id);

  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessage(db_message_id, DirtyType::kMessageOnly))
      .Times(1);
  std::move(succes_callback).Run(true);
}

TEST_F(MessagingBackendServiceImplTest, TestInstantMessageCallbackFails) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Store all stored messages to this field
  collaboration_pb::Message db_message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&db_message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      succes_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&succes_callback)));

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group);

  // If the callback provides success=false we should not clear the bit.
  EXPECT_CALL(*unowned_messaging_backend_store_, ClearDirtyMessage(_, _))
      .Times(0);
  std::move(succes_callback).Run(false);
}

TEST_F(MessagingBackendServiceImplTest, TestMemberAddedCreatesInstantMessage) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupData group_data;
  group_data.group_token.group_id = data_sharing::GroupId("my group id");
  data_sharing::GroupMember member1;
  member1.gaia_id = GaiaId("abc");
  member1.display_name = "Provided Diplay Name 1";
  member1.given_name = "Provided Given Name 1";
  group_data.members.emplace_back(member1);

  data_sharing::GroupMember member2;
  member2.gaia_id = GaiaId("def");
  member2.display_name = "Provided Display Name 2";
  member2.given_name = "";  // No given name available.
  group_data.members.emplace_back(member2);

  base::Time now = base::Time::Now();

  collaboration_pb::Message db_message;
  EXPECT_CALL(*unowned_messaging_backend_store_, AddMessage(_))
      .WillRepeatedly(SaveArg<0>(&db_message));

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(SaveArg<0>(&message));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(group_data.group_token.group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  ds_notifier_observer_->OnGroupMemberAdded(group_data, member2.gaia_id, now);

  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            message.collaboration_event);
  EXPECT_EQ(member2.gaia_id, message.attribution.affected_user->gaia_id);
  ASSERT_TRUE(message.attribution.tab_group_metadata);
  EXPECT_EQ(tab_group.saved_guid(),
            message.attribution.tab_group_metadata->sync_tab_group_id);
}

TEST_F(MessagingBackendServiceImplTest, TestTabSelectionClearsChipByDefault) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  PersistentMessage expected_message_chip;
  expected_message_chip.collaboration_event = CollaborationEvent::UNDEFINED;
  expected_message_chip.type = PersistentNotificationType::CHIP;
  PersistentMessage expected_message_dot;
  expected_message_dot.collaboration_event = CollaborationEvent::UNDEFINED;
  expected_message_dot.type = PersistentNotificationType::DIRTY_TAB;

  // Make sure there are still dirty tabs in the group.
  std::vector<collaboration_pb::Message> db_messages;
  collaboration_pb::Message db_message;
  db_messages.emplace_back(db_message);
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);

  PersistentMessage last_persistent_message_chip;
  PersistentMessage last_persistent_message_dot;

  // Select tab 1, it should clear the chip and dot for tab 1.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                      DirtyType::kDotAndChip))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  tg_notifier_observer_->OnTabSelected(*tab1);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Select tab 2, it should clear the chip and dot for tab 2.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                      DirtyType::kDotAndChip))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  tg_notifier_observer_->OnTabSelected(*tab2);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Selecting a tab outside a tab group should not do anything.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(_, _, _))
      .Times(0);
  tg_notifier_observer_->OnTabSelected(std::nullopt);
}

TEST_F(MessagingBackendServiceImplTest,
       TestTabSelectionClearsAfterUnselectBasedOnConfiguration) {
  configuration.clear_chip_on_tab_selection = false;
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  PersistentMessage expected_message_chip;
  expected_message_chip.collaboration_event = CollaborationEvent::UNDEFINED;
  expected_message_chip.type = PersistentNotificationType::CHIP;
  PersistentMessage expected_message_dot;
  expected_message_dot.collaboration_event = CollaborationEvent::UNDEFINED;
  expected_message_dot.type = PersistentNotificationType::DIRTY_TAB;

  // Make sure there are still dirty tabs in the group.
  std::vector<collaboration_pb::Message> db_messages;
  collaboration_pb::Message db_message;
  db_messages.emplace_back(db_message);
  EXPECT_CALL(*unowned_messaging_backend_store_,
              GetDirtyMessagesForGroup(collaboration_group_id, DirtyType::kDot))
      .WillRepeatedly(Return(db_messages));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);

  PersistentMessage last_persistent_message_chip;
  PersistentMessage last_persistent_message_dot;

  // Select tab 1, it should clear the dot for tab 1.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                      DirtyType::kDot))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  tg_notifier_observer_->OnTabSelected(*tab1);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Select tab 2, it should clear the chip for tab 1 and the dot for tab 2.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                      DirtyType::kChip))
      .Times(1);
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                      DirtyType::kDot))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  tg_notifier_observer_->OnTabSelected(*tab2);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Selecting a tab outside a tab group should clear the chip for tab 2.
  EXPECT_CALL(*unowned_messaging_backend_store_,
              ClearDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                      DirtyType::kChip))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  tg_notifier_observer_->OnTabSelected(std::nullopt);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
}

}  // namespace collaboration::messaging
