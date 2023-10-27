// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/haptics_util.h"

#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromeos::haptics_util {

namespace {

ui::InputController* g_test_input_controller = nullptr;

}  // namespace

void SetInputControllerForTesting(ui::InputController* input_controller) {
  g_test_input_controller = input_controller;
}

void PlayHapticTouchpadEffect(ui::HapticTouchpadEffect effect,
                              ui::HapticTouchpadEffectStrength strength) {
  ui::InputController* input_controller =
      g_test_input_controller
          ? g_test_input_controller
          : ui::OzonePlatform::GetInstance()->GetInputController();
  DCHECK(input_controller);
  input_controller->PlayHapticTouchpadEffect(effect, strength);
}

void PlayHapticToggleEffect(bool on,
                            ui::HapticTouchpadEffectStrength strength) {
  PlayHapticTouchpadEffect(on ? ui::HapticTouchpadEffect::kToggleOn
                              : ui::HapticTouchpadEffect::kToggleOff,
                           strength);
}

}  // namespace chromeos::haptics_util
