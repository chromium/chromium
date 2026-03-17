// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
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

contextual_tasks::ContextualTask CreateTaskWithThread(
    const base::Uuid& task_id,
    const std::string& server_id,
    const std::string& title,
    int64_t last_turn_time_ms = 0) {
  contextual_tasks::ContextualTask task(task_id);
  contextual_tasks::Thread thread(contextual_tasks::ThreadType::kAiMode,
                                  server_id, title, last_turn_time_ms,
                                  "conversation_turn_id");
  task.AddThread(thread);
  return task;
}

const contextual_tasks::ContextualTask& GetTask1() {
  static const base::NoDestructor<contextual_tasks::ContextualTask> task(
      CreateTaskWithThread(
          base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001"),
          "id1", "Title 1", 1000));
  return *task;
}

const contextual_tasks::ContextualTask& GetTask1WithUpdatedTitle() {
  static const base::NoDestructor<contextual_tasks::ContextualTask> task(
      CreateTaskWithThread(
          base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001"),
          "id1", "Updated Title 1", 1000));
  return *task;
}

const contextual_tasks::ContextualTask& GetTask1WithUpdatedLastTurnTime() {
  static const base::NoDestructor<contextual_tasks::ContextualTask> task(
      CreateTaskWithThread(
          base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000001"),
          "id1", "Title 1", 3000));
  return *task;
}

const contextual_tasks::ContextualTask& GetTask2() {
  static const base::NoDestructor<contextual_tasks::ContextualTask> task(
      CreateTaskWithThread(
          base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000002"),
          "id2", "Title 2", 2000));
  return *task;
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
  ProjectsPanelControllerTest() {
    EXPECT_CALL(mock_browser_window_interface_, GetBrowserForMigrationOnly())
        .WillRepeatedly(testing::Return(nullptr));
  }

  std::unique_ptr<ProjectsPanelController> GetInitializedController() {
    auto controller = std::make_unique<ProjectsPanelController>(
        &mock_browser_window_interface_, &mock_tab_group_sync_service_,
        &mock_contextual_tasks_service_, &mock_contextual_tasks_ui_service_);
    controller->OnInitialized();
    return controller;
  }

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  testing::NiceMock<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_;
  testing::NiceMock<contextual_tasks::MockContextualTasksUiService>
      mock_contextual_tasks_ui_service_;
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

TEST_F(ProjectsPanelControllerTest, AddsGroupAtTopIfNoPosition) {
  std::vector<tab_groups::SavedTabGroup> groups = {GetGroup(),
                                                   GetGroup2DaysOld()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  tab_groups::SavedTabGroup group_to_add =
      CreateGroup(u"New Group", kFixedTime + base::Days(10));
  controller->OnTabGroupAdded(group_to_add, tab_groups::TriggerSource::REMOTE);

  const auto& tab_groups = controller->GetTabGroups();
  ASSERT_EQ(3u, tab_groups.size());
  EXPECT_EQ(group_to_add.saved_guid(), tab_groups[0].saved_guid());
  EXPECT_EQ(GetGroup().saved_guid(), tab_groups[1].saved_guid());
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

  controller->OpenTabGroup(uuid);
}

TEST_F(ProjectsPanelControllerTest, MoveTabGroupUpCallsReorderGroupBefore) {
  std::vector<tab_groups::SavedTabGroup> groups = {
      GetGroup(), GetGroup1DayOlder(), GetNewGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  // Move "New Group" (index 2) to index 0.
  // Should call ReorderGroupBefore("New Group", "Group 1").
  EXPECT_CALL(mock_tab_group_sync_service_,
              ReorderGroupBefore(testing::Eq(GetNewGroup().saved_guid()),
                                 testing::Eq(GetGroup().saved_guid())))
      .Times(1);

  controller->MoveTabGroup(GetNewGroup().saved_guid(), 0);
}

TEST_F(ProjectsPanelControllerTest, MoveTabGroupDownCallsReorderGroupAfter) {
  std::vector<tab_groups::SavedTabGroup> groups = {
      GetGroup(), GetGroup1DayOlder(), GetNewGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(groups));

  auto controller = GetInitializedController();

  // Move "Group 1" (index 0) to index 2.
  // Should call ReorderGroupAfter("Group 1", "New Group").
  EXPECT_CALL(mock_tab_group_sync_service_,
              ReorderGroupAfter(testing::Eq(GetGroup().saved_guid()),
                                testing::Eq(GetNewGroup().saved_guid())))
      .Times(1);

  controller->MoveTabGroup(GetGroup().saved_guid(), 2);
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

  controller->OpenTabGroup(uuid);
}

TEST_F(ProjectsPanelControllerTest, OpenThreadCallsService) {
  auto controller = GetInitializedController();

  controller->OnTaskAdded(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  EXPECT_CALL(
      mock_contextual_tasks_ui_service_,
      GetThreadUrlFromTaskId(testing::Eq(GetTask1().GetTaskId()), testing::_))
      .Times(1);

  controller->OpenThread(GetTask1().GetThread()->server_id);
}

TEST_F(ProjectsPanelControllerTest, HandlesTaskAdded) {
  auto controller = GetInitializedController();

  // Task 1 has an older last turn time than task 2.
  controller->OnTaskAdded(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);
  controller->OnTaskAdded(
      GetTask2(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  const auto& threads = controller->GetThreads();
  ASSERT_EQ(2u, threads.size());
  ASSERT_LT(GetTask1().GetThread()->last_turn_time,
            GetTask2().GetThread()->last_turn_time);

  // Should be sorted by most to least recent last turn time.
  EXPECT_EQ(GetTask2().GetThread()->server_id, threads[0].server_id);
  EXPECT_EQ(GetTask1().GetThread()->server_id, threads[1].server_id);
}

TEST_F(ProjectsPanelControllerTest, HandlesTaskUpdates) {
  auto controller = GetInitializedController();

  controller->OnTaskAdded(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);
  controller->OnTaskAdded(
      GetTask2(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  // Update task 1.
  controller->OnTaskUpdated(
      GetTask1WithUpdatedTitle(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  const auto& threads = controller->GetThreads();
  ASSERT_EQ(2u, threads.size());
  EXPECT_EQ(GetTask2().GetThread()->server_id, threads[0].server_id);
  EXPECT_EQ(GetTask1().GetThread()->server_id, threads[1].server_id);
  EXPECT_EQ(GetTask1WithUpdatedTitle().GetThread()->title, threads[1].title);
}

TEST_F(ProjectsPanelControllerTest, HandlesTaskRemoval) {
  auto controller = GetInitializedController();

  controller->OnTaskAdded(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);
  ASSERT_EQ(1u, controller->GetThreads().size());

  controller->OnTaskRemoved(
      GetTask1().GetTaskId(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  EXPECT_TRUE(controller->GetThreads().empty());
}

TEST_F(ProjectsPanelControllerTest, AddsTaskWhenMissingTaskUpdated) {
  auto controller = GetInitializedController();

  // Task 1 is not in the controller yet.
  EXPECT_TRUE(controller->GetThreads().empty());

  controller->OnTaskUpdated(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  const auto& threads = controller->GetThreads();
  ASSERT_EQ(1u, threads.size());
  EXPECT_EQ(GetTask1().GetThread()->server_id, threads[0].server_id);
}

TEST_F(ProjectsPanelControllerTest, OrdersTasksByLastTurnTimeWhenUpdated) {
  auto controller = GetInitializedController();

  controller->OnTaskAdded(
      GetTask1(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);
  controller->OnTaskAdded(
      GetTask2(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  // Task 2 has a more recent last turn time (2000) than task 1 (1000).
  ASSERT_EQ(GetTask2().GetThread()->server_id,
            controller->GetThreads()[0].server_id);
  ASSERT_LT(GetTask1().GetThread()->last_turn_time,
            GetTask2().GetThread()->last_turn_time);

  // Update Task 1 with a more recent last turn time (3000).
  controller->OnTaskUpdated(
      GetTask1WithUpdatedLastTurnTime(),
      contextual_tasks::ContextualTasksService::TriggerSource::kLocal);

  const auto& threads = controller->GetThreads();
  ASSERT_EQ(2u, threads.size());

  // Now, Task 1 should be first.
  EXPECT_EQ(GetTask1().GetThread()->server_id, threads[0].server_id);
  EXPECT_EQ(GetTask2().GetThread()->server_id, threads[1].server_id);
}

class ProjectsPanelControllerObserverTest : public ProjectsPanelControllerTest {
 public:
  void SetUp() override {
    controller_ = std::make_unique<ProjectsPanelController>(
        &mock_browser_window_interface_, &mock_tab_group_sync_service_,
        &mock_contextual_tasks_service_, &mock_contextual_tasks_ui_service_);
    controller_->AddObserver(&observer_);
  }

  void InitializeController() { controller_->OnInitialized(); }

 protected:
  std::unique_ptr<ProjectsPanelController> controller_;
  MockProjectsPanelControllerObserver observer_;
};

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnGroupAdd) {
  InitializeController();
  tab_groups::SavedTabGroup group = GetNewGroup();
  group.SetPosition(0);
  EXPECT_CALL(observer_, OnTabGroupAdded(GroupIs(group), 0));
  controller_->OnTabGroupAdded(group, tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnGroupUpdate) {
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

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnGroupRemove) {
  std::vector<tab_groups::SavedTabGroup> initial_groups = {GetGroup()};
  EXPECT_CALL(mock_tab_group_sync_service_, GetAllGroups())
      .WillOnce(testing::Return(initial_groups));
  InitializeController();

  EXPECT_CALL(observer_, OnTabGroupRemoved(GetGroup().saved_guid(), 0));
  controller_->OnTabGroupRemoved(GetGroup().saved_guid(),
                                 tab_groups::TriggerSource::LOCAL);
}

TEST_F(ProjectsPanelControllerObserverTest,
       NotifiesObserverOnGroupLocalIdChange) {
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

TEST_F(ProjectsPanelControllerObserverTest, NotifiesObserverOnGroupReorder) {
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
