// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_button.h"

#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace chromeos {

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
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kMultitaskBaseButtonBorderRadius);
  SetAccessibleName(name);
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

  const auto* color_provider = GetColorProvider();
  const bool is_jelly = features::IsJellyEnabled();
  if (paint_as_active_ || GetState() == Button::STATE_HOVERED ||
      GetState() == Button::STATE_PRESSED) {
    fill_flags.setColor(
        is_jelly ? SkColorSetA(color_provider->GetColor(ui::kColorSysPrimary),
                               kMultitaskHoverBackgroundOpacity)
                 : kMultitaskButtonViewHoverColor);
    const auto hovered_color = color_provider->GetColor(ui::kColorSysPrimary);
    border_flags.setColor(is_jelly ? hovered_color
                                   : kMultitaskButtonPrimaryHoverColor);
    pattern_flags.setColor(is_jelly ? hovered_color : gfx::kGoogleBlue600);
  } else if (GetState() == Button::STATE_DISABLED) {
    fill_flags.setColor(is_jelly ? SK_ColorTRANSPARENT
                                 : kMultitaskButtonViewHoverColor);
    const auto disabled_color =
        is_jelly ? SkColorSetA(color_provider->GetColor(ui::kColorSysOnSurface),
                               kMultitaskDisabledButtonOpacity)
                 : kMultitaskButtonDisabledColor;
    border_flags.setColor(disabled_color);
    pattern_flags.setColor(disabled_color);
  } else {
    fill_flags.setColor(SK_ColorTRANSPARENT);
    const auto default_color =
        is_jelly ? SkColorSetA(color_provider->GetColor(ui::kColorSysOnSurface),
                               kMultitaskDefaultButtonOpacity)
                 : kMultitaskButtonDefaultColor;
    border_flags.setColor(default_color);
    pattern_flags.setColor(default_color);
  }

  const gfx::RectF local_bounds_f(GetLocalBounds());
  canvas->DrawRoundRect(local_bounds_f, kMultitaskBaseButtonBorderRadius,
                        fill_flags);

  // Draw a border on the background circle. Inset by half the stroke width,
  // otherwise half of the stroke will be out of bounds.
  gfx::RectF border_bounds = local_bounds_f;
  border_bounds.Inset(kButtonBorderSize / 2.f);
  border_flags.setStrokeWidth(kButtonBorderSize);
  canvas->DrawRoundRect(border_bounds, kMultitaskBaseButtonBorderRadius,
                        border_flags);

  gfx::RectF pattern_bounds;
  switch (type_) {
    case Type::kFloat: {
      // Float pattern is located at the bottom left or bottom right with a
      // little padding. Default is bottom right, mirrored is bottom left.
      gfx::Rect float_pattern_bounds(GetLocalBounds().bottom_right(),
                                     kFloatPatternSize);
      float_pattern_bounds.Offset(-kFloatPatternSize.width() - kButtonPadding,
                                  -kFloatPatternSize.height() - kButtonPadding);
      float_pattern_bounds = GetMirroredRect(float_pattern_bounds);
      pattern_bounds = gfx::RectF(float_pattern_bounds);
      break;
    }
    case Type::kFull: {
      pattern_bounds = local_bounds_f;
      pattern_bounds.Inset(gfx::InsetsF(kButtonPadding));
      break;
    }
  }

  canvas->DrawRoundRect(pattern_bounds, kButtonCornerRadius, pattern_flags);
}

BEGIN_METADATA(MultitaskButton, views::Button)
END_METADATA

}  // namespace chromeos
