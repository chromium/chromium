// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "components/permissions/request_type.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/window/frame_caption_button.h"

class ImmersiveModeControllerChromeosWebAppBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  ImmersiveModeControllerChromeosWebAppBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ImmersiveModeControllerChromeosWebAppBrowserTest(
      const ImmersiveModeControllerChromeosWebAppBrowserTest&) = delete;
  ImmersiveModeControllerChromeosWebAppBrowserTest& operator=(
      const ImmersiveModeControllerChromeosWebAppBrowserTest&) = delete;

  ~ImmersiveModeControllerChromeosWebAppBrowserTest() override = default;

  // WebAppBrowserTestBase override:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    const GURL app_url = GetAppUrl();
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app_info->theme_color = SK_ColorBLUE;

    app_id = InstallWebApp(std::move(web_app_info));
  }

  GURL GetAppUrl() { return https_server_.GetURL("/simple.html"); }

  void LaunchAppBrowser(bool wait = true) {
    ui_test_utils::UrlLoadObserver url_observer(GetAppUrl());
    browser_ = LaunchWebAppBrowser(app_id);

    if (wait) {
      // Wait for the URL to load so that the location bar end-state stabilizes.
      url_observer.Wait();
    }
    controller_ = browser_view()->immersive_mode_controller();

    // Disable animations in immersive fullscreen before we show the window,
    // which triggers an animation.
    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(controller_)
            ->controller())
        .SetupForTest();

    browser_->window()->Show();
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_ = controller_->GetRevealedLock(
          ImmersiveModeControllerChromeos::ANIMATE_REVEAL_NO);
    }
  }

  void VerifyButtonsInImmersiveMode(BrowserView* browser_view) {
    WebAppFrameToolbarView* container =
        browser_view->web_app_frame_toolbar_for_testing();
    views::test::InkDropHostTestApi ink_drop_api(
        views::InkDrop::Get(container->GetAppMenuButton()));
    EXPECT_TRUE(container->GetContentSettingContainerForTesting()->layer());
    EXPECT_EQ(views::InkDropHost::InkDropMode::ON,
              ink_drop_api.ink_drop_mode());
  }

  Browser* browser() { return browser_; }
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser_);
  }
  ImmersiveModeController* controller() { return controller_; }
  base::TimeDelta titlebar_animation_delay() {
    return WebAppToolbarButtonContainer::kTitlebarAnimationDelay;
  }

 private:
  webapps::AppId app_id;
  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;
  raw_ptr<ImmersiveModeController, DanglingUntriaged> controller_ = nullptr;

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock_;

  net::EmbeddedTestServer https_server_;
};

// Test the layout and visibility of the TopContainerView and web contents when
// a web app is put into immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerChromeosWebAppBrowserTest,
                       Layout) {
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

  EnterImmersiveFullscreenMode(browser());
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
  ExitImmersiveFullscreenMode(browser());
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_EQ(header_height, GetBoundsInWidget(contents_web_view).y());
}

// Verify the immersive mode status is as expected in tablet mode (titlebars are
// autohidden in tablet mode).
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerChromeosWebAppBrowserTest,
                       ImmersiveModeStatusTabletMode) {
  LaunchAppBrowser();
  ASSERT_FALSE(controller()->IsEnabled());

  aura::Window* aura_window = browser_view()->frame()->GetNativeWindow();
  // Verify that after entering tablet mode, immersive mode is enabled, and the
  // the associated window's top inset is 0 (the top of the window is not
  // visible).
  EnterTabletMode();
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
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  ExitTabletMode();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that immersive mode remains if the browser was fullscreened when
  // entering tablet mode.
  EnterTabletMode();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that if the browser is not fullscreened, upon exiting tablet mode,
  // immersive mode is not enabled, and the associated window's top inset is
  // greater than 0 (the top of the window is visible).
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  ExitTabletMode();
  EXPECT_FALSE(controller()->IsEnabled());

  EXPECT_GT(aura_window->GetProperty(aura::client::kTopViewInset), 0);
}

// Verify that the frame layout is as expected when using immersive mode in
// tablet mode.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerChromeosWebAppBrowserTest,
                       FrameLayoutToggleTabletMode) {
  LaunchAppBrowser();
  ASSERT_FALSE(controller()->IsEnabled());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      static_cast<BrowserNonClientFrameViewChromeOS*>(
          browser_view->GetWidget()->non_client_view()->frame_view());
  chromeos::FrameCaptionButtonContainerView* caption_button_container =
      frame_view->caption_button_container();
  chromeos::FrameCaptionButtonContainerView::TestApi frame_test_api(
      caption_button_container);

  EXPECT_TRUE(frame_test_api.size_button()->GetVisible());

  // Verify the size button is hidden in tablet mode.
  EnterTabletMode();
  frame_test_api.EndAnimations();

  EXPECT_FALSE(frame_test_api.size_button()->GetVisible());

  VerifyButtonsInImmersiveMode(browser_view);

  // Verify the size button is visible in clamshell mode, and that it does not
  // cover the other two buttons.
  ExitTabletMode();
  frame_test_api.EndAnimations();

  EXPECT_TRUE(frame_test_api.size_button()->GetVisible());
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.close_button()->GetBoundsInScreen()));
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.minimize_button()->GetBoundsInScreen()));

  VerifyButtonsInImmersiveMode(browser_view);
}

// Verify that the frame layout for new windows is as expected when using
// immersive mode in tablet mode.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerChromeosWebAppBrowserTest,
                       FrameLayoutStartInTabletMode) {
  // Start in tablet mode
  EnterTabletMode();

  // Launch app window while in tablet mode
  LaunchAppBrowser(false);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  {
    // Skip the title bar animation.
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);
    task_runner->FastForwardBy(titlebar_animation_delay());
  }

  VerifyButtonsInImmersiveMode(browser_view);

  // Verify the size button is visible in clamshell mode, and that it does not
  // cover the other two buttons.
  ExitTabletMode();
  VerifyButtonsInImmersiveMode(browser_view);
}

// Tests that the permissions bubble dialog is anchored to the correct location.
// The dialog's anchor is normally the app menu button which is on the header.
// In immersive mode but not revealed, the app menu button is placed off screen
// but still drawn. In this case, we should have a null anchor view so that the
// bubble gets placed in the default top left corner. Regression test for
// https://crbug.com/1087143.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerChromeosWebAppBrowserTest,
                       PermissionsBubbleAnchor) {
  LaunchAppBrowser();
  auto test_api =
      std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  EXPECT_TRUE(test_api->manager());

  // Add a permission bubble using the test api.
  test_api->AddSimpleRequest(browser()
                                 ->tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetPrimaryMainFrame(),
                             permissions::RequestType::kGeolocation);

  // The permission prompt is shown asynchronously. Without immersive mode
  // enabled the anchor should exist.
  // TODO(crbug.com/40835018): Change from RunUntilIdle to a more
  // explicit notification.
  base::RunLoop().RunUntilIdle();

  views::Widget* prompt_widget = test_api->GetPromptWindow();
  views::BubbleDialogDelegate* bubble_dialog =
      prompt_widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_dialog);
  EXPECT_TRUE(bubble_dialog->GetAnchorView());

  // Turn on immersive, but do not reveal.
  auto* immersive_mode_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->immersive_mode_controller();
  immersive_mode_controller->SetEnabled(true);

  // Since a bubble was visible and anchored to the header, the header should
  // have been automatically revealed.
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(bubble_dialog->GetAnchorView());

  // Closing the bubble should cause the header to no longer be revealed.
  bubble_dialog->AcceptDialog();
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  // Make sure the old permission prompt fully goes away before opening a new
  // prompt.
  // TODO(crbug.com/40835018): Change from RunUntilIdle to a more
  // explicit notification.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(test_api->GetPromptWindow());

  // Opening a new permission bubble should not cause the header to reveal.
  test_api->AddSimpleRequest(browser()
                                 ->tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetPrimaryMainFrame(),
                             permissions::RequestType::kMicStream);

  // The permission prompt is shown asynchronously.
  // TODO(crbug.com/40835018): Change from RunUntilIdle to a more
  // explicit notification.
  base::RunLoop().RunUntilIdle();
  prompt_widget = test_api->GetPromptWindow();
  ASSERT_TRUE(prompt_widget);
  ASSERT_TRUE(prompt_widget->widget_delegate());
  bubble_dialog = prompt_widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_dialog);

  // The app menu button is hidden from
  // sight so the anchor should be null. The bubble will get placed in the top
  // left corner of the app.
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(bubble_dialog->GetAnchorView());

  // Reveal the header. The anchor should exist since the app menu button is
  // now visible.
  {
    std::unique_ptr<ImmersiveRevealedLock> focus_reveal_lock =
        immersive_mode_controller->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_YES);
    EXPECT_TRUE(immersive_mode_controller->IsRevealed());
    EXPECT_TRUE(bubble_dialog->GetAnchorView());
  }

  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(bubble_dialog->GetAnchorView());
}
