// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/mirroring_media_controller_host_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

using testing::_;

namespace media_router {

namespace {

MediaRoute CreateLocalDisplayRoute() {
  auto route = MediaRoute("routeId1", MediaSource("source1"), "sinkId1",
                          "description", true);
  route.set_controller_type(RouteControllerType::kMirroring);
  return route;
}

MediaRoute CreateNonLocalDisplayRoute() {
  return MediaRoute("routeId2", MediaSource("source2"), "sinkId2",
                    "description", false);
}

}  // namespace

class CastBrowserControllerTest : public InProcessBrowserTest {
 public:
  CastBrowserControllerTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kToolbarPinning, features::kPinnedCastButton}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    PinnedToolbarActionsModel::Get(browser()->profile())
        ->UpdatePinnedState(kActionRouteMedia, true);
    button_ = BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar()
                  ->pinned_toolbar_actions_container()
                  ->GetButtonFor(kActionRouteMedia);
    controller_ =
        browser()->browser_window_features()->cast_browser_controller();
    media_router_ =
        MediaRouterFactory::GetApiForBrowserContext(browser()->profile());

    const ui::ColorProvider* color_provider = button_->GetColorProvider();
    idle_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterIdleChromeRefreshIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
    warning_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterWarningChromeRefreshIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
    active_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterActiveChromeRefreshIcon,
        color_provider->GetColor(kColorMediaRouterIconActive)));
    paused_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterPausedIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    button_ = nullptr;
    media_router_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool IsWarningIcon() {
    return gfx::test::AreImagesEqual(warning_chrome_refresh_icon_, GetIcon());
  }

  bool IsIdleIcon() {
    return gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon());
  }

 protected:
  gfx::Image GetIcon() {
    return gfx::Image(button_->GetImage(views::Button::STATE_NORMAL));
  }

  std::unique_ptr<MirroringMediaControllerHostImpl> mirroring_controller_host_;

  raw_ptr<CastBrowserController> controller_ = nullptr;
  raw_ptr<PinnedActionToolbarButton> button_ = nullptr;
  raw_ptr<MediaRouter> media_router_ = nullptr;
  gfx::Image idle_chrome_refresh_icon_;
  gfx::Image warning_chrome_refresh_icon_;
  gfx::Image active_chrome_refresh_icon_;
  gfx::Image paused_icon_;
  base::test::ScopedFeatureList scoped_feature_list_;

  const std::vector<MediaRoute> local_display_route_list_ = {
      CreateLocalDisplayRoute()};
  const std::vector<MediaRoute> non_local_display_route_list_ = {
      CreateNonLocalDisplayRoute()};
};

IN_PROC_BROWSER_TEST_F(CastBrowserControllerTest, UpdateIssues) {
  controller_->UpdateIcon();
  EXPECT_TRUE(IsIdleIcon());

  controller_->OnIssue(Issue::CreateIssueWithIssueInfo(IssueInfo(
      "title notification", IssueInfo::Severity::NOTIFICATION, "sinkId1")));
  EXPECT_TRUE(IsIdleIcon());

  controller_->OnIssue(Issue::CreateIssueWithIssueInfo(
      IssueInfo("title warning", IssueInfo::Severity::WARNING, "sinkId1")));
  EXPECT_TRUE(IsWarningIcon());

  controller_->OnIssue(Issue::CreatePermissionRejectedIssue());
  EXPECT_TRUE(IsWarningIcon());

  controller_->OnIssuesCleared();
  EXPECT_TRUE(IsIdleIcon());
}

IN_PROC_BROWSER_TEST_F(CastBrowserControllerTest, PausedIcon) {
  // Enable the proper prefs.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled,
                                               true);

  controller_->UpdateIcon();
  EXPECT_TRUE(IsIdleIcon());

  static_cast<MediaRouterDesktop*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(
          browser()->profile()))
      ->OnRoutesUpdated(mojom::MediaRouteProviderId::CAST,
                        local_display_route_list_);

  media_router::mojom::MediaStatusPtr status = mojom::MediaStatus::New();
  status->can_play_pause = true;
  status->play_state = mojom::MediaStatus::PlayState::PAUSED;
  media_router::MediaRouterFactory::GetApiForBrowserContext(
      browser()->profile())
      ->GetMirroringMediaControllerHost("routeId1")
      ->OnMediaStatusUpdated(std::move(status));

  controller_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(gfx::test::AreImagesEqual(paused_icon_, GetIcon()));
}

}  // namespace media_router
