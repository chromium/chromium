// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace {

const int kSkipAdButtonWidth = 72;
const int kSkipAdButtonHeight = 24;
const int kSkipAdButtonMarginBottom = 48;

constexpr SkColor kSkipAdButtonTextColor = SK_ColorWHITE;
constexpr SkColor kSkipAdButtonBorderColor = SK_ColorWHITE;
constexpr SkColor kSkipAdButtonBackgroundColor = gfx::kGoogleGrey700;

}  // namespace

namespace views {

SkipAdLabelButton::SkipAdLabelButton(ButtonListener* listener)
    : LabelButton(listener,
                  l10n_util::GetStringUTF16(
                      IDS_PICTURE_IN_PICTURE_SKIP_AD_CONTROL_TEXT)) {
  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateRoundRectWith1PxBorderPainter(
          kSkipAdButtonBackgroundColor, kSkipAdButtonBorderColor, 1.f)));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetEnabledTextColors(kSkipAdButtonTextColor);
  SetSize(gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight));

  // Accessibility.
  SetFocusForPlatform();
  SetAccessibleName(label()->GetText());
  SetTooltipText(label()->GetText());
  SetInstallFocusRingOnFocus(true);
}

void SkipAdLabelButton::SetPosition(const gfx::Size& size) {
  LabelButton::SetPosition(gfx::Point(
      size.width() - kSkipAdButtonWidth + 1 /* border offset */,
      size.height() - kSkipAdButtonHeight - kSkipAdButtonMarginBottom));
}

void SkipAdLabelButton::ToggleVisibility(bool is_visible) {
  layer()->SetVisible(is_visible);
  SetEnabled(is_visible);
  SetSize(is_visible ? gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight)
                     : gfx::Size());
}

}  // namespace views
