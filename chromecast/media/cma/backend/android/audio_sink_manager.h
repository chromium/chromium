// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_

#include <map>
#include <vector>

#include "base/synchronization/lock.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"

namespace chromecast {
namespace media {

// Implementation of a manager for a group of audio sinks in order to control
// and configure them (currently for volume control).
class AudioSinkManager {
 public:
  static AudioSinkManager* Get();

  AudioSinkManager(const AudioSinkManager&) = delete;
  AudioSinkManager& operator=(const AudioSinkManager&) = delete;

  // Adds the given sink instance to the vector.
  void Add(AudioSinkAndroid* sink);

  // Removes the given sink instance from the vector.
  void Remove(AudioSinkAndroid* sink);

  // Sets the type volume level (in dBFS) for the given content |type|. Note
  // that this volume is not actually applied to the sink, as the volume
  // controller applies it directly in Android OS. Instead the value is just
  // stored and used to calculate the proper limiter multiplier.
  void SetTypeVolumeDb(AudioContentType type, float level_db);

  // Sets the volume limit (in dBFS) for the given content |type|.
  void SetOutputLimitDb(AudioContentType type, float limit_db);

 protected:
  AudioSinkManager();
  virtual ~AudioSinkManager();

 private:
  // Contains volume control information for an audio content type.
  struct VolumeInfo {
    float volume_db = 0.0f;
    float limit_db = 1.0f;
  };

  // Updates the limiter multipliers for all sinks of the given type.
  void UpdateAllLimiterMultipliers(AudioContentType type);

  // Updates the limiter multiplier for the given sink.
  void UpdateLimiterMultiplier(AudioSinkAndroid* sink);

  // Returns the limiter multiplier such that:
  //   multiplier * volume = min(volume, limit).
  float GetLimiterMultiplier(AudioContentType type);

  base::Lock lock_;

  std::map<AudioContentType, VolumeInfo> volume_info_;

  std::vector<AudioSinkAndroid*> sinks_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_
