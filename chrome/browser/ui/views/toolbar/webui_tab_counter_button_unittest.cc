// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

using WebUITabCounterButtonTest = TestWithBrowserView;

TEST_F(WebUITabCounterButtonTest, CheckFocusBehavior) {
  auto button = CreateWebUITabCounterButton(views::Button::PressedCallback(),
                                            browser_view());
  EXPECT_EQ(views::View::FocusBehavior::ACCESSIBLE_ONLY,
            button->GetFocusBehavior());
}
