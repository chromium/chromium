// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UTILS_HAPTICS_UTIL_H_
#define CHROMEOS_UTILS_HAPTICS_UTIL_H_

#include "base/component_export.h"

namespace ui {
class InputController;
enum class HapticTouchpadEffect;
enum class HapticTouchpadEffectStrength;
}  // namespace ui

// Utility that provides methods to trigger haptic effects throughout Ash.
// These call `InputController` functions that will play the effects if a haptic
// touchpad is available.
namespace chromeos::haptics_util {

// Sets test input controller for testing. When `g_test_input_controller1 is not
// nullptr, `PlayHapticTouchpadEffect()` will call the test controller instead
// of the real one from ozone.
COMPONENT_EXPORT(CHROMEOS_UTILS) void SetInputControllerForTesting(
    ui::InputController* input_controller);

// Plays a touchpad haptic feedback effect according to the given `effect` type,
// and the given `strength`. By default it uses ozone's input controller, unless
// it was overridden by the above `SetInputControllerForTesting()`.
COMPONENT_EXPORT(CHROMEOS_UTILS) void PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength);

// Plays a `ToggleOn` or `ToggleOff` haptic effect based on the `on` bool value.
COMPONENT_EXPORT(CHROMEOS_UTILS) void PlayHapticToggleEffect(
    bool on,
    ui::HapticTouchpadEffectStrength strength);

}  // namespace chromeos::haptics_util

#endif  // CHROMEOS_UTILS_HAPTICS_UTIL_H_
