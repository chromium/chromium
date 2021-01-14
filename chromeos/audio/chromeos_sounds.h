// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_
#define CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_

// This file declares sound resources keys for ChromeOS.
namespace chromeos {

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
  kDictationEnd,
  kDictationStart,
  kDictationCancel,
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_
