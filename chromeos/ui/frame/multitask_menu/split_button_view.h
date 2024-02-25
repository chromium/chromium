// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace chromeos {

// A button view with 2 divided buttons, left/top and right/bottom. On click,
// each button will either snap primary or snap secondary, depending on the
// location of the button and the orientation of the device.
class SplitButtonView : public views::BoxLayoutView {
  METADATA_HEADER(SplitButtonView, views::BoxLayoutView)

 public:
  enum class SplitButtonType {
    kHalfButtons,
    kPartialButtons,
  };

  using SplitButtonCallback = base::RepeatingCallback<void(SnapDirection)>;

  SplitButtonView(SplitButtonType type,
                  SplitButtonCallback split_button_callback,
                  aura::Window* window,
                  bool is_portrait_mode);
  SplitButtonView(const SplitButtonView&) = delete;
  SplitButtonView& operator=(const SplitButtonView&) = delete;

  ~SplitButtonView() override = default;

  // Updates the split button layout and a11y names. The split button callbacks
  // will be updated in MultitaskMenuView.
  void UpdateButtons(bool is_portrait_mode, bool is_reversed);

  views::Button* GetLeftTopButton();
  views::Button* GetRightBottomButton();

 private:
  class SplitButton;

  // Called when either button is hovered or pressed. Updates button colors.
  void OnButtonHoveredOrPressed();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Pointers to the buttons that are owned by the views hierarchy. The names
  // refer to the physical location of the button, which do not change in RTL
  // languages.
  raw_ptr<SplitButton> left_top_button_ = nullptr;
  raw_ptr<SplitButton> right_bottom_button_ = nullptr;

  const SplitButtonType type_;

  SkColor border_color_ = SK_ColorTRANSPARENT;
  SkColor fill_color_ = SK_ColorTRANSPARENT;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_SPLIT_BUTTON_VIEW_H_
