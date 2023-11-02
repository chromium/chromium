// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CACHE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CACHE_H_

#include <vector>

#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

// Wrapper class to inject an API into VolumeCache that is used to populate the
// cache.
class SystemVolumeTableAccessApi {
 public:
  SystemVolumeTableAccessApi() = default;
  virtual ~SystemVolumeTableAccessApi() = default;

  virtual int GetMaxVolumeIndex(AudioContentType type) = 0;
  virtual float VolumeToDbFS(AudioContentType type, float volume) = 0;
};

// Builds a cache of the system's volume table and provides access to it.
class VolumeCache {
 public:
  VolumeCache(AudioContentType type, SystemVolumeTableAccessApi* api);

  VolumeCache(const VolumeCache&) = delete;
  VolumeCache& operator=(const VolumeCache&) = delete;

  ~VolumeCache();

  // Returns the mapped and interpolated dBFS value for the given volume level,
  // using the cached volume table.
  float VolumeToDbFS(float vol_level);

  // Returns the mapped and interpolated volume value for the given dBFS value,
  // using the cached volume table.
  float DbFSToVolume(float db);

 private:
  const int kMaxVolumeIndex;

  std::vector<float> cache_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CACHE_H_
