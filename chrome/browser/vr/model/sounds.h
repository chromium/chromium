// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_SOUNDS_H_
#define CHROME_BROWSER_VR_MODEL_SOUNDS_H_

#include "chrome/browser/vr/model/sound_id.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

struct VR_EXPORT Sounds {
  SoundId hover_enter = kSoundNone;
  SoundId hover_leave = kSoundNone;
  SoundId hover_move = kSoundNone;
  SoundId button_down = kSoundNone;
  SoundId button_up = kSoundNone;
  SoundId touch_move = kSoundNone;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_SOUNDS_H_
