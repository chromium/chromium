// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
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
      public display::DisplayObserver,
      public aura::WindowObserver {
  METADATA_HEADER(MultitaskMenu, views::BubbleDialogDelegateView)

 public:
  MultitaskMenu(views::View* anchor,
                views::Widget* parent_widget,
                bool close_on_move_out);
  MultitaskMenu(const MultitaskMenu&) = delete;
  MultitaskMenu& operator=(const MultitaskMenu&) = delete;
  ~MultitaskMenu() override;

  MultitaskMenuView* multitask_menu_view() {
    return multitask_menu_view_.get();
  }

  void HideBubble();

  base::WeakPtr<MultitaskMenu> GetWeakPtr();

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  raw_ptr<MultitaskMenuView> multitask_menu_view_ = nullptr;

  std::optional<display::ScopedDisplayObserver> display_observer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::WeakPtrFactory<MultitaskMenu> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_H_
