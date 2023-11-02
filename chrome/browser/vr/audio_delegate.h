// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_AUDIO_DELEGATE_H_
#define CHROME_BROWSER_VR_AUDIO_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/vr/model/sound_id.h"

namespace vr {

// This delegate interface describes an audio implementation supplied to the UI.
class AudioDelegate {
 public:
  virtual ~AudioDelegate() {}

  // Clears all registered sounds. This must be done before re-registering a
  // particular sound.
  virtual void ResetSounds() = 0;

  // The delegate must assume ownership of the audio data. A sound may only be
  // registered once.  To change the sound later, call ResetSounds and
  // re-register all sounds.
  virtual bool RegisterSound(SoundId id, std::unique_ptr<std::string> data) = 0;

  virtual void PlaySound(SoundId id) = 0;
};

}  //  namespace vr

#endif  // CHROME_BROWSER_VR_AUDIO_DELEGATE_H_
