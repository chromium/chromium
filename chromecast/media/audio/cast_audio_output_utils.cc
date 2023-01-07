// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_utils.h"

#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

bool IsValidDeviceId(const std::string& device_id) {
  return device_id == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId;
}

std::string GetGroupId(const std::string& device_id) {
  return IsValidDeviceId(device_id) ? "" : device_id;
}

}  // namespace media
}  // namespace chromecast
