// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/style/counter_expand_button.h"
#include "ash/style/error_message_toast.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/test/ash_test_util.h"
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
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

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
        glanceables_tasks_test_util::InitializeFakeTasksClient(date);
    fake_glanceables_tasks_client_->set_http_error(
        google_apis::ApiErrorCode::HTTP_SUCCESS);
    fake_glanceables_classroom_client_ =
        std::make_unique<FakeGlanceablesClassroomClient>();

    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_,
        GlanceablesController::ClientsRegistration{
            .classroom_client = fake_glanceables_classroom_client_.get(),
            .tasks_client = fake_glanceables_tasks_client_.get()});
    Shell::Get()->glanceables_controller()->OnActiveUserSessionChanged(
        account_id_);

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());
  }

  DateTray* GetDateTray() const {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
  }

  ui::test::EventGenerator* GetEventGenerator() const {
    return event_generator_.get();
  }

  void ToggleDateTray() {
    GetEventGenerator()->MoveMouseTo(
        GetDateTray()->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() const {
    return GetDateTray()->glanceables_bubble_for_test();
  }

  api::FakeTasksClient* fake_glanceables_tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  GlanceablesTasksView* GetTasksView() const {
    return views::AsViewClass<GlanceablesTasksView>(
        GetGlanceableTrayBubble()->GetTasksView());
  }

  Combobox* GetTasksComboBoxView() const {
    return views::AsViewClass<Combobox>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTimeManagementBubbleComboBox)));
  }

  views::ScrollView* GetTasksScrollView() const {
    return views::AsViewClass<views::ScrollView>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  views::View* GetTasksItemContainerView() const {
    return views::AsViewClass<views::View>(
        GetTasksView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListContainer)));
  }

  CounterExpandButton* GetTasksExpandButtonView() const {
    return views::AsViewClass<CounterExpandButton>(
        GetTasksView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  views::LabelButton* GetAddNewTaskButton() const {
    return views::AsViewClass<views::LabelButton>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
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

  void SetStudentAssignmentsCount(size_t count) {
    fake_glanceables_classroom_client_->SetAssignmentsCount(count);
  }

  views::View* GetStudentView() const {
    return GetGlanceableTrayBubble()->GetClassroomStudentView();
  }

  views::View* GetStudentComboBoxView() const {
    return views::AsViewClass<views::View>(GetStudentView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTimeManagementBubbleComboBox)));
  }

  CounterExpandButton* GetStudentExpandButtonView() const {
    return views::AsViewClass<CounterExpandButton>(
        GetStudentView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  views::View* GetStudentItemContainerView() const {
    return views::AsViewClass<views::View>(
        GetStudentView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListContainer)));
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
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  std::unique_ptr<FakeGlanceablesClassroomClient>
      fake_glanceables_classroom_client_;
};

// -----------------------------------------------------------------------------

class GlanceablesMvpBrowserTest : public GlanceablesBrowserTest {
 public:
  GlanceablesMvpBrowserTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlanceablesTimeManagementClassroomStudentView},
        /*disabled_features=*/{features::kGlanceablesTimeManagementTasksView});
  }

  void SetUpOnMainThread() override {
    GlanceablesBrowserTest::SetUpOnMainThread();
    base::AddFeatureIdTagToTestResult(
        "screenplay-ace3b729-5402-40cd-b2bf-d488bc95b7e2");
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlanceablesMvpBrowserTest, OpenStudentCourseItemURL) {
  ASSERT_TRUE(glanceables_controller()->GetClassroomClient());

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

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
  SetStudentAssignmentsCount(101);

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetStudentView());

  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetStudentView()->GetBoundsInScreen()));

  // Click the "See All" button in the student glanceable footer, and check that
  // the correct URL is opened.
  GetStudentFooterSeeAllButton()->ScrollViewToVisible();
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
  ToggleDateTray();

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

// -----------------------------------------------------------------------------

class GlanceablesTasksBrowserTest : public GlanceablesBrowserTest {
 public:
  GlanceablesTasksBrowserTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kGlanceablesTimeManagementTasksView},
        /*disabled_features=*/{
            features::kGlanceablesTimeManagementClassroomStudentView});
  }

  void SetUpOnMainThread() override {
    GlanceablesBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(glanceables_controller()->GetTasksClient());
  }

  // Returns the task view at `item_index`.
  GlanceablesTaskView* GetTaskItemView(int item_index) {
    return views::AsViewClass<GlanceablesTaskView>(
        GetTasksItemContainerView()->children()[item_index]);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, ViewAndSwitchTaskLists) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-07815625-e657-471e-80b4-73fca7bd939b");  // view
  base::AddFeatureIdTagToTestResult(
      "screenplay-1b44afe6-b6fa-4391-92dd-7063555091ff");  // switch

  EXPECT_FALSE(GetGlanceableTrayBubble());

  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

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

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, CheckOffTaskItems) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-399a1737-5502-4bba-a909-2dbe5a97e2c7");

  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  EXPECT_FALSE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_FALSE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());

  // Click to check off the first task item and check that it has been marked
  // complete.
  GetEventGenerator()->MoveMouseTo(GetTaskItemView(/*item_index=*/0)
                                       ->GetCheckButtonForTest()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_FALSE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());

  // Click to check off the second task item and check that it has been marked
  // complete.
  GetEventGenerator()->MoveMouseTo(GetTaskItemView(/*item_index=*/1)
                                       ->GetCheckButtonForTest()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/0)->GetCompletedForTest());
  EXPECT_TRUE(GetTaskItemView(/*item_index=*/1)->GetCompletedForTest());
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, AddTaskItem) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-14a3974a-cb94-44fb-8a70-95e5f761fa2d");

  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  ASSERT_TRUE(GetGlanceableTrayBubble());
  ASSERT_TRUE(GetTasksView());

  const auto* const add_task_button = GetAddNewTaskButton();
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
    EXPECT_FALSE(title_label);
    ASSERT_TRUE(title_text_field);
    EXPECT_TRUE(title_text_field->IsDrawn());
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
    base::RunLoop().RunUntilIdle();
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
    EXPECT_TRUE(title_label->IsDrawn());
    EXPECT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"New task");
  }
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, EditTaskItem) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-aa9616ed-117c-4d9f-b223-f2e603506780");

  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

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
    EXPECT_TRUE(title_label->IsDrawn());
    EXPECT_FALSE(title_text_field);
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
    EXPECT_FALSE(title_label);
    ASSERT_TRUE(title_text_field);
    EXPECT_TRUE(title_text_field->IsDrawn());
    EXPECT_EQ(title_text_field->GetText(), u"Task List 1 Item 1 Title");

    // Append " upd" text.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_U);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_P);
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_D);

    // Finish editing by pressing Esc key.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
    base::RunLoop().RunUntilIdle();
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
    EXPECT_TRUE(title_label->IsDrawn());
    EXPECT_EQ(title_label->GetText(), u"Task List 1 Item 1 Title upd");
    EXPECT_FALSE(title_text_field);
  }
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, TasksViewLayout) {
  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  ASSERT_TRUE(GetGlanceableTrayBubble());
  ASSERT_TRUE(GetTasksView());

  // Calculate the available space for tasks and make sure there is enough for
  // additional task view.
  auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int kGlanceableMargins = 8;
  const int kCalendarHeight = 340;
  const int available_height_for_tasks =
      display.work_area().height() - kCalendarHeight - kGlanceableMargins;
  const int original_task_view_height = GetTasksView()->height();
  ASSERT_GT(available_height_for_tasks, original_task_view_height);

  const auto* const add_task_button = GetAddNewTaskButton();
  ASSERT_TRUE(add_task_button);

  const auto* const task_items_container = GetTasksItemContainerView();
  ASSERT_TRUE(task_items_container);

  // Use the visibility of the scroll bar to determine if the contents of the
  // scroll view is larger than its viewport. In this case, they should have the
  // same sizes.
  const auto* scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_FALSE(scroll_bar->GetVisible());

  // Click on `add_task_button` and verify that `task_items_container` has the
  // new "pending" item.
  EXPECT_EQ(task_items_container->children().size(), 2u);
  GetEventGenerator()->MoveMouseTo(
      add_task_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(task_items_container->children().size(), 3u);
  GetTasksView()->EndResizeAnimationForTest();
  GetTasksView()->GetWidget()->LayoutRootViewIfNecessary();

  // The tasks view should update its height if there is space available.
  EXPECT_GT(GetTasksView()->height(), original_task_view_height);
  EXPECT_FALSE(scroll_bar->GetVisible());

  // Commit the empty new task, which removes the temporary task view.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  GetTasksView()->EndResizeAnimationForTest();
  GetTasksView()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(task_items_container->children().size(), 2u);

  // Verify that the tasks view height is resized to its original height without
  // the new task.
  EXPECT_EQ(GetTasksView()->height(), original_task_view_height);
  EXPECT_FALSE(scroll_bar->GetVisible());
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, ShowsCachedDataBasic) {
  auto* const client = fake_glanceables_tasks_client();
  client->set_paused_on_fetch(true);

  // Click the date tray to show the glanceable bubbles. For the first time the
  // glanceables are shown, the tasks need to be fetched and the view should not
  // be shown before the data returns.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetGlanceableTrayBubble());
  ASSERT_FALSE(GetTasksView());

  client->RunPendingGetTaskListsCallbacks();
  client->RunPendingGetTasksCallbacks();
  ASSERT_TRUE(GetTasksView());

  // Close the glanceables.
  ToggleDateTray();
  ASSERT_FALSE(GetGlanceableTrayBubble());

  // The second and following times when the tasks are shown, the cached
  // tasks should be shown while waiting the new change to be fetched.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetGlanceableTrayBubble());
  ASSERT_TRUE(GetTasksView());
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest,
                       CachedTaskListAreUpdatedAfterFetch) {
  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  // Close the glanceables.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Turn on the pause_on_fetch to verify the cached tasks and the updated
  // tasks.
  auto* const client = fake_glanceables_tasks_client();
  client->set_paused_on_fetch(true);

  // Add a task in Task List 1 directly via the client as an updated task.
  client->AddTask(
      /*task_list_id=*/"TaskListID1",
      std::make_unique<api::Task>(
          /*id=*/"TaskListItem5", /*title=*/"Task List 1 Item 3 Title",
          /*due=*/base::Time::Now(), /*completed=*/false,
          /*has_subtasks=*/false, /*has_email_link=*/false,
          /*has_notes=*/false, /*updated=*/base::Time::Now(),
          /*web_view_link=*/GURL(), api::Task::OriginSurfaceType::kRegular));

  // Open the glanceables again.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Check that only the cached task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  client->RunPendingGetTaskListsCallbacks();
  EXPECT_FALSE(GetTasksView()->GetCanProcessEventsWithinSubtree());
  client->RunPendingGetTasksCallbacks();

  // After running the get callbacks, the newly added task is shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>({"Task List 1 Item 1 Title",
                                      "Task List 1 Item 2 Title",
                                      "Task List 1 Item 3 Title"}));
  EXPECT_TRUE(GetTasksView()->GetCanProcessEventsWithinSubtree());
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest,
                       UpdateShownListIfCachedTaskListDeleted) {
  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that task list items from the first list are shown.
  const auto* combobox = GetTasksComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  // Close the glanceables.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Turn on the pause_on_fetch to verify the cached tasks and the updated
  // tasks.
  auto* const client = fake_glanceables_tasks_client();
  client->set_paused_on_fetch(true);

  // Delete the task list that was shown.
  client->DeleteTaskList("TaskListID1");

  // Open the glanceables again.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Check that deleted list is still showing as it is cached.
  combobox = GetTasksComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  client->RunPendingGetTaskListsCallbacks();
  client->RunPendingGetTasksCallbacks();

  // After running the get callbacks, the task list shown is updated.
  combobox = GetTasksComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 2 Title");
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>({"Task List 2 Item 1 Title",
                                      "Task List 2 Item 2 Title",
                                      "Task List 2 Item 3 Title"}));
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, DontShowTasksIfNoNetwork) {
  fake_glanceables_tasks_client()->set_get_task_lists_error(true);

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_FALSE(GetTasksView());
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest,
                       ShowFailedToLoadViewIfNoNetwork) {
  fake_glanceables_tasks_client()->set_get_tasks_error(true);

  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  auto* error_view = views::AsViewClass<ErrorMessageToast>(
      GetTasksView()->GetViewByID(base::to_underlying(
          GlanceablesViewId::kTimeManagementErrorMessageToast)));
  ASSERT_TRUE(error_view);
  EXPECT_EQ(error_view->GetMessageForTest(), u"Couldn't load items.");
  EXPECT_EQ(error_view->GetButtonForTest()->GetText(), u"Reload");

  // Reset the error flag so that the next tasks fetch will succeed.
  fake_glanceables_tasks_client()->set_get_tasks_error(false);

  GetEventGenerator()->MoveMouseTo(
      error_view->GetButtonForTest()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(GetTasksView()->GetViewByID(base::to_underlying(
      GlanceablesViewId::kTimeManagementErrorMessageToast)));
  auto* combobox = GetTasksComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest, SwitchTaskListsWithError) {
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Set the error flag to true so that it fails on the next time the tasks are
  // fetched.
  fake_glanceables_tasks_client()->set_get_tasks_error(true);

  // Check that task list items from the first list are shown.
  const auto* combobox = GetTasksComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");

  // Click on the combo box to show the task lists.
  GetEventGenerator()->MoveMouseTo(combobox->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  views::Label* second_menu_item_label =
      FindMenuItemLabelWithString(u"Task List 2 Title");

  // Click on the second menu item label to switch to the second task list.
  ASSERT_TRUE(second_menu_item_label);
  GetEventGenerator()->MoveMouseTo(
      second_menu_item_label->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  base::RunLoop().RunUntilIdle();

  // Failing to update the task list will reset the combobox to the task list
  // before switching.
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");

  auto* error_view = views::AsViewClass<ErrorMessageToast>(
      GetTasksView()->GetViewByID(base::to_underlying(
          GlanceablesViewId::kTimeManagementErrorMessageToast)));
  ASSERT_TRUE(error_view);
  EXPECT_EQ(error_view->GetMessageForTest(), u"Couldn't load items.");
  EXPECT_EQ(error_view->GetButtonForTest()->GetText(), u"Dismiss");
}

IN_PROC_BROWSER_TEST_F(GlanceablesTasksBrowserTest,
                       SavelyRemoveTaskViewInEditState) {
  // Click the date tray to show the glanceable bubbles.
  ToggleDateTray();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that task list items from the first list are shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  // Close the glanceables.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Turn on the pause_on_fetch to pause in between the cached tasks is shown
  // and the tasks has started fetching.
  auto* const client = fake_glanceables_tasks_client();
  client->set_paused_on_fetch(true);

  // Delete the whole task list.
  client->DeleteTaskList(/*task_list_id=*/"TaskListID1");

  // Open the glanceables again.
  ToggleDateTray();
  base::RunLoop().RunUntilIdle();

  // Check that the deleted task list is still shown.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>(
                {"Task List 1 Item 1 Title", "Task List 1 Item 2 Title"}));

  GetTasksView()->GetWidget()->LayoutRootViewIfNecessary();

  // Before fetch, click on the cached task and see if the textfield shows up.
  auto* first_task_view_label =
      GetTaskItemView(/*item_index=*/0)
          ->GetViewByID(
              base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel));
  GetEventGenerator()->MoveMouseTo(
      first_task_view_label->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  auto* first_task_view_textfield =
      GetTaskItemView(/*item_index=*/0)
          ->GetViewByID(
              base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField));
  ASSERT_TRUE(first_task_view_textfield);
  ASSERT_TRUE(first_task_view_textfield->GetVisible());

  // Start fetching new data.
  client->RunPendingGetTaskListsCallbacks();
  EXPECT_FALSE(GetTasksView()->GetCanProcessEventsWithinSubtree());
  client->RunPendingGetTasksCallbacks();
  EXPECT_TRUE(GetTasksView()->GetCanProcessEventsWithinSubtree());

  // Check if the second list is shown after fetch and nothing crashed.
  EXPECT_EQ(GetCurrentTaskListItemTitles(),
            std::vector<std::string>({"Task List 2 Item 1 Title",
                                      "Task List 2 Item 2 Title",
                                      "Task List 2 Item 3 Title"}));
}

}  // namespace ash
