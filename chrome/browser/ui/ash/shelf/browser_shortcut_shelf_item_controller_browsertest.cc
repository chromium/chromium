// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"

using BrowserShortcutShelfItemControllerTest = InProcessBrowserTest;

namespace {

ash::ShelfItemDelegate::AppMenuItems GetAppMenuItems(
    BrowserShortcutShelfItemController* controller,
    int event_flags) {
  return controller->GetAppMenuItems(event_flags, base::NullCallback());
}

}  // namespace

// Test the browser application menu for some browser window and tab states.
IN_PROC_BROWSER_TEST_F(BrowserShortcutShelfItemControllerTest, AppMenu) {
  BrowserShortcutShelfItemController* controller =
      ChromeShelfController::instance()
          ->GetBrowserShortcutShelfItemControllerForTesting();
  ASSERT_TRUE(controller);

  // InProcessBrowserTest's default browser window is shown with a blank tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  auto items = GetAppMenuItems(controller, ui::EF_NONE);
  ASSERT_EQ(1U, items.size());
  EXPECT_EQ(u"about:blank", items[0].title);

  // Browsers are not listed in the menu if their windows have not been shown.
  Browser* browser1 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  EXPECT_FALSE(browser1->window()->IsVisible());
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(1U, GetAppMenuItems(controller, ui::EF_NONE).size());

  // Browsers shown with no active tab appear as "New Tab" without crashing.
  browser1->window()->Show();
  EXPECT_TRUE(browser1->window()->IsVisible());
  EXPECT_FALSE(browser1->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(2U, browser_list->size());
  items = GetAppMenuItems(controller, ui::EF_NONE);
  ASSERT_EQ(2U, items.size());
  EXPECT_EQ(u"about:blank", items[0].title);
  EXPECT_EQ(u"New Tab", items[1].title);

  // Browsers are listed with the title of their active contents.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<title>0</title>")));
  AddBlankTabAndShow(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, GURL("data:text/html,<title>1</title>")));
  AddBlankTabAndShow(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, GURL("data:text/html,<title>2</title>")));
  EXPECT_EQ(1, browser1->tab_strip_model()->active_index());
  items = GetAppMenuItems(controller, ui::EF_NONE);
  ASSERT_EQ(2U, items.size());
  EXPECT_EQ(u"0", items[0].title);
  EXPECT_EQ(u"2", items[1].title);

  // Setting the window title will update the app menu item.
  browser1->SetWindowUserTitle("foobar");
  items = GetAppMenuItems(controller, ui::EF_NONE);
  ASSERT_EQ(2U, items.size());
  EXPECT_EQ(u"0", items[0].title);
  EXPECT_EQ(u"foobar", items[1].title);

  // If the window title is cleared, the active content title will be set again
  // as the menu item title.
  browser1->SetWindowUserTitle("");
  items = GetAppMenuItems(controller, ui::EF_NONE);
  ASSERT_EQ(2U, items.size());
  EXPECT_EQ(u"0", items[0].title);
  EXPECT_EQ(u"2", items[1].title);

  // Shift-click will list all tabs in the applicable browsers.
  items = GetAppMenuItems(controller, ui::EF_SHIFT_DOWN);
  ASSERT_EQ(items.size(), 3U);
  EXPECT_EQ(u"0", items[0].title);
  EXPECT_EQ(u"1", items[1].title);
  EXPECT_EQ(u"2", items[2].title);

  // Close the window and wait for all asynchronous window teardown.
  CloseBrowserSynchronously(browser1);
  EXPECT_EQ(1U, browser_list->size());
  // Selecting an app menu item for the closed browser window should not crash.
  controller->ExecuteCommand(/*from_context_menu=*/false, /*command_id=*/1,
                             ui::EF_NONE, display::kInvalidDisplayId);

  // Create and close a window, but don't allow asynchronous teardown to occur.
  browser1 = CreateBrowser(browser()->profile());
  EXPECT_EQ(2U, browser_list->size());
  CloseBrowserAsynchronously(browser1);
  EXPECT_EQ(2U, browser_list->size());
  // The app menu should not list the browser window while it is closing.
  items = GetAppMenuItems(controller, ui::EF_NONE);
  EXPECT_EQ(1U, items.size());
  // Now, allow the asynchronous teardown to occur.
  EXPECT_EQ(2U, browser_list->size());
  ui_test_utils::WaitForBrowserToClose(browser1);
  EXPECT_EQ(1U, browser_list->size());
}
