// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/accessibility_checker.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using AccessibilityCheckerTest = ChromeViewsTestBase;

// Test that a view that is not accessible will fail the accessibility audit.
TEST_F(AccessibilityCheckerTest, VerifyAccessibilityCheckerFailAndPass) {
  // Create containing widget.
  views::Widget widget;
  views::Widget::InitParams params =
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  params.context = GetContext();
  widget.Init(std::move(params));
  widget.Show();

  // Add the button.
  views::ImageButton* button = new views::ImageButton(nullptr);
  widget.GetContentsView()->AddChildView(button);

  // Accessibility test should pass as it is focusable but has a name.
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetAccessibleName(base::ASCIIToUTF16("Some name"));
  AddFailureOnWidgetAccessibilityError(&widget);

  // Accessibility test should pass as it has no name but is not focusable.
  button->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  button->SetAccessibleName(base::ASCIIToUTF16(""));
  AddFailureOnWidgetAccessibilityError(&widget);

  // Accessibility test should fail as it has no name and is focusable.
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  EXPECT_NONFATAL_FAILURE(AddFailureOnWidgetAccessibilityError(&widget),
                          "Accessibility");
}
