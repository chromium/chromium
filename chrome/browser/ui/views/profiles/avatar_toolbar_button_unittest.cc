// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/widget/widget.h"

using AvatarToolbarButtonTest = TestWithBrowserView;

// CrOS only shows the avatar button for incognito/guest.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(AvatarToolbarButtonTest, HighlightMeetsMinimumContrast) {
  auto* button = browser_view()->GetWidget()->GetContentsView()->AddChildView(
      std::make_unique<AvatarToolbarButton>(browser_view()));

  SkColor toolbar_color =
      ToolbarButton::GetDefaultBackgroundColor(button->GetThemeProvider());
  SkColor highlight_color = SkColorSetRGB(0xFE, 0x00, 0x00);

  DCHECK_LT(color_utils::GetContrastRatio(highlight_color, toolbar_color),
            color_utils::kMinimumReadableContrastRatio);

  SkColor result = ToolbarButton::AdjustHighlightColorForContrast(
      button->GetThemeProvider(), highlight_color,
      SkColorSetRGB(0xFF, 0x00, 0x00), SK_ColorBLACK, SK_ColorWHITE);
  EXPECT_GT(color_utils::GetContrastRatio(result, toolbar_color),
            color_utils::kMinimumReadableContrastRatio);

  button->parent()->RemoveChildViewT(button);
}

#endif
