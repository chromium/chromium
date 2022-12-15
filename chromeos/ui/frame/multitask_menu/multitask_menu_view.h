// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class View;
}  // namespace views

namespace chromeos {

enum class SnapDirection;
class MultitaskButton;
class SplitButtonView;

// Contains buttons which can fullscreen, snap, or float a window.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) MultitaskMenuView
    : public views::View {
 public:
  METADATA_HEADER(MultitaskMenuView);

  // Bitmask for the buttons to show on the multitask menu view.
  enum MultitaskButtons : uint8_t {
    kHalfSplit = 1 << 0,
    kPartialSplit = 1 << 1,
    kFullscreen = 1 << 2,
    kFloat = 1 << 3,
  };

  MultitaskMenuView(aura::Window* window,
                    base::RepeatingClosure on_any_button_pressed,
                    uint8_t buttons);

  MultitaskMenuView(const MultitaskMenuView&) = delete;
  MultitaskMenuView& operator=(const MultitaskMenuView&) = delete;

  ~MultitaskMenuView() override;

  // For testing.
  SplitButtonView* half_button_for_testing() {
    return half_button_for_testing_.get();
  }
  SplitButtonView* partial_button_for_testing() {
    return partial_button_for_testing_.get();
  }
  MultitaskButton* full_button_for_testing() {
    return full_button_for_testing_.get();
  }
  MultitaskButton* float_button_for_testing() {
    return float_button_for_testing_.get();
  }

 private:
  // Callbacks for the buttons in the multitask menu view.
  void SplitButtonPressed(SnapDirection direction);
  void PartialButtonPressed(SnapDirection direction);
  void FullScreenButtonPressed();
  void FloatButtonPressed();

  // Saved for testing purpose.
  raw_ptr<SplitButtonView> half_button_for_testing_ = nullptr;
  raw_ptr<SplitButtonView> partial_button_for_testing_ = nullptr;
  raw_ptr<MultitaskButton> full_button_for_testing_ = nullptr;
  raw_ptr<MultitaskButton> float_button_for_testing_ = nullptr;

  // The window which the buttons act on. It is guaranteed to outlive `this`.
  aura::Window* const window_;

  // Runs after any of the buttons are pressed.
  base::RepeatingClosure on_any_button_pressed_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
