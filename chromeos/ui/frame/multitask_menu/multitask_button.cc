// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_button.h"

#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace chromeos {

namespace {

// Round rect pattern indicate the Full/Float window bounds.
constexpr gfx::Rect kFloatPatternLandscapeBounds(72, 24, 32, 44);
constexpr gfx::Rect kFloatPatternPortraitBounds(36, 60, 32, 44);
constexpr gfx::Rect kFullPatternLandscapeBounds(4, 4, 100, 64);
constexpr gfx::Rect kFullPatternPortraitBounds(4, 4, 64, 100);

}  // namespace

MultitaskButton::MultitaskButton(PressedCallback callback,
                                 Type type,
                                 bool is_portrait_mode,
                                 bool paint_as_active,
                                 const std::u16string& name)
    : views::Button(std::move(callback)),
      type_(type),
      is_portrait_mode_(is_portrait_mode),
      paint_as_active_(paint_as_active) {
  SetPreferredSize(is_portrait_mode_ ? kMultitaskButtonPortraitSize
                                     : kMultitaskButtonLandscapeSize);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(kMultitaskButtonDefaultColor);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kMultitaskBaseButtonBorderRadius);
  SetTooltipText(name);
}

void MultitaskButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setAntiAlias(true);
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);

  cc::PaintFlags border_flags;
  border_flags.setAntiAlias(true);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);

  cc::PaintFlags pattern_flags;
  pattern_flags.setAntiAlias(true);
  pattern_flags.setStyle(cc::PaintFlags::kFill_Style);

  if (paint_as_active_ || GetState() == Button::STATE_HOVERED ||
      GetState() == Button::STATE_PRESSED) {
    fill_flags.setColor(kMultitaskButtonViewHoverColor);
    border_flags.setColor(kMultitaskButtonPrimaryHoverColor);
    pattern_flags.setColor(gfx::kGoogleBlue600);
  } else if (GetState() == Button::STATE_DISABLED) {
    fill_flags.setColor(kMultitaskButtonViewHoverColor);
    border_flags.setColor(kMultitaskButtonDisabledColor);
    pattern_flags.setColor(kMultitaskButtonDisabledColor);
  } else {
    fill_flags.setColor(SK_ColorTRANSPARENT);
    border_flags.setColor(kMultitaskButtonDefaultColor);
    pattern_flags.setColor(kMultitaskButtonDefaultColor);
  }

  canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()),
                        kMultitaskBaseButtonBorderRadius, fill_flags);

  // Draw a border on the background circle. Inset by half the stroke width,
  // otherwise half of the stroke will be out of bounds.
  gfx::RectF border_bounds(GetLocalBounds());
  border_bounds.Inset(kButtonBorderSize / 2.f);
  border_flags.setStrokeWidth(kButtonBorderSize);
  canvas->DrawRoundRect(border_bounds, kMultitaskBaseButtonBorderRadius,
                        border_flags);

  gfx::Rect bounds;
  if (is_portrait_mode_) {
    bounds = type_ == Type::kFloat ? kFloatPatternPortraitBounds
                                   : kFullPatternPortraitBounds;
  } else {
    bounds = type_ == Type::kFloat ? kFloatPatternLandscapeBounds
                                   : kFullPatternLandscapeBounds;
  }

  canvas->DrawRoundRect(gfx::RectF(bounds), kButtonCornerRadius, pattern_flags);
}

void MultitaskButton::OnThemeChanged() {
  // TODO(shidi): Implement the theme change after dark/light mode integration.
  views::Button::OnThemeChanged();
}

BEGIN_METADATA(MultitaskButton, views::Button)
END_METADATA

}  // namespace chromeos
