// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/chromeos/crosier/aura_window_title_observer.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "components/app_constants/constants.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/env.h"
#include "ui/aura/test/find_window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

// The title of a window at the new tab page varies based on branding.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char16_t kBrowserWindowTitle[] = u"Chrome - New Tab";
#else
const char16_t kBrowserWindowTitle[] = u"Chromium - New Tab";
#endif

using ShelfIntegrationTest = InteractiveAshTest;

IN_PROC_BROWSER_TEST_F(ShelfIntegrationTest, OpenCloseSwitchApps) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-441c1034-bfff-42c0-ad81-79332c9e2304");

  SetupContextWidget();

  // Ensure shelf icon positions are stable.
  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  InstallSystemApps();  // For help app.

  // Get the primary display's shelf view.
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_TRUE(shelf);
  ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ASSERT_TRUE(shelf_view);
  ShelfViewTestAPI test_api(shelf_view);

  // The shelf starts with the browser, Files, and Discover app pinned.
  ASSERT_EQ(test_api.GetButtonCount(), 3u);
  auto* shelf_model = ShelfModel::Get();
  ASSERT_TRUE(shelf_model->IsAppPinned(app_constants::kChromeAppId));
  ASSERT_TRUE(shelf_model->IsAppPinned(file_manager::kFileManagerSwaAppId));
  ASSERT_TRUE(shelf_model->IsAppPinned(web_app::kHelpAppId));

  // Use the chrome button and the help app buttons because they use simpler
  // web pages than the files app.
  ShelfAppButton* chrome_button = test_api.GetButton(0);
  ASSERT_TRUE(chrome_button);
  ShelfAppButton* help_app_button = test_api.GetButton(2);
  ASSERT_TRUE(help_app_button);

  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  // Define our own AuraWindowTitleObservers because we wait for window titles
  // more than once, and hence can't use the WaitForWindowWithTitle() helper.
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(AuraWindowTitleObserver,
                                      kBrowserTitleObserver);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(AuraWindowTitleObserver,
                                      kHelpAppTitleObserver);

  RunTestSequence(
      Log("Clicking the chrome shelf button"),
      ObserveState(
          kBrowserTitleObserver,
          std::make_unique<AuraWindowTitleObserver>(env, kBrowserWindowTitle)),
      MoveMouseTo(chrome_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(),

      Log("Waiting for Chrome window to open"),
      WaitForState(kBrowserTitleObserver, true),

      Log("Clicking the help app shelf button"),
      ObserveState(kHelpAppTitleObserver,
                   std::make_unique<AuraWindowTitleObserver>(env, u"Explore")),
      MoveMouseTo(help_app_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(),

      Log("Waiting for help app window to open"),
      WaitForState(kHelpAppTitleObserver, true),

      // Wait for help app to move to foreground.
      FlushEvents(),

      Log("Clicking the chrome shelf button again"),
      MoveMouseTo(chrome_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(),

      Log("Verifying the browser window is activated"), Check([&]() {
        aura::Window* browser_window =
            aura::test::FindWindowWithTitle(env, kBrowserWindowTitle);
        CHECK(browser_window);
        aura::Window* active_window = window_util::GetActiveWindow();
        CHECK(active_window);
        return browser_window == active_window;
      }),

      Log("Clicking the help app shelf button again"),
      MoveMouseTo(help_app_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(),

      Log("Verifying the help app window is activated"), Check([&]() {
        aura::Window* help_window =
            aura::test::FindWindowWithTitle(env, u"Explore");
        CHECK(help_window);
        aura::Window* active_window = window_util::GetActiveWindow();
        CHECK(active_window);
        return help_window == active_window;
      }),

      Log("Closing Chrome via right-click menu"),
      MoveMouseTo(chrome_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(ui_controls::RIGHT), SelectMenuItem(kShelfCloseMenuItem),
      FlushEvents(),

      Log("Verifying the browser window is gone"), Check([&]() {
        return !aura::test::FindWindowWithTitle(env, kBrowserWindowTitle);
      }),

      Log("Closing help app via right-click menu"),
      MoveMouseTo(help_app_button->GetBoundsInScreen().CenterPoint()),
      ClickMouse(ui_controls::RIGHT), SelectMenuItem(kShelfCloseMenuItem),
      FlushEvents(),

      Log("Verifying the help app window is gone"), Check([&]() {
        return !aura::test::FindWindowWithTitle(env, u"Explore");
      }),

      Log("Test completed"));
}

}  // namespace
}  // namespace ash
