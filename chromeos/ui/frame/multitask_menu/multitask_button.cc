// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_button.h"

#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace chromeos {

namespace {

// Round rect pattern indicate the Full/Float window bounds.
// TODO(shidi): Update when implement portrait mode.
constexpr gfx::Rect kFloatPatternBounds(72, 24, 32, 44);
constexpr gfx::Rect kFullPatternBounds(4, 4, 100, 64);

}  // namespace

MultitaskBaseButton::MultitaskBaseButton(PressedCallback callback,
                                         Type type,
                                         const std::u16string& name)
    : views::Button(std::move(callback)), type_(type) {
  SetPreferredSize(kMultitaskButtonSize);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(kMultitaskButtonDefaultColor);
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(kMultitaskButtonDefaultColor);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kMultitaskBaseButtonBorderRadius);
  SetAccessibleName(name);
}

void MultitaskBaseButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags border_flags;
  SkColor fill_color;

  border_flags.setAntiAlias(true);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);

  cc::PaintFlags pattern_flags;
  pattern_flags.setAntiAlias(true);
  pattern_flags.setStyle(cc::PaintFlags::kFill_Style);

  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);

  if (GetState() == Button::STATE_HOVERED) {
    border_flags.setColor(kMultitaskButtonPrimaryHoverColor);
    pattern_flags.setColor(gfx::kGoogleBlue600);
    fill_color = kMultitaskButtonViewHoverColor;
  } else if (GetState() == Button::STATE_DISABLED) {
    border_flags.setColor(kMultitaskButtonDisabledColor);
    pattern_flags.setColor(kMultitaskButtonDisabledColor);
    fill_color = kMultitaskButtonViewHoverColor;
  } else {
    border_flags.setColor(kMultitaskButtonDefaultColor);
    pattern_flags.setColor(kMultitaskButtonDefaultColor);
    fill_color = SK_ColorTRANSPARENT;
  }

  // Draw a border on the background circle.
  border_flags.setStrokeWidth(kButtonBorderSize);
  canvas->DrawRoundRect(GetLocalBounds(), kMultitaskBaseButtonBorderRadius,
                        border_flags);

  fill_flags.setColor(fill_color);
  canvas->DrawRoundRect(GetLocalBounds(), kMultitaskBaseButtonBorderRadius,
                        fill_flags);

  gfx::Rect bounds =
      type_ == Type::kFloat ? kFloatPatternBounds : kFullPatternBounds;
  canvas->DrawRoundRect(bounds, kButtonCornerRadius, pattern_flags);
}

void MultitaskBaseButton::OnThemeChanged() {
  // TODO(shidi): Implement the theme change after dark/light mode integration.
  views::Button::OnThemeChanged();
}

}  // namespace chromeos
