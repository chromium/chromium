// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

const int kSkipAdButtonWidth = 72;
const int kSkipAdButtonHeight = 24;
const int kSkipAdButtonMarginBottom = 48;

constexpr SkColor kSkipAdButtonTextColor = kPipWindowIconColor;
constexpr SkColor kSkipAdButtonBorderColor = kPipWindowIconColor;
constexpr SkColor kSkipAdButtonBackgroundColor = gfx::kGoogleGrey700;

}  // namespace

namespace views {

SkipAdLabelButton::SkipAdLabelButton(PressedCallback callback)
    : LabelButton(std::move(callback),
                  l10n_util::GetStringUTF16(
                      IDS_PICTURE_IN_PICTURE_SKIP_AD_CONTROL_TEXT)) {
  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateRoundRectWith1PxBorderPainter(
          kSkipAdButtonBackgroundColor, kSkipAdButtonBorderColor, 1.f)));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetEnabledTextColors(kSkipAdButtonTextColor);
  SetTextColor(views::Button::STATE_DISABLED, kSkipAdButtonTextColor);
  SetSize(gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight));

  // Accessibility.
  SetAccessibleName(label()->GetText());
  SetTooltipText(label()->GetText());
  SetInstallFocusRingOnFocus(true);
}

void SkipAdLabelButton::SetPosition(const gfx::Size& size) {
  LabelButton::SetPosition(gfx::Point(
      size.width() - kSkipAdButtonWidth + 1 /* border offset */,
      size.height() - kSkipAdButtonHeight - kSkipAdButtonMarginBottom));
}

void SkipAdLabelButton::SetVisible(bool visible) {
  // We need to do more than the usual visibility change because otherwise the
  // overlay window cannot be dragged when grabbing within the label area.
  LabelButton::SetVisible(visible);
  SetSize(visible ? gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight)
                  : gfx::Size());
}

BEGIN_METADATA(SkipAdLabelButton, views::LabelButton)
END_METADATA

}  // namespace views
