// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_

#include <set>

#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

// Class responsible for updating insets for windows overlapping the virtual
// keyboard.
class ChromeKeyboardBoundsObserver : public aura::WindowObserver {
 public:
  explicit ChromeKeyboardBoundsObserver(aura::Window* keyboard_window);
  ~ChromeKeyboardBoundsObserver() override;

  // Provides the bounds occluded by the keyboard any time they change.
  // (i.e. by the KeyboardController through KeyboardUI::InitInsets).
  void UpdateOccludedBounds(const gfx::Rect& occluded_bounds);

 private:
  void AddObservedWindow(aura::Window* window);
  void RemoveAllObservedWindows();

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;

  void UpdateInsetsForWindow(aura::Window* window);
  bool ShouldWindowOverscroll(aura::Window* window);
  bool ShouldEnableInsets(aura::Window* window);

  aura::Window* const keyboard_window_;
  std::set<aura::Window*> observed_windows_;
  gfx::Rect occluded_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardBoundsObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_BOUNDS_OBSERVER_H_
