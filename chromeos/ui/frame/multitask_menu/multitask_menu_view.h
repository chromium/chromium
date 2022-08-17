// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/split_button.h"

namespace views {
class View;
}  // namespace views

namespace chromeos {

enum class SnapDirection;
class MultitaskBaseButton;

// Contains buttons which can fullscreen, snap, or float a window.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) MultitaskMenuView
    : public views::View {
 public:
  MultitaskMenuView(aura::Window* window,
                    base::RepeatingClosure on_any_button_pressed);

  MultitaskMenuView(const MultitaskMenuView&) = delete;
  MultitaskMenuView& operator=(const MultitaskMenuView&) = delete;

  ~MultitaskMenuView() override;

  // For testing.
  SplitButtonView* half_button_for_testing() { return half_button_.get(); }
  SplitButtonView* partial_button_for_testing() {
    return partial_button_.get();
  }
  MultitaskBaseButton* full_button_for_testing() { return full_button_.get(); }
  MultitaskBaseButton* float_button_for_testing() {
    return float_button_.get();
  }

 private:
  // Callbacks for the buttons in the multitask menu view.
  void SplitButtonPressed(SnapDirection snap);
  void PartialButtonPressed(SnapDirection snap);
  void FullScreenButtonPressed();
  void FloatButtonPressed();

  // Saved for testing purpose.
  raw_ptr<SplitButtonView> half_button_;
  raw_ptr<SplitButtonView> partial_button_;
  raw_ptr<MultitaskBaseButton> full_button_;
  raw_ptr<MultitaskBaseButton> float_button_;

  // The window which the buttons act on. It is guaranteed to outlive `this`.
  aura::Window* const window_;

  // Runs after any of the buttons are pressed.
  base::RepeatingClosure on_any_button_pressed_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
