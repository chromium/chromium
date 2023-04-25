// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace chromeos {

// MultitaskMenu is the window layout menu attached to frame size button.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) MultitaskMenu
    : public views::BubbleDialogDelegateView,
      public display::DisplayObserver {
 public:
  METADATA_HEADER(MultitaskMenu);

  MultitaskMenu(views::View* anchor,
                views::Widget* parent_widget,
                bool close_on_move_out);
  MultitaskMenu(const MultitaskMenu&) = delete;
  MultitaskMenu& operator=(const MultitaskMenu&) = delete;
  ~MultitaskMenu() override;

  MultitaskMenuView* multitask_menu_view_for_testing() {
    return multitask_menu_view_.get();
  }

  void HideBubble();

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  raw_ptr<MultitaskMenuView> multitask_menu_view_ = nullptr;

  absl::optional<display::ScopedDisplayObserver> display_observer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
