// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SOUNDS_MANAGER_AUDIO_DELEGATE_H_
#define CHROME_BROWSER_VR_SOUNDS_MANAGER_AUDIO_DELEGATE_H_

#include <unordered_map>

#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

class VR_EXPORT SoundsManagerAudioDelegate : public AudioDelegate {
 public:
  SoundsManagerAudioDelegate();

  SoundsManagerAudioDelegate(const SoundsManagerAudioDelegate&) = delete;
  SoundsManagerAudioDelegate& operator=(const SoundsManagerAudioDelegate&) =
      delete;

  ~SoundsManagerAudioDelegate() override;

  // AudioDelegate implementation.
  void ResetSounds() override;
  bool RegisterSound(SoundId, std::unique_ptr<std::string> data) override;
  void PlaySound(SoundId id) override;

 private:
  std::unordered_map<SoundId, std::unique_ptr<std::string>> sounds_;
};

}  //  namespace vr

#endif  // CHROME_BROWSER_VR_SOUNDS_MANAGER_AUDIO_DELEGATE_H_
