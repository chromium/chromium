// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/split_button.h"

#include <memory>

#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"

namespace chromeos {

namespace {

constexpr int kMultitaskHalfButtonWidth = 54;
constexpr int kMultitaskHalfButtonHeight = 72;
constexpr int kMultitaskOneThirdButtonWidth = 38;
constexpr int kMultitaskTwoThirdButtonWidth = 70;

constexpr gfx::Insets kPrimaryInsets = gfx::Insets::TLBR(4, 4, 4, 2);
constexpr gfx::Insets kSecondaryInsets = gfx::Insets::TLBR(4, 2, 4, 4);

// TODO(shidi): Button name needs to be internationalized.
const std::u16string kPrimaryButtonName = u"Split Primary";
const std::u16string kSecondaryButtonName = u"Split Secondary";

// Change to secondary hover color when the other button on the same
// `SplitButtonView` is hovered.
constexpr SkColor kSplitButtonSecondaryHoverColor =
    SkColorSetA(gfx::kGoogleBlue600, SK_AlphaOPAQUE * 0.4);

}  // namespace

SplitButton::SplitButton(views::Button::PressedCallback pressed_callback,
                         base::RepeatingClosure hovered_callback,
                         const std::u16string& name,
                         const gfx::Insets& insets)
    : views::Button(std::move(pressed_callback)),
      button_color_(kMultitaskButtonDefaultColor),
      insets_(insets),
      hovered_callback_(std::move(hovered_callback)) {
  SetAccessibleName(name);
}

SplitButton::~SplitButton() = default;

void SplitButton::StateChanged(ButtonState old_state) {
  if (old_state == STATE_HOVERED || GetState() == STATE_HOVERED)
    hovered_callback_.Run();
}

void SplitButton::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags pattern_flags;
  pattern_flags.setAntiAlias(true);
  pattern_flags.setColor(button_color_);
  pattern_flags.setStyle(cc::PaintFlags::kFill_Style);
  gfx::Rect pattern_bounds = GetLocalBounds();
  pattern_bounds.Inset(insets_);
  canvas->DrawRoundRect(pattern_bounds, kButtonCornerRadius, pattern_flags);
}

SplitButtonView::SplitButtonView(
    SplitButton::SplitButtonType type,
    views::Button::PressedCallback primary_callback,
    views::Button::PressedCallback secondary_callback)
    : type_(type) {
  SetPreferredSize(kMultitaskButtonSize);

  auto primary_hover_callback = base::BindRepeating(
      &SplitButtonView::OnButtonHovered, base::Unretained(this));
  auto secondary_hover_callback = base::BindRepeating(
      &SplitButtonView::OnButtonHovered, base::Unretained(this));
  primary_button_ = AddChildView(
      std::make_unique<SplitButton>(primary_callback, primary_hover_callback,
                                    kPrimaryButtonName, kPrimaryInsets));
  secondary_button_ = AddChildView(std::make_unique<SplitButton>(
      secondary_callback, secondary_hover_callback, kSecondaryButtonName,
      kSecondaryInsets));

  const int primary_width = type_ == SplitButton::SplitButtonType::kHalfButtons
                                ? kMultitaskHalfButtonWidth
                                : kMultitaskTwoThirdButtonWidth;
  const int secondary_width =
      type_ == SplitButton::SplitButtonType::kHalfButtons
          ? kMultitaskHalfButtonWidth
          : kMultitaskOneThirdButtonWidth;

  primary_button_->SetPreferredSize(
      gfx::Size(primary_width, kMultitaskHalfButtonHeight));
  secondary_button_->SetPreferredSize(
      gfx::Size(secondary_width, kMultitaskHalfButtonHeight));
}

void SplitButtonView::OnButtonHovered() {
  border_color_ = kMultitaskButtonPrimaryHoverColor;
  fill_color_ = kMultitaskButtonViewHoverColor;
  if (secondary_button_->GetState() == views::Button::STATE_HOVERED) {
    secondary_button_->set_button_color(kMultitaskButtonPrimaryHoverColor);
    primary_button_->set_button_color(kSplitButtonSecondaryHoverColor);
  } else if (primary_button_->GetState() == views::Button::STATE_HOVERED) {
    primary_button_->set_button_color(kMultitaskButtonPrimaryHoverColor);
    secondary_button_->set_button_color(kSplitButtonSecondaryHoverColor);
  } else {
    // Reset color.
    border_color_ = kMultitaskButtonDefaultColor;
    fill_color_ = SK_ColorTRANSPARENT;
    secondary_button_->set_button_color(kMultitaskButtonDefaultColor);
    primary_button_->set_button_color(kMultitaskButtonDefaultColor);
  }
  primary_button_->SchedulePaint();
  secondary_button_->SchedulePaint();
  SchedulePaint();
}

void SplitButtonView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetContentsBounds();

  cc::PaintFlags border_flags;
  border_flags.setAntiAlias(true);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setColor(border_color_);
  border_flags.setStrokeWidth(kButtonBorderSize);
  canvas->DrawRoundRect(bounds, kMultitaskBaseButtonBorderRadius, border_flags);

  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(fill_color_);
  canvas->DrawRoundRect(bounds, kMultitaskBaseButtonBorderRadius, fill_flags);
}

void SplitButtonView::OnThemeChanged() {
  // TODO(shidi): Implement the theme change after dark/light mode integration.
  views::View::OnThemeChanged();
}

}  // namespace chromeos
