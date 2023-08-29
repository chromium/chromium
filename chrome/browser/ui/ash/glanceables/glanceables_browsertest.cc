// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
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

// Tests for the glanceables feature, which adds a "welcome back" screen on
// some logins.
class GlanceablesBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // The test harness adds --no-first-run. Remove it so glanceables show up.
    command_line->RemoveSwitch(::switches::kNoFirstRun);

    // Don't open a browser window, because doing so would hide glanceables.
    // Note that InProcessBrowserTest::browser() will be null.
    command_line->AppendSwitch(::switches::kNoStartupWindow);

    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void CreateAndStartUserSession() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, kTestUserName, false);

    profile_ = &profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(),
        ProfileHelper::GetProfilePathByUserIdHash(
            user_manager::UserManager::Get()
                ->FindUser(account_id)
                ->username_hash()));

    session_manager->NotifyUserProfileLoaded(account_id);
    session_manager->SessionStarted();
  }

  GlanceablesController* glanceables_controller() {
    return Shell::Get()->glanceables_controller();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfileIfExists(profile_);
  }

 protected:
  base::test::ScopedFeatureList features_{features::kGlanceables};
  raw_ptr<Profile, DanglingUntriaged | ExperimentalAsh> profile_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(GlanceablesBrowserTest, ShowsAndHide) {
  // Not showing on the login screen.
  EXPECT_FALSE(glanceables_controller()->IsShowing());

  CreateAndStartUserSession();

  // Not showing right after login if there's no refresh token yet.
  EXPECT_FALSE(glanceables_controller()->IsShowing());

  // Makes the primary account available, generates a refresh token and runs
  // `IdentityManager` callbacks for signin success.
  signin::MakePrimaryAccountAvailable(identity_manager(), kTestUserName,
                                      signin::ConsentLevel::kSignin);

  // Showing once the refresh token is loaded.
  EXPECT_TRUE(glanceables_controller()->IsShowing());

  // Open a browser window.
  CreateBrowser(ProfileManager::GetLastUsedProfile());

  // Glanceables should close because a window opened.
  EXPECT_FALSE(glanceables_controller()->IsShowing());
}

class GlanceablesV2BrowserTest : public InProcessBrowserTest {
 public:
  GlanceablesV2Controller* glanceables_controller() {
    return Shell::Get()->glanceables_v2_controller();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    base::AddFeatureIdTagToTestResult(
        "screenplay-ace3b729-5402-40cd-b2bf-d488bc95b7e2");

    base::Time date;
    ASSERT_TRUE(base::Time::FromString(kDueDate, &date));
    fake_glanceables_tasks_client_ =
        std::make_unique<FakeGlanceablesTasksClient>(date);
    fake_glanceables_classroom_client_ =
        std::make_unique<FakeGlanceablesClassroomClient>(
            glanceables_controller()->GetClassroomClient());

    Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
        account_id_,
        GlanceablesV2Controller::ClientsRegistration{
            .classroom_client = fake_glanceables_classroom_client_.get(),
            .tasks_client = fake_glanceables_tasks_client_.get()});
    Shell::Get()->glanceables_v2_controller()->OnActiveUserSessionChanged(
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

  FakeGlanceablesTasksClient* fake_glanceables_tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  TasksBubbleView* GetTasksView() const {
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
  std::unique_ptr<FakeGlanceablesTasksClient> fake_glanceables_tasks_client_;
  std::unique_ptr<FakeGlanceablesClassroomClient>
      fake_glanceables_classroom_client_;

  base::test::ScopedFeatureList features_{features::kGlanceablesV2};
};

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest, OpenStudentCourseItemURL) {
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

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest, ClickSeeAllStudentButton) {
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

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest,
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

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest, ViewAndSwitchTaskLists) {
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

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest, ClickSeeAllTasksButton) {
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

IN_PROC_BROWSER_TEST_F(GlanceablesV2BrowserTest, CheckOffTaskItems) {
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

}  // namespace ash
