// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
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
    action_controller_ =
        MediaRouterUIService::Get(browser()->profile())->action_controller();

    routes_ = {MediaRoute("routeId1", MediaSource("sourceId"), "sinkId1",
                          "description", true, true)};
  }

  // Returns the dialog controller for the active WebContents.
  MediaRouterDialogController* GetDialogController() {
    return MediaRouterDialogController::GetOrCreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  CastToolbarButton* GetCastIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->cast_button();
  }

  ui::SimpleMenuModel* GetIconContextMenu() {
    return static_cast<ui::SimpleMenuModel*>(
        GetCastIcon()->menu_model_for_test());
  }

  void PressToolbarIcon() {
    GetCastIcon()->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0), gfx::Point(0, 0),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  bool ToolbarIconExists() {
    base::RunLoop().RunUntilIdle();
    return GetCastIcon()->GetVisible();
  }

  void SetAlwaysShowActionPref(bool always_show) {
    MediaRouterActionController::SetAlwaysShowActionPref(browser()->profile(),
                                                         always_show);
  }

 protected:
  Issue issue_;

  // A vector of MediaRoutes that includes a local route.
  std::vector<MediaRoute> routes_;

  MediaRouterActionController* action_controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, OpenDialogFromContextMenu) {
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

  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, OpenDialogFromAppMenu) {
  // Start with one tab showing about:blank.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  std::unique_ptr<test::AppMenuTestApi> app_menu_test_api =
      test::AppMenuTestApi::Create(browser());
  app_menu_test_api->ShowMenu();

  MediaRouterDialogController* dialog_controller = GetDialogController();
  ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  app_menu_test_api->ExecuteCommand(IDC_ROUTE_MEDIA);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  dialog_controller->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

// TODO(crbug.com/1004635) Disabled due to flake on Windows and Linux
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_EphemeralToolbarIconForDialog EphemeralToolbarIconForDialog
#else
#define MAYBE_EphemeralToolbarIconForDialog DISABLED_EphemeralToolbarIconForDialog
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       MAYBE_EphemeralToolbarIconForDialog) {
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest, PinAndUnpinToolbarIcon) {
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconWithMultipleWindows) {
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

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       OpenDialogWithMediaRouterAction) {
  MediaRouterDialogController* dialog_controller = GetDialogController();
  // We start off at about:blank page.
  // Make sure there is 1 tab and media router is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ToolbarIconExists());

  PressToolbarIcon();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Reload the browser and wait.
  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();

  // The reload should have closed the dialog.
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  PressToolbarIcon();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // The navigation should have closed the dialog.
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  SetAlwaysShowActionPref(false);
}

}  // namespace media_router
