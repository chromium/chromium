// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <vector>

#include "base/time/time.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper to create a SavedTabGroup.
tab_groups::SavedTabGroup CreateGroup(const std::u16string& title,
                                      const base::Time& creation_time) {
  return tab_groups::SavedTabGroup(
      title, tab_groups::TabGroupColorId::kGrey, /*urls=*/{},
      /*position=*/std::nullopt, /*saved_guid=*/base::Uuid::GenerateRandomV4(),
      /*local_group_id=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false, creation_time);
}

const base::Time kNow = base::Time::Now();
const tab_groups::SavedTabGroup kGroup1 = CreateGroup(u"Group 1", kNow);
const tab_groups::SavedTabGroup kGroup2 =
    CreateGroup(u"Group 2", kNow - base::Days(1));
const tab_groups::SavedTabGroup kGroup3 =
    CreateGroup(u"Group 3", kNow + base::Days(1));
const tab_groups::SavedTabGroup kGroup2DaysOld =
    CreateGroup(u"Group 4", kNow - base::Days(2));
const tab_groups::SavedTabGroup kNewGroup = CreateGroup(u"New Group", kNow);

class MockProjectsPanelControllerObserver
    : public ProjectsPanelController::Observer {
 public:
  MOCK_METHOD(void,
              OnTabGroupAdded,
              (const tab_groups::SavedTabGroup&),
              (override));
  MOCK_METHOD(void,
              OnTabGroupUpdated,
              (const tab_groups::SavedTabGroup&),
              (override));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&), (override));
};

MATCHER_P(GroupIs, expected_group, "") {
  return arg.saved_guid() == expected_group.saved_guid();
}

}  // namespace

class ProjectsPanelControllerTest : public testing::Test {
 protected:
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
};

TEST_F(ProjectsPanelControllerTest, SortsGroupsOnConstruction) {
  std::vector<tab_groups::SavedTabGroup> groups = {kGroup1, kGroup2, kGroup3};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(kGroup3.saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(kGroup1.saved_guid(), tab_groups[1].saved_guid());
  EXPECT_EQ(kGroup2.saved_guid(), tab_groups[2].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, AddsGroupInCorrectOrder) {
  std::vector<tab_groups::SavedTabGroup> groups = {kGroup1, kGroup2DaysOld};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);

  controller->OnTabGroupAdded(kGroup2, tab_groups::TriggerSource::REMOTE);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(kGroup1.saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(kGroup2.saved_guid(), tab_groups[1].saved_guid());
  EXPECT_EQ(kGroup2DaysOld.saved_guid(), tab_groups[2].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, UpdatesExistingGroup) {
  std::vector<tab_groups::SavedTabGroup> groups = {kGroup1};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);

  tab_groups::SavedTabGroup updated_group = kGroup1;
  updated_group.SetTitle(u"Updated Title");
  controller->OnTabGroupUpdated(updated_group,
                                tab_groups::TriggerSource::LOCAL);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(1u, tab_groups.size());
  EXPECT_EQ(u"Updated Title", tab_groups[0].title());
}

TEST_F(ProjectsPanelControllerTest, RemovesGroup) {
  std::vector<tab_groups::SavedTabGroup> groups = {kGroup1, kGroup2};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);

  controller->OnTabGroupRemoved(kGroup1.saved_guid(),
                                tab_groups::TriggerSource::LOCAL);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(1u, tab_groups.size());
  EXPECT_EQ(kGroup2.saved_guid(), tab_groups[0].saved_guid());
}

class ProjectsPanelControllerObserverTest : public ProjectsPanelControllerTest {
 public:
  void SetUp() override {
    controller_ = std::make_unique<ProjectsPanelController>(
        &mock_tab_group_sync_service_);
    controller_->AddObserver(&observer_);
  }

 protected:
  std::unique_ptr<ProjectsPanelController> controller_;
  MockProjectsPanelControllerObserver observer_;
};

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnAdd) {
  EXPECT_CALL(observer_, OnTabGroupAdded(GroupIs(kNewGroup)));
  controller_->OnTabGroupAdded(kNewGroup, tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnUpdate) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {kGroup1};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  controller_ =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);
  controller_->AddObserver(&observer_);

  tab_groups::SavedTabGroup updated_group = kGroup1;
  updated_group.SetTitle(u"Updated Title");
  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(updated_group)));
  controller_->OnTabGroupUpdated(updated_group,
                                 tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnRemove) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {kGroup1};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  controller_ =
      std::make_unique<ProjectsPanelController>(&mock_tab_group_sync_service_);
  controller_->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnTabGroupRemoved(kGroup1.saved_guid()));
  controller_->OnTabGroupRemoved(kGroup1.saved_guid(),
                                 tab_groups::TriggerSource::LOCAL);
}
