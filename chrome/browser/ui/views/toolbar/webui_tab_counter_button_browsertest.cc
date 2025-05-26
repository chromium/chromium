// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"

#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

using WebUITabCounterButtonTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WebUITabCounterButtonTest, CheckFocusBehavior) {
  auto button = CreateWebUITabCounterButton(
      views::Button::PressedCallback(),
      BrowserView::GetBrowserViewForBrowser(browser()));
  EXPECT_EQ(button->GetFocusBehavior(),
            views::View::FocusBehavior::ACCESSIBLE_ONLY);
}
