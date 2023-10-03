// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_POWER_MANAGER_EMITTER_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_POWER_MANAGER_EMITTER_H_

#include "chromeos/dbus/power_manager/input_event.pb.h"

// This class emulates the system power manager daemon to allow tests to
// broadcast synthetic power events.
//
// It does this by stopping the power manager and using dbus-send to
// broadcast the event in its place.
class PowerManagerEmitter {
 public:
  PowerManagerEmitter();
  ~PowerManagerEmitter();

  // Returns true on success.
  bool EmitInputEvent(power_manager::InputEvent_Type type);
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_POWER_MANAGER_EMITTER_H_
