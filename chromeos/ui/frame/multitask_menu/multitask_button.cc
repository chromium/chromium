// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_button.h"

#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kMultitaskBaseButtonBorderRadius);
  GetViewAccessibility().SetName(name);
}

void MultitaskButton::StateChanged(views::Button::ButtonState old_state) {
  if (GetState() == views::Button::STATE_HOVERED) {
    haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kSnap,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
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
  if (paint_as_active_ || GetState() == Button::STATE_HOVERED ||
      GetState() == Button::STATE_PRESSED) {
    const SkColor primary_color =
        color_provider->GetColor(ui::kColorSysPrimary);
    fill_flags.setColor(
        SkColorSetA(primary_color, kMultitaskHoverBackgroundOpacity));
    border_flags.setColor(primary_color);
    pattern_flags.setColor(primary_color);
  } else if (GetState() == Button::STATE_DISABLED) {
    fill_flags.setColor(SK_ColorTRANSPARENT);
    const SkColor disabled_color =
        SkColorSetA(color_provider->GetColor(ui::kColorSysOnSurface),
                    kMultitaskDisabledButtonOpacity);
    border_flags.setColor(disabled_color);
    pattern_flags.setColor(disabled_color);
  } else {
    fill_flags.setColor(SK_ColorTRANSPARENT);
    const auto default_color =
        SkColorSetA(color_provider->GetColor(ui::kColorSysOnSurface),
                    kMultitaskDefaultButtonOpacity);
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

BEGIN_METADATA(MultitaskButton)
END_METADATA

}  // namespace chromeos
