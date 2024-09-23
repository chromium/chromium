// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "content/public/test/browser_test.h"

class WebAppAshInteractiveUITest : public web_app::WebAppBrowserTestBase {
 public:
  WebAppAshInteractiveUITest() = default;

  WebAppAshInteractiveUITest(const WebAppAshInteractiveUITest&) = delete;
  WebAppAshInteractiveUITest& operator=(const WebAppAshInteractiveUITest&) =
      delete;

  ~WebAppAshInteractiveUITest() override = default;

  // web_app::WebAppBrowserTestBase override:
  void SetUpOnMainThread() override {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL("https://test.org"));
    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

    Browser* browser = LaunchWebAppBrowser(app_id);
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser);

    controller_ = browser_view_->immersive_mode_controller();
    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(controller_)
            ->controller())
        .SetupForTest();
    WebAppToolbarButtonContainer::DisableAnimationForTesting(true);
  }

  void CheckWebAppMenuClickable() {
    AppMenuButton* menu_button =
        browser_view_->toolbar_button_provider()->GetAppMenuButton();

    // Open the app menu by clicking on it.
    base::RunLoop open_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        menu_button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        open_loop.QuitClosure());
    open_loop.Run();
    EXPECT_TRUE(menu_button->IsMenuShowing());

    // Close the app menu by clicking on it again.
    base::RunLoop close_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        menu_button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        close_loop.QuitClosure());
    close_loop.Run();
    EXPECT_FALSE(menu_button->IsMenuShowing());
  }

  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<ImmersiveModeController, DanglingUntriaged> controller_ = nullptr;
};

// Test that the web app menu button opens a menu on click.
IN_PROC_BROWSER_TEST_F(WebAppAshInteractiveUITest, MenuButtonClickable) {
  CheckWebAppMenuClickable();
}

// Test that the web app menu button opens a menu on click in immersive mode.
IN_PROC_BROWSER_TEST_F(WebAppAshInteractiveUITest,
                       ImmersiveMenuButtonClickable) {
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      controller_->GetRevealedLock(
          ImmersiveModeControllerChromeos::ANIMATE_REVEAL_NO);

  CheckWebAppMenuClickable();
}
