// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_

#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace chromeos {

// A button view with 2 divided buttons, primary and secondary.
class SplitButtonView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(SplitButtonView);

  enum class SplitButtonType {
    kHalfButtons,
    kPartialButtons,
  };

  SplitButtonView(SplitButtonType type,
                  views::Button::PressedCallback primary_callback,
                  views::Button::PressedCallback secondary_callback,
                  bool is_portrait_mode);
  SplitButtonView(const SplitButtonView&) = delete;
  SplitButtonView& operator=(const SplitButtonView&) = delete;

  ~SplitButtonView() override = default;

 private:
  class SplitButton;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Called when either primary/secondary button is hovered. Updates button
  // colors.
  void OnButtonHovered();

  // Pointers to the buttons that are owned by the views hierarchy. Primary
  // refers to the button that is physically associated with the left or top;
  // secondary refers to the button that is physically associated with the
  // bottom or right.
  // TODO(shidi): Consider renaming these as primary/secondary snapped is
  // different from physical left/top or right/bottom.
  SplitButton* primary_button_;
  SplitButton* secondary_button_;

  const SplitButtonType type_;

  SkColor border_color_ = kMultitaskButtonDefaultColor;
  SkColor fill_color_ = SK_ColorTRANSPARENT;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_
