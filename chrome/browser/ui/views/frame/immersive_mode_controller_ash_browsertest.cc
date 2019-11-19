// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/ash/tablet_mode_page_behavior.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/window/frame_caption_button.h"

class ImmersiveModeControllerAshWebAppBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  ImmersiveModeControllerAshWebAppBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~ImmersiveModeControllerAshWebAppBrowserTest() override = default;

  // InProcessBrowserTest override:
  void SetUpOnMainThread() override {
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    const GURL app_url = GetAppUrl();
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = app_url;
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app_info->theme_color = SK_ColorBLUE;

    app_id = InstallWebApp(std::move(web_app_info));
  }

  GURL GetAppUrl() { return https_server_.GetURL("/simple.html"); }

  void LaunchAppBrowser(bool wait = true) {
    ui_test_utils::UrlLoadObserver url_observer(
        GetAppUrl(), content::NotificationService::AllSources());
    browser_ = LaunchWebAppBrowser(app_id);

    if (wait) {
      // Wait for the URL to load so that the location bar end-state stabilizes.
      url_observer.Wait();
    }
    controller_ = browser_view()->immersive_mode_controller();

    // Disable animations in immersive fullscreen before we show the window,
    // which triggers an animation.
    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(controller_)->controller())
        .SetupForTest();

    browser_->window()->Show();
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  // Toggle the browser's fullscreen state.
  void ToggleFullscreen() {
    // The fullscreen change notification is sent asynchronously. The
    // notification is used to trigger changes in whether the shelf is auto
    // hidden.
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveModeControllerAsh::ANIMATE_REVEAL_NO));
    }
  }

  void VerifyButtonsInImmersiveMode(BrowserNonClientFrameViewAsh* frame_view) {
    WebAppFrameToolbarView* container =
        frame_view->web_app_frame_toolbar_for_testing();
    views::test::InkDropHostViewTestApi ink_drop_api(
        container->web_app_menu_button_);
    EXPECT_TRUE(container->GetContentSettingContainerForTesting()->layer());
    EXPECT_EQ(views::InkDropHostView::InkDropMode::ON,
              ink_drop_api.ink_drop_mode());
  }

  Browser* browser() { return browser_; }
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser_);
  }
  ImmersiveModeController* controller() { return controller_; }
  base::TimeDelta titlebar_animation_delay() {
    return WebAppFrameToolbarView::kTitlebarAnimationDelay;
  }

 private:
  web_app::AppId app_id;
  Browser* browser_ = nullptr;
  ImmersiveModeController* controller_ = nullptr;

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock_;

  net::EmbeddedTestServer https_server_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshWebAppBrowserTest);
};

// Test the layout and visibility of the TopContainerView and web contents when
// a web app is put into immersive fullscreen.
IN_PROC_BROWSER_TEST_P(ImmersiveModeControllerAshWebAppBrowserTest, Layout) {
  LaunchAppBrowser();
  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view = browser_view()->contents_web_view();
  views::View* top_container = browser_view()->top_container();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // The tabstrip is not visible for web apps.
  EXPECT_FALSE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());

  // The window header should be above the web contents.
  int header_height = GetBoundsInWidget(contents_web_view).y();

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Entering immersive fullscreen should make the web contents flush with the
  // top of the widget. The popup browser type doesn't support tabstrip and
  // toolbar feature, thus invisible.
  EXPECT_FALSE(tabstrip->GetVisible());
  EXPECT_FALSE(toolbar->GetVisible());
  EXPECT_TRUE(top_container->GetVisibleBounds().IsEmpty());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Reveal the window header.
  AttemptReveal();

  // The tabstrip should still be hidden and the web contents should still be
  // flush with the top of the screen.
  EXPECT_FALSE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // During an immersive reveal, the window header should be painted to the
  // TopContainerView. The TopContainerView should be flush with the top of the
  // widget and have |header_height|.
  gfx::Rect top_container_bounds_in_widget(GetBoundsInWidget(top_container));
  EXPECT_EQ(0, top_container_bounds_in_widget.y());
  EXPECT_EQ(header_height, top_container_bounds_in_widget.height());

  // Exit immersive fullscreen. The web contents should be back below the window
  // header.
  ToggleFullscreen();
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_EQ(header_height, GetBoundsInWidget(contents_web_view).y());
}

// Verify the immersive mode status is as expected in tablet mode (titlebars are
// autohidden in tablet mode).
IN_PROC_BROWSER_TEST_P(ImmersiveModeControllerAshWebAppBrowserTest,
                       ImmersiveModeStatusTabletMode) {
  LaunchAppBrowser();
  ASSERT_FALSE(controller()->IsEnabled());

  aura::Window* aura_window = browser_view()->frame()->GetNativeWindow();
  // Verify that after entering tablet mode, immersive mode is enabled, and the
  // the associated window's top inset is 0 (the top of the window is not
  // visible).
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(0, aura_window->GetProperty(aura::client::kTopViewInset));

  // Verify that after minimizing, immersive mode is disabled.
  browser()->window()->Minimize();
  EXPECT_TRUE(browser()->window()->IsMinimized());
  EXPECT_FALSE(controller()->IsEnabled());

  // Verify that after showing the browser, immersive mode is reenabled.
  browser()->window()->Show();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that immersive mode remains if fullscreen is toggled while in tablet
  // mode.
  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(false));
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that immersive mode remains if the browser was fullscreened when
  // entering tablet mode.
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that if the browser is not fullscreened, upon exiting tablet mode,
  // immersive mode is not enabled, and the associated window's top inset is
  // greater than 0 (the top of the window is visible).
  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(false));
  EXPECT_FALSE(controller()->IsEnabled());

  EXPECT_GT(aura_window->GetProperty(aura::client::kTopViewInset), 0);
}

// Verify that the frame layout is as expected when using immersive mode in
// tablet mode.
IN_PROC_BROWSER_TEST_P(ImmersiveModeControllerAshWebAppBrowserTest,
                       FrameLayoutToggleTabletMode) {
  LaunchAppBrowser();
  ASSERT_FALSE(controller()->IsEnabled());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          browser_view->GetWidget()->non_client_view()->frame_view());
  ash::FrameCaptionButtonContainerView* caption_button_container =
      frame_view->caption_button_container_;
  ash::FrameCaptionButtonContainerView::TestApi frame_test_api(
      caption_button_container);

  EXPECT_TRUE(frame_test_api.size_button()->GetVisible());

  // Verify the size button is hidden in tablet mode.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  frame_test_api.EndAnimations();

  EXPECT_TRUE(frame_test_api.size_button()->GetVisible());

  VerifyButtonsInImmersiveMode(frame_view);

  // Verify the size button is visible in clamshell mode, and that it does not
  // cover the other two buttons.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  frame_test_api.EndAnimations();

  EXPECT_TRUE(frame_test_api.size_button()->GetVisible());
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.close_button()->GetBoundsInScreen()));
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.minimize_button()->GetBoundsInScreen()));

  VerifyButtonsInImmersiveMode(frame_view);
}

// Verify that the frame layout for new windows is as expected when using
// immersive mode in tablet mode.
IN_PROC_BROWSER_TEST_P(ImmersiveModeControllerAshWebAppBrowserTest,
                       FrameLayoutStartInTabletMode) {
  // Start in tablet mode
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  BrowserNonClientFrameViewAsh* frame_view = nullptr;
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

    // Launch app window while in tablet mode
    LaunchAppBrowser(false);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    frame_view = static_cast<BrowserNonClientFrameViewAsh*>(
        browser_view->GetWidget()->non_client_view()->frame_view());

    task_runner->FastForwardBy(titlebar_animation_delay());

    VerifyButtonsInImmersiveMode(frame_view);
  }

  // Verify the size button is visible in clamshell mode, and that it does not
  // cover the other two buttons.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  VerifyButtonsInImmersiveMode(frame_view);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImmersiveModeControllerAshWebAppBrowserTest,
    ::testing::Values(
        web_app::ControllerType::kHostedAppController,
        web_app::ControllerType::kUnifiedControllerWithBookmarkApp,
        web_app::ControllerType::kUnifiedControllerWithWebApp));
