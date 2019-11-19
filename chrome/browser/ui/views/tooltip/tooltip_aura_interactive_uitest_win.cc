// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"

class TooltipAuraUiaTest : public InProcessBrowserTest {
 public:
  TooltipAuraUiaTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExperimentalUIAutomation);
  }
};

// Flakily tests: http://crbug.com/990214
IN_PROC_BROWSER_TEST_F(TooltipAuraUiaTest, DISABLED_TooltipUiaEvents) {
  // Setup accessibility waiter
  HWND window_handle = GetDesktopWindow();
  UiaAccessibilityWaiterInfo opened_info = {
      window_handle, base::ASCIIToUTF16("tooltip"),
      base::ASCIIToUTF16("Reload this page"), ax::mojom::Event::kTooltipOpened};
  UiaAccessibilityEventWaiter opened_waiter(opened_info);

  // Move mouse to Refresh button
  views::View* refresh_view =
      BrowserView::GetBrowserViewForBrowser(browser())->GetViewByID(
          VIEW_ID_RELOAD_BUTTON);
  gfx::Point center_refresh =
      ui_test_utils::GetCenterInScreenCoordinates(refresh_view);
  ui_controls::SendMouseMove(center_refresh.x(), center_refresh.y());

  // Wait for accessibility event
  opened_waiter.Wait();

  // Setup accessibility waiter
  UiaAccessibilityWaiterInfo closed_info = {
      window_handle, base::ASCIIToUTF16("tooltip"),
      base::ASCIIToUTF16("Reload this page"), ax::mojom::Event::kTooltipClosed};
  UiaAccessibilityEventWaiter closed_waiter(closed_info);

  // Move mouse away from the refresh button
  views::View* omnibox_view =
      BrowserView::GetBrowserViewForBrowser(browser())->GetViewByID(
          VIEW_ID_OMNIBOX);
  gfx::Point center_omnibox =
      ui_test_utils::GetCenterInScreenCoordinates(omnibox_view);
  ui_controls::SendMouseMove(center_omnibox.x(), center_omnibox.y());

  // Wait for accessibility event
  closed_waiter.Wait();
}
