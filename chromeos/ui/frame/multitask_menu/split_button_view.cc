// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/split_button_view.h"

#include <memory>

#include "chromeos/ui/frame/frame_utils.h"
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

// TODO(shidi): Button name needs to be internationalized.
const std::u16string kLeftButtonName = u"Split Left";
const std::u16string kTopButtonName = u"Split Top";
const std::u16string kRightButtonName = u"Split Right";
const std::u16string kBottomButtonName = u"Split Bottom";

// Change to secondary hover color when the other button on the same
// `SplitButtonView` is hovered.
constexpr SkColor kSplitButtonSecondaryHoverColor =
    SkColorSetA(gfx::kGoogleBlue600, SK_AlphaOPAQUE * 0.4);

bool IsHoveredOrPressedState(views::Button::ButtonState button_state) {
  return button_state == views::Button::STATE_PRESSED ||
         button_state == views::Button::STATE_HOVERED;
}

}  // namespace

// -----------------------------------------------------------------------------
// SplitButton:
// A button used for SplitButtonView to trigger snapping.
class SplitButtonView::SplitButton : public views::Button {
 public:
  SplitButton(views::Button::PressedCallback pressed_callback,
              base::RepeatingClosure hovered_pressed_callback,
              const std::u16string& name,
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
    SetTooltipText(name);
  }

  SplitButton(const SplitButton&) = delete;
  SplitButton& operator=(const SplitButton&) = delete;
  ~SplitButton() override {}

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
      is_portrait_mode ? kTopButtonName : kLeftButtonName,
      is_portrait_mode ? kTopButtonInsets : kLeftButtonInsets));
  right_bottom_button_ = AddChildView(std::make_unique<SplitButton>(
      on_right_bottom_press, on_hover_pressed,
      is_portrait_mode ? kBottomButtonName : kRightButtonName,
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
