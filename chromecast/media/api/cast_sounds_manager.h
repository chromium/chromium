// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CAST_SOUNDS_MANAGER_H_
#define CHROMECAST_MEDIA_API_CAST_SOUNDS_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

// Plays a sound stored as a resource. All methods are thread-safe.
class CastSoundsManager {
 public:
  using DurationCallback = base::OnceCallback<void(base::TimeDelta duration)>;

  virtual ~CastSoundsManager() = default;

  // Adds a sound where |resource_id| contains the audio data. If |multichannel|
  // is true, the sound is played on all devices in a multichannel group, and if
  // it is false, the sound is played only on this device. If |repeat| is true,
  // the sound is played repeatedly until Stop() is called for the sound or
  // another sound is played. Both |multichannel| and |repeat| cannot be true.
  virtual void AddSound(int key,
                        int resource_id,
                        bool multichannel,
                        bool repeat) = 0;

  // Adds a sound by binding |key| to the provided |audio_data|.
  // This is to support playing sounds outside of resource packs on the
  // device(s). If |multichannel| is true, the sound is played on all devices in
  // a multichannel group, and if it is false, the sound is played only on this
  // device. If |repeat| is true, the sound is played repeatedly until Stop() is
  // called for the sound or another sound is played. |multichannel| and
  // |repeat| cannot both be true.
  virtual void AddSoundWithAudioData(int key,
                                     const std::string audio_data,
                                     bool multichannel,
                                     bool repeat) = 0;

  // Plays the sound added for |key|.
  virtual void Play(int key, AudioContentType content_type) = 0;

  // Stops playing the sound added for |key| if it is currently playing.
  virtual void Stop(int key) = 0;

  // Calls |callback| with the duration of the sound added for |key|, or a
  // duration of ::media::kInfiniteDuration if no sound was added for |key|.
  // There is no guarantee about whether |callback| is called synchronously or
  // asynchronously or on which thread it is called.
  virtual void GetDuration(int key, DurationCallback callback) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CAST_SOUNDS_MANAGER_H_
