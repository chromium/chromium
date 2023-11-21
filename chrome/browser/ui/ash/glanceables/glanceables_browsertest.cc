// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/classroom_bubble_student_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/test/ash_test_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr char kTestUserName[] = "test@test.test";
constexpr char kTestUserGaiaId[] = "123456";

constexpr char kDueDate[] = "2 Aug 2025 10:00 GMT";

views::Label* FindViewWithLabel(views::View* search_root,
                                const std::u16string& label) {
  if (views::Label* const label_view =
          views::AsViewClass<views::Label>(search_root);
      label_view && label_view->GetText() == label) {
    return label_view;
  }

  // Keep searching in children views.
  for (views::View* const child : search_root->children()) {
    if (views::Label* const found = FindViewWithLabel(child, label)) {
      return found;
    }
  }

  return nullptr;
}

views::Label* FindViewWithLabelFromWindow(aura::Window* search_root,
                                          const std::u16string& label) {
  if (views::Widget* const root_widget =
          views::Widget::GetWidgetForNativeWindow(search_root)) {
    return FindViewWithLabel(root_widget->GetRootView(), label);
  }

  for (aura::Window* const child : search_root->children()) {
    if (auto* found = FindViewWithLabelFromWindow(child, label)) {
      return found;
    }
  }

  return nullptr;
}

views::Label* FindMenuItemLabelWithString(const std::u16string& label) {
  return FindViewWithLabelFromWindow(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_MenuContainer),
      label);
}

}  // namespace

class GlanceablesBrowserTest : public InProcessBrowserTest {
 public:
  GlanceablesController* glanceables_controller() {
    return Shell::Get()->glanceables_controller();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    base::Time date;
    ASSERT_TRUE(base::Time::FromString(kDueDate, &date));
    fake_glanceables_tasks_client_ =
        std::make_unique<api::FakeTasksClient>(date);
    fake_glanceables_classroom_client_ =
        std::make_unique<FakeGlanceablesClassroomClient>();

    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_,
        GlanceablesController::ClientsRegistration{
            .classroom_client = fake_glanceables_classroom_client_.get(),
            .tasks_client = fake_glanceables_tasks_client_.get()});
    Shell::Get()->glanceables_controller()->OnActiveUserSessionChanged(
        account_id_);

    date_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());
  }

  DateTray* GetDateTray() const { return date_tray_; }

  ui::test::EventGenerator* GetEventGenerator() const {
    return event_generator_.get();
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() const {
    return date_tray_->bubble_.get();
  }

  api::FakeTasksClient* fake_glanceables_tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  views::View* GetTasksView() const {
    return GetGlanceableTrayBubble()->GetTasksView();
  }

  Combobox* GetTasksComboBoxView() const {
    return views::AsViewClass<Combobox>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox)));
  }

  views::View* GetTasksItemContainerView() const {
    return views::AsViewClass<views::View>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer)));
  }

  views::LabelButton* GetTaskListFooterSeeAllButton() const {
    return views::AsViewClass<views::LabelButton>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  }

  std::vector<std::string> GetCurrentTaskListItemTitles() const {
    std::vector<std::string> current_items;
    for (views::View* child : GetTasksItemContainerView()->children()) {
      if (views::View* task_item = views::AsViewClass<views::View>(child)) {
        current_items.push_back(
            base::UTF16ToUTF8(views::AsViewClass<views::Label>(
                                  task_item->GetViewByID(base::to_underlying(
                                      GlanceablesViewId::kTaskItemTitleLabel)))
                                  ->GetText()));
      }
    }
    return current_items;
  }

  GlanceablesTaskView* GetTaskItemView(int item_index) {
    return views::AsViewClass<GlanceablesTaskView>(
        GetTasksItemContainerView()->children()[item_index]);
  }

  ClassroomBubbleStudentView* GetStudentView() const {
    return GetGlanceableTrayBubble()->GetClassroomStudentView();
  }

  views::View* GetStudentComboBoxView() const {
    return views::AsViewClass<views::View>(GetStudentView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  }

  views::View* GetStudentItemContainerView() const {
    return views::AsViewClass<views::View>(GetStudentView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer)));
  }

  std::vector<std::string> GetCurrentStudentAssignmentCourseWorkTitles() const {
    std::vector<std::string> assignment_titles;
    for (views::View* child : GetStudentItemContainerView()->children()) {
      if (views::View* assignment = views::AsViewClass<views::View>(child)) {
        assignment_titles.push_back(base::UTF16ToUTF8(
            views::AsViewClass<views::Label>(
                assignment->GetViewByID(base::to_underlying(
                    GlanceablesViewId::kClassroomItemCourseWorkTitleLabel)))
                ->GetText()));
      }
    }
    return assignment_titles;
  }

  GlanceablesClassroomItemView* GetClassroomItemView(int item_index) {
    return views::AsViewClass<GlanceablesClassroomItemView>(
        GetStudentItemContainerView()->children()[item_index]);
  }

  views::LabelButton* GetStudentFooterSeeAllButton() const {
    return views::AsViewClass<views::LabelButton>(GetStudentView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  }

 private:
  raw_ptr<DateTray, DanglingUntriaged | ExperimentalAsh> date_tray_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  std::unique_ptr<FakeGlanceablesClassroomClient>
      fake_glanceables_classroom_client_;

  base::test::ScopedFeatureList features_{features::kGlanceablesV2};
};

class GlanceablesMvpBrowserTest : public GlanceablesBrowserTest {
  void SetUpOnMainThread() override {
    GlanceablesBrowserTest::SetUpOnMainThread();
    base::AddFeatureIdTagToTestResult(
        "screenplay-ace3b729-5402-40cd-b2bf-d488bc95b7e2");
  }
};

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, OpenStudentCourseItemURL) {
  ASSERT_TRUE(glanceables_controller()->GetClassroomClient());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetStudentView());

  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetStudentView()->GetBoundsInScreen()));

  // Check that the approaching course work items are shown.
  EXPECT_EQ(GetCurrentStudentAssignmentCourseWorkTitles(),
            std::vector<std::string>({"Approaching Course Work 0",
                                      "Approaching Course Work 1",
                                      "Approaching Course Work 2"}));

  // Click the first item view assignment and check that its url was opened.
  GetEventGenerator()->MoveMouseTo(GetClassroomItemView(/*item_index=*/0)
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      "https://classroom.google.com/c/test/a/test_course_id_0/details");
}

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, ClickSeeAllStudentButton) {
  ASSERT_TRUE(glanceables_controller()->GetClassroomClient());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetStudentView());

  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetStudentView()->GetBoundsInScreen()));

  // Check that the approaching course work items are shown.
  EXPECT_EQ(GetCurrentStudentAssignmentCourseWorkTitles(),
            std::vector<std::string>({"Approaching Course Work 0",
                                      "Approaching Course Work 1",
                                      "Approaching Course Work 2"}));

  // Click the "See All" button in the student glanceable footer, and check that
  // the correct URL is opened.
  GetEventGenerator()->MoveMouseTo(
      GetStudentFooterSeeAllButton()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      "https://classroom.google.com/u/0/a/not-turned-in/all");
}

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest,
                       ViewAndSwitchStudentClassroomLists) {
  ASSERT_TRUE(glanceables_controller()->GetClassroomClient());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetStudentView());

  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetStudentView()->GetBoundsInScreen()));

  // Check that the approaching course work items are shown.
  EXPECT_EQ(GetCurrentStudentAssignmentCourseWorkTitles(),
            std::vector<std::string>({"Approaching Course Work 0",
                                      "Approaching Course Work 1",
                                      "Approaching Course Work 2"}));

  // Click on the combo box to show the student classroom lists.
  GetEventGenerator()->MoveMouseTo(
      GetStudentComboBoxView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Expect that the correct menu items are shown for the student glanceable.
  const views::View* const due_soon_menu_item =
      FindMenuItemLabelWithString(u"Due soon");
  const views::View* const no_due_date_menu_item =
      FindMenuItemLabelWithString(u"No due date");
  const views::View* const missing_menu_item =
      FindMenuItemLabelWithString(u"Missing");
  const views::View* const done_menu_item =
      FindMenuItemLabelWithString(u"Done");
  EXPECT_TRUE(due_soon_menu_item);
  EXPECT_TRUE(no_due_date_menu_item);
  EXPECT_TRUE(missing_menu_item);
  EXPECT_TRUE(done_menu_item);

  // Click on the no due date label to switch to a new assignment list.
  ASSERT_TRUE(no_due_date_menu_item);
  GetEventGenerator()->MoveMouseTo(
      no_due_date_menu_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Check that the no due date course work items are shown after switching
  // lists.
  EXPECT_EQ(GetCurrentStudentAssignmentCourseWorkTitles(),
            std::vector<std::string>({"No Due Date Course Work 0",
                                      "No Due Date Course Work 1",
                                      "No Due Date Course Work 2"}));
}

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, ViewAndSwitchTaskLists) {
  ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  // Click on the combo box to show the task lists.
  GetEventGenerator()->MoveMouseTo(
      GetTasksComboBoxView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  views::Label* second_menu_item_label =
      FindMenuItemLabelWithString(u"Task List 2 Title");

  // Click on the second menu item label to switch to the second task list.
  ASSERT_TRUE(second_menu_item_label);
  GetEventGenerator()->MoveMouseTo(
      second_menu_item_label->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Make sure that task list items from the second list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>({"Task List 2 Item 1 Title",
                                      "Task List 2 Item 2 Title",
                                      "Task List 2 Item 3 Title"}));
}

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, ClickSeeAllTasksButton) {
  ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  // Click the "See All" button in the tasks glanceable footer, and check that
  // the correct URL is opened.
  GetEventGenerator()->MoveMouseTo(
      GetTaskListFooterSeeAllButton()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      "https://calendar.google.com/calendar/u/0/r/week?opentasks=1");
}

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, CheckOffTaskItems) {
  ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  EXPECT_FALSE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_FALSE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());

  // Click to check off the first task item and check that it has been marked
  // complete.
  GetEventGenerator()->MoveMouseTo(GetTaskItemView(/*item_index=*/0)
                                       ->GetButtonForTest()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_FALSE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());

  // Click to check off the second task item and check that it has been marked
  // complete.
  GetEventGenerator()->MoveMouseTo(GetTaskItemView(/*item_index=*/1)
                                       ->GetButtonForTest()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());
}

class GlanceablesWithAddEditBrowserTest : public GlanceablesBrowserTest {
 private:
  base::test::ScopedFeatureList features_{
      features::kGlanceablesTimeManagementStableLaunch};
};

IN_PROC_BROWSER_TEST_F(GlanceablesWithAddEditBrowserTest, AddTaskItem) {
  ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  ASSERT_TRUE(GetGlanceableTrayBubble());
  ASSERT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  const auto* const add_task_button =
      views::AsViewClass<views::LabelButton>(GetTasksView()->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
  ASSERT_TRUE(add_task_button);

  const auto* const task_items_container = GetTasksItemContainerView();
  ASSERT_TRUE(task_items_container);

  // Click on `add_task_button` and verify that `task_items_container` has the
  // new "pending" item.
  EXPECT_EQ(task_items_container->children().size(), 2u);
  GetEventGenerator()->MoveMouseTo(
      add_task_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(task_items_container->children().size(), 3u);

  const auto* const pending_task_view = GetTaskItemView(0);

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(pending_task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(pending_task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    // Check that the view is in "edit" mode (the text field is displayed).
    ASSERT_FALSE(title_label);
    ASSERT_TRUE(title_text_field);
    EXPECT_TRUE(title_text_field->GetText().empty());

    // Append "New task" text.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_E);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_W);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_T);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_S);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_K);

    // Finish editing by pressing Esc key.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(pending_task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(pending_task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    // Check that the view is in "view" mode with the expected label
    ASSERT_TRUE(title_label);
    ASSERT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"New task");
  }
}

IN_PROC_BROWSER_TEST_F(GlanceablesWithAddEditBrowserTest, EditTaskItem) {
  ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  const auto* const task_view = GetTaskItemView(0);
  ASSERT_TRUE(task_view);

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    // Check that the view is in "view" mode (the label is displayed).
    ASSERT_TRUE(title_label);
    ASSERT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"Task List 1 Item 1 Title");

    // Click the label to switch to "edit" mode.
    GetEventGenerator()->MoveMouseTo(
        title_label->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    // Check that the view is in "edit" mode (the text field is displayed).
    ASSERT_FALSE(title_label);
    ASSERT_TRUE(title_text_field);
    EXPECT_EQ(title_text_field->GetText(), u"Task List 1 Item 1 Title");

    // Append " upd" text.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_U);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_P);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_D);

    // Finish editing by pressing Esc key.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    // Check that the view is in "view" mode with the updated label
    ASSERT_TRUE(title_label);
    ASSERT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"Task List 1 Item 1 Title upd");
  }
}

}  // namespace ash
