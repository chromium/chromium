// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "url/gurl.h"

using AppMenuBrowserTest = InProcessBrowserTest;

namespace {

bool TabRestoreServiceHasClosedWindow(sessions::TabRestoreService* service) {
  for (const auto& entry : service->entries()) {
    if (entry->type == sessions::TabRestoreService::WINDOW)
      return true;
  }
  return false;
}

}  // namespace

// This test shows the app-menu with a closed window added to the
// TabRestoreService. This is a regression test to ensure menu code handles this
// properly (this was triggering a crash in AppMenu where it was trying to make
// use of RecentTabsMenuModelDelegate before created). See
// https://crbug.com/1249741 for more.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, ShowWithRecentlyClosedWindow) {
  // Create an additional browser, close it, and ensure it is added to the
  // TabRestoreService.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  TabRestoreServiceLoadWaiter tab_restore_service_load_waiter(
      tab_restore_service);
  tab_restore_service_load_waiter.Wait();
  Browser* second_browser = CreateBrowser(browser()->profile());
  content::WebContents* new_contents = chrome::AddSelectedTabWithURL(
      second_browser,
      ui_test_utils::GetTestUrl(base::FilePath(),
                                base::FilePath().AppendASCII("simple.html")),
      ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  chrome::CloseWindow(second_browser);
  ui_test_utils::WaitForBrowserToClose(second_browser);
  EXPECT_TRUE(TabRestoreServiceHasClosedWindow(tab_restore_service));

  // Show the AppMenu.
  BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->app_menu_button()
      ->ShowMenu(views::MenuRunner::NO_FLAGS);
}
