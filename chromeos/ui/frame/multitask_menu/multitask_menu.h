// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
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

  views::Widget* bubble_widget_for_testing() { return bubble_widget_.get(); }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Displays the MultitaskMenu.
  void ShowBubble();

  // Hides the currently-showing MultitaskMenu.
  void HideBubble();

  MultitaskMenuView* multitask_menu_view_for_testing() {
    return multitask_menu_view_.get();
  }

 private:
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observer_{this};

  raw_ptr<MultitaskMenuView> multitask_menu_view_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
