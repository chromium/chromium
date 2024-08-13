// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "testing/gmock/include/gmock/gmock.h"

using media_router::MediaRoute;
using testing::NiceMock;

class FakeCastToolbarIcon : public CastToolbarButtonController::Observer {
 public:
  FakeCastToolbarIcon() = default;
  ~FakeCastToolbarIcon() override = default;

  void ShowIcon() override { icon_shown_ = true; }

  void HideIcon() override { icon_shown_ = false; }

  void ActivateIcon() override {}
  void DeactivateIcon() override {}

  bool IsShown() const { return icon_shown_; }

 private:
  bool icon_shown_ = false;
};

class CastToolbarButtonControllerUnitTest : public BrowserWithTestWindowTest {
 public:
  CastToolbarButtonControllerUnitTest()
      : issue_(media_router::Issue::CreateIssueWithIssueInfo(
            media_router::IssueInfo(
                "title notification",
                media_router::IssueInfo::Severity::NOTIFICATION,
                "sinkId1"))) {}

  CastToolbarButtonControllerUnitTest(
      const CastToolbarButtonControllerUnitTest&) = delete;
  CastToolbarButtonControllerUnitTest& operator=(
      const CastToolbarButtonControllerUnitTest&) = delete;

  ~CastToolbarButtonControllerUnitTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    controller_ =
        std::make_unique<CastToolbarButtonController>(profile(), router_.get());
    controller_->AddObserver(&icon_);

    SetAlwaysShowActionPref(false);

    local_mirroring_route_ = MediaRoute("routeId1", mirroring_source_,
                                        "sinkId1", "description", true);
    local_cast_route_ =
        MediaRoute("routeId2", cast_source_, "sinkId2", "description", true);
    non_local_mirroring_route_ = MediaRoute("routeId3", mirroring_source_,
                                            "sinkId3", "description", false);
  }

  void TearDown() override {
    controller_.reset();
    router_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  bool IsIconShown() const {
    base::RunLoop().RunUntilIdle();
    return icon_.IsShown();
  }

  void UpdateRoutesAndExpectIconShown(
      std::vector<media_router::MediaRoute> routes) {
    controller_->OnRoutesUpdated(routes);
    EXPECT_TRUE(controller_->has_local_display_route_);
    EXPECT_TRUE(IsIconShown());
  }

  void UpdateRoutesAndExpectIconHidden(
      std::vector<media_router::MediaRoute> routes) {
    controller_->OnRoutesUpdated(routes);
    EXPECT_FALSE(controller_->has_local_display_route_);
    EXPECT_FALSE(IsIconShown());
  }

  void SetAlwaysShowActionPref(bool always_show) {
    CastToolbarButtonController::SetAlwaysShowActionPref(profile(),
                                                         always_show);
  }

 protected:
  std::unique_ptr<CastToolbarButtonController> controller_;
  std::unique_ptr<media_router::MockMediaRouter> router_;
  FakeCastToolbarIcon icon_;

  // Fake Sources, used for the Routes.
  const media_router::MediaSource cast_source_{"cast:1234"};
  const media_router::MediaSource mirroring_source_{
      "urn:x-org.chromium.media:source:tab:*"};

  MediaRoute local_mirroring_route_;
  MediaRoute local_cast_route_;
  MediaRoute non_local_mirroring_route_;
  const media_router::Issue issue_;
};

TEST_F(CastToolbarButtonControllerUnitTest, EphemeralIconForRoutes) {
  EXPECT_FALSE(IsIconShown());
  // A local mirroring route should show the action icon.
  UpdateRoutesAndExpectIconShown({local_mirroring_route_});

  // A cast route should hide the action icon.
  UpdateRoutesAndExpectIconHidden({local_cast_route_});

  // A non local route should hide the action icon.
  UpdateRoutesAndExpectIconHidden({non_local_mirroring_route_});
}

TEST_F(CastToolbarButtonControllerUnitTest, EphemeralIconForIssues) {
  EXPECT_FALSE(IsIconShown());

  // Creating an issue should show the action icon.
  controller_->OnIssue(issue_);
  EXPECT_TRUE(controller_->has_issue_);
  EXPECT_TRUE(IsIconShown());
  // Removing the issue should hide the icon.
  controller_->OnIssuesCleared();
  EXPECT_FALSE(controller_->has_issue_);
  EXPECT_FALSE(IsIconShown());
  // Adding a permission rejected issue should not show the icon.
  controller_->OnIssue(media_router::Issue::CreatePermissionRejectedIssue());
  EXPECT_FALSE(controller_->has_issue_);
  EXPECT_FALSE(IsIconShown());

  // When the issue disappears, the icon should remain visible if there's
  // a local mirroring route.
  controller_->OnIssue(issue_);
  controller_->OnRoutesUpdated({local_mirroring_route_});
  controller_->OnIssuesCleared();
  EXPECT_TRUE(IsIconShown());
  UpdateRoutesAndExpectIconHidden({});
}

TEST_F(CastToolbarButtonControllerUnitTest, EphemeralIconForDialog) {
  EXPECT_FALSE(IsIconShown());

  // Showing a dialog should show the icon.
  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  // Showing and hiding a dialog shouldn't hide the icon as long as we have a
  // positive number of dialogs.
  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  // When we have zero dialogs, the icon should be hidden.
  controller_->OnDialogHidden();
  EXPECT_FALSE(IsIconShown());

  // Hiding the dialog while there are local mirroring routes shouldn't hide the
  // icon.
  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnRoutesUpdated({local_mirroring_route_});
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  UpdateRoutesAndExpectIconHidden({});

  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnIssue(issue_);
  // Hiding the dialog while there is an issue shouldn't hide the icon.
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  controller_->OnIssuesCleared();
  EXPECT_FALSE(IsIconShown());
}

TEST_F(CastToolbarButtonControllerUnitTest, EphemeralIconForContextMenu) {
  EXPECT_FALSE(IsIconShown());

  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnDialogHidden();
  controller_->OnContextMenuShown();
  // Hiding the dialog immediately before showing a context menu shouldn't hide
  // the icon.
  EXPECT_TRUE(IsIconShown());

  // Hiding the context menu should hide the icon.
  controller_->OnContextMenuHidden();
  EXPECT_FALSE(IsIconShown());
}

TEST_F(CastToolbarButtonControllerUnitTest, ObserveAlwaysShowPrefChange) {
  EXPECT_FALSE(IsIconShown());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(IsIconShown());

  // Unchecking the option while having a local route shouldn't hide the icon.
  controller_->OnRoutesUpdated({local_mirroring_route_});
  SetAlwaysShowActionPref(false);
  EXPECT_TRUE(IsIconShown());

  // Removing the local mirroring route should not hide the icon.
  SetAlwaysShowActionPref(true);
  controller_->OnRoutesUpdated({});
  EXPECT_TRUE(IsIconShown());

  SetAlwaysShowActionPref(false);
  EXPECT_FALSE(IsIconShown());
}
