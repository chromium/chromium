// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
#define COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_

#include "base/timer/timer.h"
#include "components/exo/seat_observer.h"
#include "ui/events/event_handler.h"

namespace exo {

class Seat;

extern const base::TimeDelta kLongPressEscapeDuration;

// Listens for long presses on the Escape key, which breaks out of various
// kinds of "locks" that a window may hold.
//
// TODO(cpelling): For now this is just non-immersive fullscreen. Eventually
// this should also break pointer lock.
//
// The "long keypress" design is inspired by Chromium's Keyboard Lock feature
// (see https://chromestatus.com/feature/5642959835889664).
class UILockController : public ui::EventHandler, public SeatObserver {
 public:
  explicit UILockController(Seat* seat);
  UILockController(const UILockController&) = delete;
  UILockController& operator=(const UILockController&) = delete;
  ~UILockController() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden from SeatObserver:
  void OnSurfaceFocusing(Surface* gaining_focus) override {}
  void OnSurfaceFocused(Surface* gained_focus) override;

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
};

}  // namespace exo

#endif  // COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
