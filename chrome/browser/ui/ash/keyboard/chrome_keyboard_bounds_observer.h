// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_

#include <set>

#include "base/macros.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class RenderWidgetHostView;
}

// Class responsible for updating insets for windows overlapping the virtual
// keyboard.
class ChromeKeyboardBoundsObserver
    : public views::WidgetObserver,
      public ChromeKeyboardControllerClient::Observer {
 public:
  explicit ChromeKeyboardBoundsObserver(aura::Window* keyboard_window);
  ~ChromeKeyboardBoundsObserver() override;

  // keyboard::ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override {}
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;

 private:
  // Provides the bounds occluded by the keyboard any time they change.
  // (i.e. by the KeyboardController through KeyboardUI::InitInsets).
  void UpdateOccludedBounds(const gfx::Rect& screen_bounds);

  void AddObservedWindow(aura::Window* window);
  void RemoveAllObservedWindows();

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  void UpdateInsets(aura::Window* window, content::RenderWidgetHostView* view);
  bool ShouldWindowOverscroll(aura::Window* window);
  bool ShouldEnableInsets(aura::Window* window);

  aura::Window* const keyboard_window_;
  std::set<views::Widget*> observed_widgets_;
  gfx::Rect occluded_bounds_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardBoundsObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_
