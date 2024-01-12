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
  METADATA_HEADER(MultitaskMenuView, views::View)

 public:
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
                    base::RepeatingClosure dismiss_callback,
                    uint8_t buttons,
                    views::View* anchor_view);

  MultitaskMenuView(const MultitaskMenuView&) = delete;
  MultitaskMenuView& operator=(const MultitaskMenuView&) = delete;

  ~MultitaskMenuView() override;

  SplitButtonView* partial_button() { return partial_button_.get(); }

  // Forwarded from the size button which is the anchor of `this`'s widget. When
  // an event starts on the size button, it will receive all subsequent events.
  // Therefore, we have to manually forward them if the pointer is visually on
  // `this`. Receives the event and updates the hover state or fires the
  // callback of the button that the pointer is visually on top of.
  // `OnSizeButtonRelease` returns true if any button was activated.
  void OnSizeButtonDrag(const gfx::Point& event_screen_location);
  bool OnSizeButtonRelease(const gfx::Point& event_screen_location);

  // views::View:
  void AddedToWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* parent_window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;

  // If the menu is opened because of mouse hover, moving the mouse outside the
  // menu for 3 seconds will result in it auto closing. This function reduces
  // that 3 second delay to zero.
  static void SetSkipMouseOutDelayForTesting(bool val);

 private:
  class MenuPreTargetHandler;
  friend class MultitaskMenuViewTestApi;

  // Callbacks for the buttons in the multitask menu view.
  void HalfButtonPressed(SnapDirection direction);
  void PartialButtonPressed(SnapDirection direction);
  void FullScreenButtonPressed();
  void FloatButtonPressed();

  // Owned by views hierarchy.
  raw_ptr<SplitButtonView> half_button_ = nullptr;
  raw_ptr<SplitButtonView> partial_button_ = nullptr;
  raw_ptr<MultitaskButton> full_button_ = nullptr;
  raw_ptr<MultitaskButton> float_button_ = nullptr;

  // True if the menu buttons should be painted in reverse, when the `Alt` key
  // is pressed. Toggled on every `Alt` press.
  bool is_reversed_ = false;

  // The window which the buttons act on.
  raw_ptr<aura::Window> window_;

  // The view the menu is anchored to if any. This is only passed if we want to
  // close the menu when the mouse moves out of the multitask menu or its anchor
  // view.
  const raw_ptr<views::View, DanglingUntriaged> anchor_view_;

  // Runs when the widget which contains `this` should be destroyed. For
  // example, after any of the buttons are pressed, or a press out of the menu
  // bounds.
  base::RepeatingClosure close_callback_;

  // Run by the `MenuPreTargetHandler` to dismiss the menu when clicking outside
  // the menu (or anchor) bounds, or after timeout.
  base::RepeatingClosure dismiss_callback_;

  std::unique_ptr<MenuPreTargetHandler> event_handler_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_H_
