// Copyright 2013 The Chromium Authors. All rights reserved.
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
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the Chrome OS source code
// directory migration is finished.
namespace chromeos {
using ::ash::Sound;
}

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_SOUNDS_H_
