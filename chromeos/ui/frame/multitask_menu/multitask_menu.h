// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_

#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/split_button.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace chromeos {

// MultitaskMenu is the window operation menu attached to frame
// size button.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) MultitaskMenu
    : public views::BubbleDialogDelegateView,
      public views::WidgetObserver {
 public:
  MultitaskMenu(views::View* anchor, aura::Window* parent_window);

  MultitaskMenu(const MultitaskMenu&) = delete;
  MultitaskMenu& operator=(const MultitaskMenu&) = delete;

  ~MultitaskMenu() override;

  // For testing.
  SplitButtonView* half_button_for_testing() const {
    return half_button_.get();
  }
  SplitButtonView* partial_button_for_testing() const {
    return partial_button_.get();
  }
  MultitaskBaseButton* full_button_for_testing() const {
    return full_button_.get();
  }
  MultitaskBaseButton* float_button_for_testing() const {
    return float_button_.get();
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Displays the MultitaskMenu.
  void ShowBubble();
  // Hides the currently-showing MultitaskMenu.
  void HideBubble();

 private:
  // Callbacks for the buttons in the multitask menu view.
  void SplitButtonPressed(SnapDirection snap);
  void PartialButtonPressed(SnapDirection snap);

  void FullScreenButtonPressed();
  void FloatButtonPressed();

  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observer_{this};

  // Saved for testing purpose.
  raw_ptr<SplitButtonView> half_button_;
  raw_ptr<SplitButtonView> partial_button_;
  raw_ptr<MultitaskBaseButton> full_button_;
  raw_ptr<MultitaskBaseButton> float_button_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
