// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMING_SEAT_DELEGATE_H_
#define COMPONENTS_EXO_GAMING_SEAT_DELEGATE_H_

namespace ui {
struct GamepadDevice;
}  // namespace ui

namespace exo {
class Surface;
class GamepadDelegate;
class GamingSeat;

// It send gamepad_added event and generate the GamepadDelegate.
class GamingSeatDelegate {
 public:
  // Gives the delegate a chance to clean up when the GamingSeat instance is
  // destroyed
  virtual void OnGamingSeatDestroying(GamingSeat* gaming_seat) = 0;

  // This should return true if |surface| is a valid target for this gaming
  // seat. E.g. the surface is owned by the same client as the gaming seat.
  virtual bool CanAcceptGamepadEventsForSurface(Surface* surface) const = 0;

  // When a new gamepad is connected, gaming seat call this to get the
  // gamepad delegate.
  virtual GamepadDelegate* GamepadAdded(const ui::GamepadDevice& device) = 0;

 protected:
  virtual ~GamingSeatDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMING_SEAT_DELEGATE_H_
