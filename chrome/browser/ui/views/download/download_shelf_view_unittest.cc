// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_view.h"

#include <memory>

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_theme_provider.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"

using DownloadShelfViewTest = BrowserWithTestWindowTest;

TEST_F(DownloadShelfViewTest, ShowAllViewColors) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.context = GetContext();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  ChromeTestWidget widget;
  widget.Init(std::move(params));
  DownloadShelfView* view = widget.SetContentsView(
      std::make_unique<DownloadShelfView>(browser(), nullptr));
  views::MdTextButton* button = view->show_all_view_;

  // Event With the default theme, button text color is derived from
  // color_utils::PickGoogleColor from the SK_ColorBLUE class of colors.
  EXPECT_TRUE(button->GetBgColorOverride().has_value());
  SkColor default_text_color = button->GetCurrentTextColor();

  // Custom theme will update text and bg.
  auto custom_theme = std::make_unique<TestThemeProvider>();
  SkColor expected_text_color = color_utils::PickGoogleColor(
      SK_ColorBLUE, SK_ColorGREEN, color_utils::kMinimumReadableContrastRatio);
  custom_theme->SetColor(
      ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_BACKGROUND, SK_ColorGREEN);
  custom_theme->SetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_TEXT,
                         expected_text_color);
  widget.SetThemeProvider(std::move(custom_theme));
  // The button bg color is the shelf color.
  EXPECT_TRUE(button->GetBgColorOverride().has_value());
  EXPECT_EQ(button->GetBgColorOverride(), SK_ColorGREEN);
  EXPECT_EQ(button->GetCurrentTextColor(), expected_text_color);

  // Setting back to a default theme will revert.
  widget.SetThemeProvider(std::make_unique<TestThemeProvider>());
  EXPECT_TRUE(button->GetBgColorOverride().has_value());
  EXPECT_EQ(button->GetCurrentTextColor(), default_text_color);
}
