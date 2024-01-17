// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/volume_map.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

namespace {

constexpr char kKeyVolumeMap[] = "volume_map";
constexpr char kKeyLevel[] = "level";
constexpr char kKeyDb[] = "db";
constexpr float kMinDbFS = -120.0f;

VolumeMap& GetVolumeMap() {
  static base::NoDestructor<VolumeMap> volume_map;
  return *volume_map;
}

}  // namespace

VolumeMap::VolumeMap()
    : VolumeMap(std::make_unique<CastAudioJsonProviderImpl>()) {}

VolumeMap::VolumeMap(std::unique_ptr<CastAudioJsonProvider> config_provider)
    : config_provider_(std::move(config_provider)) {
  DCHECK(config_provider_);
  // base::Unretained is safe because VolumeMap outlives |config_provider_|.
  config_provider_->SetTuningChangedCallback(
      base::BindRepeating(&VolumeMap::LoadVolumeMap, base::Unretained(this)));
  LoadFromFile();
}

VolumeMap::~VolumeMap() = default;

void VolumeMap::LoadFromFile() {
  LoadVolumeMap(config_provider_->GetCastAudioConfig());
}

void VolumeMap::LoadVolumeMap(
    std::optional<base::Value::Dict> cast_audio_config) {
  if (!cast_audio_config) {
    LOG(WARNING) << "No cast audio config found; using default volume map.";
    UseDefaultVolumeMap();
    return;
  }

  const base::Value::List* volume_map_list =
      cast_audio_config->FindList(kKeyVolumeMap);
  if (!volume_map_list) {
    LOG(WARNING) << "No volume map found; using default volume map.";
    UseDefaultVolumeMap();
    return;
  }

  double prev_level = -1.0;
  std::vector<LevelToDb> new_map;

  for (const auto& value : *volume_map_list) {
    const base::Value::Dict& volume_map_entry = value.GetDict();

    std::optional<double> level = volume_map_entry.FindDouble(kKeyLevel);
    CHECK(level);
    CHECK_GE(*level, 0.0);
    CHECK_LE(*level, 1.0);
    CHECK_GT(*level, prev_level);
    prev_level = *level;

    std::optional<double> db = volume_map_entry.FindDouble(kKeyDb);
    CHECK(db);
    CHECK_LE(*db, 0.0);

    new_map.push_back({static_cast<float>(*level), static_cast<float>(*db)});
  }

  if (new_map.empty()) {
    LOG(FATAL) << "No entries in volume map.";
  }

  if (new_map[0].level > 0.0) {
    new_map.insert(new_map.begin(), {0.0, kMinDbFS});
  }

  if (new_map.rbegin()->level < 1.0) {
    new_map.push_back({1.0, 0.0});
  }
  base::AutoLock lock(lock_);
  volume_map_ = std::move(new_map);
}

float VolumeMap::VolumeToDbFS(float volume) {
  base::AutoLock lock(lock_);
  if (volume <= volume_map_[0].level) {
    return volume_map_[0].db;
  }
  for (size_t i = 1; i < volume_map_.size(); ++i) {
    if (volume < volume_map_[i].level) {
      const float x_range = volume_map_[i].level - volume_map_[i - 1].level;
      const float y_range = volume_map_[i].db - volume_map_[i - 1].db;
      const float x_pos = volume - volume_map_[i - 1].level;

      return volume_map_[i - 1].db + x_pos * y_range / x_range;
    }
  }
  return volume_map_[volume_map_.size() - 1].db;
}

float VolumeMap::DbFSToVolume(float db) {
  base::AutoLock lock(lock_);
  if (db <= volume_map_[0].db) {
    return volume_map_[0].level;
  }
  for (size_t i = 1; i < volume_map_.size(); ++i) {
    if (db < volume_map_[i].db) {
      const float x_range = volume_map_[i].db - volume_map_[i - 1].db;
      const float y_range = volume_map_[i].level - volume_map_[i - 1].level;
      const float x_pos = db - volume_map_[i - 1].db;

      return volume_map_[i - 1].level + x_pos * y_range / x_range;
    }
  }
  return volume_map_[volume_map_.size() - 1].level;
}

void VolumeMap::UseDefaultVolumeMap() {
  std::vector<LevelToDb> new_map = {{0.0f, kMinDbFS},
                                    {0.01f, -58.0f},
                                    {0.090909f, -48.0f},
                                    {0.818182f, -8.0f},
                                    {1.0f, 0.0f}};
  base::AutoLock lock(lock_);
  volume_map_ = std::move(new_map);
}

// static
float VolumeControl::VolumeToDbFS(float volume) {
  return GetVolumeMap().VolumeToDbFS(volume);
}

// static
float VolumeControl::DbFSToVolume(float db) {
  return GetVolumeMap().DbFSToVolume(db);
}

// static
void VolumeMap::Reload() {
  return GetVolumeMap().LoadFromFile();
}

}  // namespace media
}  // namespace chromecast
