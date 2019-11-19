// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"

namespace {

AppMenuButton* GetAppMenuButtonFromBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetAppMenuButton();
}

class AppMenuShowingWaiter : public AppMenuButtonObserver {
 public:
  explicit AppMenuShowingWaiter(AppMenuButton* button);
  ~AppMenuShowingWaiter() override = default;

  // AppMenuButtonObserver:
  void AppMenuShown() override;

  void Wait();

 private:
  bool observed_ = false;
  base::RunLoop run_loop_;
  ScopedObserver<AppMenuButton, AppMenuButtonObserver> observer_{this};
};

AppMenuShowingWaiter::AppMenuShowingWaiter(AppMenuButton* button) {
  DCHECK(button);
  if (button->IsMenuShowing())
    observed_ = true;
  observer_.Add(button);
}

void AppMenuShowingWaiter::AppMenuShown() {
  observed_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

void AppMenuShowingWaiter::Wait() {
  if (!observed_)
    run_loop_.Run();
}

// Tests clicking on an overflowed toolbar action. This is called when the app
// menu is open, and handles actually clicking on the action.
// |button| specifies the mouse button to click with. Optionally
// |toolbar_action_view| can be provided to receive the targeted
// ToolbarActionView.
void TestOverflowedToolbarAction(Browser* browser,
                                 ui_controls::MouseButton button,
                                 ToolbarActionView** toolbar_action_view) {
  // A bunch of plumbing to safely get at the overflowed toolbar action.
  auto* app_menu_button = GetAppMenuButtonFromBrowser(browser);
  EXPECT_TRUE(app_menu_button->IsMenuShowing());
  AppMenu* app_menu = app_menu_button->app_menu();
  ASSERT_TRUE(app_menu);
  ExtensionToolbarMenuView* menu_view =
      app_menu->extension_toolbar_for_testing();
  ASSERT_TRUE(menu_view);
  BrowserActionsContainer* overflow_container =
      menu_view->container_for_testing();
  ASSERT_TRUE(overflow_container);
  ToolbarActionView* action_view =
      overflow_container->GetToolbarActionViewAt(0);
  EXPECT_TRUE(action_view->GetVisible());

  // Click on the toolbar action to activate it.
  gfx::Point action_view_loc =
      ui_test_utils::GetCenterInScreenCoordinates(action_view);
  EXPECT_TRUE(
      ui_controls::SendMouseMove(action_view_loc.x(), action_view_loc.y()));
  EXPECT_TRUE(ui_controls::SendMouseClick(button));
  base::RunLoop().RunUntilIdle();

  if (toolbar_action_view)
    *toolbar_action_view = action_view;
}

// Tests the context menu of an overflowed action.
void TestWhileContextMenuOpen(Browser* browser,
                              ToolbarActionView* context_menu_action) {
  views::MenuItemView* menu_root =
      context_menu_action->context_menu_controller_for_testing()
          ->menu_for_testing();
  ASSERT_TRUE(menu_root);
  ASSERT_TRUE(menu_root->GetSubmenu());
  EXPECT_TRUE(menu_root->GetSubmenu()->IsShowing());
  views::MenuItemView* first_menu_item =
      menu_root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(first_menu_item);

  // Make sure we're showing the right context menu.
  EXPECT_EQ(base::UTF8ToUTF16("Browser Action Popup"),
            first_menu_item->title());
  EXPECT_TRUE(first_menu_item->GetEnabled());

  // Get the overflow container.
  auto* app_menu_button = GetAppMenuButtonFromBrowser(browser);
  AppMenu* app_menu = app_menu_button->app_menu();
  ASSERT_TRUE(app_menu);
  ExtensionToolbarMenuView* menu_view =
      app_menu->extension_toolbar_for_testing();
  ASSERT_TRUE(menu_view);
  BrowserActionsContainer* overflow_container =
      menu_view->container_for_testing();
  ASSERT_TRUE(overflow_container);

  // Get the first action on the second row of the overflow container.
  int second_row_index = overflow_container->toolbar_actions_bar()
                             ->platform_settings()
                             .icons_per_overflow_menu_row;
  ToolbarActionView* second_row_action =
      overflow_container->GetToolbarActionViewAt(second_row_index);

  EXPECT_TRUE(second_row_action->GetVisible());
  EXPECT_TRUE(second_row_action->GetEnabled());

  gfx::Point action_view_loc =
      ui_test_utils::GetCenterInScreenCoordinates(second_row_action);
  gfx::Point action_view_loc_in_menu_item_bounds = action_view_loc;
  views::View::ConvertPointFromScreen(first_menu_item,
                                      &action_view_loc_in_menu_item_bounds);
  // Regression test for crbug.com/538414: The first menu item is overlapping
  // the second row action button. With crbug.com/538414, the click would go to
  // the menu button, instead of the menu item.
  EXPECT_TRUE(
      first_menu_item->HitTestPoint(action_view_loc_in_menu_item_bounds));

  // Click on the first menu item (which shares bounds, but overlaps, the second
  // row action).
  EXPECT_TRUE(
      ui_controls::SendMouseMove(action_view_loc.x(), action_view_loc.y()));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  base::RunLoop().RunUntilIdle();
  // Test resumes in the main test body.
}

}  // namespace

class ToolbarActionViewInteractiveUITest
    : public extensions::ExtensionBrowserTest {
 protected:
  ToolbarActionViewInteractiveUITest();
  ToolbarActionViewInteractiveUITest(
      const ToolbarActionViewInteractiveUITest&) = delete;
  ToolbarActionViewInteractiveUITest& operator=(
      const ToolbarActionViewInteractiveUITest&) = delete;
  ~ToolbarActionViewInteractiveUITest() override;

  // extensions::ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;
};

ToolbarActionViewInteractiveUITest::ToolbarActionViewInteractiveUITest() {}
ToolbarActionViewInteractiveUITest::~ToolbarActionViewInteractiveUITest() {}

void ToolbarActionViewInteractiveUITest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

void ToolbarActionViewInteractiveUITest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
}

// TODO(crbug.com/963678): fails on ChromeOS as it's assuming SendMouseMove()
// synchronously updates the location of the mouse (which is needed by
// SendMouseClick()).
#if defined(OS_CHROMEOS)
// TODO(pkasting): https://crbug.com/911374 Menu controller thinks the mouse is
// already down when handling the left click.
#define MAYBE_TestClickingOnOverflowedAction \
  DISABLED_TestClickingOnOverflowedAction
#else
#define MAYBE_TestClickingOnOverflowedAction TestClickingOnOverflowedAction
#endif
// Tests clicking on an overflowed extension action.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_TestClickingOnOverflowedAction) {
  // Load an extension.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Reduce visible count to 0 so that the action is overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  // When the extension is activated, it will send a message that its popup
  // opened. Listen for the message.
  ExtensionTestMessageListener listener("Popup opened", false);

  auto* app_menu_button = GetAppMenuButtonFromBrowser(browser());

  // Click on the app button.
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  EXPECT_TRUE(
      ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y()));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  AppMenuShowingWaiter waiter(app_menu_button);
  waiter.Wait();

  TestOverflowedToolbarAction(browser(), ui_controls::LEFT, nullptr);

  // The app menu should no longer be showing.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());

  // And the extension should have been activated.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

#if defined(OS_CHROMEOS)
// TODO(pkasting): https://crbug.com/911374 Menu controller thinks the mouse is
// already down when handling the left click.
#define MAYBE_TestContextMenuOnOverflowedAction \
  DISABLED_TestContextMenuOnOverflowedAction
#else
#define MAYBE_TestContextMenuOnOverflowedAction \
  TestContextMenuOnOverflowedAction
#endif
// Tests the context menus of overflowed extension actions.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_TestContextMenuOnOverflowedAction) {
  views::MenuController::TurnOffMenuSelectionHoldForTest();
  views::test::DisableMenuClosureAnimations();

  // Load an extension that has a home page (important for the context menu's
  // first item being enabled).
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Aaaannnnd... Load a bunch of other extensions so that the overflow menu
  // is spread across multiple rows.
  for (int i = 0; i < 15; ++i) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
            .SetLocation(extensions::Manifest::INTERNAL)
            .Build();
    extension_service()->AddExtension(extension.get());
  }

  const auto* const actions_bar = browser()->window()->GetToolbarActionsBar();
  ASSERT_EQ(16u, actions_bar->toolbar_actions_unordered().size());

  // Reduce visible count to 0 so that all actions are overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  auto* app_menu_button = GetAppMenuButtonFromBrowser(browser());
  // Click on the app button.
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  EXPECT_TRUE(
      ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y()));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  AppMenuShowingWaiter waiter(app_menu_button);
  waiter.Wait();

  // Right click on the action view. This should trigger the context menu.
  ToolbarActionView* action_view = nullptr;
  TestOverflowedToolbarAction(browser(), ui_controls::RIGHT, &action_view);

  // Ensure that the menu actually opened.
  EXPECT_TRUE(action_view->IsMenuRunningForTesting());

  // Trigger the action within the context menu. This should load the extension
  // webpage, and close the menu.
  TestWhileContextMenuOpen(browser(), action_view);

  EXPECT_FALSE(action_view->IsMenuRunningForTesting());
  // We should have navigated to the extension's home page, which is google.com.
  EXPECT_EQ(
      GURL("https://www.google.com/"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// Tests that clicking on the toolbar action a second time when the action is
// already open results in closing the popup, and doesn't re-open it.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       DoubleClickToolbarActionToClose) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  BrowserActionsContainer* browser_actions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->browser_actions();
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_container->toolbar_actions_bar();
  ToolbarActionView* toolbar_action_view =
      browser_actions_container->GetToolbarActionViewAt(0);

  // When the extension is activated, it will send a message that its popup
  // opened. Listen for the message.
  ExtensionTestMessageListener listener("Popup opened", false);

  // Click on the action, and wait for the popup to fully load.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
     ui_test_utils::GetCenterInScreenCoordinates(toolbar_action_view)));

  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionActionViewController* view_controller =
      static_cast<ExtensionActionViewController*>(
          toolbar_action_view->view_controller());
  EXPECT_EQ(view_controller, toolbar_actions_bar->popup_owner());
  EXPECT_TRUE(view_controller->IsShowingPopup());

  // Click down on the action button; this should close the popup, though the
  // closure may be asynchronous.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));
  observer.Wait();
  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, toolbar_actions_bar->popup_owner());

  // Releasing the mouse shouldn't result in the popup being shown again.
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, toolbar_actions_bar->popup_owner());
}

// TODO(crbug.com/963678): fails on ChromeOS as it's assuming SendMouseMove()
// synchronously updates the location of the mouse (which is needed by
// SendMouseClick()).
#if defined(OS_CHROMEOS)
#define MAYBE_ActivateOverflowedToolbarActionWithKeyboard \
  DISABLED_ActivateOverflowedToolbarActionWithKeyboard
#else
#define MAYBE_ActivateOverflowedToolbarActionWithKeyboard \
  ActivateOverflowedToolbarActionWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_ActivateOverflowedToolbarActionWithKeyboard) {
  views::MenuController::TurnOffMenuSelectionHoldForTest();
  // Load an extension with an action.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Reduce visible count to 0 so that all actions are overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  // Set up a listener for the extension being triggered.
  ExtensionTestMessageListener listener("Popup opened", false);

  // Open the app menu, navigate to the toolbar action, and activate it via the
  // keyboard.
  auto* app_menu_button = GetAppMenuButtonFromBrowser(browser());
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  EXPECT_TRUE(
      ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y()));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  AppMenuShowingWaiter waiter(app_menu_button);
  waiter.Wait();

  EXPECT_TRUE(app_menu_button->IsMenuShowing());
  gfx::NativeWindow native_window =
      views::MenuController::GetActiveInstance()->owner()->GetNativeWindow();
  // Send a key down event followed by the return key.
  // The key down event targets the toolbar action in the app menu.
  EXPECT_TRUE(ui_controls::SendKeyPress(native_window, ui::VKEY_DOWN, false,
                                        false, false, false));
  // The triggering of the action and subsequent widget destruction occurs on
  // the message loop. Wait for this all to complete.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      native_window, ui::VKEY_RETURN, false, false, false, false));

  // The menu should be closed.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());
  // And the extension should have been activated.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}
