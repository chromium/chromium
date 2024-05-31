// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_SOUND_PLAYER_H_
#define CHROMECAST_MEDIA_API_SOUND_PLAYER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

class SoundPlayer {
 public:
  using AudioData = base::RefCountedData<std::string>;

  // Plays the sound resource with |resource_id|. Any sound that is currently
  // playing is stopped. Once played, audio data is registered with a
  // |sound_key| so the Stop() function can later identify which sound should be
  // stopped. If |repeat| is true, the sound is repeated until Stop() is called
  // with the same |sound_key| or another sound is played.
  virtual void Play(int sound_key,
                    int resource_id,
                    bool repeat,
                    AudioContentType content_type) = 0;

  // Plays the sound using the provided |audio_data|.
  // Once played, audio data is registered with a |sound_key| so the Stop()
  // function can later identify which sound should be stopped. If |repeat| is
  // true, the sound is repeated until Stop() is called with the same
  // |sound_key| or another sound is played.
  virtual void PlayAudioData(int sound_key,
                             scoped_refptr<AudioData> audio_data,
                             bool repeat,
                             AudioContentType content_type) = 0;

  // Plays the sound resource with |resource_id| starting at |timestamp|.
  // Any sound that is currently playing is stopped immediately.
  // If |audio_channel| is kLeft or kRight, only that channel is played.
  virtual void PlayAtTime(int resource_id,
                          int64_t timestamp,
                          media::AudioChannel audio_channel,
                          AudioContentType content_type) = 0;

  // Plays the sound using the provided |audio_data| starting at |timestamp|.
  // Any sound that is currently playing is stopped immediately.
  // If |audio_channel| is kLeft or kRight, only that channel is played.
  virtual void PlayAudioDataAtTime(scoped_refptr<AudioData> audio_data,
                                   int64_t timestamp,
                                   AudioChannel audio_channel,
                                   AudioContentType content_type) = 0;

  // Stops playing the sound resource with |resource_id| if it is currently
  // playing.
  virtual void Stop(int resource_id) = 0;

 protected:
  virtual ~SoundPlayer() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_SOUND_PLAYER_H_
