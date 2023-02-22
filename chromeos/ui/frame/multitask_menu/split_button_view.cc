// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/split_button_view.h"

#include <memory>

#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
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

// Change to secondary hover color when the other button on the same
// `SplitButtonView` is hovered.
constexpr SkColor kSplitButtonSecondaryHoverColor =
    SkColorSetA(gfx::kGoogleBlue600, SK_AlphaOPAQUE * 0.4);

bool IsHoveredOrPressedState(views::Button::ButtonState button_state) {
  return button_state == views::Button::STATE_PRESSED ||
         button_state == views::Button::STATE_HOVERED;
}

// Gets the string for the direction (top/bottom/left/right) of the split
// button. Used in various tooltips or a11y names.
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

std::u16string GetTooltipName(SplitButtonView::SplitButtonType type,
                              bool left_top,
                              bool portrait_mode) {
  int template_id;
  switch (type) {
    case SplitButtonView::SplitButtonType::kHalfButtons:
      template_id = IDS_MULTITASK_MENU_HALF_BUTTON_TOOLTIP_NAME;
      break;
    case SplitButtonView::SplitButtonType::kPartialButtons:
      // Left/top button is always the larger one.
      template_id = left_top
                        ? IDS_MULTITASK_MENU_PARTIAL_BUTTON_LARGE_TOOLTIP_NAME
                        : IDS_MULTITASK_MENU_PARTIAL_BUTTON_SMALL_TOOLTIP_NAME;
      return l10n_util::GetStringUTF16(template_id);
  }
  return l10n_util::GetStringFUTF16(
      template_id, GetDirectionString(left_top, portrait_mode));
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
 public:
  SplitButton(views::Button::PressedCallback pressed_callback,
              base::RepeatingClosure hovered_pressed_callback,
              const std::u16string& tooltip_name,
              const std::u16string& a11y_name,
              const gfx::Insets& insets)
      : views::Button(std::move(pressed_callback)),
        button_color_(kMultitaskButtonDefaultColor),
        insets_(insets),
        hovered_pressed_callback_(std::move(hovered_pressed_callback)) {
    // Subtract by the preferred insets so that the focus ring is drawn around
    // the painted region below. Also, use the parent's rounded radius so the
    // ring matches the parent border.
    views::InstallRoundRectHighlightPathGenerator(
        this, insets - kPreferredInsets, kMultitaskBaseButtonBorderRadius);
    SetTooltipText(tooltip_name);
    SetAccessibleName(a11y_name);
  }

  SplitButton(const SplitButton&) = delete;
  SplitButton& operator=(const SplitButton&) = delete;
  ~SplitButton() override = default;

  void set_button_color(SkColor color) { button_color_ = color; }

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags pattern_flags;
    pattern_flags.setAntiAlias(true);
    pattern_flags.setColor(GetEnabled() ? button_color_
                                        : kMultitaskButtonDisabledColor);
    pattern_flags.setStyle(cc::PaintFlags::kFill_Style);
    gfx::Rect pattern_bounds = GetLocalBounds();
    pattern_bounds.Inset(insets_);
    canvas->DrawRoundRect(pattern_bounds, kButtonCornerRadius, pattern_flags);
  }

  void StateChanged(ButtonState old_state) override {
    if (IsHoveredOrPressedState(old_state) ||
        IsHoveredOrPressedState(GetState())) {
      hovered_pressed_callback_.Run();
    }
  }

 private:
  SkColor button_color_;
  // The inset between the button window pattern and the border.
  gfx::Insets insets_;
  // Callback to `SplitButtonView` to change button color. When one split button
  // is hovered or pressed, both split buttons on `SplitButtonView` change
  // color.
  base::RepeatingClosure hovered_pressed_callback_;
};

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
  SetPreferredSize(is_portrait_mode ? kMultitaskButtonPortraitSize
                                    : kMultitaskButtonLandscapeSize);

  auto on_hover_pressed = base::BindRepeating(
      &SplitButtonView::OnButtonHoveredOrPressed, base::Unretained(this));

  const SnapDirection left_top_direction =
      GetSnapDirectionForWindow(window, /*left_top=*/true);
  const SnapDirection right_bottom_direction =
      GetSnapDirectionForWindow(window, /*left_top=*/false);

  // Modify `split_button_callback` to pass a direction as well.
  auto on_left_top_press =
      base::BindRepeating(split_button_callback, left_top_direction);
  auto on_right_bottom_press =
      base::BindRepeating(split_button_callback, right_bottom_direction);

  left_top_button_ = AddChildView(std::make_unique<SplitButton>(
      on_left_top_press, on_hover_pressed,
      GetTooltipName(type, /*left_top=*/true, is_portrait_mode),
      GetA11yName(type, /*left_top=*/true, is_portrait_mode),
      is_portrait_mode ? kTopButtonInsets : kLeftButtonInsets));
  right_bottom_button_ = AddChildView(std::make_unique<SplitButton>(
      on_right_bottom_press, on_hover_pressed,
      GetTooltipName(type, /*left_top=*/false, is_portrait_mode),
      GetA11yName(type, /*left_top=*/false, is_portrait_mode),
      is_portrait_mode ? kBottomButtonInsets : kRightButtonInsets));

  const int left_top_width = type_ == SplitButtonType::kHalfButtons
                                 ? kMultitaskHalfButtonWidth
                                 : kMultitaskTwoThirdButtonWidth;
  const int right_bottom_width = type_ == SplitButtonType::kHalfButtons
                                     ? kMultitaskHalfButtonWidth
                                     : kMultitaskOneThirdButtonWidth;

  left_top_button_->SetPreferredSize(
      is_portrait_mode ? gfx::Size(kMultitaskHalfButtonHeight, left_top_width)
                       : gfx::Size(left_top_width, kMultitaskHalfButtonHeight));
  right_bottom_button_->SetPreferredSize(
      is_portrait_mode
          ? gfx::Size(kMultitaskHalfButtonHeight, right_bottom_width)
          : gfx::Size(right_bottom_width, kMultitaskHalfButtonHeight));
}

views::Button* SplitButtonView::GetRightBottomButton() {
  return static_cast<views::Button*>(right_bottom_button_);
}

void SplitButtonView::OnButtonHoveredOrPressed() {
  border_color_ = kMultitaskButtonPrimaryHoverColor;
  fill_color_ = kMultitaskButtonViewHoverColor;
  if (IsHoveredOrPressedState(right_bottom_button_->GetState())) {
    right_bottom_button_->set_button_color(kMultitaskButtonPrimaryHoverColor);
    left_top_button_->set_button_color(kSplitButtonSecondaryHoverColor);
  } else if (IsHoveredOrPressedState(left_top_button_->GetState())) {
    left_top_button_->set_button_color(kMultitaskButtonPrimaryHoverColor);
    right_bottom_button_->set_button_color(kSplitButtonSecondaryHoverColor);
  } else {
    // Reset color.
    border_color_ = kMultitaskButtonDefaultColor;
    fill_color_ = SK_ColorTRANSPARENT;
    right_bottom_button_->set_button_color(kMultitaskButtonDefaultColor);
    left_top_button_->set_button_color(kMultitaskButtonDefaultColor);
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
  // TODO(shidi): Implement the theme change after dark/light mode integration.
  views::View::OnThemeChanged();
}

BEGIN_METADATA(SplitButtonView, View)
END_METADATA

}  // namespace chromeos
