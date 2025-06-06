// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <ctime>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/instant_message_processor.h"
#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "components/collaboration/internal/messaging/storage/empty_messaging_backend_database.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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
    data_sharing::GroupId collaboration_group_id,
    base::Uuid tab_group_sync_id,
    std::vector<tab_groups::SavedTabGroupTab> tabs) {
  tab_groups::SavedTabGroup tab_group(u"Tab Group Title",
                                      tab_groups::TabGroupColorId::kOrange,
                                      tabs, std::nullopt, tab_group_sync_id);
  tab_group.SetCollaborationId(
      tab_groups::CollaborationId(collaboration_group_id.value()));
  return tab_group;
}

tab_groups::SavedTabGroup CreateSharedTabGroup(
    data_sharing::GroupId collaboration_group_id) {
  base::Uuid tab_group_sync_id = base::Uuid::GenerateRandomV4();

  std::vector<tab_groups::SavedTabGroupTab> tabs;
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    tab_group_sync_id, std::nullopt);
  tab_groups::SavedTabGroupTab tab2(GURL("https://www.example2.com/"), u"Tab 2",
                                    tab_group_sync_id, std::nullopt);
  tab1.SetLocalTabID(tab_groups::test::GenerateRandomTabID());
  tab2.SetLocalTabID(tab_groups::test::GenerateRandomTabID());
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
  MOCK_METHOD(void,
              HideInstantaneousMessage,
              (const std::set<base::Uuid>& message_ids),
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
    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        "test@foo.com", signin::ConsentLevel::kSync);

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

    auto messaging_backend_store = std::make_unique<MessagingBackendStoreImpl>(
        std::make_unique<EmptyMessagingBackendDatabase>());
    unowned_messaging_backend_store_ = messaging_backend_store.get();

    service_ = std::make_unique<MessagingBackendServiceImpl>(
        configuration, std::move(tab_group_change_notifier),
        std::move(data_sharing_change_notifier),
        std::move(messaging_backend_store),
        std::make_unique<InstantMessageProcessorImpl>(),
        mock_tab_group_sync_service_.get(), mock_data_sharing_service_.get(),
        identity_test_env_.identity_manager());
  }

  void InitializeService() {
    base::RunLoop run_loop;
    unowned_messaging_backend_store_->Initialize(base::BindOnce(
        [](base::RunLoop* run_loop, bool success) { run_loop->Quit(); },
        &run_loop));
    run_loop.Run();

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

  collaboration_pb::Message GetLastMessageFromDB() {
    return unowned_messaging_backend_store_->GetLastMessageForTesting().value();
  }

  bool HasLastMessageFromDB() {
    return unowned_messaging_backend_store_->GetLastMessageForTesting()
        .has_value();
  }

  void AddMessage(const collaboration_pb::Message& message) {
    unowned_messaging_backend_store_->AddMessage(message);
  }

  bool HasDirtyMessages() {
    return unowned_messaging_backend_store_->HasAnyDirtyMessages(
        DirtyType::kAll);
  }

  std::optional<collaboration_pb::Message> GetDirtyMessageForTab(
      const data_sharing::GroupId& collaboration_id,
      const base::Uuid& tab_id,
      DirtyType dirty_type) {
    return unowned_messaging_backend_store_->GetDirtyMessageForTab(
        collaboration_id, tab_id, dirty_type);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo account_info_;

  // Use default configuration unless we specify something else.
  MessagingBackendConfiguration configuration;
  std::unique_ptr<data_sharing::MockDataSharingService>
      mock_data_sharing_service_;
  std::unique_ptr<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  std::unique_ptr<MockInstantMessageDelegate> mock_instant_message_delegate_;
  MockPersistentMessageObserver mock_persistent_message_observer_;
  std::unique_ptr<MessagingBackendServiceImpl> service_;
  raw_ptr<MockTabGroupChangeNotifier> unowned_tab_group_change_notifier_;
  raw_ptr<MockDataSharingChangeNotifier> unowned_data_sharing_change_notifier_;
  raw_ptr<MessagingBackendStoreImpl> unowned_messaging_backend_store_;
  raw_ptr<TabGroupChangeNotifier::Observer> tg_notifier_observer_;
  raw_ptr<DataSharingChangeNotifier::Observer> ds_notifier_observer_;
};

TEST_F(MessagingBackendServiceImplTest, TestInitialization) {
  CreateService();
  base::RunLoop run_loop;
  EXPECT_FALSE(service_->IsInitialized());
  unowned_messaging_backend_store_->Initialize(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) { run_loop->Quit(); },
      &run_loop));
  run_loop.Run();
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

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(group_data.group_token.group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Current given name should be first priority.
  base::Time time = base::Time::Now();
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member1.gaia_id, time);
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, group_id.value(),
                           collaboration_pb::COLLABORATION_MEMBER_ADDED,
                           DirtyType::kMessageOnly, time.ToTimeT());
  EXPECT_EQ(member1.gaia_id, GaiaId(message.affected_user_gaia_id()));
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogWithNoEvents) {
  CreateAndInitializeService();
  ActivityLogQueryParams params;
  params.collaboration_id = data_sharing::GroupId("my group id");
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  EXPECT_EQ(0u, activity_log.size());
}

TEST_F(MessagingBackendServiceImplTest, TestTabActivityLogWithNoEvents) {
  CreateAndInitializeService();
  ActivityLogQueryParams params;
  params.collaboration_id = data_sharing::GroupId("my group id");
  params.local_tab_id = tab_groups::LocalTabID();
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  EXPECT_EQ(0u, activity_log.size());
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogAcceptsMaxLength) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  base::Time now = base::Time::Now();
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED, DirtyType::kNone,
      now + base::Seconds(4));
  message1.set_affected_user_gaia_id("gaia_1");
  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_REMOVED,
      DirtyType::kNone, now + base::Seconds(3));
  message2.set_affected_user_gaia_id("gaia_2");
  collaboration_pb::Message message3 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kNone, now + base::Seconds(2));
  message3.set_triggering_user_gaia_id("gaia_1");
  // COLLABORATION_ADDED should never be returned.
  collaboration_pb::Message message4 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::COLLABORATION_ADDED,
      DirtyType::kNone, now + base::Seconds(1));
  message4.set_triggering_user_gaia_id("gaia_1");

  AddMessage(message1);
  AddMessage(message2);
  AddMessage(message3);
  AddMessage(message4);

  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_1"), "gaia1@gmail.com",
                                     "Display Name", "Given Name 1")));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_2"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_2"), "gaia2@gmail.com",
                                     "Display Name 2", "Given Name 2")));

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
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED, DirtyType::kNone,
      now);
  message1.set_affected_user_gaia_id("gaia_1");

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_REMOVED,
      DirtyType::kNone, now);
  message2.set_affected_user_gaia_id("gaia_2");

  AddMessage(message1);
  AddMessage(message2);

  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_1"), "gaia1@gmail.com",
                                     "Display Name 1", "Given Name 1")));
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_2"))))
      .WillRepeatedly(Return(CreatePartialMember(
          GaiaId("gaia_2"), "gaia2@gmail.com", "Display Name 2", "")));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(2u, activity_log.size());
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_REMOVED,
            activity_log[1].collaboration_event);

  EXPECT_EQ(u"Given Name 1 joined the group", activity_log[0].title_text);
  EXPECT_EQ(u"gaia1@gmail.com", activity_log[0].description_text);
  EXPECT_EQ(u"Display Name 2 left the group", activity_log[1].title_text);
  EXPECT_EQ(u"gaia2@gmail.com", activity_log[1].description_text);
  // We should also fill in the MessageAttribution.
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[1].activity_metadata.affected_user->email);
}

TEST_F(MessagingBackendServiceImplTest, TestTabActivityLogCollaborationEvents) {
  CreateAndInitializeService();

  // Create a saved group with a 2 tabs. Only 1 message is stored per tab.
  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();

  base::Time now = base::Time::Now();
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED, DirtyType::kNone,
      now + base::Seconds(4));
  message1.set_affected_user_gaia_id("gaia_1");
  message1.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kNone, now + base::Seconds(3));
  message2.set_triggering_user_gaia_id("gaia_1");
  message2.mutable_tab_data()->set_sync_tab_id(
      tab1_sync_id.AsLowercaseString());
  message2.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  collaboration_pb::Message message3 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kNone, now + base::Seconds(2));
  message3.mutable_tab_data()->set_sync_tab_id(
      tab2_sync_id.AsLowercaseString());
  message3.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());

  AddMessage(message1);
  AddMessage(message2);
  AddMessage(message3);

  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_1"))))
      .WillRepeatedly(
          Return(CreatePartialMember(GaiaId("gaia_1"), "gaia1@gmail.com",
                                     "Display Name", "Given Name 1")));

  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  params.result_length = 1;
  params.local_tab_id = tab_group.GetTab(tab1_sync_id)->local_tab_id().value();

  std::vector<ActivityLogItem> tab1_activity_log =
      service_->GetActivityLog(params);
  ASSERT_EQ(1u, tab1_activity_log.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED,
            tab1_activity_log[0].collaboration_event);
  EXPECT_EQ(u"Given Name 1 added this tab", tab1_activity_log[0].title_text);
  EXPECT_EQ(u"example.com", tab1_activity_log[0].description_text);

  params.local_tab_id = tab_group.GetTab(tab2_sync_id)->local_tab_id().value();
  std::vector<ActivityLogItem> tab2_activity_log =
      service_->GetActivityLog(params);
  ASSERT_EQ(1u, tab2_activity_log.size());
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
            tab2_activity_log[0].collaboration_event);
  EXPECT_EQ(u"Deleted account changed this tab",
            tab2_activity_log[0].title_text);
  EXPECT_EQ(u"example2.com", tab2_activity_log[0].description_text);
}

TEST_F(MessagingBackendServiceImplTest, TestStoringTabGroupEventsFromRemote) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetCreatedByAttribution(gaia1);
  tab_group.SetUpdatedByAttribution(gaia2);

  tg_notifier_observer_->OnTabGroupAdded(tab_group,
                                         tab_groups::TriggerSource::REMOTE);
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_ADDED, DirtyType::kNone,
                           now.ToTimeT());
  EXPECT_EQ(gaia1, GaiaId(message.triggering_user_gaia_id()));

  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_REMOVED,
                           DirtyType::kTombstonedAndInstantMessage,
                           now.ToTimeT());
  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));

  tg_notifier_observer_->OnTabGroupNameUpdated(
      tab_group, tab_groups::TriggerSource::REMOTE);
  message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_NAME_UPDATED,
                           DirtyType::kNone, now.ToTimeT());
  EXPECT_EQ(gaia2, GaiaId(GetLastMessageFromDB().triggering_user_gaia_id()));

  tg_notifier_observer_->OnTabGroupColorUpdated(
      tab_group, tab_groups::TriggerSource::REMOTE);
  message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_GROUP_COLOR_UPDATED,
                           DirtyType::kNone, now.ToTimeT());
  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
}

TEST_F(MessagingBackendServiceImplTest, TestStoringTabGroupEventsFromLocal) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetCreatedByAttribution(gaia1);
  tab_group.SetUpdatedByAttribution(gaia2);

  // We should never add a message for any of the local tab group events.
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  tg_notifier_observer_->OnTabGroupAdded(tab_group,
                                         tab_groups::TriggerSource::LOCAL);
  tg_notifier_observer_->OnTabGroupNameUpdated(
      tab_group, tab_groups::TriggerSource::LOCAL);
  tg_notifier_observer_->OnTabGroupColorUpdated(
      tab_group, tab_groups::TriggerSource::LOCAL);
  EXPECT_FALSE(HasLastMessageFromDB());

  auto message = CreateStoredMessage(collaboration_group_id,
                                     collaboration_pb::EventType::TAB_ADDED,
                                     DirtyType::kDot, base::Time::Now());
  AddMessage(message);
  EXPECT_TRUE(
      unowned_messaging_backend_store_->HasAnyDirtyMessages(DirtyType::kDot));

  // Removing a tab group should remove all messages for the group from the DB.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::LOCAL);
  EXPECT_FALSE(
      unowned_messaging_backend_store_->HasAnyDirtyMessages(DirtyType::kDot));
}

TEST_F(MessagingBackendServiceImplTest, TestActivityLogTabGroupEvents) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  GaiaId gaia1("gaia_1");
  GaiaId gaia2("gaia_2");

  base::Time now = base::Time::Now();
  // Adding and removing tab groups should not be part of activity log.
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_GROUP_ADDED,
      DirtyType::kNone, now);
  message1.set_triggering_user_gaia_id("gaia_1");
  AddMessage(message1);

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_GROUP_REMOVED,
      DirtyType::kNone, now);
  message2.set_triggering_user_gaia_id("gaia_2");
  AddMessage(message2);

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
  AddMessage(message3);

  collaboration_pb::Message message4 =
      CreateStoredMessage(collaboration_group_id,
                          collaboration_pb::EventType::TAB_GROUP_COLOR_UPDATED,
                          DirtyType::kNone, now);
  message4.mutable_tab_group_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  message4.set_triggering_user_gaia_id("gaia_2");
  AddMessage(message4);

  // Add support for looking up GAIA IDs.
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

  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Query for all itemms, which should only be name and color updates.
  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(2u, activity_log.size());

  EXPECT_EQ(CollaborationEvent::TAB_GROUP_NAME_UPDATED,
            activity_log[0].collaboration_event);
  EXPECT_EQ(u"Given Name 2 changed the group name", activity_log[0].title_text);
  EXPECT_EQ(u"Tab Group Title", activity_log[0].description_text);
  EXPECT_EQ(u"Just now", activity_log[0].time_delta_text);

  EXPECT_EQ(CollaborationEvent::TAB_GROUP_COLOR_UPDATED,
            activity_log[1].collaboration_event);
  EXPECT_EQ(u"Given Name 2 changed the group color",
            activity_log[1].title_text);
  EXPECT_EQ(u"", activity_log[1].description_text);
  EXPECT_EQ(u"Just now", activity_log[1].time_delta_text);
}

TEST_F(MessagingBackendServiceImplTest, TestReceivingTabEventsFromSync) {
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
  auto db_message = CreateStoredMessage(collaboration_group_id,
                                        collaboration_pb::EventType::TAB_ADDED,
                                        DirtyType::kNone, base::Time::Now());
  AddMessage(db_message);

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a tab to check for addition from sync.
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  // Make creation and update GaiaId unique.
  tab1->SetCreatedByAttribution(gaia1);
  tab1->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab1->SetUpdateTime(now + base::Seconds(1));

  // Create a second tab to check for update from sync.
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  // Make creation and update GaiaId unique.
  tab2->SetCreatedByAttribution(gaia1);
  tab2->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab2->SetUpdateTime(now + base::Seconds(1));

  // Create a third tab to check for removal from sync.
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

  // SCENARIO 1: Add a new tab from sync.
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
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabAdded(*tab1, tab_groups::TriggerSource::REMOTE);
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_ADDED, DirtyType::kDotAndChip,
                           now.ToTimeT());
  EXPECT_EQ(gaia1, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab1->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab1->creation_time().ToTimeT(), message.event_timestamp());
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_chip.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_dot.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);
  EXPECT_FALSE(
      last_persistent_message_dot_tab_group.attribution.id.has_value());

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
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabUpdated(*tab2, *tab2,
                                      tab_groups::TriggerSource::REMOTE, false);
  message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_UPDATED,
                           DirtyType::kDotAndChip, now.ToTimeT());
  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab2->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab2->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab2->update_time().ToTimeT(), message.event_timestamp());
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_chip.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_dot.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);
  EXPECT_FALSE(
      last_persistent_message_dot_tab_group.attribution.id.has_value());

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
  unowned_messaging_backend_store_->ClearDirtyTabMessagesForGroup(
      collaboration_group_id);
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dot_tab_group)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot_tab_group));
  tg_notifier_observer_->OnTabRemoved(tab3, tab_groups::TriggerSource::REMOTE,
                                      false);
  message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, collaboration_group_id.value(),
                           collaboration_pb::TAB_REMOVED,
                           DirtyType::kTombstoned, now.ToTimeT());
  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab3.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab3.update_time().ToTimeT(), message.event_timestamp());
  EXPECT_EQ(tab3.url().spec(), message.tab_data().last_url());
  EXPECT_EQ(tab3_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_chip.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_chip.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab3_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message_dot.attribution
                                        .tab_group_metadata->sync_tab_group_id);
  EXPECT_EQ(message.uuid(),
            last_persistent_message_dot.attribution.id->AsLowercaseString());
  EXPECT_EQ(tab_group.saved_guid(),
            last_persistent_message_dot_tab_group.attribution
                .tab_group_metadata->sync_tab_group_id);
  EXPECT_FALSE(
      last_persistent_message_dot_tab_group.attribution.id.has_value());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabAddedFromLocal) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a tab to check for addition from local.
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  // Make creation and update GaiaId unique.
  tab1->SetCreatedByAttribution(gaia1);
  tab1->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab1->SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Add a new tab locally.
  // It should add a messages for this tab to the DB.
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabAdded(*tab1, tab_groups::TriggerSource::LOCAL);
  EXPECT_TRUE(HasLastMessageFromDB());

  // Verify that a message is created for local tab addition.
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id", collaboration_pb::TAB_ADDED,
                           DirtyType::kNone, now.ToTimeT());

  EXPECT_EQ(gaia1, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab1->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(now.ToTimeT(), message.event_timestamp());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabAddedFromRemote_AlreadySeen) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    group_guid, std::nullopt);
  tab1.SetLastSeenTime(base::Time::Now() + base::Seconds(10));
  tab1.SetNavigationTime(base::Time::Now());
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id, group_guid, {tab1});

  // Create a tab to check for addition from local.
  base::Uuid tab1_sync_id = tab1.saved_tab_guid();
  // Make creation and update GaiaId unique.
  tab1.SetCreatedByAttribution(gaia1);
  tab1.SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab1.SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Add a new tab from remote which has been seen already on another device.
  // It should add a message for this tab to the DB, but not result in
  // persistent message notification.
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabAdded(tab1, tab_groups::TriggerSource::REMOTE);
  EXPECT_TRUE(HasLastMessageFromDB());

  // Verify that a message is created for local tab addition.
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id", collaboration_pb::TAB_ADDED,
                           DirtyType::kNone, now.ToTimeT());

  EXPECT_EQ(gaia1, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab1.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(now.ToTimeT(), message.event_timestamp());
}

TEST_F(MessagingBackendServiceImplTest,
       TestOnTabUpdatedFromRemote_AlreadySeen) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    group_guid, std::nullopt);
  tab1.SetLastSeenTime(base::Time::Now() + base::Seconds(10));
  tab1.SetNavigationTime(base::Time::Now());
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id, group_guid, {tab1});

  base::Uuid tab1_sync_id = tab1.saved_tab_guid();

  // Create dirty message for tab 1.
  collaboration_pb::Message db_message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDot, base::Time::Now());
  db_message.mutable_tab_data()->set_sync_tab_id(
      tab1_sync_id.AsLowercaseString());
  db_message.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message);

  // Create a tab to check for addition from remote.
  // Make creation and update GaiaId unique.
  tab1.SetCreatedByAttribution(gaia1);
  tab1.SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab1.SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Add a new tab from remote which has been seen already on another device.
  // It should add a message for this tab to the DB, but not result in
  // persistent message notification.
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  tg_notifier_observer_->OnTabUpdated(tab1, tab1,
                                      tab_groups::TriggerSource::REMOTE, false);

  // Verify that a message is created for remote tab addition.
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::TAB_UPDATED, DirtyType::kNone,
                           now.ToTimeT());

  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab1.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabUpdatedFromLocal) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a second tab to check for update from local.
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  // Make creation and update GaiaId unique.
  tab2->SetCreatedByAttribution(gaia1);
  tab2->SetUpdatedByAttribution(gaia2);
  // Make creation and update time unique.
  tab2->SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Create dirty message for tab 2.
  collaboration_pb::Message db_message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDot, base::Time::Now());
  db_message.mutable_tab_data()->set_sync_tab_id(
      tab2_sync_id.AsLowercaseString());
  db_message.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message);

  // Update a tab locally.
  // Should clear any dirty message for the tab and hide any persistent message
  // already showing.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(testing::AtLeast(1));

  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kDot));
  tg_notifier_observer_->OnTabUpdated(*tab2, *tab2,
                                      tab_groups::TriggerSource::LOCAL, false);
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                     DirtyType::kDot));

  // Verify that a message is created for local tab update.
  auto message = GetLastMessageFromDB();
  EXPECT_NE(db_message.uuid(), message.uuid());
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::TAB_UPDATED, DirtyType::kNone,
                           now.ToTimeT());

  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab2->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab2->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabRemovedFromLocal) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  GaiaId gaia1("abc");
  GaiaId gaia2("def");

  base::Time now = base::Time::Now();
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a third tab to check for removal from sync.
  base::Uuid tab3_sync_id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab3(GURL("https://www.example3.com/"), u"Tab 3",
                                    tab_group.saved_guid(), std::nullopt,
                                    tab3_sync_id);
  tab3.SetCreatedByAttribution(gaia1);
  tab3.SetUpdatedByAttribution(gaia2);

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Remove a tab locally.
  // Should clear any dirty message for the tab and hide any persistent or
  // instant message already showing.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(3);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabRemoved(tab3, tab_groups::TriggerSource::LOCAL,
                                      false);

  // Verify that a message is created for local tab removal.
  EXPECT_TRUE(HasLastMessageFromDB());
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::TAB_REMOVED, DirtyType::kNone,
                           now.ToTimeT());

  EXPECT_EQ(gaia2, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab3.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(now.ToTimeT(), message.event_timestamp());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabAddedFromRemoteByYourself) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a tab to check for addition from local.
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);
  // Make creation and update GaiaId unique.
  tab1->SetCreatedByAttribution(account_info_.gaia);
  tab1->SetUpdatedByAttribution(account_info_.gaia);
  // Make creation and update time unique.
  tab1->SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Add a new tab locally.
  // It should add a messages for this tab to the DB.
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabAdded(*tab1, tab_groups::TriggerSource::REMOTE);
  EXPECT_TRUE(HasLastMessageFromDB());

  // Verify that a message is created for local tab addition.
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id", collaboration_pb::TAB_ADDED,
                           DirtyType::kNone, now.ToTimeT());

  EXPECT_EQ(account_info_.gaia, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab1->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab1->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(now.ToTimeT(), message.event_timestamp());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabUpdatedFromRemoteByYourself) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a second tab to check for update from local.
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  // Make creation and update GaiaId unique.
  tab2->SetCreatedByAttribution(account_info_.gaia);
  tab2->SetUpdatedByAttribution(account_info_.gaia);
  // Make creation and update time unique.
  tab2->SetUpdateTime(now + base::Seconds(1));

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Create dirty message for tab 2.
  collaboration_pb::Message db_message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDot, base::Time::Now());
  db_message.mutable_tab_data()->set_sync_tab_id(
      tab2_sync_id.AsLowercaseString());
  db_message.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message);

  // Update the selected tab from remote by the same user.
  // Should clear any dirty message for the tab and hide any persistent message
  // already showing. Also should not trigger instant message.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_instant_message_delegate_, DisplayInstantaneousMessage)
      .Times(0);

  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kDot));
  tg_notifier_observer_->OnTabUpdated(*tab2, *tab2,
                                      tab_groups::TriggerSource::REMOTE, true);
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                     DirtyType::kDot));

  // Verify that a message is created for local tab update.
  auto message = GetLastMessageFromDB();
  EXPECT_NE(db_message.uuid(), message.uuid());
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::TAB_UPDATED, DirtyType::kNone,
                           now.ToTimeT());

  EXPECT_EQ(account_info_.gaia, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab2->saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab2->saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
}

TEST_F(MessagingBackendServiceImplTest, TestOnTabRemovedFromRemoteByYourself) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  base::Time now = base::Time::Now();
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);

  // Create a third tab to check for removal from sync.
  base::Uuid tab3_sync_id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab3(GURL("https://www.example3.com/"), u"Tab 3",
                                    tab_group.saved_guid(), std::nullopt,
                                    tab3_sync_id);
  tab3.SetCreatedByAttribution(account_info_.gaia);
  tab3.SetUpdatedByAttribution(account_info_.gaia);

  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Remove the selected tab from remote by the same user.
  // Should clear any dirty message for the tab and hide any persistent or
  // instant message already showing.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(3);
  EXPECT_CALL(*mock_instant_message_delegate_, DisplayInstantaneousMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabRemoved(tab3, tab_groups::TriggerSource::REMOTE,
                                      true);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify that a message is created for the tab removal even for the same
  // user.
  EXPECT_TRUE(HasLastMessageFromDB());
  auto message = GetLastMessageFromDB();
  VerifyGenericMessageData(message, "my group id",
                           collaboration_pb::TAB_REMOVED, DirtyType::kNone,
                           now.ToTimeT());

  EXPECT_EQ(account_info_.gaia, GaiaId(message.triggering_user_gaia_id()));
  EXPECT_EQ(tab3.saved_tab_guid().AsLowercaseString(),
            message.tab_data().sync_tab_id());
  EXPECT_EQ(tab3.saved_group_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(tab_group.saved_guid().AsLowercaseString(),
            message.tab_data().sync_tab_group_id());
  EXPECT_EQ(now.ToTimeT(), message.event_timestamp());
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

  // Create a fourth tab to check for unknown user.
  tab_groups::SavedTabGroupTab tab4(GURL("https://www.example4.com/"), u"Tab 4",
                                    tab_group.saved_guid(), std::nullopt);

  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now - base::Minutes(5));
  message1.set_triggering_user_gaia_id("gaia_1");
  message1.mutable_tab_data()->set_sync_tab_id(
      tab1->saved_tab_guid().AsLowercaseString());
  message1.mutable_tab_data()->set_sync_tab_group_id(
      tab1->saved_group_guid().AsLowercaseString());
  AddMessage(message1);

  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, now - base::Hours(10));
  message2.set_triggering_user_gaia_id("gaia_2");
  message2.mutable_tab_data()->set_sync_tab_id(
      tab2->saved_tab_guid().AsLowercaseString());
  message2.mutable_tab_data()->set_sync_tab_group_id(
      tab2->saved_group_guid().AsLowercaseString());
  AddMessage(message2);

  collaboration_pb::Message message3 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_REMOVED,
      DirtyType::kNone, now - base::Days(20));
  message3.set_triggering_user_gaia_id("gaia_2");
  message3.mutable_tab_data()->set_sync_tab_id(
      tab3.saved_tab_guid().AsLowercaseString());
  message3.mutable_tab_data()->set_sync_tab_group_id(
      tab3.saved_group_guid().AsLowercaseString());
  message3.mutable_tab_data()->set_last_url(tab3.url().spec());
  AddMessage(message3);

  collaboration_pb::Message message4 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_REMOVED,
      DirtyType::kNone, now - base::Days(25));
  message4.set_triggering_user_gaia_id("gaia_3");
  message4.mutable_tab_data()->set_sync_tab_id(
      tab4.saved_tab_guid().AsLowercaseString());
  message4.mutable_tab_data()->set_sync_tab_group_id(
      tab4.saved_group_guid().AsLowercaseString());
  message4.mutable_tab_data()->set_last_url(tab4.url().spec());
  AddMessage(message4);

  // Add support for looking up GAIA IDs.
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
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroupMember(Eq(collaboration_group_id),
                                            Eq(GaiaId("gaia_3"))))
      .WillRepeatedly(Return(std::nullopt));

  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  ActivityLogQueryParams params;
  params.collaboration_id = collaboration_group_id;
  std::vector<ActivityLogItem> activity_log = service_->GetActivityLog(params);
  ASSERT_EQ(4u, activity_log.size());

  // Verify tab 1.
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, activity_log[0].collaboration_event);
  EXPECT_EQ(tab1->url().spec(),
            *activity_log[0].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example.com", activity_log[0].description_text);
  EXPECT_EQ(u"Given Name 1 added a tab", activity_log[0].title_text);
  EXPECT_EQ(u"5m ago", activity_log[0].time_delta_text);
  EXPECT_EQ("gaia1@gmail.com",
            activity_log[0].activity_metadata.triggering_user->email);

  // Verify tab 2.
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
            activity_log[1].collaboration_event);
  EXPECT_EQ(tab2->url().spec(),
            *activity_log[1].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example2.com", activity_log[1].description_text);
  EXPECT_EQ(u"Given Name 2 changed a tab", activity_log[1].title_text);
  EXPECT_EQ(u"10h ago", activity_log[1].time_delta_text);
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[1].activity_metadata.triggering_user->email);

  // Verify tab 3.
  EXPECT_EQ(CollaborationEvent::TAB_REMOVED,
            activity_log[2].collaboration_event);
  EXPECT_EQ(tab3.url().spec(),
            *activity_log[2].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example3.com", activity_log[2].description_text);
  EXPECT_EQ(u"Given Name 2 removed a tab", activity_log[2].title_text);
  EXPECT_EQ(u"20d ago", activity_log[2].time_delta_text);
  EXPECT_EQ("gaia2@gmail.com",
            activity_log[2].activity_metadata.triggering_user->email);

  // Verify tab 4.
  EXPECT_EQ(CollaborationEvent::TAB_REMOVED,
            activity_log[3].collaboration_event);
  EXPECT_EQ(tab4.url().spec(),
            *activity_log[3].activity_metadata.tab_metadata->last_known_url);
  EXPECT_EQ(u"example4.com", activity_log[3].description_text);
  EXPECT_EQ(u"Deleted account removed a tab", activity_log[3].title_text);
  EXPECT_EQ(u"25d ago", activity_log[3].time_delta_text);
  EXPECT_EQ(std::nullopt, activity_log[3].activity_metadata.triggering_user);
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesNoMessages) {
  CreateAndInitializeService();
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesOneMessage) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  AddMessage(message);

  // Our service will need to also query for dirty dot messages for a group.
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

  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  collaboration_pb::Message message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, now);
  AddMessage(message1);
  AddMessage(message2);

  messages = service_->GetMessages(std::nullopt);
  // Should become two PersistentMessages for each tab, and one for the tab
  // group.
  ASSERT_EQ(5u, messages.size());

  if (messages.at(0).attribution.id->AsLowercaseString() == message1.uuid()) {
    EXPECT_EQ(CollaborationEvent::TAB_ADDED,
              messages.at(0).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
    EXPECT_EQ(CollaborationEvent::TAB_ADDED,
              messages.at(1).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
    EXPECT_EQ(CollaborationEvent::UNDEFINED,
              messages.at(2).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);
    EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
              messages.at(3).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(3).type);
    EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
              messages.at(4).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(4).type);
  } else {
    EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
              messages.at(0).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
    EXPECT_EQ(CollaborationEvent::TAB_UPDATED,
              messages.at(1).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
    EXPECT_EQ(CollaborationEvent::UNDEFINED,
              messages.at(2).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);
    EXPECT_EQ(CollaborationEvent::TAB_ADDED,
              messages.at(3).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(3).type);
    EXPECT_EQ(CollaborationEvent::TAB_ADDED,
              messages.at(4).collaboration_event);
    EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(4).type);
  }
}

TEST_F(MessagingBackendServiceImplTest,
       TestGetMessagesForGroupAndClearDirtyTabMessagesForGroup) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  // Start with no messages in the DB.
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  // Add a tab message to the DB.
  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  AddMessage(message);

  // Setup a tab group in TabGroupSyncService associated with the collaboration.
  // It's necessary because messaging backend will consult TabGroupSyncService
  // for the group info.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  EXPECT_CALL(*mock_tab_group_sync_service_,
              GetGroup(tab_groups::EitherGroupID(tab_group.saved_guid())))
      .WillRepeatedly(Return(tab_group));
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Query service for the messages of the group. It should have two persistent
  // messages for the tab (chip and dirty dot), and one for the tab group (dirty
  // dot).
  messages = service_->GetMessagesForGroup(
      tab_groups::EitherGroupID(tab_group.saved_guid()), std::nullopt);
  ASSERT_EQ(3u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
  EXPECT_EQ(CollaborationEvent::UNDEFINED, messages.at(2).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);

  // ClearDirtyTabMessagesForGroup should result in hiding the persistent
  // messages that are already showing.
  // 1. Hide two persistent messages for the tab (chip and dirty dot).
  // 2. Hide one persistent message for tab group dirty dot.

  PersistentMessage message1, message2, message3;
  testing::InSequence sequence;
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message1));  // Capture the first message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message2));  // Capture the second message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message3));  // Capture the third message

  // Invoke the service API.
  service_->ClearDirtyTabMessagesForGroup(collaboration_group_id);

  // Verify the messages that were hidden.
  // Chip message of tab.
  EXPECT_TRUE(message1.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message1.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, message1.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message1.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::CHIP, message1.type);

  // Dirty dot of tab.
  EXPECT_TRUE(message2.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message2.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, message2.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message2.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, message2.type);

  // Dirty dot of tab group.
  EXPECT_FALSE(message3.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message3.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::UNDEFINED, message3.collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, message3.type);
  EXPECT_EQ(tab_group.saved_guid(),
            message3.attribution.tab_group_metadata->sync_tab_group_id.value());
}

TEST_F(MessagingBackendServiceImplTest,
       TestTabGroupRemovalWillHideExistingMessagesFromUi) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  // Start with no messages in the DB.
  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  // Add a tab message to the DB.
  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  AddMessage(message);

  collaboration_pb::Message instant_message_db = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_REMOVED,
      DirtyType::kMessageOnly, now);
  AddMessage(instant_message_db);

  // Setup a tab group in TabGroupSyncService associated with the collaboration.
  // It's necessary because messaging backend will consult TabGroupSyncService
  // for the group info.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  EXPECT_CALL(*mock_tab_group_sync_service_,
              GetGroup(tab_groups::EitherGroupID(tab_group.saved_guid())))
      .WillRepeatedly(Return(tab_group));
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Query service for the messages of the group. It should have two persistent
  // messages for the tab (chip and dirty dot), and one for the tab group (dirty
  // dot).
  messages = service_->GetMessagesForGroup(
      tab_groups::EitherGroupID(tab_group.saved_guid()), std::nullopt);
  ASSERT_EQ(3u, messages.size());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(0).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::CHIP, messages.at(0).type);
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, messages.at(1).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, messages.at(1).type);
  EXPECT_EQ(CollaborationEvent::UNDEFINED, messages.at(2).collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, messages.at(2).type);

  // OnTabGroupRemoved should result in hiding the persistent
  // messages that are already showing.
  // 1. Hide two persistent messages for the tab (chip and dirty dot).
  // 2. Hide one persistent message for tab group dirty dot.
  // 3. Hide all instant messages that it knows about.

  PersistentMessage message1, message2, message3;
  testing::InSequence sequence;
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message1));  // Capture the first message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message2));  // Capture the second message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message3));  // Capture the third message

  std::set<base::Uuid> instant_message_ids;
  EXPECT_CALL(*mock_instant_message_delegate_, HideInstantaneousMessage(_))
      .WillOnce(SaveArg<0>(&instant_message_ids));

  // Invoke the service API for tab group removal (e.g. unshare flow).
  service_->OnTabGroupRemoved(tab_group, tab_groups::TriggerSource::REMOTE);

  // Verify the messages that were hidden.
  // Chip message of tab.
  EXPECT_TRUE(message1.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message1.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, message1.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message1.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::CHIP, message1.type);

  // Dirty dot of tab.
  EXPECT_TRUE(message2.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message2.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::TAB_ADDED, message2.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message2.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, message2.type);

  // Dirty dot of tab group.
  EXPECT_FALSE(message3.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message3.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::UNDEFINED, message3.collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, message3.type);
  EXPECT_EQ(tab_group.saved_guid(),
            message3.attribution.tab_group_metadata->sync_tab_group_id.value());

  // We forcefully hide all messages that could have been instant messages.
  EXPECT_FALSE(instant_message_ids.empty());
  EXPECT_TRUE(
      instant_message_ids.contains(base::Uuid::ParseLowercase(message.uuid())));
  EXPECT_TRUE(instant_message_ids.contains(
      base::Uuid::ParseLowercase(instant_message_db.uuid())));
}

TEST_F(MessagingBackendServiceImplTest,
       TestClearPersistentMessage_SpecificType) {
  CreateAndInitializeService();

  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  auto message = CreateStoredMessage(collaboration_group_id,
                                     collaboration_pb::EventType::TAB_ADDED,
                                     DirtyType::kDot, base::Time::Now());
  message.set_uuid(uuid1.AsLowercaseString());
  AddMessage(message);

  EXPECT_TRUE(HasDirtyMessages());
  service_->ClearPersistentMessage(uuid1,
                                   PersistentNotificationType::DIRTY_TAB);
  EXPECT_FALSE(HasDirtyMessages());
}

TEST_F(MessagingBackendServiceImplTest, TestClearPersistentMessage_AllTypes) {
  CreateAndInitializeService();

  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  auto message = CreateStoredMessage(collaboration_group_id,
                                     collaboration_pb::EventType::TAB_ADDED,
                                     DirtyType::kDotAndChip, base::Time::Now());
  message.set_uuid(uuid1.AsLowercaseString());
  AddMessage(message);

  EXPECT_TRUE(HasDirtyMessages());
  service_->ClearPersistentMessage(uuid1, std::nullopt);
  EXPECT_FALSE(HasDirtyMessages());
}

TEST_F(MessagingBackendServiceImplTest, TestRemoveMessages) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  auto message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, base::Time::Now());
  message1.set_uuid(uuid1.AsLowercaseString());
  AddMessage(message1);
  base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  auto message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, base::Time::Now());
  message2.set_uuid(uuid2.AsLowercaseString());
  AddMessage(message2);

  service_->RemoveMessages({uuid1, uuid2});
  EXPECT_TRUE(unowned_messaging_backend_store_
                  ->GetRecentMessagesForGroup(collaboration_group_id)
                  .empty());
}

TEST_F(MessagingBackendServiceImplTest, TestSyncDisabled) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  auto message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, base::Time::Now());
  message1.set_uuid(uuid1.AsLowercaseString());
  AddMessage(message1);

  EXPECT_TRUE(HasDirtyMessages());
  tg_notifier_observer_->OnSyncDisabled();
  EXPECT_FALSE(HasDirtyMessages());
}

TEST_F(MessagingBackendServiceImplTest, TestGetMessagesForTab) {
  CreateAndInitializeService();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");
  base::Time now = base::Time::Now();

  std::vector<PersistentMessage> messages = service_->GetMessages(std::nullopt);
  EXPECT_EQ(0u, messages.size());

  // The query should come for the given tab's tab group.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();

  collaboration_pb::Message message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now);
  message.mutable_tab_data()->set_sync_tab_id(tab1_sync_id.AsLowercaseString());
  message.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(message);

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

TEST_F(MessagingBackendServiceImplTest, TestSelectedTabGetsUpdated) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();
  AddPersistentMessageObserver();

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

  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               true);

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));

  // Save the last invocation of DisplayPersistentMessage.
  PersistentMessage last_persistent_message;
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message));

  // Updating the currently selected tab should inform the delegate.
  tab_groups::SavedTabGroupTab old_tab1 = *tab1;
  old_tab1.SetURL(GURL("https://www.example3.com/"));
  tg_notifier_observer_->OnTabUpdated(old_tab1, *tab1,
                                      tab_groups::TriggerSource::REMOTE, true);
  task_environment_.FastForwardBy(base::Seconds(10));

  // We should have received a stored message about the updated tab.
  auto db_message = GetLastMessageFromDB();
  EXPECT_NE("", db_message.uuid());
  base::Uuid db_message_id = base::Uuid::ParseLowercase(db_message.uuid());
  EXPECT_EQ(db_message_id, message.attributions[0].id);

  // Verify that the dirty bit is chip only and no dot.
  EXPECT_FALSE(static_cast<int>(DirtyType::kDot) & db_message.dirty());
  EXPECT_TRUE(static_cast<int>(DirtyType::kChip) & db_message.dirty());

  // Verify persistent notification. There should be only a chip notification
  // and no tab group notification.
  EXPECT_EQ(PersistentNotificationType::CHIP, last_persistent_message.type);

  // Verify instant message.
  EXPECT_EQ(CollaborationEvent::TAB_UPDATED, message.collaboration_event);
  EXPECT_EQ(InstantNotificationType::UNDEFINED, message.type);
  EXPECT_EQ(1u, message.attributions.size());
  EXPECT_TRUE(message.attributions[0].tab_metadata.has_value());
  EXPECT_EQ(old_tab1.url().spec(),
            message.attributions[0].tab_metadata->previous_url);
  EXPECT_EQ(tab1->url().spec(),
            message.attributions[0].tab_metadata->last_known_url);

  std::move(success_callback).Run(true);
  EXPECT_FALSE(unowned_messaging_backend_store_->HasAnyDirtyMessages(
      DirtyType::kMessageOnly));
}

TEST_F(MessagingBackendServiceImplTest, TestSelectedTabGetsRemoved) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

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

  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               true);

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));

  // Removing the currently selected tab should inform the delegate.
  tg_notifier_observer_->OnTabRemoved(*tab1, tab_groups::TriggerSource::REMOTE,
                                      true);
  task_environment_.FastForwardBy(base::Seconds(10));

  // We should have received a stored message about the removed tab.
  auto db_message = GetLastMessageFromDB();
  EXPECT_NE("", db_message.uuid());
  base::Uuid db_message_id = base::Uuid::ParseLowercase(db_message.uuid());
  EXPECT_EQ(db_message_id, message.attributions[0].id);

  EXPECT_EQ(CollaborationEvent::TAB_REMOVED, message.collaboration_event);
  EXPECT_EQ(InstantNotificationType::CONFLICT_TAB_REMOVED, message.type);

  std::move(success_callback).Run(true);
  EXPECT_FALSE(unowned_messaging_backend_store_->HasAnyDirtyMessages(
      DirtyType::kMessageOnly));
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

  InitializeService();
  SetupInstantMessageDelegate();

  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));
  tg_notifier_observer_->OnTabRemoved(*tab1, tab_groups::TriggerSource::REMOTE,
                                      true);
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(CollaborationEvent::TAB_REMOVED, message.collaboration_event);
  EXPECT_EQ(InstantNotificationType::CONFLICT_TAB_REMOVED, message.type);
}

TEST_F(MessagingBackendServiceImplTest, TestUnselectedTabGetsRemoved) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

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

  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               true);

  // Removing tab 2 should not invoke the delegate.
  base::Uuid tab2_sync_id = tab_group.saved_tabs().at(1).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab2 = tab_group.GetTab(tab2_sync_id);
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .Times(0);
  tg_notifier_observer_->OnTabRemoved(*tab2, tab_groups::TriggerSource::REMOTE,
                                      false);
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(MessagingBackendServiceImplTest, TestTabGroupRemovedInstantMessage) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

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
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));
  // Save the last invocation of DisplayPersistentMessage.
  PersistentMessage last_persistent_message;
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message));

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify persistent notification.
  EXPECT_EQ(PersistentNotificationType::TOMBSTONED,
            last_persistent_message.type);
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED,
            last_persistent_message.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(), last_persistent_message.attribution
                                        .tab_group_metadata->sync_tab_group_id);

  // We should have received a stored message about the removed tab group.
  auto db_message = GetLastMessageFromDB();
  EXPECT_NE("", db_message.uuid());
  base::Uuid db_message_id = base::Uuid::ParseLowercase(db_message.uuid());
  EXPECT_EQ(db_message_id, message.attributions[0].id);

  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED, message.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message.attributions[0].tab_group_metadata->sync_tab_group_id);
  EXPECT_TRUE(static_cast<int>(DirtyType::kTombstoned) & db_message.dirty());
  EXPECT_TRUE(static_cast<int>(DirtyType::kMessageOnly) & db_message.dirty());
  EXPECT_EQ("Tab Group Title",
            message.attributions[0].tab_group_metadata->last_known_title);

  std::move(success_callback).Run(true);
  EXPECT_FALSE(unowned_messaging_backend_store_->HasAnyDirtyMessages(
      DirtyType::kMessageOnly));
}

TEST_F(MessagingBackendServiceImplTest, TestTabGroupFallbackTitleNTabs) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  tab_group.SetTitle(std::u16string());
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Save the last invocation of calls to the InstantMessageDelegate.
  InstantMessage message;
  MessagingBackendService::InstantMessageDelegate::SuccessCallback
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  task_environment_.FastForwardBy(base::Seconds(10));

  // Verify tab group empty title.
  EXPECT_EQ(CollaborationEvent::TAB_GROUP_REMOVED, message.collaboration_event);
  EXPECT_EQ("2 tabs",
            message.attributions[0].tab_group_metadata->last_known_title);
}

TEST_F(MessagingBackendServiceImplTest,
       LeavingCollaborationDoesNotResultInNotifications) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Mimic that the user has just left the group.
  EXPECT_CALL(*mock_data_sharing_service_,
              IsLeavingOrDeletingGroup(Eq(collaboration_group_id)))
      .WillRepeatedly(Return(true));

  // Remove the group which results due to leaving the collaboration. There
  // should be no instant, persistent or db message.
  EXPECT_CALL(*mock_instant_message_delegate_, DisplayInstantaneousMessage)
      .Times(0);
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(MessagingBackendServiceImplTest,
       TestUnshareDoesNotResultInNotifications) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  data_sharing::GroupData group_data;
  group_data.group_token.group_id = collaboration_group_id;
  data_sharing::GroupMember member1;
  member1.gaia_id = account_info_.gaia;
  member1.display_name = account_info_.full_name;
  member1.given_name = account_info_.given_name;
  member1.role = data_sharing::MemberRole::kOwner;
  group_data.members.emplace_back(member1);
  EXPECT_CALL(*mock_data_sharing_service_,
              GetPossiblyRemovedGroup(Eq(collaboration_group_id)))
      .WillRepeatedly(Return(group_data));

  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));

  // Save the last invocation of calls to the InstantMessageDelegate.
  EXPECT_CALL(*mock_instant_message_delegate_, DisplayInstantaneousMessage)
      .Times(0);
  EXPECT_CALL(mock_persistent_message_observer_, DisplayPersistentMessage)
      .Times(0);
  EXPECT_FALSE(HasLastMessageFromDB());

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(MessagingBackendServiceImplTest, TestInstantMessageCallbackFails) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

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
      success_callback;
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .WillRepeatedly(
          DoAll(SaveArg<0>(&message), MoveArg<1>(&success_callback)));

  // Removing the tab group should inform the delegate.
  tg_notifier_observer_->OnTabGroupRemoved(tab_group,
                                           tab_groups::TriggerSource::REMOTE);
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(unowned_messaging_backend_store_->HasAnyDirtyMessages(
      DirtyType::kMessageOnly));
  std::move(success_callback).Run(false);
  EXPECT_TRUE(unowned_messaging_backend_store_->HasAnyDirtyMessages(
      DirtyType::kMessageOnly));
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

  EXPECT_CALL(*mock_data_sharing_service_,
              ReadGroup(Eq(group_data.group_token.group_id)))
      .WillRepeatedly(Return(group_data));

  ds_notifier_observer_->OnGroupMemberAdded(group_data, member2.gaia_id, now);
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(CollaborationEvent::COLLABORATION_MEMBER_ADDED,
            message.collaboration_event);
  const auto& attribution = message.attributions[0];
  EXPECT_EQ(member2.gaia_id, attribution.affected_user->gaia_id);
  ASSERT_TRUE(message.attributions[0].tab_group_metadata);
  EXPECT_EQ(tab_group.saved_guid(),
            attribution.tab_group_metadata->sync_tab_group_id);
}

TEST_F(MessagingBackendServiceImplTest, TestMemberAddedOrRemovedIsOwner) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupData group_data;
  group_data.group_token.group_id = data_sharing::GroupId("my group id");
  data_sharing::GroupMember member1;
  member1.gaia_id = GaiaId("abc");
  member1.display_name = "Provided Diplay Name 1";
  member1.given_name = "Provided Given Name 1";
  member1.role = data_sharing::MemberRole::kOwner;
  group_data.members.emplace_back(member1);

  base::Time time = base::Time::Now();

  // Owner added event shouldn't get stored in DB or have an instant message.
  EXPECT_FALSE(HasLastMessageFromDB());
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .Times(0);

  // Owner is added.
  ds_notifier_observer_->OnGroupMemberAdded(group_data, member1.gaia_id, time);

  // Owner is removed.
  time += base::Seconds(1);
  ds_notifier_observer_->OnGroupMemberRemoved(group_data, member1.gaia_id,
                                              time);
  task_environment_.FastForwardBy(base::Seconds(10));
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
  collaboration_pb::Message db_message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDot, base::Time::Now());
  AddMessage(db_message);

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

  // Create dirty message for tab 1.
  collaboration_pb::Message db_message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, base::Time::Now());
  db_message1.mutable_tab_data()->set_sync_tab_id(
      tab1_sync_id.AsLowercaseString());
  db_message1.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message1);

  // Create dirty message for tab 2.
  collaboration_pb::Message db_message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, base::Time::Now());
  db_message2.mutable_tab_data()->set_sync_tab_id(
      tab2_sync_id.AsLowercaseString());
  db_message2.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message2);

  PersistentMessage last_persistent_message_chip;
  PersistentMessage last_persistent_message_dot;

  // Select tab 1, it should clear the chip and dot for tab 1.
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
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                    DirtyType::kDotAndChip)
                  .has_value());

  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               true);

  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                     DirtyType::kDotAndChip)
                   .has_value());
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Select tab 2, it should clear the chip and dot for tab 2.
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
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kDotAndChip)
                  .has_value());

  tg_notifier_observer_->OnTabSelectionChanged(tab2->local_tab_id().value(),
                                               true);

  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                     DirtyType::kDotAndChip)
                   .has_value());
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Selecting a tab outside a tab group should not do anything.
  tg_notifier_observer_->OnTabSelectionChanged(
      tab_groups::test::GenerateRandomTabID(), true);
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
  collaboration_pb::Message db_message = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDot, base::Time::Now());
  AddMessage(db_message);

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

  // Create dirty message for tab 1.
  collaboration_pb::Message db_message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, base::Time::Now());
  db_message1.mutable_tab_data()->set_sync_tab_id(
      tab1_sync_id.AsLowercaseString());
  db_message1.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message1);

  // Create dirty message for tab 2.
  collaboration_pb::Message db_message2 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_UPDATED,
      DirtyType::kDotAndChip, base::Time::Now());
  db_message2.mutable_tab_data()->set_sync_tab_id(
      tab2_sync_id.AsLowercaseString());
  db_message2.mutable_tab_data()->set_sync_tab_group_id(
      tab_group.saved_guid().AsLowercaseString());
  AddMessage(db_message2);

  PersistentMessage last_persistent_message_chip;
  PersistentMessage last_persistent_message_dot;

  // Select tab 1, it should clear the dot for tab 1.
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_dot));
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                    DirtyType::kDot)
                  .has_value());
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                    DirtyType::kChip)
                  .has_value());
  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               true);
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                     DirtyType::kDot)
                   .has_value());
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                    DirtyType::kChip)
                  .has_value());
  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Select tab 2 and unselect tab 1, it should clear the chip for tab 1 and the
  // dot for tab 2.
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
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kDot)
                  .has_value());
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kChip)
                  .has_value());

  tg_notifier_observer_->OnTabSelectionChanged(tab1->local_tab_id().value(),
                                               false);
  tg_notifier_observer_->OnTabSelectionChanged(tab2->local_tab_id().value(),
                                               true);
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab1_sync_id,
                                     DirtyType::kChip)
                   .has_value());
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                     DirtyType::kDot)
                   .has_value());
  EXPECT_TRUE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                    DirtyType::kChip)
                  .has_value());

  EXPECT_EQ(tab1_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_dot.attribution.tab_metadata->sync_tab_id);

  // Selecting a tab outside a tab group should clear the chip for tab 2.
  EXPECT_CALL(mock_persistent_message_observer_,
              HidePersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1)
      .WillOnce(SaveArg<0>(&last_persistent_message_chip));
  tg_notifier_observer_->OnTabSelectionChanged(tab2->local_tab_id().value(),
                                               false);
  tg_notifier_observer_->OnTabSelectionChanged(
      tab_groups::test::GenerateRandomTabID(), true);
  EXPECT_FALSE(GetDirtyMessageForTab(collaboration_group_id, tab2_sync_id,
                                     DirtyType::kChip)
                   .has_value());
  EXPECT_EQ(tab2_sync_id,
            last_persistent_message_chip.attribution.tab_metadata->sync_tab_id);
}

TEST_F(MessagingBackendServiceImplTest,
       TestOpenTabGroupShowPersistentMessages) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Create a tab group.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  EXPECT_CALL(*mock_tab_group_sync_service_,
              GetGroup(tab_groups::EitherGroupID(tab_group.saved_guid())))
      .WillRepeatedly(Return(tab_group));
  base::Uuid tab1_sync_id = tab_group.saved_tabs().at(0).saved_tab_guid();
  tab_groups::SavedTabGroupTab* tab1 = tab_group.GetTab(tab1_sync_id);

  // Create a dirty tab db message.
  base::Time now = base::Time::Now();
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id, collaboration_pb::EventType::TAB_ADDED,
      DirtyType::kDotAndChip, now - base::Minutes(5));
  message1.set_triggering_user_gaia_id("gaia_1");
  message1.mutable_tab_data()->set_sync_tab_id(
      tab1->saved_tab_guid().AsLowercaseString());
  message1.mutable_tab_data()->set_sync_tab_group_id(
      tab1->saved_group_guid().AsLowercaseString());
  AddMessage(message1);

  // Expect to show 3 messages (dirty tab, chip and dirty tab group).
  PersistentMessage expected_message_chip;
  expected_message_chip.collaboration_event = CollaborationEvent::TAB_ADDED;
  expected_message_chip.type = PersistentNotificationType::CHIP;
  PersistentMessage expected_message_dot;
  expected_message_dot.collaboration_event = CollaborationEvent::TAB_ADDED;
  expected_message_dot.type = PersistentNotificationType::DIRTY_TAB;
  PersistentMessage expected_message_dirty_tab_group;
  expected_message_dirty_tab_group.collaboration_event =
      CollaborationEvent::UNDEFINED;
  expected_message_dirty_tab_group.type =
      PersistentNotificationType::DIRTY_TAB_GROUP;
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_dot)))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(
                  PersistentMessageTypeAndEventEq(expected_message_chip)))
      .Times(1);
  EXPECT_CALL(mock_persistent_message_observer_,
              DisplayPersistentMessage(PersistentMessageTypeAndEventEq(
                  expected_message_dirty_tab_group)))
      .Times(1);

  tg_notifier_observer_->OnTabGroupOpened(tab_group);
}

TEST_F(MessagingBackendServiceImplTest,
       TestOpenTabGroupRedeliverDirtyInstantMessages) {
  CreateAndInitializeService();
  SetupInstantMessageDelegate();

  data_sharing::GroupId collaboration_group_id =
      data_sharing::GroupId("my group id");

  // Create a tab group.
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(collaboration_group_id);
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  EXPECT_CALL(*mock_tab_group_sync_service_,
              GetGroup(tab_groups::EitherGroupID(tab_group.saved_guid())))
      .WillRepeatedly(Return(tab_group));

  // Create a dirty db instant message.
  base::Time now = base::Time::Now();
  collaboration_pb::Message message1 = CreateStoredMessage(
      collaboration_group_id,
      collaboration_pb::EventType::COLLABORATION_MEMBER_ADDED,
      DirtyType::kMessageOnly, now - base::Minutes(5));
  message1.set_triggering_user_gaia_id("gaia_1");
  AddMessage(message1);

  // Expect to show instant message.
  EXPECT_CALL(*mock_instant_message_delegate_,
              DisplayInstantaneousMessage(_, _))
      .Times(1);

  tg_notifier_observer_->OnTabGroupOpened(tab_group);
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(MessagingBackendServiceImplTest, OnTabLastSeenTimeChanged_Remote) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  // Create a group in the service with a local tab.
  data_sharing::GroupId test_group("test_collab");
  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    group_guid, std::nullopt);
  tab1.SetLastSeenTime(base::Time::Now() + base::Seconds(10));
  tab1.SetNavigationTime(base::Time::Now());
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(test_group, group_guid, {tab1});
  auto expected_tab_guid = tab1.saved_tab_guid();
  auto expected_group_guid = tab_group.saved_guid();
  EXPECT_CALL(*mock_tab_group_sync_service_, GetGroup(expected_group_guid))
      .WillRepeatedly(Return(tab_group));
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Create a message in the messaging backend.
  collaboration_pb::Message message =
      CreateStoredMessage(test_group, collaboration_pb::TAB_UPDATED,
                          DirtyType::kDotAndChip, base::Time::Now());
  message.mutable_tab_data()->set_sync_tab_id(
      expected_tab_guid.AsLowercaseString());
  message.mutable_tab_data()->set_sync_tab_group_id(
      expected_group_guid.AsLowercaseString());
  AddMessage(message);

  ASSERT_TRUE(HasDirtyMessages());

  PersistentMessage message1, message2, message3;
  testing::InSequence sequence;
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message1));  // Capture the first message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message2));  // Capture the second message
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage(_))
      .WillOnce(SaveArg<0>(&message3));  // Capture the third message

  // Perform a change notification.
  service_->OnTabLastSeenTimeChanged(expected_tab_guid,
                                     tab_groups::TriggerSource::REMOTE);

  // Expect that the dirty message was cleared.
  auto dirty_message = GetDirtyMessageForTab(test_group, expected_tab_guid,
                                             DirtyType::kDotAndChip);
  EXPECT_FALSE(dirty_message.has_value());

  // Verify the messages that were hidden.
  // Chip message of tab.
  EXPECT_TRUE(message1.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message1.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::UNDEFINED, message1.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message1.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::CHIP, message1.type);

  // Dirty dot of tab.
  EXPECT_TRUE(message2.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message2.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::UNDEFINED, message2.collaboration_event);
  EXPECT_EQ(tab_group.saved_guid(),
            message2.attribution.tab_group_metadata->sync_tab_group_id.value());
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB, message2.type);

  // Dirty dot of tab group.
  EXPECT_FALSE(message3.attribution.tab_metadata.has_value());
  EXPECT_TRUE(message3.attribution.tab_group_metadata.has_value());
  EXPECT_EQ(CollaborationEvent::UNDEFINED, message3.collaboration_event);
  EXPECT_EQ(PersistentNotificationType::DIRTY_TAB_GROUP, message3.type);
  EXPECT_EQ(tab_group.saved_guid(),
            message3.attribution.tab_group_metadata->sync_tab_group_id.value());
}

TEST_F(MessagingBackendServiceImplTest, OnTabLastSeenTimeChanged_NonRemote) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  // Create a group in the service with a local tab.
  data_sharing::GroupId test_group("test_collab");
  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    group_guid, std::nullopt);
  tab1.SetLastSeenTime(base::Time::Now() + base::Seconds(10));
  tab1.SetNavigationTime(base::Time::Now());
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(test_group, group_guid, {tab1});
  auto expected_tab_guid = tab1.saved_tab_guid();
  auto expected_group_guid = tab_group.saved_guid();
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Create a message in the messaging backend.
  collaboration_pb::Message message =
      CreateStoredMessage(test_group, collaboration_pb::TAB_UPDATED,
                          DirtyType::kDotAndChip, base::Time::Now());
  message.mutable_tab_data()->set_sync_tab_id(
      expected_tab_guid.AsLowercaseString());
  message.mutable_tab_data()->set_sync_tab_group_id(
      expected_group_guid.AsLowercaseString());
  AddMessage(message);

  ASSERT_TRUE(HasDirtyMessages());

  // Perform a change notification.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(0);
  service_->OnTabLastSeenTimeChanged(expected_tab_guid,
                                     tab_groups::TriggerSource::LOCAL);

  // Expect that the dirty message was not cleared.
  auto dirty_message = GetDirtyMessageForTab(test_group, expected_tab_guid,
                                             DirtyType::kDotAndChip);
  EXPECT_TRUE(dirty_message.has_value());
}

TEST_F(MessagingBackendServiceImplTest,
       OnTabLastSeenTimeChanged_Remote_SeenTimeOlderThanNavigationTime) {
  CreateAndInitializeService();
  AddPersistentMessageObserver();

  // Create a group in the service with a local tab.
  data_sharing::GroupId test_group("test_collab");
  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/"), u"Tab 1",
                                    group_guid, std::nullopt);
  tab1.SetLastSeenTime(base::Time::Now() + base::Seconds(10));
  tab1.SetNavigationTime(base::Time::Now() + base::Seconds(20));
  tab_groups::SavedTabGroup tab_group =
      CreateSharedTabGroup(test_group, group_guid, {tab1});
  auto expected_tab_guid = tab1.saved_tab_guid();
  auto expected_group_guid = tab_group.saved_guid();
  std::vector<tab_groups::SavedTabGroup> all_groups = {tab_group};
  EXPECT_CALL(*mock_tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_groups));

  // Create a message in the messaging backend.
  collaboration_pb::Message message =
      CreateStoredMessage(test_group, collaboration_pb::TAB_UPDATED,
                          DirtyType::kDotAndChip, base::Time::Now());
  message.mutable_tab_data()->set_sync_tab_id(
      expected_tab_guid.AsLowercaseString());
  message.mutable_tab_data()->set_sync_tab_group_id(
      expected_group_guid.AsLowercaseString());
  AddMessage(message);

  ASSERT_TRUE(HasDirtyMessages());

  // Perform a change notification.
  EXPECT_CALL(mock_persistent_message_observer_, HidePersistentMessage)
      .Times(0);
  service_->OnTabLastSeenTimeChanged(expected_tab_guid,
                                     tab_groups::TriggerSource::REMOTE);

  // Expect that the dirty message was not cleared.
  auto dirty_message = GetDirtyMessageForTab(test_group, expected_tab_guid,
                                             DirtyType::kDotAndChip);
  EXPECT_TRUE(dirty_message.has_value());
}

TEST_F(MessagingBackendServiceImplTest, TruncateTabTitle) {
  constexpr int kMaxNonTruncatedSize = 28;

  char16_t chr = u'X';
  const std::u16string kLargeTitleNoTrim(50, chr);
  const std::u16string kSmallTitleNoTrim(10, chr);
  const std::u16string kExactSizeTitleNoTrim(kMaxNonTruncatedSize, chr);
  char16_t kEllipsis = u'\u2026';

  const std::u16string kNoTitleNoTrim(50, u' ');
  const std::u16string kNoTitleAllTrim = u"";

  const std::u16string kLargeTitleWithTrim = u"   " + kLargeTitleNoTrim;
  const std::u16string kSmallTitleWithTrim =
      std::u16string(25, ' ') + kSmallTitleNoTrim + std::u16string(25, u' ');

  const std::u16string kMultiGraphemeCharacter = u"A\u0301";  // “Á”
  std::u16string exact_sized_title_with_multi_grapheme_characters = u"";
  for (int i = 0; i < kMaxNonTruncatedSize; i++) {
    exact_sized_title_with_multi_grapheme_characters.append(
        kMultiGraphemeCharacter);
  }
  std::u16string small_title_with_multi_grapheme_characters = u"";
  for (int i = 0; i < 20; i++) {
    small_title_with_multi_grapheme_characters.append(kMultiGraphemeCharacter);
  }

  // Truncating empty strings works correctly.
  EXPECT_EQ(kNoTitleAllTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kNoTitleNoTrim));
  EXPECT_EQ(kNoTitleAllTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kNoTitleAllTrim));

  std::u16string kLargeTitleAfterTruncation =
      std::u16string(28, chr) + kEllipsis;
  // Large titles truncate correctly
  EXPECT_EQ(kLargeTitleAfterTruncation,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kLargeTitleNoTrim));
  EXPECT_EQ(kLargeTitleAfterTruncation,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kLargeTitleWithTrim));

  // Small titles trim correctly.
  EXPECT_EQ(kSmallTitleNoTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kSmallTitleNoTrim));
  EXPECT_EQ(kSmallTitleNoTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kSmallTitleWithTrim));

  // Exact sizing works correctly.
  EXPECT_EQ(kExactSizeTitleNoTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kExactSizeTitleNoTrim));
  EXPECT_EQ(kExactSizeTitleNoTrim,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kExactSizeTitleNoTrim + u" "));
  EXPECT_EQ(kExactSizeTitleNoTrim + kEllipsis,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                kExactSizeTitleNoTrim + chr));

  // Multi grapheme characters work correctly.
  EXPECT_EQ(small_title_with_multi_grapheme_characters,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                small_title_with_multi_grapheme_characters));
  EXPECT_EQ(exact_sized_title_with_multi_grapheme_characters,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                exact_sized_title_with_multi_grapheme_characters));
  EXPECT_EQ(exact_sized_title_with_multi_grapheme_characters + kEllipsis,
            MessagingBackendServiceImpl::GetTruncatedTabTitleForTesting(
                exact_sized_title_with_multi_grapheme_characters +
                kMultiGraphemeCharacter));
}

}  // namespace collaboration::messaging
