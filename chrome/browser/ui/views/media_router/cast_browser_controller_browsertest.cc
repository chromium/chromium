// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_test_accessor.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/mirroring_media_controller_host_impl.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/ui_base_features.h"

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

class CastBrowserControllerTest : public UiBrowserTest {
 public:
  CastBrowserControllerTest() = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {}
  bool VerifyUi() override { return true; }
  void WaitForUserDismissal() override {}

  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();

    PinnedToolbarActionsModel::Get(browser()->GetProfile())
        ->UpdatePinnedState(kActionRouteMedia, true);
    controller_ = CastBrowserController::From(browser());
    media_router_ =
        MediaRouterFactory::GetApiForBrowserContext(browser()->GetProfile());

  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    media_router_ = nullptr;
    UiBrowserTest::TearDownOnMainThread();
  }

  bool IsWarningIcon() {
    const auto* icon =
        features::IsRoundedIconsEnabled()
            ? &vector_icons::kCastWarningIcon
            : &vector_icons::kMediaRouterWarningChromeRefreshOldIcon;
    return GetVectorIcon() == icon;
  }

  bool IsIdleIcon() {
    const auto* icon =
        features::IsRoundedIconsEnabled()
            ? &vector_icons::kCastIcon
            : &vector_icons::kMediaRouterIdleChromeRefreshOldIcon;
    return GetVectorIcon() == icon;
  }

  bool IsPausedIcon() {
    const auto* icon = features::IsRoundedIconsEnabled()
                           ? &vector_icons::kCastPauseIcon
                           : &vector_icons::kMediaRouterPausedOldIcon;
    return GetVectorIcon() == icon;
  }

  bool VerifyCastButtonScreenshot(const std::string& screenshot_name) {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kVerifyPixels)) {
      return true;
    }

    PinnedActionTestAccessor accessor(browser(), kActionRouteMedia);
    ui::TrackedElement* element = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      element = accessor.GetElement();
      return element != nullptr;
    }));
    if (!element) {
      return false;
    }
    return VerifyPixelUi(element, "CastBrowserControllerTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

 protected:
  const gfx::VectorIcon* GetVectorIcon() {
    auto model =
        PinnedActionTestAccessor(browser(), kActionRouteMedia).GetImageModel();
    return model.IsVectorIcon() ? model.GetVectorIcon().vector_icon() : nullptr;
  }

  std::unique_ptr<MirroringMediaControllerHostImpl> mirroring_controller_host_;

  raw_ptr<CastBrowserController> controller_ = nullptr;
  raw_ptr<MediaRouter> media_router_ = nullptr;

  const std::vector<MediaRoute> local_display_route_list_ = {
      CreateLocalDisplayRoute()};
  const std::vector<MediaRoute> non_local_display_route_list_ = {
      CreateNonLocalDisplayRoute()};
};

IN_PROC_BROWSER_TEST_F(CastBrowserControllerTest, UpdateIssues) {
  controller_->UpdateIcon();
  EXPECT_TRUE(IsIdleIcon());
  EXPECT_TRUE(VerifyCastButtonScreenshot("UpdateIssues_InitialIdle"));

  controller_->OnIssue(Issue::CreateIssueWithIssueInfo(IssueInfo(
      "title notification", IssueInfo::Severity::NOTIFICATION, "sinkId1")));
  EXPECT_TRUE(IsIdleIcon());

  controller_->OnIssue(Issue::CreateIssueWithIssueInfo(
      IssueInfo("title warning", IssueInfo::Severity::WARNING, "sinkId1")));
  EXPECT_TRUE(IsWarningIcon());
  EXPECT_TRUE(VerifyCastButtonScreenshot("UpdateIssues_Warning"));

  controller_->OnIssue(Issue::CreatePermissionRejectedIssue());
  EXPECT_TRUE(IsWarningIcon());

  controller_->OnIssuesCleared();
  EXPECT_TRUE(IsIdleIcon());
}

IN_PROC_BROWSER_TEST_F(CastBrowserControllerTest, PausedIcon) {
  // Enable the proper prefs.
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled,
                                                  true);

  controller_->UpdateIcon();
  EXPECT_TRUE(IsIdleIcon());

  static_cast<MediaRouterDesktop*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(
          browser()->GetProfile()))
      ->OnRoutesUpdated(mojom::MediaRouteProviderId::CAST,
                        local_display_route_list_);

  media_router::mojom::MediaStatusPtr status = mojom::MediaStatus::New();
  status->can_play_pause = true;
  status->play_state = mojom::MediaStatus::PlayState::PAUSED;
  media_router::MediaRouterFactory::GetApiForBrowserContext(
      browser()->GetProfile())
      ->GetMirroringMediaControllerHost("routeId1")
      ->OnMediaStatusUpdated(std::move(status));

  controller_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(IsPausedIcon());
  EXPECT_TRUE(VerifyCastButtonScreenshot("PausedIcon"));
}

}  // namespace media_router
