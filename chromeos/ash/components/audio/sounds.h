// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_SOUNDS_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_SOUNDS_H_

// This file declares sound resources keys for ChromeOS.
namespace ash {

enum class Sound {
  kStartup,
  kLock,
  kObjectDelete,
  kCameraSnap,
  kUnlock,
  kShutdown,
  kSpokenFeedbackEnabled,
  kSpokenFeedbackDisabled,
  kVolumeAdjust,
  kPassthrough,
  kExitScreen,
  kEnterScreen,
  kSpokenFeedbackToggleCountdownHigh,
  kSpokenFeedbackToggleCountdownLow,
  kTouchType,
  // Sound keys for the moment when the device is plugged in a charger cable.
  kChargeHighBattery,
  kChargeMediumBattery,
  kChargeLowBattery,
  // Sound key for low battery when the device isn't charging.
  kNoChargeLowBattery,
  // Sound key for the Focus mode notifying the ending moment.
  kFocusModeEndingMoment,
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_SOUNDS_H_
