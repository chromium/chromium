// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_H_

#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"

#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace chromeos {

// A button used for SplitButtonView to trigger primary/secondary split.
class SplitButton : public views::Button {
 public:
  enum class SplitButtonType {
    kHalfButtons,
    kPartialButtons,
  };

  SplitButton(views::Button::PressedCallback pressed_callback,
              base::RepeatingClosure hovered_callback,
              const std::u16string& name,
              const gfx::Insets& insets);

  SplitButton(const SplitButton&) = delete;
  SplitButton& operator=(const SplitButton&) = delete;
  ~SplitButton() override;

  void set_button_color(SkColor color) { button_color_ = color; };

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void StateChanged(ButtonState old_state) override;

 private:
  SkColor button_color_;
  // The insert between the button window pattern and the border.
  gfx::Insets insets_;
  // Callback to `SplitButtonView` to change button color.
  // When one split button is hovered, both split buttons on SplitButtonView
  // changed color.
  base::RepeatingClosure hovered_callback_;
};

// A button view with 2 divided buttons, primary and secondary.
class SplitButtonView : public views::BoxLayoutView {
 public:
  SplitButtonView(SplitButton::SplitButtonType type,
                  views::Button::PressedCallback primary_callback,
                  views::Button::PressedCallback secondary_callback);
  SplitButtonView(const SplitButtonView&) = delete;
  SplitButtonView& operator=(const SplitButtonView&) = delete;

  ~SplitButtonView() override = default;

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Called when either primary/secondary button is hovered,
  // Update button colors.
  void OnButtonHovered();

  SplitButton* primary_button_;
  SplitButton* secondary_button_;
  SplitButton::SplitButtonType type_;
  SkColor border_color_ = kMultitaskButtonDefaultColor;
  SkColor fill_color_ = SK_ColorTRANSPARENT;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_H_
