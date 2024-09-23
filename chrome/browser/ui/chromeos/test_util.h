// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_TEST_UTIL_H_
#define CHROME_BROWSER_UI_CHROMEOS_TEST_UTIL_H_

#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

class Browser;
class BrowserNonClientFrameViewChromeOS;
class BrowserView;

namespace views {
class Widget;
}

// This class is meant primarily for writing UI-centric ChromeOS browser tests
// that work for both Ash and Lacros(*), and in the latter case do so by using
// crosapi's TestController behind the scenes to perform system-level UI actions
// such as toggling tablet mode.
//
// (*) Typically, these tests are built into the `browser_tests` target for Ash,
// and into the `lacros_chrome_browsertests` target for Lacros.
class ChromeOSBrowserUITest : public MixinBasedInProcessBrowserTest {
 public:
  ChromeOSBrowserUITest() = default;
  ChromeOSBrowserUITest(const ChromeOSBrowserUITest&) = delete;
  ChromeOSBrowserUITest& operator=(const ChromeOSBrowserUITest&) = delete;
  ~ChromeOSBrowserUITest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  // Checks whether the ChromeOS UI is currently in tablet mode (as opposed to
  // clamshell mode).
  static bool InTabletMode();

  // Makes the ChromeOS UI enter/exit tablet mode.
  void EnterTabletMode();
  void ExitTabletMode();

  // Makes the ChromeOS UI enter/exit overview mode.
  static void EnterOverviewMode();
  static void ExitOverviewMode();

  // Checks whether the `SnapWindow` method can be called.
  static bool IsSnapWindowSupported();

  // Snaps the given window to the given part of the screen
  static void SnapWindow(aura::Window* window,
                         crosapi::mojom::SnapPosition position);

  // Pins the given window. This is also known as locked fullscreen mode.
  static void PinWindow(aura::Window* window, bool trusted);

  // Checks whether the `IsShelfVisible` method can be called.
  static bool IsIsShelfVisibleSupported();

  // Checks whether the ChromeOS UI currently shows the shelf (on the primary
  // display).
  static bool IsShelfVisible();

  // Deactivates the given widget.
  static void DeactivateWidget(views::Widget* widget);

  // Enters/exits immersive fullscreen mode for the given browser.
  static void EnterImmersiveFullscreenMode(Browser* browser);
  static void ExitImmersiveFullscreenMode(Browser* browser);

  // Enters/exits fullscreen mode in th tab associated with the given contents.
  static void EnterTabFullscreenMode(Browser* browser,
                                     content::WebContents* web_contents);
  static void ExitTabFullscreenMode(Browser* browser,
                                    content::WebContents* web_contents);

  // Returns the non-client frame view for `browser_view`.
  static BrowserNonClientFrameViewChromeOS* GetFrameViewChromeOS(
      BrowserView* browser_view);

 private:
  void SetTabletMode(bool enabled);
  static void SetOverviewMode(bool enabled);
};

#endif  // CHROME_BROWSER_UI_CHROMEOS_TEST_UTIL_H_
