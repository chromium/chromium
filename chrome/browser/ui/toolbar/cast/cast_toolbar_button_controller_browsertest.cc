// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

using media_router::MediaRoute;
using testing::NiceMock;

class CastToolbarButtonControllerBrowserTest : public InProcessBrowserTest {
 public:
  CastToolbarButtonControllerBrowserTest()
      : issue_(media_router::Issue::CreateIssueWithIssueInfo(
            media_router::IssueInfo(
                "title notification",
                media_router::IssueInfo::Severity::NOTIFICATION,
                "sinkId1"))) {}

  void SetUpOnMainThread() override {
    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    controller_ = std::make_unique<CastToolbarButtonController>(
        browser()->profile(), router_.get());

    local_mirroring_route_ = MediaRoute("routeId1", mirroring_source_,
                                        "sinkId1", "description", true);
    local_cast_route_ =
        MediaRoute("routeId2", cast_source_, "sinkId2", "description", true);
    non_local_mirroring_route_ = MediaRoute("routeId3", mirroring_source_,
                                            "sinkId3", "description", false);
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    router_.reset();
  }

  bool IsIconShown() const {
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container());
    auto* cast_button = BrowserView::GetBrowserViewForBrowser(browser())
                            ->toolbar()
                            ->GetCastButton();
    return cast_button && cast_button->GetVisible();
  }

  void UpdateRoutesAndExpectIconShown(
      std::vector<media_router::MediaRoute> routes) {
    controller_->OnRoutesUpdated(routes);
    EXPECT_TRUE(controller_->GetHasLocalDisplayRouteForTesting());
    EXPECT_TRUE(IsIconShown());
  }

  void UpdateRoutesAndExpectIconHidden(
      std::vector<media_router::MediaRoute> routes) {
    controller_->OnRoutesUpdated(routes);
    EXPECT_FALSE(controller_->GetHasLocalDisplayRouteForTesting());
    EXPECT_FALSE(IsIconShown());
  }

  void SetAlwaysShowActionPref(bool always_show) {
    PinnedToolbarActionsModel::Get(browser()->profile())
        ->UpdatePinnedState(kActionRouteMedia, always_show);
  }

  CastToolbarButtonController* controller() { return controller_.get(); }

 protected:
  std::unique_ptr<CastToolbarButtonController> controller_;
  std::unique_ptr<media_router::MockMediaRouter> router_;

  // Fake Sources, used for the Routes.
  const media_router::MediaSource cast_source_{"cast:1234"};
  const media_router::MediaSource mirroring_source_{
      "urn:x-org.chromium.media:source:tab:*"};

  MediaRoute local_mirroring_route_;
  MediaRoute local_cast_route_;
  MediaRoute non_local_mirroring_route_;
  const media_router::Issue issue_;
};

IN_PROC_BROWSER_TEST_F(CastToolbarButtonControllerBrowserTest,
                       EphemeralIconForRoutes) {
  EXPECT_FALSE(IsIconShown());
  // A local mirroring route should show the action icon.
  UpdateRoutesAndExpectIconShown({local_mirroring_route_});

  // A cast route should hide the action icon.
  UpdateRoutesAndExpectIconHidden({local_cast_route_});

  // A non local route should hide the action icon.
  UpdateRoutesAndExpectIconHidden({non_local_mirroring_route_});
}

IN_PROC_BROWSER_TEST_F(CastToolbarButtonControllerBrowserTest,
                       EphemeralIconForIssues) {
  EXPECT_FALSE(IsIconShown());

  // Creating an issue should show the action icon.
  controller()->OnIssue(issue_);
  EXPECT_TRUE(controller()->GetHasIssueForTesting());
  EXPECT_TRUE(IsIconShown());
  // Removing the issue should hide the icon.
  controller()->OnIssuesCleared();
  EXPECT_FALSE(controller()->GetHasIssueForTesting());
  EXPECT_FALSE(IsIconShown());
  // Adding a permission rejected issue should not show the icon.
  controller()->OnIssue(media_router::Issue::CreatePermissionRejectedIssue());
  EXPECT_FALSE(controller()->GetHasIssueForTesting());
  EXPECT_FALSE(IsIconShown());

  // When the issue disappears, the icon should remain visible if there's
  // a local mirroring route.
  controller()->OnIssue(issue_);
  controller()->OnRoutesUpdated({local_mirroring_route_});
  controller()->OnIssuesCleared();
  EXPECT_TRUE(IsIconShown());
  UpdateRoutesAndExpectIconHidden({});
}

IN_PROC_BROWSER_TEST_F(CastToolbarButtonControllerBrowserTest,
                       EphemeralIconForDialog) {
  EXPECT_FALSE(IsIconShown());

  // Showing a dialog should show the icon.
  controller()->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  // Showing and hiding a dialog shouldn't hide the icon as long as we have a
  // positive number of dialogs.
  controller()->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller()->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  // When we have zero dialogs, the icon should be hidden.
  controller()->OnDialogHidden();
  EXPECT_FALSE(IsIconShown());

  // Hiding the dialog while there are local mirroring routes shouldn't hide the
  // icon.
  controller()->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller()->OnRoutesUpdated({local_mirroring_route_});
  controller()->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  UpdateRoutesAndExpectIconHidden({});

  controller()->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller()->OnIssue(issue_);
  // Hiding the dialog while there is an issue shouldn't hide the icon.
  controller()->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  controller()->OnIssuesCleared();
  EXPECT_FALSE(IsIconShown());
}

IN_PROC_BROWSER_TEST_F(CastToolbarButtonControllerBrowserTest,
                       EphemeralIconForContextMenu) {
  EXPECT_FALSE(IsIconShown());

  controller()->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller()->OnDialogHidden();
  controller()->OnContextMenuShown();
  // Hiding the dialog immediately before showing a context menu shouldn't hide
  // the icon.
  EXPECT_TRUE(IsIconShown());

  // Hiding the context menu should hide the icon.
  controller()->OnContextMenuHidden();
  EXPECT_FALSE(IsIconShown());
}

IN_PROC_BROWSER_TEST_F(CastToolbarButtonControllerBrowserTest,
                       ObserveAlwaysShowPrefChange) {
  EXPECT_FALSE(IsIconShown());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(IsIconShown());

  // Unchecking the option while having a local route shouldn't hide the icon.
  controller()->OnRoutesUpdated({local_mirroring_route_});
  SetAlwaysShowActionPref(false);
  EXPECT_TRUE(IsIconShown());

  // Removing the local mirroring route should not hide the icon.
  SetAlwaysShowActionPref(true);
  controller()->OnRoutesUpdated({});
  EXPECT_TRUE(IsIconShown());

  SetAlwaysShowActionPref(false);
  EXPECT_FALSE(IsIconShown());
}
