// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_TEST_UTIL_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_TEST_UTIL_H_

#include "components/user_education/views/help_bubble_delegate.h"

#include <vector>

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class TrackedElement;
}

namespace user_education::test {

// Fake delegate implementation that does not depend on the browser.
class TestHelpBubbleDelegate : public HelpBubbleDelegate {
 public:
  TestHelpBubbleDelegate();
  ~TestHelpBubbleDelegate() override;

  std::vector<ui::Accelerator> GetPaneNavigationAccelerators(
      ui::TrackedElement* anchor_element) const override;

  // These methods return text contexts that will be handled by the app's
  // typography system.
  int GetTitleTextContext() const override;
  int GetBodyTextContext() const override;

  // These methods return color codes that will be handled by the app's theming
  // system.
  ui::ColorId GetHelpBubbleBackgroundColorId() const override;
  ui::ColorId GetHelpBubbleForegroundColorId() const override;
  ui::ColorId GetHelpBubbleDefaultButtonBackgroundColorId() const override;
  ui::ColorId GetHelpBubbleDefaultButtonForegroundColorId() const override;
  ui::ColorId GetHelpBubbleButtonBorderColorId() const override;
  ui::ColorId GetHelpBubbleCloseButtonInkDropColorId() const override;
};

// Fake theme provider. There's a similar TestThemeProvider in chrome/test but
// we're avoiding using chrome-specific code here.
class TestThemeProvider : public ui::ThemeProvider {
 public:
  TestThemeProvider();
  ~TestThemeProvider() override;

  gfx::ImageSkia* GetImageSkiaNamed(int id) const override;
  color_utils::HSL GetTint(int id) const override;
  int GetDisplayProperty(int id) const override;
  bool ShouldUseNativeFrame() const override;
  bool HasCustomImage(int id) const override;
  base::RefCountedMemory* GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const override;
};

// A top-level widget that reports a dummy theme provider.
class TestThemedWidget : public views::Widget {
 public:
  TestThemedWidget();
  ~TestThemedWidget() override;

  const ui::ThemeProvider* GetThemeProvider() const override;

 private:
  TestThemeProvider test_theme_provider_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_TEST_UTIL_H_
