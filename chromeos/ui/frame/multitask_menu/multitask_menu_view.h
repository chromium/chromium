// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/window_observer.h"
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
    : public views::View,
      public aura::WindowObserver {
 public:
  METADATA_HEADER(MultitaskMenuView);

  // Bitmask for the buttons to show on the multitask menu view.
  enum MultitaskButtons : uint8_t {
    kHalfSplit = 1 << 0,
    kPartialSplit = 1 << 1,
    kFullscreen = 1 << 2,
    kFloat = 1 << 3,
  };

  // `window` is the window that the buttons on this view act on. `anchor_view`
  // should be passed when we want the functionality of auto-closing the menu
  // when the mouse moves out of the menu or the anchor.
  MultitaskMenuView(aura::Window* window,
                    base::RepeatingClosure close_callback,
                    uint8_t buttons,
                    views::View* anchor_view);

  MultitaskMenuView(const MultitaskMenuView&) = delete;
  MultitaskMenuView& operator=(const MultitaskMenuView&) = delete;

  ~MultitaskMenuView() override;

  // views::View:
  void AddedToWidget() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* parent_window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;

  // If the menu is opened because of mouse hover, moving the mouse outside the
  // menu for 3 seconds will result in it auto closing. This function reduces
  // that 3 second dealy to
  static void SetSkipMouseOutDelayForTesting(bool val);

  SplitButtonView* partial_button() { return partial_button_.get(); }

  // For testing.
  SplitButtonView* half_button_for_testing() {
    return half_button_for_testing_.get();
  }
  MultitaskButton* full_button_for_testing() {
    return full_button_for_testing_.get();
  }
  MultitaskButton* float_button_for_testing() {
    return float_button_for_testing_.get();
  }

 private:
  class MenuPreTargetHandler;

  // Callbacks for the buttons in the multitask menu view.
  void SplitButtonPressed(SnapDirection direction);
  void PartialButtonPressed(SnapDirection direction);
  void FullScreenButtonPressed();
  void FloatButtonPressed();

  raw_ptr<SplitButtonView> partial_button_ = nullptr;

  // Saved for testing purposes.
  raw_ptr<SplitButtonView> half_button_for_testing_ = nullptr;
  raw_ptr<MultitaskButton> full_button_for_testing_ = nullptr;
  raw_ptr<MultitaskButton> float_button_for_testing_ = nullptr;

  // The window which the buttons act on.
  raw_ptr<aura::Window, ExperimentalAsh> window_;

  // The view the menu is anchored to if any. This is only passed if we want to
  // close the menu when the mouse moves out of the multitask menu or its anchor
  // view.
  const raw_ptr<views::View, ExperimentalAsh> anchor_view_;

  // Runs when the widget which contains `this` should be destroyed. For
  // example, after any of the buttons are pressed, or a press out of the menu
  // bounds.
  base::RepeatingClosure close_callback_;

  std::unique_ptr<MenuPreTargetHandler> event_handler_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
