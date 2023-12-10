// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_OBSERVER_H_
#define COMPONENTS_EXO_SEAT_OBSERVER_H_

namespace aura {
class Window;
}

namespace exo {

class Pointer;
class Surface;

// Observers can listen to various events on the Seats.
class SeatObserver {
 public:
  virtual void OnSurfaceCreated(Surface* surface) {}

  // Called when a new surface receives keyboard focus.
  virtual void OnSurfaceFocused(Surface* gained_focus,
                                Surface* lost_focus,
                                bool has_focused_client) = 0;

  // Called when a pointer is captured by the given window.
  virtual void OnPointerCaptureEnabled(Pointer* pointer,
                                       aura::Window* capture_window) {}

  // Called when the given pointer is no longer captured by the given window.
  virtual void OnPointerCaptureDisabled(Pointer* pointer,
                                        aura::Window* capture_window) {}

  // Called when the keyboard modifiers is updated.
  virtual void OnKeyboardModifierUpdated() {}

 protected:
  virtual ~SeatObserver() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_OBSERVER_H_
