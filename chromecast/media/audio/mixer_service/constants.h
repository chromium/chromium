// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONSTANTS_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONSTANTS_H_

#include <stdint.h>

#include "chromecast/media/audio/mixer_service/mixer_service_buildflags.h"

namespace chromecast {
namespace media {
namespace mixer_service {

#if BUILDFLAG(USE_UNIX_SOCKETS)
constexpr char kDefaultUnixDomainSocketPath[] = "/tmp/mixer-service";
#else
constexpr int kDefaultTcpPort = 12854;
#endif

enum class MessageType : int16_t {
  kMetadata,
  kAudio,
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONSTANTS_H_
