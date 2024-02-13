// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_util_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"

// TODO(lgrey): Convert these into unit tests.
class FullscreenUtilMacTest : public InProcessBrowserTest {
 protected:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  void EnterTabFullscreen() {
    content::WebContentsDelegate* contents_delegate =
        static_cast<content::WebContentsDelegate*>(browser());
    contents_delegate->EnterFullscreenModeForTab(
        GetWebContents()->GetPrimaryMainFrame(), {});
  }

  void ExitTabFullscreen() {
    content::WebContentsDelegate* contents_delegate =
        static_cast<content::WebContentsDelegate*>(browser());
    contents_delegate->ExitFullscreenModeForTab(GetWebContents());
  }

  FullscreenController* GetFullscreenController() {
    return browser()->exclusive_access_manager()->fullscreen_controller();
  }

  bool IsBrowserFullscreen() {
    return GetFullscreenController()->IsFullscreenForBrowser();
  }
};

IN_PROC_BROWSER_TEST_F(FullscreenUtilMacTest, IsInContentFullscreen) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
  // By default
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));

  // Via extension API
  // Toggle on:
  GetFullscreenController()->ToggleBrowserFullscreenModeWithExtension(
      GURL("https://example.com"));
  EXPECT_TRUE(fullscreen_utils::IsInContentFullscreen(browser()));

  // Toggle off:
  GetFullscreenController()->ToggleBrowserFullscreenModeWithExtension(
      GURL("https://example.com"));
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));

  // Via web API
  EnterTabFullscreen();
  EXPECT_TRUE(fullscreen_utils::IsInContentFullscreen(browser()));

  ExitTabFullscreen();
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));

  // Browser fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(IsBrowserFullscreen());
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));

  // Nested
  EnterTabFullscreen();
  EXPECT_TRUE(fullscreen_utils::IsInContentFullscreen(browser()));

  ExitTabFullscreen();
  ASSERT_TRUE(IsBrowserFullscreen());
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));

  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_FALSE(IsBrowserFullscreen());
  EXPECT_FALSE(fullscreen_utils::IsInContentFullscreen(browser()));
}

IN_PROC_BROWSER_TEST_F(FullscreenUtilMacTest, AlwaysShowToolbar) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  bool original_always_show = prefs->GetBoolean(prefs::kShowFullscreenToolbar);

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, false);
  EXPECT_FALSE(fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()));

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);
  EXPECT_TRUE(fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()));

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, original_always_show);
  // TODO(lgrey): Add PWA test if anyone can think of a good way to do that.
}
