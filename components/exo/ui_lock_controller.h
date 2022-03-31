// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
#define COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_

#include "ash/shell.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/exo/seat_observer.h"
#include "ui/events/event_handler.h"

class FullscreenControlPopup;

namespace exo {

class Pointer;
class Seat;

extern const base::TimeDelta kLongPressEscapeDuration;

// Helps users to break out of various kinds of "locks" that a window may hold
// (fullscreen, pointer lock).
//
// In some cases this is achieved by pressing and holding Escape, similar to
// Chromium's Keyboard Lock feature
// (see https://chromestatus.com/feature/5642959835889664). In other cases we
// nudge the user to use Overview.
class UILockController : public ui::EventHandler, public SeatObserver {
 public:
  explicit UILockController(Seat* seat);
  UILockController(const UILockController&) = delete;
  UILockController& operator=(const UILockController&) = delete;
  ~UILockController() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focued_surface) override;
  void OnPointerCaptureEnabled(Pointer* pointer,
                               aura::Window* capture_window) override;
  void OnPointerCaptureDisabled(Pointer* pointer,
                                aura::Window* capture_window) override;

  views::Widget* GetEscNotificationForTesting(aura::Window* window);
  views::Widget* GetPointerCaptureNotificationForTesting(aura::Window* window);
  FullscreenControlPopup* GetExitPopupForTesting(aura::Window* window);

 private:
  void OnEscapeKey(bool pressed);
  void OnEscapeHeld();
  void StopTimer();

  Seat* seat_;
  base::OneShotTimer exit_fullscreen_timer_;

  // The surface which was focused when |exit_fullscreen_timer_| started
  // running, or nullptr if the timer isn't running. Do not dereference; may
  // dangle if the Surface is destroyed while the timer is running. Valid only
  // for comparison purposes.
  Surface* focused_surface_to_unlock_ = nullptr;

  // Pointers currently being captured.
  base::flat_set<base::raw_ptr<Pointer>> captured_pointers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
