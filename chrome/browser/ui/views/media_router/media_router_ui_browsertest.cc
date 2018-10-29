// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/media_router/media_router_dialog_controller_impl_base.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/app_menu_test_api.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace media_router {

// Base class containing setup code and test cases shared between WebUI and
// Views dialog tests.
class MediaRouterUIBrowserTest : public InProcessBrowserTest {
 public:
  MediaRouterUIBrowserTest()
      : issue_(IssueInfo("title notification",
                         IssueInfo::Action::DISMISS,
                         IssueInfo::Severity::NOTIFICATION)) {}
  ~MediaRouterUIBrowserTest() override {}

  void SetUpOnMainThread() override {
    toolbar_actions_bar_ =
        BrowserActionTestUtil::Create(browser(), true)->GetToolbarActionsBar();

    action_controller_ =
        MediaRouterUIService::Get(browser()->profile())->action_controller();

    routes_ = {MediaRoute("routeId1", MediaSource("sourceId"), "sinkId1",
                          "description", true, true)};
  }

  void OpenMediaRouterDialogAndWaitForNewWebContents() {
    content::TestNavigationObserver nav_observer(NULL);
    nav_observer.StartWatchingNewWebContents();

    // When the Media Router Action executes, it opens a dialog with web
    // contents to chrome://media-router.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MediaRouterUIBrowserTest::PressToolbarIcon,
                                  base::Unretained(this)));

    std::unique_ptr<test::AppMenuTestApi> test_api =
        test::AppMenuTestApi::Create(browser());
    test_api->ShowMenu();

    nav_observer.Wait();
    EXPECT_FALSE(test_api->IsMenuShowing());
    ASSERT_EQ(chrome::kChromeUIMediaRouterURL,
        nav_observer.last_navigation_url().spec());
    nav_observer.StopWatchingNewWebContents();
  }

  // Returns the dialog controller for the active WebContents.
  MediaRouterDialogControllerImplBase* GetDialogController() {
    return MediaRouterDialogControllerImplBase::GetOrCreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MediaRouterAction* GetMediaRouterAction() {
    return GetDialogController()->action();
  }

  CastToolbarButton* GetTrustedAreaIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->cast_button();
  }

  ui::SimpleMenuModel* GetIconContextMenu() {
    return static_cast<ui::SimpleMenuModel*>(
        ShouldUseViewsDialog() ? GetTrustedAreaIcon()->menu_model_for_test()
                               : GetMediaRouterAction()->GetContextMenu());
  }

  void PressToolbarIcon() {
    if (ShouldUseViewsDialog()) {
      GetTrustedAreaIcon()->OnMousePressed(ui::MouseEvent(
          ui::ET_MOUSE_PRESSED, gfx::Point(0, 0), gfx::Point(0, 0),
          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    } else {
      GetMediaRouterAction()->ExecuteAction(true);
    }
  }

  bool ToolbarIconExists() {
    base::RunLoop().RunUntilIdle();
    if (ShouldUseViewsDialog())
      return GetTrustedAreaIcon()->visible();
    return ToolbarActionsModel::Get(browser()->profile())
        ->HasComponentAction(
            ComponentToolbarActionsFactory::kMediaRouterActionId);
  }

  void SetAlwaysShowActionPref(bool always_show) {
    MediaRouterActionController::SetAlwaysShowActionPref(browser()->profile(),
                                                         always_show);
  }

 protected:
  // Test* methods contain tests that are run with both WebUI and Views
  // implementations of the dialog.
  void TestOpenDialogFromContextMenu() {
    // Start with one tab showing about:blank.
    ASSERT_EQ(1, browser()->tab_strip_model()->count());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    MediaRouterDialogController* dialog_controller = GetDialogController();
    content::ContextMenuParams params;
    params.page_url =
        web_contents->GetController().GetLastCommittedEntry()->GetURL();
    TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
    menu.Init();

    ASSERT_TRUE(menu.IsItemPresent(IDC_ROUTE_MEDIA));
    ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
    menu.ExecuteCommand(IDC_ROUTE_MEDIA, 0);
    EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

    // Executing the command again should be a no-op, and there should only be
    // one dialog opened per tab.
    menu.ExecuteCommand(IDC_ROUTE_MEDIA, 0);
    EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());
    dialog_controller->HideMediaRouterDialog();
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  }

  void TestOpenDialogFromAppMenu() {
    // Start with one tab showing about:blank.
    ASSERT_EQ(1, browser()->tab_strip_model()->count());

    std::unique_ptr<test::AppMenuTestApi> app_menu_test_api =
        test::AppMenuTestApi::Create(browser());
    app_menu_test_api->ShowMenu();

    MediaRouterDialogController* dialog_controller = GetDialogController();
    ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
    app_menu_test_api->ExecuteCommand(IDC_ROUTE_MEDIA);
    EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

    // Executing the command again should be a no-op, and there should only be
    // one dialog opened per tab.
    app_menu_test_api->ExecuteCommand(IDC_ROUTE_MEDIA);
    EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());
    dialog_controller->HideMediaRouterDialog();
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  }

  void TestEphemeralToolbarIconForDialog() {
    MediaRouterDialogController* dialog_controller = GetDialogController();

    EXPECT_FALSE(ToolbarIconExists());
    dialog_controller->ShowMediaRouterDialog();
    EXPECT_TRUE(ToolbarIconExists());
    dialog_controller->HideMediaRouterDialog();
    EXPECT_FALSE(ToolbarIconExists());

    dialog_controller->ShowMediaRouterDialog();
    EXPECT_TRUE(ToolbarIconExists());
    // Clicking on the toolbar icon should hide both the dialog and the icon.
    PressToolbarIcon();
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
    EXPECT_FALSE(ToolbarIconExists());

    dialog_controller->ShowMediaRouterDialog();
    SetAlwaysShowActionPref(true);
    // When the pref is set to true, hiding the dialog shouldn't hide the icon.
    dialog_controller->HideMediaRouterDialog();
    EXPECT_TRUE(ToolbarIconExists());
    dialog_controller->ShowMediaRouterDialog();
    // While the dialog is showing, setting the pref to false shouldn't hide the
    // icon.
    SetAlwaysShowActionPref(false);
    EXPECT_TRUE(ToolbarIconExists());
    dialog_controller->HideMediaRouterDialog();
    EXPECT_FALSE(ToolbarIconExists());
  }

  void TestEphemeralToolbarIconForRoutesAndIssues() {
    action_controller_->OnIssue(issue_);
    EXPECT_TRUE(ToolbarIconExists());
    action_controller_->OnIssuesCleared();
    EXPECT_FALSE(ToolbarIconExists());

    action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
    EXPECT_TRUE(ToolbarIconExists());
    action_controller_->OnRoutesUpdated(std::vector<MediaRoute>(),
                                        std::vector<MediaRoute::Id>());
    EXPECT_FALSE(ToolbarIconExists());

    SetAlwaysShowActionPref(true);
    EXPECT_TRUE(ToolbarIconExists());
    SetAlwaysShowActionPref(false);
    EXPECT_FALSE(ToolbarIconExists());
  }

  void TestEphemeralToolbarIconWithMultipleWindows() {
    action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
    EXPECT_TRUE(ToolbarIconExists());

    // Opening and closing a window shouldn't affect the state of the ephemeral
    // icon. Creating and removing the icon with multiple windows open should
    // also work.
    Browser* browser2 = CreateBrowser(browser()->profile());
    EXPECT_TRUE(ToolbarIconExists());
    action_controller_->OnRoutesUpdated(std::vector<MediaRoute>(),
                                        std::vector<MediaRoute::Id>());
    EXPECT_FALSE(ToolbarIconExists());
    action_controller_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
    EXPECT_TRUE(ToolbarIconExists());
    browser2->window()->Close();
    EXPECT_TRUE(ToolbarIconExists());
  }

  ToolbarActionsBar* toolbar_actions_bar_ = nullptr;

  Issue issue_;

  // A vector of MediaRoutes that includes a local route.
  std::vector<MediaRoute> routes_;

  MediaRouterActionController* action_controller_ = nullptr;
};

// Runs dialog-related tests with the WebUI Cast dialog.
class MediaRouterWebUIBrowserTest : public MediaRouterUIBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kViewsCastDialog);
    MediaRouterUIBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest, OpenDialogFromContextMenu) {
  TestOpenDialogFromContextMenu();
}

// Disabled on macOS due to many timeouts. Seems fine on all other platforms.
// crbug.com/849146
#if defined(OS_MACOSX)
#define MAYBE_OpenDialogFromAppMenu DISABLED_OpenDialogFromAppMenu
#else
#define MAYBE_OpenDialogFromAppMenu OpenDialogFromAppMenu
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       MAYBE_OpenDialogFromAppMenu) {
  TestOpenDialogFromAppMenu();
}

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       EphemeralToolbarIconForDialog) {
  TestEphemeralToolbarIconForDialog();
}

// Flaky on chromeos, linux, win: https://crbug.com/658005
// Flaky on MacViews: https://crbug.com/817408
// TODO(https://crbug.com/678472): Replace this test case with a Views version.
// Close out the bugs above when doing so.
IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       DISABLED_OpenDialogWithMediaRouterAction) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and media router is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ToolbarIconExists());

  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Reload the browser and wait.
  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();

  // The reload should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // The navigation should have removed the previously created dialog.
  // We expect a new dialog WebContents to be created by calling this.
  OpenMediaRouterDialogAndWaitForNewWebContents();

  SetAlwaysShowActionPref(false);
}

#if defined(MEMORY_SANITIZER)
// Flaky crashes. crbug.com/863945
#define MAYBE_OpenDialogsInMultipleTabs DISABLED_OpenDialogsInMultipleTabs
#else
#define MAYBE_OpenDialogsInMultipleTabs OpenDialogsInMultipleTabs
#endif

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       MAYBE_OpenDialogsInMultipleTabs) {
  // Start with two tabs.
  chrome::NewTab(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  MediaRouterDialogController* dialog_controller1 =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  MediaRouterDialogController* dialog_controller2 =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(1));

  // Show the media router action on the toolbar.
  SetAlwaysShowActionPref(true);

  // Open a dialog in the first tab using the toolbar action.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_FALSE(dialog_controller1->IsShowingMediaRouterDialog());
  PressToolbarIcon();
  EXPECT_TRUE(dialog_controller1->IsShowingMediaRouterDialog());

  // Move to the second tab, which shouldn't have a dialog at first. Open and
  // close a dialog in that tab.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_FALSE(dialog_controller2->IsShowingMediaRouterDialog());
  PressToolbarIcon();
  EXPECT_TRUE(dialog_controller2->IsShowingMediaRouterDialog());
  PressToolbarIcon();
  EXPECT_FALSE(dialog_controller2->IsShowingMediaRouterDialog());

  // Move back to the first tab, whose dialog should still be open. Hide the
  // dialog.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(dialog_controller1->IsShowingMediaRouterDialog());
  PressToolbarIcon();
  EXPECT_FALSE(dialog_controller1->IsShowingMediaRouterDialog());

  // Reset the preference showing the toolbar action.
  SetAlwaysShowActionPref(false);
}

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
  TestEphemeralToolbarIconForRoutesAndIssues();
}

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest,
                       EphemeralToolbarIconWithMultipleWindows) {
  TestEphemeralToolbarIconWithMultipleWindows();
}

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest, UpdateActionLocation) {
  SetAlwaysShowActionPref(true);

  // Get the index for "Hide in Chrome menu" / "Show in toolbar" menu item.
  const int command_index = GetIconContextMenu()->GetIndexOfCommandId(
      IDC_MEDIA_ROUTER_SHOW_IN_TOOLBAR);
  GetMediaRouterAction()->OnContextMenuClosed();

  // Start out with the action visible on the main bar.
  EXPECT_TRUE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
  GetIconContextMenu()->ActivatedAt(command_index);
  GetMediaRouterAction()->OnContextMenuClosed();

  // The action should get hidden in the overflow menu.
  EXPECT_FALSE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
  GetIconContextMenu()->ActivatedAt(command_index);
  GetMediaRouterAction()->OnContextMenuClosed();

  // The action should be back on the main bar.
  EXPECT_TRUE(
      toolbar_actions_bar_->IsActionVisibleOnMainBar(GetMediaRouterAction()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterWebUIBrowserTest, PinAndUnpinToolbarIcon) {
  GetDialogController()->ShowMediaRouterDialog();
  EXPECT_TRUE(ToolbarIconExists());
  // Pin the icon via its context menu.
  ui::SimpleMenuModel* context_menu = GetIconContextMenu();
  const int command_index = context_menu->GetIndexOfCommandId(
      IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION);
  context_menu->ActivatedAt(command_index);
  GetMediaRouterAction()->OnContextMenuClosed();
  GetDialogController()->HideMediaRouterDialog();
  EXPECT_TRUE(ToolbarIconExists());

  // Unpin the icon via its context menu.
  CHECK(GetIconContextMenu());
  GetIconContextMenu()->ActivatedAt(command_index);
  EXPECT_FALSE(ToolbarIconExists());
}

// Runs dialog-related tests with the Views Cast dialog.
class MediaRouterViewsUIBrowserTest : public MediaRouterUIBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kViewsCastDialog);
    MediaRouterUIBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest,
                       OpenDialogFromContextMenu) {
  TestOpenDialogFromContextMenu();
}

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest, OpenDialogFromAppMenu) {
  TestOpenDialogFromAppMenu();
}

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest,
                       EphemeralToolbarIconForDialog) {
  TestEphemeralToolbarIconForDialog();
}

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest, PinAndUnpinToolbarIcon) {
  GetDialogController()->ShowMediaRouterDialog();
  EXPECT_TRUE(ToolbarIconExists());
  // Pin the icon via its context menu.
  ui::SimpleMenuModel* context_menu = GetIconContextMenu();
  const int command_index = context_menu->GetIndexOfCommandId(
      IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION);
  context_menu->ActivatedAt(command_index);
  GetDialogController()->HideMediaRouterDialog();
  EXPECT_TRUE(ToolbarIconExists());

  // Unpin the icon via its context menu.
  GetIconContextMenu()->ActivatedAt(command_index);
  EXPECT_FALSE(ToolbarIconExists());
}

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
  TestEphemeralToolbarIconForRoutesAndIssues();
}

IN_PROC_BROWSER_TEST_F(MediaRouterViewsUIBrowserTest,
                       EphemeralToolbarIconWithMultipleWindows) {
  TestEphemeralToolbarIconWithMultipleWindows();
}

}  // namespace media_router
