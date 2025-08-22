// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_personal_collaboration_data_handler.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/data_sharing/test_support/mock_personal_collaboration_data_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

using testing::_;
using testing::Eq;
using testing::Return;
using SpecificsType = ::data_sharing::personal_collaboration_data::
    PersonalCollaborationDataService::SpecificsType;

class TabGroupSyncPersonalCollaborationDataHandlerTest : public testing::Test {
 public:
  // Tab groups and tabs need local IDs in order to be updated by the
  // account data sync bridge.
  SavedTabGroup CreateGroupWithLocalIds(
      const syncer::CollaborationId collaboration_id) {
    const LocalTabGroupID kLocalTabGroupId =
        tab_groups::test::GenerateRandomTabGroupID();

    SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
    SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
        "www.google.com", u"Google", group.saved_guid(), /*position=*/0);
    SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
        "chrome://newtab", u"new tab", group.saved_guid(), /*position=*/1);
    tab1.SetNavigationTime(base::Time::Now() + base::Seconds(1000));
    tab2.SetNavigationTime(base::Time::Now() + base::Seconds(1000));
    group.AddTabLocally(tab1);
    group.AddTabLocally(tab2);

    group.SetCollaborationId(collaboration_id);
    group.SetLocalGroupId(kLocalTabGroupId);

    for (size_t i = 0; i < group.saved_tabs().size(); i++) {
      group.saved_tabs()[i].SetLocalTabID(i);
    }

    return group;
  }

  TabGroupSyncPersonalCollaborationDataHandlerTest() = default;
  ~TabGroupSyncPersonalCollaborationDataHandlerTest() override = default;

 protected:
  void SetUp() override {
    model_ = std::make_unique<SavedTabGroupModel>();
    handler_ = std::make_unique<TabGroupSyncPersonalCollaborationDataHandler>(
        model_.get(), &personal_collaboration_data_service_);
  }

  void TearDown() override { handler_.reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<data_sharing::MockPersonalCollaborationDataService>
      personal_collaboration_data_service_;
  std::unique_ptr<TabGroupSyncPersonalCollaborationDataHandler> handler_;
};

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       ApplyUpdatesFromSpecifics) {
  // 1. Create a group and a tab in the model.
  SavedTabGroup group(test::CreateTestSavedTabGroup());
  group.SetCollaborationId(syncer::CollaborationId("test_id"));
  SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                       group.saved_guid(),
                       /*position=*/0);
  model_->AddedLocally(group);
  model_->AddTabToGroupLocally(group.saved_guid(), tab);
  ASSERT_FALSE(model_->Get(group.saved_guid())->position().has_value());

  // 2. Create specifics for the group and tab.
  sync_pb::SharedTabGroupAccountDataSpecifics group_specifics;
  group_specifics.set_guid(group.saved_guid().AsLowercaseString());
  auto* group_details = group_specifics.mutable_shared_tab_group_details();
  group_details->set_pinned_position(1);

  sync_pb::SharedTabGroupAccountDataSpecifics tab_specifics;
  tab_specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  auto* tab_details = tab_specifics.mutable_shared_tab_details();
  tab_details->set_shared_tab_group_guid(
      group.saved_guid().AsLowercaseString());
  base::Time new_last_seen_time = base::Time::Now();
  tab_details->set_last_seen_timestamp_windows_epoch(
      new_last_seen_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // 3. Mock the service to return the specifics.
  std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>
      all_specifics = {&group_specifics, &tab_specifics};
  EXPECT_CALL(personal_collaboration_data_service_, GetAllSpecifics)
      .WillOnce(testing::Return(all_specifics));

  // 4. Call OnInitialized.
  handler_->OnInitialized();

  // 5. Verify the group and tab are updated.
  const SavedTabGroup* updated_group = model_->Get(group.saved_guid());
  ASSERT_TRUE(updated_group->is_shared_tab_group());
  EXPECT_TRUE(updated_group->position().has_value());
  EXPECT_EQ(updated_group->position().value(), 1u);

  const SavedTabGroupTab* updated_tab =
      updated_group->GetTab(tab.saved_tab_guid());
  EXPECT_EQ(updated_tab->last_seen_time(), new_last_seen_time);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       ShouldAddUpdateLastSeenTimestamp) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab = created_group.saved_tabs().front();
  const base::Uuid& created_tab_id = created_tab.saved_tab_guid();

  EXPECT_CALL(personal_collaboration_data_service_, GetAllSpecifics)
      .WillOnce(Return(
          std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>()));
  model_->AddedLocally(created_group);
  handler_->OnInitialized();

  EXPECT_EQ(model_->Count(), 1);
  EXPECT_TRUE(model_->Contains(created_group_id));
  EXPECT_FALSE(created_tab.last_seen_time().has_value());

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->UpdateTabLastSeenTimeFromLocal(created_group_id, created_tab_id);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabDeletionsAndTimestampUpdatesWillBeSentToSync) {
  // Create a shared tab group with two tabs.
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab1 = created_group.saved_tabs()[0];
  const base::Uuid& tab_id1 = created_tab1.saved_tab_guid();
  const SavedTabGroupTab& created_tab2 = created_group.saved_tabs()[1];
  const base::Uuid& tab_id2 = created_tab2.saved_tab_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  EXPECT_EQ(model_->Count(), 1);
  EXPECT_TRUE(model_->Contains(group_id));
  EXPECT_FALSE(created_tab1.last_seen_time().has_value());
  EXPECT_FALSE(created_tab2.last_seen_time().has_value());

  // Send timestamp update for both tabs from sync.
  base::Time last_seen_time1 = base::Time::Now();
  base::Time last_seen_time2 = base::Time::Now() - base::Seconds(42);
  model_->UpdateTabLastSeenTimeFromSync(group_id, tab_id1, last_seen_time1);
  model_->UpdateTabLastSeenTimeFromSync(group_id, tab_id2, last_seen_time2);

  // Retrieve the tabs from model after the model has been updated. Verify the
  // last seen timestamps.
  const SavedTabGroup* group = model_->Get(group_id);
  const SavedTabGroupTab* tab1 = group->GetTab(tab_id1);
  const SavedTabGroupTab* tab2 = group->GetTab(tab_id2);

  EXPECT_TRUE(tab1->last_seen_time().has_value());
  EXPECT_EQ(tab1->last_seen_time(), last_seen_time1);
  EXPECT_TRUE(tab2->last_seen_time().has_value());
  EXPECT_EQ(tab2->last_seen_time(), last_seen_time2);

  // Update the last seen timestamp for tab1 locally. The updated timestamp
  // should be sent to sync.
  base::Time last_seen_time4 = base::Time::Now() + base::Seconds(99);

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id1);

  // Update the last seen timestamp for tab2 from sync. The updated timestamp
  // should not be sent back to sync.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(0);
  model_->UpdateTabLastSeenTimeFromSync(group_id, tab_id2, last_seen_time4);

  ASSERT_EQ(tab1->last_seen_time(), tab1->navigation_time());
  ASSERT_EQ(tab2->last_seen_time(), last_seen_time4);

  // Delete the tab1 locally and tab2 from sync. The corresponding sync entries
  // for both tabs should be deleted.
  EXPECT_CALL(personal_collaboration_data_service_, DeleteSpecifics(_, _))
      .Times(2);
  model_->RemoveTabFromGroupLocally(group_id, tab_id1);
  model_->RemoveTabFromGroupFromSync(group_id, tab_id2);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupDeletionLocallyWillDeleteAllTabsFromSync) {
  // Create a shared tab group with two tabs.
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();
  const base::Uuid& tab_id1 = created_group.saved_tabs()[0].saved_tab_guid();
  const base::Uuid& tab_id2 = created_group.saved_tabs()[1].saved_tab_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  ASSERT_EQ(model_->Count(), 1);
  ASSERT_TRUE(model_->Contains(group_id));

  // Update the last seen timestamp for the tabs locally. The updated timestamp
  // should be sent to sync.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(2);
  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id1);
  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id2);

  // Delete the tab group locally. The corresponding sync entries for both tabs
  // should be deleted.
  EXPECT_CALL(personal_collaboration_data_service_, DeleteSpecifics(_, _))
      .Times(3);
  model_->RemovedLocally(group_id);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupDeletionFromSyncWillDeleteAllTabsFromSync) {
  // Create a shared tab group with two tabs.
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();
  const base::Uuid& tab_id1 = created_group.saved_tabs()[0].saved_tab_guid();
  const base::Uuid& tab_id2 = created_group.saved_tabs()[1].saved_tab_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  ASSERT_EQ(model_->Count(), 1);
  ASSERT_TRUE(model_->Contains(group_id));

  // Update the last seen timestamp for the tabs locally. The updated timestamp
  // should be sent to sync.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(2);
  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id1);
  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id2);

  // Delete the tab group from sync. The corresponding sync entries for both
  // tabs should be deleted.
  EXPECT_CALL(personal_collaboration_data_service_, DeleteSpecifics(_, _))
      .Times(3);
  model_->RemovedFromSync(group_id);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       IncrementalUpdateShouldSetPosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  // Add group locally.
  model_->AddedLocally(created_group);

  // Receive update from sync.
  sync_pb::SharedTabGroupAccountDataSpecifics group_specifics;
  group_specifics.set_guid(created_group_id.AsLowercaseString());
  auto* group_details = group_specifics.mutable_shared_tab_group_details();
  const size_t kPosition = 5;
  group_details->set_pinned_position(kPosition);

  handler_->OnSpecificsUpdated(SpecificsType::kSharedTabGroupSpecifics,
                               "some_key", group_specifics);

  // Verify position is set correctly.
  const SavedTabGroup* group = model_->Get(created_group_id);
  EXPECT_EQ(kPosition, group->position());
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       IncrementalUpdateShouldUpdateTabLastSeenTime) {
  // 1. Create a group and a tab in the model.
  const syncer::CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();
  const SavedTabGroupTab& tab = created_group.saved_tabs().front();
  const base::Uuid& tab_id = tab.saved_tab_guid();

  model_->AddedLocally(created_group);
  ASSERT_FALSE(
      model_->Get(group_id)->GetTab(tab_id)->last_seen_time().has_value());

  // 2. Create specifics for the tab.
  sync_pb::SharedTabGroupAccountDataSpecifics tab_specifics;
  tab_specifics.set_guid(tab_id.AsLowercaseString());
  auto* tab_details = tab_specifics.mutable_shared_tab_details();
  tab_details->set_shared_tab_group_guid(group_id.AsLowercaseString());
  base::Time new_last_seen_time = base::Time::Now();
  tab_details->set_last_seen_timestamp_windows_epoch(
      new_last_seen_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // 3. Call OnSpecificsUpdated.
  handler_->OnSpecificsUpdated(SpecificsType::kSharedTabSpecifics, "some_key",
                               tab_specifics);

  // 4. Verify the tab is updated.
  const SavedTabGroupTab* updated_tab = model_->Get(group_id)->GetTab(tab_id);
  EXPECT_TRUE(updated_tab->last_seen_time().has_value());
  EXPECT_EQ(updated_tab->last_seen_time(), new_last_seen_time);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupAddedFromSyncShouldSetPosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  // Mock the service to return specifics with a position.
  sync_pb::SharedTabGroupAccountDataSpecifics group_specifics;
  group_specifics.set_guid(created_group_id.AsLowercaseString());
  auto* group_details = group_specifics.mutable_shared_tab_group_details();
  const size_t kPosition = 5;
  group_details->set_pinned_position(kPosition);

  EXPECT_CALL(personal_collaboration_data_service_, GetSpecifics(_, _))
      .WillOnce(Return(group_specifics));

  // Add group from sync.
  model_->AddedFromSync(created_group);

  // Verify position is set correctly.
  const SavedTabGroup* group = model_->Get(created_group_id);
  EXPECT_EQ(kPosition, group->position());
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupAddedLocallyShouldSavePosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  const int kPosition = 5;
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  created_group.SetPosition(kPosition);

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupTogglePinStateShouldSavePosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  base::Uuid guid = created_group.saved_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  // Pin the tab group.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->TogglePinState(guid);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupRemovedLocallyShouldRemovePosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  base::Uuid guid = created_group.saved_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  EXPECT_CALL(personal_collaboration_data_service_, DeleteSpecifics(_, _))
      .Times(3);
  model_->RemovedLocally(guid);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupRemovedFromSyncShouldRemovePosition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  base::Uuid guid = created_group.saved_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  model_->AddedLocally(created_group);

  EXPECT_CALL(personal_collaboration_data_service_, DeleteSpecifics(_, _))
      .Times(3);
  model_->RemovedFromSync(guid);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupReorderLocallyShouldSavePosition) {
  const syncer::CollaborationId kCollaborationId1("collaboration1");
  SavedTabGroup created_group1 = CreateGroupWithLocalIds(kCollaborationId1);
  created_group1.SetPosition(0);
  base::Uuid guid1 = created_group1.saved_guid();

  const syncer::CollaborationId kCollaborationId2("collaboration2");
  SavedTabGroup created_group2 = CreateGroupWithLocalIds(kCollaborationId2);
  created_group2.SetPosition(1);

  // Add 2 groups.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(2);
  model_->AddedLocally(created_group1);
  model_->AddedLocally(created_group2);

  // Reorder group.
  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(2);
  model_->ReorderGroupLocally(guid1, 1);
}

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       TabGroupShouldOnlySaveIfPositionChanged) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  created_group.SetPosition(0);
  base::Uuid guid = created_group.saved_guid();

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(1);
  ON_CALL(personal_collaboration_data_service_, GetSpecifics)
      .WillByDefault(Return(std::nullopt));
  model_->AddedLocally(created_group);

  sync_pb::SharedTabGroupAccountDataSpecifics group_specifics;
  group_specifics.set_guid(guid.AsLowercaseString());
  auto* group_details = group_specifics.mutable_shared_tab_group_details();
  group_details->set_pinned_position(created_group.position().value());
  EXPECT_CALL(personal_collaboration_data_service_, GetSpecifics)
      .WillRepeatedly(Return(group_specifics));

  EXPECT_CALL(personal_collaboration_data_service_,
              CreateOrUpdateSpecifics(_, _, _))
      .Times(0);
  model_->UpdateArchivalStatus(guid, true);
  model_->UpdateArchivalStatus(guid, false);
}

}  // namespace
}  // namespace tab_groups
