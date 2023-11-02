// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/volume_cache.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"

namespace chromecast {
namespace media {

VolumeCache::VolumeCache(AudioContentType type, SystemVolumeTableAccessApi* api)
    : kMaxVolumeIndex(api->GetMaxVolumeIndex(type)) {
  LOG(INFO) << "Build volume cache for type " << static_cast<int>(type) << ":";
  cache_.resize(kMaxVolumeIndex + 1);
  for (size_t v_idx = 0; v_idx < cache_.size(); v_idx++) {
    float v_level = static_cast<float>(v_idx) / kMaxVolumeIndex;
    float dbVolume = api->VolumeToDbFS(type, v_level);
    if (isinf(dbVolume)) {
      // In Android volume tables, -inf started to be used for mute. Use a value
      // that is effectively mute, but allows math to be performed.
      dbVolume = -120.0;
    }
    cache_[v_idx] = dbVolume;
    LOG(INFO) << "     " << v_idx << "(" << v_level << ") -> " << cache_[v_idx];
  }
}

VolumeCache::~VolumeCache() = default;

float VolumeCache::VolumeToDbFS(float vol_level) {
  if (vol_level <= 0.0f)
    return cache_[0];
  if (vol_level >= 1.0f)
    return cache_[kMaxVolumeIndex];

  float vol_idx = vol_level * kMaxVolumeIndex;
  // Find the nearest integers below and above vol_idx.
  float vol_idx_high = std::ceil(vol_idx);
  float vol_idx_low = std::floor(vol_idx);
  float db_high = cache_[static_cast<int>(vol_idx_high)];
  if (vol_idx_high == vol_idx_low) {
    return db_high;
  }
  // We are in between two consecutive volume points, so interpolate.
  // Note that vol_idx_high = vol_idx_low + 1.
  float db_low = cache_[static_cast<int>(vol_idx_low)];
  float m = (db_high - db_low) / 1.0f;
  float db_interpolated = db_low + m * (vol_idx - vol_idx_low);
  return db_interpolated;
}

float VolumeCache::DbFSToVolume(float db) {
  auto db_high_it = std::lower_bound(cache_.begin(), cache_.end(), db);
  if (db_high_it == cache_.end())
    return 1.0f;
  if (db_high_it == cache_.begin())
    return 0.0f;

  int vol_idx_high = db_high_it - cache_.begin();
  if (db == *db_high_it)
    return static_cast<float>(vol_idx_high) / kMaxVolumeIndex;

  // We are in between two consecutive volume points, so interpolate.
  // Note that vol_idx_high = vol_idx_low + 1.
  auto db_low_it = std::prev(db_high_it);
  float m = 1.0f / (*db_high_it - *db_low_it);
  float vol_idx = static_cast<float>(vol_idx_high) - m * (*db_high_it - db);
  return vol_idx / kMaxVolumeIndex;
}

}  // namespace media
}  // namespace chromecast
