// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_UTILS_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_UTILS_H_

#include <string>

namespace chromecast {
namespace media {

// Returns whether the device is valid.
bool IsValidDeviceId(const std::string& device_id);

// Returns the group id for provided device id.
std::string GetGroupId(const std::string& device_id);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_UTILS_H_
