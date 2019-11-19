// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTIL_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTIL_H_

#include <cstdint>
#include <memory>

#include "base/optional.h"
#include "media/base/audio_bus.h"

namespace chromecast {
namespace media {
namespace capture_service {

// Parse the header of the message and copy audio data to AudioBus. Nullopt will
// be returned if parsing fails. If an invalid timestamp is parsed, it won't be
// assigned to |timestamp| nor stop the function, however, an error message will
// be printed.
// The header of the message consists of <uint16_t channels><uint16_t format>
// <uint16_t padding><uint64_t timestamp_us>, where the unsigned |timestamp_us|
// will be converted to signed |timestamp| if valid.
base::Optional<std::unique_ptr<::media::AudioBus>>
ReadDataToAudioBus(const char* data, size_t size, int64_t* timestamp);

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTIL_H_
