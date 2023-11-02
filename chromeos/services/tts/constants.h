// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_CONSTANTS_H_
#define CHROMEOS_SERVICES_TTS_CONSTANTS_H_

namespace chromeos {
namespace tts {
// The location of the Chrome text-to-speech engine library.
extern const char kLibchromettsPath[];

// The location of read-write text-to-speech data.
extern const char kTempDataDirectory[];

// Default sample rate for audio playback.
extern const int kDefaultSampleRate;

// Default buffer size for audio playback.
extern const int kDefaultBufferSize;

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_CONSTANTS_H_
