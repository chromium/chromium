// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "build/build_config.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace web_app {

using WebAppTabRestoreBrowserTest = WebAppNavigationBrowserTest;

// Tests that desktop PWAs are reopened at the correct size.
// TODO(crbug.com/40852083): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ReopenedPWASizeIsCorrectlyRestored \
  DISABLED_ReopenedPWASizeIsCorrectlyRestored
#else
#define MAYBE_ReopenedPWASizeIsCorrectlyRestored \
  ReopenedPWASizeIsCorrectlyRestored
#endif
IN_PROC_BROWSER_TEST_F(WebAppTabRestoreBrowserTest,
                       MAYBE_ReopenedPWASizeIsCorrectlyRestored) {
  InstallTestWebApp();
  Browser* const app_browser = LaunchWebAppBrowserAndWait(test_web_app_id());

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));
  NavigateViaLinkClickToURLAndWait(app_browser, test_web_app_start_url());

  const gfx::Rect bounds = gfx::Rect(50, 50, 550, 500);
  app_browser->window()->SetBounds(bounds);
  CloseAndWait(app_browser);

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  ASSERT_GT(service->entries().size(), 0U);
  sessions::tab_restore::Entry* entry = service->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  const auto* entry_win = static_cast<sessions::tab_restore::Window*>(entry);
  EXPECT_EQ(bounds, entry_win->bounds);

  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithTab(restored_web_contents);
  EXPECT_EQ(restored_browser->override_bounds(), bounds);
}

// Tests that app windows are correctly restored.
IN_PROC_BROWSER_TEST_F(WebAppTabRestoreBrowserTest, RestoreAppWindow) {
  InstallTestWebApp();
  Browser* const app_browser = LaunchWebAppBrowserAndWait(test_web_app_id());

  ASSERT_TRUE(app_browser->is_type_app());
  CloseAndWait(app_browser);

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithTab(restored_web_contents);

  EXPECT_TRUE(restored_browser->is_type_app());
}

// Tests that app popup windows are correctly restored.
IN_PROC_BROWSER_TEST_F(WebAppTabRestoreBrowserTest, RestoreAppPopupWindow) {
  InstallTestWebApp();
  Browser* const app_browser = web_app::LaunchWebAppBrowserAndWait(
      profile(), test_web_app_id(), WindowOpenDisposition::NEW_POPUP);

  ASSERT_TRUE(app_browser->is_type_app_popup());
  CloseAndWait(app_browser);

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithTab(restored_web_contents);

  EXPECT_TRUE(restored_browser->is_type_app_popup());
}

}  // namespace web_app
