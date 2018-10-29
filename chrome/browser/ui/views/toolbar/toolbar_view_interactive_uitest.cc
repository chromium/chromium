// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <stddef.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;

class ToolbarViewInteractiveUITest : public extensions::ExtensionBrowserTest {
 public:
  ToolbarViewInteractiveUITest();
  ~ToolbarViewInteractiveUITest() override;

 protected:
  ToolbarView* toolbar_view() { return toolbar_view_; }
  BrowserActionsContainer* browser_actions() { return browser_actions_; }

  // Performs a drag-and-drop operation by moving the mouse to |start|, clicking
  // the left button, moving the mouse to |end|, and releasing the left button.
  // TestWhileInDragOperation() is called after the mouse has moved to |end|,
  // but before the click is released.
  void DoDragAndDrop(const gfx::Point& start, const gfx::Point& end);

  // A function to perform testing actions while in the drag operation from
  // DoDragAndDrop.
  void TestWhileInDragOperation();

 private:
  // Finishes the drag-and-drop operation started in DoDragAndDrop().
  void FinishDragAndDrop(base::Closure quit_closure);

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  ToolbarView* toolbar_view_;

  BrowserActionsContainer* browser_actions_;

  // The drag-and-drop background thread.
  std::unique_ptr<base::Thread> dnd_thread_;
};

ToolbarViewInteractiveUITest::ToolbarViewInteractiveUITest()
    : toolbar_view_(NULL),
      browser_actions_(NULL) {
}

ToolbarViewInteractiveUITest::~ToolbarViewInteractiveUITest() {
}

void ToolbarViewInteractiveUITest::DoDragAndDrop(const gfx::Point& start,
                                                 const gfx::Point& end) {
  // Much of this function is modeled after methods in ViewEventTestBase (in
  // particular, the |dnd_thread_|, but it's easier to move that here than try
  // to make ViewEventTestBase play nice with a BrowserView (for the toolbar).
  // TODO(devlin): In a perfect world, this would be factored better.

  // Send the mouse to |start|, and click.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(start));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner();

  ui_controls::SendMouseMoveNotifyWhenDone(
      end.x() + 10, end.y(),
      base::BindOnce(&ToolbarViewInteractiveUITest::FinishDragAndDrop,
                     base::Unretained(this), runner->QuitClosure()));

  // Also post a move task to the drag and drop thread.
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new base::Thread("mouse_move_thread"));
    dnd_thread_->Start();
  }

  dnd_thread_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove), end.x(),
                     end.y()),
      base::TimeDelta::FromMilliseconds(200));
  runner->Run();

  // The app menu should have closed once the drag-and-drop completed.
  EXPECT_FALSE(toolbar_view()->app_menu_button()->IsMenuShowing());
}

void ToolbarViewInteractiveUITest::TestWhileInDragOperation() {
  EXPECT_TRUE(toolbar_view()->app_menu_button()->IsMenuShowing());
}

void ToolbarViewInteractiveUITest::FinishDragAndDrop(
    base::Closure quit_closure) {
  dnd_thread_.reset();
  TestWhileInDragOperation();
  ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT, ui_controls::UP,
                                             quit_closure);
}

void ToolbarViewInteractiveUITest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  ToolbarActionsBar::disable_animations_for_testing_ = true;
  BrowserAppMenuButton::g_open_app_immediately_for_testing = true;
}

void ToolbarViewInteractiveUITest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  ExtensionToolbarMenuView::set_close_menu_delay_for_testing(0);

  toolbar_view_ = BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  browser_actions_ = toolbar_view_->browser_actions();
}

void ToolbarViewInteractiveUITest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  BrowserAppMenuButton::g_open_app_immediately_for_testing = false;
}

// Borrowed from chrome/browser/ui/views/bookmarks/bookmark_bar_view_test.cc,
// since these are also disabled on Linux for drag and drop.
// TODO(erg): Fix DND tests on linux_aura. crbug.com/163931
#if defined(OS_LINUX) && defined(USE_AURA)
#define MAYBE_TestAppMenuOpensOnDrag DISABLED_TestAppMenuOpensOnDrag
#elif defined(OS_MACOSX)
// Illegal thread join on the UI thread, may fix above: http://crbug.com/824570
#define MAYBE_TestAppMenuOpensOnDrag DISABLED_TestAppMenuOpensOnDrag
#else
#define MAYBE_TestAppMenuOpensOnDrag TestAppMenuOpensOnDrag
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewInteractiveUITest,
                       MAYBE_TestAppMenuOpensOnDrag) {
  // Load an extension that has a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  ASSERT_EQ(1u, browser_actions()->VisibleBrowserActions());

  ToolbarActionView* view = browser_actions()->GetToolbarActionViewAt(0);
  ASSERT_TRUE(view);

  gfx::Point browser_action_view_loc =
      ui_test_utils::GetCenterInScreenCoordinates(view);
  gfx::Point app_button_loc = ui_test_utils::GetCenterInScreenCoordinates(
      toolbar_view()->app_menu_button());

  // Perform a drag and drop from the browser action view to the app button,
  // which should open the app menu.
  DoDragAndDrop(browser_action_view_loc, app_button_loc);
}

class ToolbarViewTest : public InProcessBrowserTest {
 public:
  ToolbarViewTest() {}

  void RunToolbarCycleFocusTest(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarViewTest);
};

void ToolbarViewTest::RunToolbarCycleFocusTest(Browser* browser) {
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  CommandUpdater* updater = browser->command_controller();

  // Send focus to the toolbar as if the user pressed Alt+Shift+T.
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  views::test::WidgetActivationWaiter waiter(widget, true);
  waiter.Wait();

  views::View* first_view = focus_manager->GetFocusedView();
  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(false);
    view = focus_manager->GetFocusedView();
    ids.push_back(view->id());
    if (view->id() == VIEW_ID_RELOAD_BUTTON)
      found_reload = true;
    if (view->id() == VIEW_ID_APP_MENU)
      found_app_menu = true;
    if (view->id() == VIEW_ID_OMNIBOX)
      found_location_bar = true;
    if (ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Make sure we found a few key items.
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(true);
    view = focus_manager->GetFocusedView();
    reverse_ids.push_back(view->id());
    if (reverse_ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Assert that the views were focused in exactly the reverse order.
  // The sequences should be the same length, and the last element will
  // be the same, and the others are reverse.
  ASSERT_EQ(ids.size(), reverse_ids.size());
  size_t count = ids.size();
  for (size_t i = 0; i < count - 1; i++)
    EXPECT_EQ(ids[i], reverse_ids[count - 2 - i]);
}

#if defined(OS_MACOSX)
// Widget activation doesn't work on Mac: https://crbug.com/823543
#define MAYBE_ToolbarCycleFocus DISABLED_ToolbarCycleFocus
#else
#define MAYBE_ToolbarCycleFocus ToolbarCycleFocus
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest, MAYBE_ToolbarCycleFocus) {
  RunToolbarCycleFocusTest(browser());
}

#if defined(OS_MACOSX)
// Widget activation doesn't work on Mac: https://crbug.com/823543
#define MAYBE_ToolbarCycleFocusWithBookmarkBar \
  DISABLED_ToolbarCycleFocusWithBookmarkBar
#else
#define MAYBE_ToolbarCycleFocusWithBookmarkBar ToolbarCycleFocusWithBookmarkBar
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest,
                       MAYBE_ToolbarCycleFocusWithBookmarkBar) {
  CommandUpdater* updater = browser()->command_controller();
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::AddIfNotBookmarked(model, GURL("http://foo.com"),
                                base::ASCIIToUTF16("Foo"));

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  Browser* second_browser = CreateBrowser(browser()->profile());
  RunToolbarCycleFocusTest(second_browser);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest, BackButtonUpdate) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  EXPECT_FALSE(toolbar->back_button()->enabled());

  // Navigate to title1.html. Back button should be enabled.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(toolbar->back_button()->enabled());

  // Delete old navigations. Back button will be disabled.
  auto& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller.DeleteNavigationEntries(base::BindRepeating(
      [&](const content::NavigationEntry& entry) { return true; }));
  EXPECT_FALSE(toolbar->back_button()->enabled());
}
