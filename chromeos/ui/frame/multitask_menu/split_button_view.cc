// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/split_button_view.h"

#include <memory>

#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace chromeos {

namespace {

constexpr int kMultitaskHalfButtonWidth = 54;
constexpr int kMultitaskHalfButtonHeight = 72;
constexpr int kMultitaskOneThirdButtonWidth = 38;
constexpr int kMultitaskTwoThirdButtonWidth = 70;

// The preferred insets would be 4 on each side.
constexpr gfx::Insets kPreferredInsets(4);

// The two buttons share an edge so the inset on both sides needs to be halved
// so that visually we get the preferred insets above.
constexpr gfx::Insets kLeftButtonInsets = gfx::Insets::TLBR(4, 4, 4, 2);
constexpr gfx::Insets kTopButtonInsets = gfx::Insets::TLBR(4, 4, 2, 4);
constexpr gfx::Insets kRightButtonInsets = gfx::Insets::TLBR(4, 2, 4, 4);
constexpr gfx::Insets kBottomButtonInsets = gfx::Insets::TLBR(2, 4, 4, 4);

bool IsHoveredOrPressedState(views::Button::ButtonState button_state) {
  return button_state == views::Button::STATE_PRESSED ||
         button_state == views::Button::STATE_HOVERED;
}

// Gets the string for the direction (top/bottom/left/right) of the split
// button. Used in various for a11y names.
std::u16string GetDirectionString(bool left_top, bool portrait_mode) {
  int message_id;
  if (left_top) {
    message_id =
        portrait_mode ? IDS_MULTITASK_MENU_TOP : IDS_MULTITASK_MENU_LEFT;
  } else {
    // Right or bottom.
    message_id =
        portrait_mode ? IDS_MULTITASK_MENU_BOTTOM : IDS_MULTITASK_MENU_RIGHT;
  }
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string GetA11yName(SplitButtonView::SplitButtonType type,
                           bool left_top,
                           bool portrait_mode) {
  int template_id;
  switch (type) {
    case SplitButtonView::SplitButtonType::kHalfButtons:
      template_id = IDS_MULTITASK_MENU_HALF_BUTTON_ACCESSIBLE_NAME;
      break;
    case SplitButtonView::SplitButtonType::kPartialButtons:
      // Left/top button is always the larger one.
      template_id =
          left_top ? IDS_MULTITASK_MENU_PARTIAL_BUTTON_LARGE_ACCESSIBLE_NAME
                   : IDS_MULTITASK_MENU_PARTIAL_BUTTON_SMALL_ACCESSIBLE_NAME;
      break;
  }
  return l10n_util::GetStringFUTF16(
      template_id, GetDirectionString(left_top, portrait_mode));
}

}  // namespace

// -----------------------------------------------------------------------------
// SplitButton:
// A button used for SplitButtonView to trigger snapping.
class SplitButtonView::SplitButton : public views::Button {
  METADATA_HEADER(SplitButton, views::Button)

 public:
  SplitButton(views::Button::PressedCallback pressed_callback,
              base::RepeatingClosure hovered_pressed_callback,
              const gfx::Insets& insets)
      : views::Button(std::move(pressed_callback)),
        insets_(insets),
        hovered_pressed_callback_(std::move(hovered_pressed_callback)) {
    // Subtract by the preferred insets so that the focus ring is drawn around
    // the painted region below. Also, use the parent's rounded radius so the
    // ring matches the parent border.
    views::InstallRoundRectHighlightPathGenerator(
        this, insets - kPreferredInsets, kMultitaskBaseButtonBorderRadius);
  }

  SplitButton(const SplitButton&) = delete;
  SplitButton& operator=(const SplitButton&) = delete;
  ~SplitButton() override = default;

  void set_button_color(SkColor color) { button_color_ = color; }

  // views::Button:
  void StateChanged(views::Button::ButtonState old_state) override {
    if (GetState() == views::Button::STATE_HOVERED) {
      haptics_util::PlayHapticTouchpadEffect(
          ui::HapticTouchpadEffect::kSnap,
          ui::HapticTouchpadEffectStrength::kMedium);
    }

    if (IsHoveredOrPressedState(old_state) ||
        IsHoveredOrPressedState(GetState())) {
      hovered_pressed_callback_.Run();
    }
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags pattern_flags;
    pattern_flags.setAntiAlias(true);
    pattern_flags.setColor(
        GetEnabled()
            ? button_color_
            : SkColorSetA(GetColorProvider()->GetColor(ui::kColorSysOnSurface),
                          kMultitaskDisabledButtonOpacity));
    pattern_flags.setStyle(cc::PaintFlags::kFill_Style);
    gfx::Rect pattern_bounds = GetLocalBounds();
    pattern_bounds.Inset(insets_);
    canvas->DrawRoundRect(pattern_bounds, kButtonCornerRadius, pattern_flags);
  }

 private:
  SkColor button_color_ = SK_ColorTRANSPARENT;
  // The inset between the button window pattern and the border.
  gfx::Insets insets_;
  // Callback to `SplitButtonView` to change button color. When one split button
  // is hovered or pressed, both split buttons on `SplitButtonView` change
  // color.
  base::RepeatingClosure hovered_pressed_callback_;
};

BEGIN_METADATA(SplitButtonView, SplitButton)
END_METADATA

// -----------------------------------------------------------------------------
// SplitButtonView:

SplitButtonView::SplitButtonView(SplitButtonType type,
                                 SplitButtonCallback split_button_callback,
                                 aura::Window* window,
                                 bool is_portrait_mode)
    : type_(type) {
  // Left button should stay on the left side for RTL languages.
  SetMirrored(false);
  SetOrientation(is_portrait_mode ? views::BoxLayout::Orientation::kVertical
                                  : views::BoxLayout::Orientation::kHorizontal);

  auto on_hover_pressed = base::BindRepeating(
      &SplitButtonView::OnButtonHoveredOrPressed, base::Unretained(this));

  const SnapDirection left_top_direction =
      GetSnapDirectionForWindow(window, /*left_top=*/true);
  const SnapDirection right_bottom_direction =
      GetSnapDirectionForWindow(window, /*left_top=*/false);

  auto on_left_top_press =
      base::BindRepeating(split_button_callback, left_top_direction);
  auto on_right_bottom_press =
      base::BindRepeating(split_button_callback, right_bottom_direction);

  left_top_button_ = AddChildView(std::make_unique<SplitButton>(
      on_left_top_press, on_hover_pressed,
      is_portrait_mode ? kTopButtonInsets : kLeftButtonInsets));
  right_bottom_button_ = AddChildView(std::make_unique<SplitButton>(
      on_right_bottom_press, on_hover_pressed,
      is_portrait_mode ? kBottomButtonInsets : kRightButtonInsets));

  UpdateButtons(is_portrait_mode, /*is_reversed=*/false);
}

void SplitButtonView::UpdateButtons(bool is_portrait_mode, bool is_reversed) {
  const int left_top_width =
      type_ == SplitButtonType::kHalfButtons
          ? kMultitaskHalfButtonWidth
          : (is_reversed ? kMultitaskOneThirdButtonWidth
                         : kMultitaskTwoThirdButtonWidth);
  const int right_bottom_width =
      type_ == SplitButtonType::kHalfButtons
          ? kMultitaskHalfButtonWidth
          : (is_reversed ? kMultitaskTwoThirdButtonWidth
                         : kMultitaskOneThirdButtonWidth);

  left_top_button_->SetPreferredSize(
      is_portrait_mode ? gfx::Size(kMultitaskHalfButtonHeight, left_top_width)
                       : gfx::Size(left_top_width, kMultitaskHalfButtonHeight));
  right_bottom_button_->SetPreferredSize(
      is_portrait_mode
          ? gfx::Size(kMultitaskHalfButtonHeight, right_bottom_width)
          : gfx::Size(right_bottom_width, kMultitaskHalfButtonHeight));

  left_top_button_->GetViewAccessibility().SetName(
      GetA11yName(type_, /*left_top=*/!is_reversed, is_portrait_mode));
  right_bottom_button_->GetViewAccessibility().SetName(
      GetA11yName(type_, /*left_top=*/is_reversed, is_portrait_mode));
}

views::Button* SplitButtonView::GetLeftTopButton() {
  return left_top_button_;
}

views::Button* SplitButtonView::GetRightBottomButton() {
  return right_bottom_button_;
}

void SplitButtonView::OnButtonHoveredOrPressed() {
  const auto* color_provider = GetColorProvider();
  const SkColor primary_hover_color =
      color_provider->GetColor(ui::kColorSysPrimary);
  border_color_ = primary_hover_color;
  const SkColor secondary_hover_color =
      SkColorSetA(primary_hover_color, kMultitaskHoverButtonOpacity);
  fill_color_ =
      SkColorSetA(primary_hover_color, kMultitaskHoverBackgroundOpacity);

  if (IsHoveredOrPressedState(right_bottom_button_->GetState())) {
    right_bottom_button_->set_button_color(primary_hover_color);
    left_top_button_->set_button_color(secondary_hover_color);
  } else if (IsHoveredOrPressedState(left_top_button_->GetState())) {
    left_top_button_->set_button_color(primary_hover_color);
    right_bottom_button_->set_button_color(secondary_hover_color);
  } else {
    // Reset color.
    fill_color_ = SK_ColorTRANSPARENT;
    border_color_ =
        SkColorSetA(color_provider->GetColor(ui::kColorSysOnSurface),
                    kMultitaskDefaultButtonOpacity);
    right_bottom_button_->set_button_color(border_color_);
    left_top_button_->set_button_color(border_color_);
  }
  SchedulePaint();
}

void SplitButtonView::OnPaint(gfx::Canvas* canvas) {
  gfx::RectF bounds(GetLocalBounds());

  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(fill_color_);
  canvas->DrawRoundRect(bounds, kMultitaskBaseButtonBorderRadius, fill_flags);

  // Inset by half the stroke width, otherwise half of the stroke will be out of
  // bounds.
  bounds.Inset(kButtonBorderSize / 2.f);

  cc::PaintFlags border_flags;
  border_flags.setAntiAlias(true);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setColor(border_color_);
  border_flags.setStrokeWidth(kButtonBorderSize);
  canvas->DrawRoundRect(bounds, kMultitaskBaseButtonBorderRadius, border_flags);
}

void SplitButtonView::OnThemeChanged() {
  views::View::OnThemeChanged();
  border_color_ =
      SkColorSetA(GetColorProvider()->GetColor(ui::kColorSysOnSurface),
                  kMultitaskDefaultButtonOpacity);
  right_bottom_button_->set_button_color(border_color_);
  left_top_button_->set_button_color(border_color_);
}

BEGIN_METADATA(SplitButtonView)
END_METADATA

}  // namespace chromeos
