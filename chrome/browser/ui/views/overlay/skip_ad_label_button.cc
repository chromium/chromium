// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"

namespace {

const int kSkipAdButtonWidth = 72;
const int kSkipAdButtonHeight = 24;
const int kSkipAdButtonMarginBottom = 48;

}  // namespace

SkipAdLabelButton::SkipAdLabelButton(PressedCallback callback)
    : views::LabelButton(std::move(callback),
                         l10n_util::GetStringUTF16(
                             IDS_PICTURE_IN_PICTURE_SKIP_AD_CONTROL_TEXT)) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetSize(gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight));

  // Accessibility.
  GetViewAccessibility().SetName(label()->GetText());
  SetTooltipText(label()->GetText());
  SetInstallFocusRingOnFocus(true);
}

void SkipAdLabelButton::SetPosition(const gfx::Size& size) {
  views::LabelButton::SetPosition(gfx::Point(
      size.width() - kSkipAdButtonWidth + 1 /* border offset */,
      size.height() - kSkipAdButtonHeight - kSkipAdButtonMarginBottom));
}

void SkipAdLabelButton::SetVisible(bool visible) {
  // We need to do more than the usual visibility change because otherwise the
  // overlay window cannot be dragged when grabbing within the label area.
  views::LabelButton::SetVisible(visible);
  SetSize(visible ? gfx::Size(kSkipAdButtonWidth, kSkipAdButtonHeight)
                  : gfx::Size());
}

void SkipAdLabelButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  SetBackground(CreateBackgroundFromPainter(
      views::Painter::CreateRoundRectWith1PxBorderPainter(
          color_provider->GetColor(kColorPipWindowSkipAdButtonBackground),
          color_provider->GetColor(kColorPipWindowSkipAdButtonBorder), 1.f)));
  SetEnabledTextColorIds(kColorPipWindowForeground);
  SetTextColorId(views::Button::STATE_DISABLED, kColorPipWindowForeground);
}

BEGIN_METADATA(SkipAdLabelButton)
END_METADATA
