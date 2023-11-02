// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/constants.h"

namespace chromeos {
namespace tts {
const char kLibchromettsPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts/libchrometts.so";
const char kTempDataDirectory[] = "/tmp/tts";
const int kDefaultSampleRate = 24000;
const int kDefaultBufferSize = 512;

}  // namespace tts
}  // namespace chromeos
