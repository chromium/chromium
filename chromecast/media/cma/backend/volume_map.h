// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VOLUME_MAP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VOLUME_MAP_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"

namespace base {
class Value;
}  // namespace base

namespace chromecast {
namespace media {

class VolumeMap {
 public:
  VolumeMap();

  // For testing.
  VolumeMap(std::unique_ptr<CastAudioJsonProvider> config_provider);

  ~VolumeMap();

  float VolumeToDbFS(float volume);

  float DbFSToVolume(float db);

 private:
  struct LevelToDb {
    float level;
    float db;
  };

  void LoadVolumeMap(std::unique_ptr<base::Value> cast_audio_config);
  void UseDefaultVolumeMap();

  // |volume_map_| must be accessed with |lock_|.
  base::Lock lock_;
  std::vector<LevelToDb> volume_map_;

  std::unique_ptr<CastAudioJsonProvider> config_provider_;

  DISALLOW_COPY_AND_ASSIGN(VolumeMap);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VOLUME_MAP_H_
