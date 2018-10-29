// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"

#include "base/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

using testing::_;
using testing::WithArg;

namespace media_router {

namespace {

std::unique_ptr<KeyedService> BuildUIService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto controller = std::make_unique<MockMediaRouterActionController>(profile);
  return std::make_unique<media_router::MediaRouterUIService>(
      profile, std::move(controller));
}

MediaRoute CreateLocalDisplayRoute() {
  return MediaRoute("routeId1", MediaSource("source1"), "sinkId1",
                    "description", true, true);
}

MediaRoute CreateNonLocalDisplayRoute() {
  return MediaRoute("routeId2", MediaSource("source2"), "sinkId2",
                    "description", false, true);
}

MediaRoute CreateLocalNonDisplayRoute() {
  return MediaRoute("routeId3", MediaSource("source3"), "sinkId3",
                    "description", true, false);
}

class MockContextMenuObserver : public MediaRouterContextualMenu::Observer {
 public:
  MOCK_METHOD0(OnContextMenuShown, void());
  MOCK_METHOD0(OnContextMenuHidden, void());
};

}  // namespace

class CastToolbarButtonTest : public ChromeViewsTestBase {
 public:
  CastToolbarButtonTest() = default;
  ~CastToolbarButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    MediaRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&MockMediaRouter::Create));
    MediaRouterUIServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildUIService));

    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(&profile_, true);
    browser_params.window = window_.get();
    browser_ = std::make_unique<Browser>(browser_params);
    MediaRouter* media_router =
        MediaRouterFactory::GetApiForBrowserContext(&profile_);
    auto context_menu = std::make_unique<MediaRouterContextualMenu>(
        browser_.get(), true, false, &context_menu_observer_);
    button_ = std::make_unique<CastToolbarButton>(browser_.get(), media_router,
                                                  std::move(context_menu));
  }

  void TearDown() override {
    button_.reset();
    browser_.reset();
    window_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  gfx::Image GetIcon() {
    return gfx::Image(button_->GetImage(views::Button::STATE_NORMAL));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<BrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<CastToolbarButton> button_;
  MockContextMenuObserver context_menu_observer_;
  TestingProfile profile_;

  const gfx::Image idle_icon_ =
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kMediaRouterIdleIcon,
                                       gfx::kChromeIconGrey));
  const gfx::Image warning_icon_ =
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kMediaRouterWarningIcon,
                                       gfx::kGoogleYellow700));
  const gfx::Image error_icon_ =
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kMediaRouterErrorIcon,
                                       gfx::kGoogleRed700));
  const gfx::Image active_icon_ =
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kMediaRouterActiveIcon,
                                       gfx::kGoogleBlue500));

  const std::vector<MediaRoute> local_display_route_list_ = {
      CreateLocalDisplayRoute()};
  const std::vector<MediaRoute> non_local_display_route_list_ = {
      CreateNonLocalDisplayRoute(), CreateLocalNonDisplayRoute()};

 private:
  DISALLOW_COPY_AND_ASSIGN(CastToolbarButtonTest);
};

TEST_F(CastToolbarButtonTest, ShowAndHideButton) {
  ASSERT_FALSE(button_->visible());
  button_->ShowIcon();
  EXPECT_TRUE(button_->visible());
  button_->HideIcon();
  EXPECT_FALSE(button_->visible());
}

TEST_F(CastToolbarButtonTest, UpdateIssues) {
  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnIssue(
      Issue(IssueInfo("title notification", IssueInfo::Action::DISMISS,
                      IssueInfo::Severity::NOTIFICATION)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnIssue(
      Issue(IssueInfo("title warning", IssueInfo::Action::LEARN_MORE,
                      IssueInfo::Severity::WARNING)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(warning_icon_, GetIcon()));

  button_->OnIssue(Issue(IssueInfo("title fatal", IssueInfo::Action::DISMISS,
                                   IssueInfo::Severity::FATAL)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(error_icon_, GetIcon()));

  button_->OnIssuesCleared();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));
}

TEST_F(CastToolbarButtonTest, UpdateRoutes) {
  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnRoutesUpdated(local_display_route_list_, {});
  EXPECT_TRUE(gfx::test::AreImagesEqual(active_icon_, GetIcon()));

  // The idle icon should be shown when we only have non-local and/or
  // non-display routes.
  button_->OnRoutesUpdated(non_local_display_route_list_, {});
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnRoutesUpdated({}, {});
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));
}

}  // namespace media_router
