// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/saved_volumes.h"

#include <string>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"

namespace chromecast {
namespace media {

namespace {

constexpr float kDefaultMediaDbFS = -25.0f;
constexpr float kDefaultAlarmDbFS = -20.0f;
constexpr float kDefaultCommunicationDbFS = -25.0f;

constexpr char kKeyMediaDbFS[] = "dbfs.media";
constexpr char kKeyAlarmDbFS[] = "dbfs.alarm";
constexpr char kKeyCommunicationDbFS[] = "dbfs.communication";
constexpr char kKeyDefaultVolume[] = "default_volume";

std::string ContentTypeToDbFSKey(AudioContentType type) {
  switch (type) {
    case AudioContentType::kAlarm:
      return kKeyAlarmDbFS;
    case AudioContentType::kCommunication:
      return kKeyCommunicationDbFS;
    default:
      return kKeyMediaDbFS;
  }
}

}  // namespace

base::flat_map<AudioContentType, double> LoadSavedVolumes(
    const base::FilePath& storage_path) {
  auto types = {AudioContentType::kMedia, AudioContentType::kAlarm,
                AudioContentType::kCommunication};
  base::flat_map<AudioContentType, double> volumes;

  volumes[AudioContentType::kMedia] = kDefaultMediaDbFS;
  volumes[AudioContentType::kAlarm] = kDefaultAlarmDbFS;
  volumes[AudioContentType::kCommunication] = kDefaultCommunicationDbFS;

  JSONFileValueDeserializer deserializer(storage_path);
  auto stored_data = deserializer.Deserialize(nullptr, nullptr);
  if (stored_data && stored_data->is_dict()) {
    const auto& stored_data_dict = stored_data->GetDict();
    for (auto type : types) {
      auto v =
          stored_data_dict.FindDoubleByDottedPath(ContentTypeToDbFSKey(type));
      if (v) {
        volumes[type] = v.value();
      }
    }
    return volumes;
  }

  LOG(INFO) << "No saved volumes found";
  // If saved_volumes does not exist, use per-device default if it exists.
  auto path = media::CastAudioJson::GetFilePath();
  JSONFileValueDeserializer cast_audio_deserializer(path);
  auto cast_audio_config =
      cast_audio_deserializer.Deserialize(nullptr, nullptr);
  if (!cast_audio_config || !cast_audio_config->is_dict()) {
    LOG(INFO) << "Invalid JSON from " << path;
    return volumes;
  }

  const auto& cast_audio_config_dict = cast_audio_config->GetDict();

  const base::Value::Dict* default_volume_dict =
      cast_audio_config_dict.FindDict(kKeyDefaultVolume);
  if (!default_volume_dict) {
    LOG(INFO) << "No default volumes specified in " << path;
    return volumes;
  }

  for (auto type : types) {
    std::string key = ContentTypeToDbFSKey(type);
    auto v = default_volume_dict->FindDoubleByDottedPath(key);
    if (v) {
      LOG(INFO) << "Using default volume for " << key << " of " << v.value();
      volumes[type] = v.value();
    } else {
      LOG(INFO) << "No default volume for " << key;
    }
  }
  return volumes;
}

}  // namespace media
}  // namespace chromecast
