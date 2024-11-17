// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/app_menu_test_api.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace media_router {

class MediaRouterUIBrowserTest : public InProcessBrowserTest {
 public:
  MediaRouterUIBrowserTest()
      : issue_(Issue::CreateIssueWithIssueInfo(
            IssueInfo("title notification",
                      IssueInfo::Severity::NOTIFICATION,
                      "sinkId1"))) {}
  ~MediaRouterUIBrowserTest() override {}

  void SetUpOnMainThread() override {
    action_controller_ =
        MediaRouterUIService::Get(browser()->profile())->action_controller();

    routes_ = {MediaRoute("routeId1",
                          MediaSource("urn:x-org.chromium.media:source:tab:*"),
                          "sinkId1", "description", true)};

    auto* pinned_toolbar_actions_container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container();
    views::test::ReduceAnimationDuration(pinned_toolbar_actions_container);
  }

  bool ToolbarIconExists() {
    base::RunLoop().RunUntilIdle();
    ToolbarButton* cast_icon = GetCastIcon();
    return cast_icon && cast_icon->GetVisible();
  }

  void SetAlwaysShowActionPref(bool always_show) {
    CastToolbarButtonController::SetAlwaysShowActionPref(browser()->profile(),
                                                         always_show);
  }

 protected:
  ToolbarButton* GetCastIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->GetCastButton();
  }

  Issue issue_;

  // A vector of MediaRoutes that includes a local route.
  std::vector<MediaRoute> routes_;

  raw_ptr<CastToolbarButtonController, DanglingUntriaged> action_controller_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconForRoutesAndIssues) {
  action_controller_->OnIssue(issue_);
  EXPECT_TRUE(ToolbarIconExists());
  action_controller_->OnIssuesCleared();
  EXPECT_FALSE(ToolbarIconExists());

  action_controller_->OnRoutesUpdated(routes_);
  EXPECT_TRUE(ToolbarIconExists());
  action_controller_->OnRoutesUpdated(std::vector<MediaRoute>());
  EXPECT_FALSE(ToolbarIconExists());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ToolbarIconExists());
  SetAlwaysShowActionPref(false);
  EXPECT_FALSE(ToolbarIconExists());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIBrowserTest,
                       EphemeralToolbarIconWithMultipleWindows) {
  action_controller_->OnRoutesUpdated(routes_);
  EXPECT_TRUE(ToolbarIconExists());

  // Opening and closing a window shouldn't affect the state of the ephemeral
  // icon. Creating and removing the icon with multiple windows open should
  // also work.
  Browser* browser2 = CreateBrowser(browser()->profile());
  EXPECT_TRUE(ToolbarIconExists());
  action_controller_->OnRoutesUpdated(std::vector<MediaRoute>());
  EXPECT_FALSE(ToolbarIconExists());
  action_controller_->OnRoutesUpdated(routes_);
  EXPECT_TRUE(ToolbarIconExists());
  browser2->window()->Close();
  EXPECT_TRUE(ToolbarIconExists());
}

}  // namespace media_router
