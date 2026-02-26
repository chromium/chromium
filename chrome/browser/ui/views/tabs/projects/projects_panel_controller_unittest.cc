// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::Time kFixedTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(10));

// Helper to create a SavedTabGroup.
tab_groups::SavedTabGroup CreateGroup(const std::u16string& title,
                                      const base::Time& creation_time,
                                      bool pinned = false,
                                      size_t position = 0) {
  auto group = tab_groups::SavedTabGroup(
      title, tab_groups::TabGroupColorId::kGrey, /*urls=*/{},
      /*position=*/std::nullopt, /*saved_guid=*/base::Uuid::GenerateRandomV4(),
      /*local_group_id=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false, creation_time);
  group.SetPinned(pinned);
  if (pinned) {
    group.SetPosition(position);
  }
  return group;
}

const tab_groups::SavedTabGroup& GetGroup() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"Group 1", kFixedTime));
  return *group;
}

const tab_groups::SavedTabGroup& GetGroup1DayOlder() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"Group 2", kFixedTime - base::Days(1)));
  return *group;
}

const tab_groups::SavedTabGroup& GetGroup1DayNewer() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"Group 3", kFixedTime + base::Days(1)));
  return *group;
}

const tab_groups::SavedTabGroup& GetGroup2DaysOld() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"Group 4", kFixedTime - base::Days(2)));
  return *group;
}

const tab_groups::SavedTabGroup& GetNewGroup() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"New Group", kFixedTime));
  return *group;
}

class MockProjectsPanelControllerObserver
    : public ProjectsPanelController::Observer {
 public:
  MOCK_METHOD(void,
              OnTabGroupsInitialized,
              (const std::vector<tab_groups::SavedTabGroup>& tab_groups),
              (override));
  MOCK_METHOD(void,
              OnTabGroupAdded,
              (const tab_groups::SavedTabGroup&, int),
              (override));
  MOCK_METHOD(void,
              OnTabGroupUpdated,
              (const tab_groups::SavedTabGroup&),
              (override));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, int), (override));
  MOCK_METHOD(void,
              OnTabGroupsReordered,
              (const std::vector<tab_groups::SavedTabGroup>& tab_groups),
              (override));
  MOCK_METHOD(void,
              OnThreadsInitialized,
              (const std::vector<contextual_tasks::Thread>& threads),
              (override));
};

MATCHER_P(GroupIs, expected_group, "") {
  return arg.saved_guid() == expected_group.saved_guid();
}

}  // namespace

class ProjectsPanelControllerTest : public testing::Test {
 protected:
  std::unique_ptr<ProjectsPanelController> GetInitializedController() {
    auto controller = std::make_unique<ProjectsPanelController>(
        &mock_tab_group_sync_service_, &mock_contextual_tasks_service_);
    controller->OnInitialized();
    return controller;
  }
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  testing::NiceMock<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_;
};

TEST_F(ProjectsPanelControllerTest, PreservesOrderOnConstruction) {
  std::vector<tab_groups::SavedTabGroup> groups = {
      GetGroup(), GetGroup1DayOlder(), GetGroup1DayNewer()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(GetGroup().saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(GetGroup1DayOlder().saved_guid(), tab_groups[1].saved_guid());
  EXPECT_EQ(GetGroup1DayNewer().saved_guid(), tab_groups[2].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, AddsGroupAtCorrectPosition) {
  std::vector<tab_groups::SavedTabGroup> groups = {GetGroup(),
                                                   GetGroup2DaysOld()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  tab_groups::SavedTabGroup group_to_add = GetGroup1DayOlder();
  group_to_add.SetPosition(1);
  controller->OnTabGroupAdded(group_to_add, tab_groups::TriggerSource::REMOTE);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(GetGroup().saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(GetGroup1DayOlder().saved_guid(), tab_groups[1].saved_guid());
  EXPECT_EQ(GetGroup2DaysOld().saved_guid(), tab_groups[2].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, UpdatesExistingGroup) {
  std::vector<tab_groups::SavedTabGroup> groups = {GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  tab_groups::SavedTabGroup updated_group = GetGroup();
  updated_group.SetTitle(u"Updated Title");
  controller->OnTabGroupUpdated(updated_group,
                                tab_groups::TriggerSource::LOCAL);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(1u, tab_groups.size());
  EXPECT_EQ(u"Updated Title", tab_groups[0].title());
}

TEST_F(ProjectsPanelControllerTest, RemovesGroup) {
  std::vector<tab_groups::SavedTabGroup> groups = {GetGroup(),
                                                   GetGroup1DayOlder()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  controller->OnTabGroupRemoved(GetGroup().saved_guid(),
                                tab_groups::TriggerSource::LOCAL);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(1u, tab_groups.size());
  EXPECT_EQ(GetGroup1DayOlder().saved_guid(), tab_groups[0].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, OpenTabGroupCallsService) {
  auto controller = GetInitializedController();
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_tab_group_sync_service_,
              OpenTabGroup(testing::Eq(uuid), testing::_))
      .Times(1);

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  EXPECT_CALL(mock_browser_window_interface, GetBrowserForMigrationOnly())
      .WillOnce(testing::Return(nullptr));

  controller->OpenTabGroup(uuid, &mock_browser_window_interface);
}

TEST_F(ProjectsPanelControllerTest, MoveTabGroupCallsService) {
  auto controller = GetInitializedController();
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_tab_group_sync_service_,
              UpdateGroupPosition(testing::Eq(uuid), testing::Eq(std::nullopt),
                                  testing::Eq(2)))
      .Times(1);

  controller->MoveTabGroup(uuid, 2);
}

TEST_F(ProjectsPanelControllerTest, OpenTabGroupAutofocus) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTabGroupsFocusing,
      {{"tab_groups_focusing_default_to_focused", "true"}});

  auto controller = GetInitializedController();
  const base::Uuid uuid =
      base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001");
  auto local_group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(mock_tab_group_sync_service_,
              OpenTabGroup(testing::Eq(uuid), testing::_))
      .WillOnce(testing::Return(local_group_id));

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;

  // Verify that the browser's TabStripModel is accessed.
  // The code should check for nullptr, so returning nullptr is fine here.
  EXPECT_CALL(mock_browser_window_interface, GetTabStripModel())
      .WillOnce(testing::Return(nullptr));

  controller->OpenTabGroup(uuid, &mock_browser_window_interface);
}

class ProjectsPanelControllerObserverTest : public ProjectsPanelControllerTest {
 public:
  void SetUp() override {
    controller_ = std::make_unique<ProjectsPanelController>(
        &mock_tab_group_sync_service_, &mock_contextual_tasks_service_);
    controller_->AddObserver(&observer_);
  }

  void InitializeController() { controller_->OnInitialized(); }

 protected:
  std::unique_ptr<ProjectsPanelController> controller_;
  MockProjectsPanelControllerObserver observer_;
};

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnAdd) {
  InitializeController();
  tab_groups::SavedTabGroup group = GetNewGroup();
  group.SetPosition(0);
  EXPECT_CALL(observer_, OnTabGroupAdded(GroupIs(group), 0));
  controller_->OnTabGroupAdded(group, tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnUpdate) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  tab_groups::SavedTabGroup updated_group = GetGroup();
  updated_group.SetTitle(u"Updated Title");
  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(updated_group)));
  controller_->OnTabGroupUpdated(updated_group,
                                 tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnRemove) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  EXPECT_CALL(observer_, OnTabGroupRemoved(GetGroup().saved_guid(), 0));
  controller_->OnTabGroupRemoved(GetGroup().saved_guid(),
                                 tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnLocalIdChange) {
  auto group = CreateGroup(u"Group", kFixedTime);

  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(
          testing::Return(std::vector<tab_groups::SavedTabGroup>({group})));
  InitializeController();

  auto local_group_id = tab_groups::LocalTabGroupID::FromRawToken(
      base::Token{0x12345678, 0x9ABCDEF0});
  group.SetLocalGroupId(local_group_id);

  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(group)));
  controller_->OnTabGroupLocalIdChanged(group.saved_guid(), local_group_id);

  EXPECT_EQ(group.local_group_id(),
            controller_->GetTabGroups()[0].local_group_id());
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnReorder) {
  InitializeController();

  std::vector<tab_groups::SavedTabGroup> reordered_groups = {
      GetGroup1DayOlder(), GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(reordered_groups));

  EXPECT_CALL(observer_, OnTabGroupsReordered(testing::SizeIs(2)));
  controller_->OnTabGroupsReordered(tab_groups::TriggerSource::REMOTE);

  EXPECT_EQ(GetGroup1DayOlder().saved_guid(),
            controller_->GetTabGroups()[0].saved_guid());
  EXPECT_EQ(GetGroup().saved_guid(),
            controller_->GetTabGroups()[1].saved_guid());
}
