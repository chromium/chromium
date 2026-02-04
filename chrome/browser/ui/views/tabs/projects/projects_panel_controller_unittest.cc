// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
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

const tab_groups::SavedTabGroup& GetPinnedGroup1DayOlder() {
  static const base::NoDestructor<tab_groups::SavedTabGroup> group(
      CreateGroup(u"Group X", kFixedTime - base::Days(1), /*pinned=*/true));
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
              (const tab_groups::SavedTabGroup&, int, std::optional<int>),
              (override));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, int), (override));
};

MATCHER_P(GroupIs, expected_group, "") {
  return arg.saved_guid() == expected_group.saved_guid();
}

}  // namespace

class ProjectsPanelControllerTest : public testing::Test {
 protected:
  std::unique_ptr<ProjectsPanelController> GetInitializedController() {
    auto controller = std::make_unique<ProjectsPanelController>(
        &mock_tab_group_sync_service_);
    controller->OnInitialized();
    return controller;
  }
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
};

TEST_F(ProjectsPanelControllerTest, SortsGroupsOnConstruction) {
  std::vector<tab_groups::SavedTabGroup> groups = {
      GetGroup(), GetGroup1DayOlder(), GetGroup1DayNewer()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(GetGroup1DayNewer().saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(GetGroup().saved_guid(), tab_groups[1].saved_guid());
  EXPECT_EQ(GetGroup1DayOlder().saved_guid(), tab_groups[2].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, SortsPinnedGroupsFirst) {
  tab_groups::SavedTabGroup unpinned_new = GetGroup1DayNewer();
  tab_groups::SavedTabGroup pinned_old = GetPinnedGroup1DayOlder();

  std::vector<tab_groups::SavedTabGroup> groups = {unpinned_new, pinned_old};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(2u, tab_groups.size());
  // Pinned group should be first, even thought it's older.
  EXPECT_EQ(pinned_old.saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(unpinned_new.saved_guid(), tab_groups[1].saved_guid());
}

TEST_F(ProjectsPanelControllerTest, AddsGroupInCorrectOrder) {
  std::vector<tab_groups::SavedTabGroup> groups = {GetGroup(),
                                                   GetGroup2DaysOld()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  controller->OnTabGroupAdded(GetGroup1DayOlder(),
                              tab_groups::TriggerSource::REMOTE);

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
  const base::Uuid uuid =
      base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001");

  EXPECT_CALL(mock_tab_group_sync_service_,
              OpenTabGroup(testing::Eq(uuid), testing::_))
      .Times(1);

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  EXPECT_CALL(mock_browser_window_interface, GetBrowserForMigrationOnly())
      .WillOnce(testing::Return(nullptr));

  controller->OpenTabGroup(uuid, &mock_browser_window_interface);
}

class ProjectsPanelControllerObserverTest : public ProjectsPanelControllerTest {
 public:
  void SetUp() override {
    controller_ = std::make_unique<ProjectsPanelController>(
        &mock_tab_group_sync_service_);
    controller_->AddObserver(&observer_);
  }

  void InitializeController() { controller_->OnInitialized(); }

 protected:
  std::unique_ptr<ProjectsPanelController> controller_;
  MockProjectsPanelControllerObserver observer_;
};

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnAdd) {
  InitializeController();
  EXPECT_CALL(observer_, OnTabGroupAdded(GroupIs(GetNewGroup()), 0));
  controller_->OnTabGroupAdded(GetNewGroup(), tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnUpdate) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  tab_groups::SavedTabGroup updated_group = GetGroup();
  updated_group.SetTitle(u"Updated Title");
  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(updated_group), 0,
                                           testing::Eq(std::nullopt)));
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

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnPinning) {
  auto group_to_pin = CreateGroup(u"Group", kFixedTime);
  std::vector<tab_groups::SavedTabGroup> initial_groups = {GetGroup1DayNewer(),
                                                           group_to_pin};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  // Pin the group. It should move from index 1 to index 0.
  group_to_pin.SetPinned(true);
  group_to_pin.SetPosition(0);

  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(group_to_pin), 1,
                                           testing::Optional(0)));
  controller_->OnTabGroupUpdated(group_to_pin,
                                 tab_groups::TriggerSource::LOCAL);

  EXPECT_EQ(group_to_pin.saved_guid(),
            controller_->GetTabGroups()[0].saved_guid());
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnUnpinning) {
  auto group_to_unpin = CreateGroup(u"Group", kFixedTime, /*pinned=*/true);
  std::vector<tab_groups::SavedTabGroup> initial_groups = {group_to_unpin,
                                                           GetGroup1DayNewer()};

  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  // Unpin the group. It should move from index 0 to index 1.
  group_to_unpin.SetPinned(false);

  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(group_to_unpin), 0,
                                           testing::Optional(1)));
  controller_->OnTabGroupUpdated(group_to_unpin,
                                 tab_groups::TriggerSource::LOCAL);

  EXPECT_EQ(group_to_unpin.saved_guid(),
            controller_->GetTabGroups()[1].saved_guid());
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

  EXPECT_CALL(observer_, OnTabGroupUpdated(GroupIs(group), 0, testing::_));
  controller_->OnTabGroupLocalIdChanged(group.saved_guid(), local_group_id);

  EXPECT_EQ(group.local_group_id(),
            controller_->GetTabGroups()[0].local_group_id());
}
