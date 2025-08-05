// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/save_to_drive/add_account_dialog_controller.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_contents_factory.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/switches.h"

namespace save_to_drive {
namespace {

class AddAccountDialogControllerTest : public InteractiveBrowserTest {
 public:
  AddAccountDialogControllerTest() = default;
  ~AddAccountDialogControllerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AddAccountDialogControllerTest, ShowAndHide) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  AddAccountDialogController dialog(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Show the dialog.
  dialog.Show();
  // A new browser window should open.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* popup_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(popup_browser);
  EXPECT_TRUE(popup_browser->is_type_popup());

  // Close the dialog.
  dialog.Close();
  // The popup window should close.
  ui_test_utils::WaitForBrowserToClose(popup_browser);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AddAccountDialogControllerTest, AtMostOnePopupWindow) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  AddAccountDialogController dialog(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Show the dialog.
  dialog.Show();
  // A new browser window should open.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Show the dialog again.
  dialog.Show();
  // The second call should not open a new window.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* popup_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(popup_browser);
  EXPECT_TRUE(popup_browser->is_type_popup());
}

IN_PROC_BROWSER_TEST_F(AddAccountDialogControllerTest,
                       PopupAtCenterOfSourceWindow) {
  // Set the source window size to be large enough to contain the popup window.
  gfx::Rect source_window_bounds = gfx::Rect(0, 0, 1000, 1000);
  browser()->window()->SetBounds(source_window_bounds);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  AddAccountDialogController dialog(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Show the dialog.
  dialog.Show();
  // A new browser window should open.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* popup_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(popup_browser);
  EXPECT_TRUE(popup_browser->is_type_popup());

  // Check popup size. The size is computed in add_account_dialog_controller.cc.
  // kPopupWindowWidth = 400, kPopupWindowPreferredHeight = 484.
  gfx::Rect bounds = popup_browser->window()->GetBounds();
  EXPECT_EQ(bounds.width(), 400);
  EXPECT_EQ(bounds.height(), 484);
  // This can be platform specific, so we only check that the popup is within
  // the source window.
  EXPECT_GT(bounds.x(), source_window_bounds.x());
  EXPECT_LT(bounds.x(),
            source_window_bounds.x() + source_window_bounds.width());
  EXPECT_GT(bounds.y(), source_window_bounds.y());
  EXPECT_LT(bounds.y(),
            source_window_bounds.y() + source_window_bounds.height());
}

IN_PROC_BROWSER_TEST_F(
    AddAccountDialogControllerTest,
    PopupPositionedInCenterOfScreenIfSourceWindowIsTooSmall) {
  // Set the source window size to be too small to contain the popup window.
  gfx::Rect source_window_bounds = gfx::Rect(0, 0, 0, 0);
  browser()->window()->SetBounds(source_window_bounds);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  AddAccountDialogController dialog(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Show the dialog.
  dialog.Show();
  // A new browser window should open.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* popup_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(popup_browser);
  EXPECT_TRUE(popup_browser->is_type_popup());

  // Check popup size. The size is computed in add_account_dialog_controller.cc.
  // kPopupWindowWidth = 400, kPopupWindowPreferredHeight = 484.
  gfx::Rect bounds = popup_browser->window()->GetBounds();
  EXPECT_EQ(bounds.width(), 400);
  EXPECT_EQ(bounds.height(), 484);
  // This can be platform specific, so we only check that the popup is not
  // centered in the source window.
  EXPECT_GT(bounds.x(), source_window_bounds.x());
  EXPECT_GT(bounds.y(), source_window_bounds.y());
}

}  // namespace
}  // namespace save_to_drive
