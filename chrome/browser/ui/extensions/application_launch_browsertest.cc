// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/native_widget_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ApplicationLaunchBrowserTest : public InProcessBrowserTest {
 public:
  ApplicationLaunchBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFocusMode);
  }

  content::WebContents* GetWebContentsForTab(Browser* browser, int index) {
    return browser->tab_strip_model()->GetWebContentsAt(index);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest,
                       ReparentWebContentsForFocusModeSingleTab) {
  const GURL url("http://aaa.com/empty.html");
  ui_test_utils::NavigateToURL(browser(), url);

  Browser* app_browser = web_app::ReparentWebContentsForFocusMode(
      GetWebContentsForTab(browser(), 0));
  EXPECT_TRUE(app_browser->is_type_app());
  EXPECT_NE(app_browser, browser());

  content::WebContents* main_browser_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(main_browser_web_contents);
  EXPECT_NE(url, main_browser_web_contents->GetLastCommittedURL());

  GURL app_browser_url = app_browser->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL();
  EXPECT_EQ(url, app_browser_url);
  EXPECT_TRUE(app_browser->is_focus_mode());
}

IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest,
                       ReparentWebContentsForFocusModeMultipleTabs) {
  const GURL url("http://aaa.com/empty.html");
  chrome::AddTabAt(browser(), url, -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_FALSE(browser()->is_focus_mode());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  Browser* app_browser = web_app::ReparentWebContentsForFocusMode(
      GetWebContentsForTab(browser(), 1));
  EXPECT_TRUE(app_browser->is_type_app());
  EXPECT_NE(app_browser, browser());
  content::WebContents* web_contents = GetWebContentsForTab(app_browser, 0);
  // Note: Since we're not using the EmbeddedTestServer, we don't expect this
  // navigation to succeed.
  content::WaitForLoadStopWithoutSuccessCheck(web_contents);
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(app_browser->is_focus_mode());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest, CreateWindowInDisplay) {
  display::Screen* screen = display::Screen::GetScreen();
  // Create 2 displays.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi display_manager_test(display_manager);
  display_manager_test.UpdateDisplay("800x800,801+0-800x800");
  int64_t display1 = screen->GetPrimaryDisplay().id();
  int64_t display2 = display_manager_test.GetSecondaryDisplay().id();
  EXPECT_EQ(2, screen->GetNumDisplays());

  // Primary display should hold browser() and be the default for new windows.
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  EXPECT_EQ(display1, screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(display1, screen->GetDisplayForNewWindows().id());

  // Create browser2 on display 2.
  apps::AppLaunchParams params(
      "app_id", apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceAppLauncher, display2);
  Browser* browser2 =
      CreateApplicationWindow(browser()->profile(), params, GURL());
  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  EXPECT_EQ(display2, screen->GetDisplayNearestWindow(window2).id());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
