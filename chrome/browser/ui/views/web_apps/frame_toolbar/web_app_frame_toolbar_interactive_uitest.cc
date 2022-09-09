// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view.h"
#include "url/gurl.h"

// Param: DesktopPWAsElidedExtensionsMenu feature.
class WebAppFrameToolbarInteractiveUITest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppFrameToolbarInteractiveUITest() {
    feature_list_.InitWithFeatureState(
        ::features::kDesktopPWAsElidedExtensionsMenu, IsExtensionsMenuElided());
  }

  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }

 protected:
  bool IsExtensionsMenuElided() const { return GetParam(); }

 private:
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that for minimal-ui web apps, the toolbar keyboard focus cycles
// among the toolbar buttons: the reload button, the extensions menu button, and
// the app menu button, in that order.
//
// TODO(https://crbug.com/1176121): Re-enable after fixing flakiness.
IN_PROC_BROWSER_TEST_P(WebAppFrameToolbarInteractiveUITest,
                       DISABLED_CycleFocus) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("simple_with_icon/")));

  const GURL app_url("https://test.org");
  helper()->InstallAndLaunchWebApp(browser(), app_url);

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  ui_test_utils::BrowserActivationWaiter waiter(helper()->app_browser());
  waiter.WaitForActivation();

  // Wait for the extensions menu button to appear.
  ExtensionsToolbarContainer* extensions_container =
      helper()->web_app_frame_toolbar()->GetExtensionsToolbarContainer();
  views::test::ReduceAnimationDuration(extensions_container);
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  // Send focus to the toolbar as if the user pressed Alt+Shift+T.
  helper()->app_browser()->command_controller()->ExecuteCommand(
      IDC_FOCUS_TOOLBAR);

  // After focusing the toolbar, the reload button should immediately have focus
  // because the back button is disabled (no navigation yet).
  views::FocusManager* const focus_manager =
      helper()->browser_view()->GetFocusManager();
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);

  // Press Tab to cycle through controls until we end up back where we started.
  // This approach is similar to ToolbarViewTest::RunToolbarCycleFocusTest().
  if (!IsExtensionsMenuElided()) {
    focus_manager->AdvanceFocus(false);
    EXPECT_EQ(focus_manager->GetFocusedView()->GetID(),
              VIEW_ID_EXTENSIONS_MENU_BUTTON);
  }
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_APP_MENU);
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);

  // Now press Shift-Tab to cycle backwards.
  focus_manager->AdvanceFocus(true);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_APP_MENU);
  if (!IsExtensionsMenuElided()) {
    focus_manager->AdvanceFocus(true);
    EXPECT_EQ(focus_manager->GetFocusedView()->GetID(),
              VIEW_ID_EXTENSIONS_MENU_BUTTON);
  }
  focus_manager->AdvanceFocus(true);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);

  // Test that back button is enabled after navigating to another page
  const GURL another_url("https://anothertest.org");
  web_app::NavigateToURLAndWait(helper()->app_browser(), another_url);
  helper()->app_browser()->command_controller()->ExecuteCommand(
      IDC_FOCUS_TOOLBAR);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_BACK_BUTTON);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFrameToolbarInteractiveUITest,
                         ::testing::Bool());
