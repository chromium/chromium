// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_ui.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SidePanelUITest, FromReturnsSidePanelUI) {
  MockBrowserWindowInterface browser;
  MockSidePanelUI side_panel_ui(browser.GetUnownedUserDataHost());

  EXPECT_EQ(SidePanelUI::From(&browser), &side_panel_ui);
}
