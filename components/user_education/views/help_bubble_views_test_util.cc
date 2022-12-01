// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_views_test_util.h"

namespace user_education::test {

TestHelpBubbleDelegate::TestHelpBubbleDelegate() = default;
TestHelpBubbleDelegate::~TestHelpBubbleDelegate() = default;

std::vector<ui::Accelerator>
TestHelpBubbleDelegate::GetPaneNavigationAccelerators(
    ui::TrackedElement* anchor_element) const {
  return std::vector<ui::Accelerator>();
}

int TestHelpBubbleDelegate::GetTitleTextContext() const {
  return 0;
}
int TestHelpBubbleDelegate::GetBodyTextContext() const {
  return 0;
}

ui::ColorId TestHelpBubbleDelegate::GetHelpBubbleBackgroundColorId() const {
  return 0;
}
ui::ColorId TestHelpBubbleDelegate::GetHelpBubbleForegroundColorId() const {
  return 0;
}
ui::ColorId
TestHelpBubbleDelegate::GetHelpBubbleDefaultButtonBackgroundColorId() const {
  return 0;
}
ui::ColorId
TestHelpBubbleDelegate::GetHelpBubbleDefaultButtonForegroundColorId() const {
  return 0;
}
ui::ColorId TestHelpBubbleDelegate::GetHelpBubbleButtonBorderColorId() const {
  return 0;
}
ui::ColorId TestHelpBubbleDelegate::GetHelpBubbleCloseButtonInkDropColorId()
    const {
  return 0;
}

TestThemeProvider::TestThemeProvider() = default;
TestThemeProvider::~TestThemeProvider() = default;

gfx::ImageSkia* TestThemeProvider::GetImageSkiaNamed(int id) const {
  return nullptr;
}
color_utils::HSL TestThemeProvider::GetTint(int id) const {
  return color_utils::HSL();
}
int TestThemeProvider::GetDisplayProperty(int id) const {
  return 0;
}
bool TestThemeProvider::ShouldUseNativeFrame() const {
  return false;
}
bool TestThemeProvider::HasCustomImage(int id) const {
  return false;
}
base::RefCountedMemory* TestThemeProvider::GetRawData(
    int id,
    ui::ResourceScaleFactor scale_factor) const {
  return nullptr;
}

TestThemedWidget::TestThemedWidget() = default;
TestThemedWidget::~TestThemedWidget() = default;

const ui::ThemeProvider* TestThemedWidget::GetThemeProvider() const {
  return &test_theme_provider_;
}

}  // namespace user_education::test
