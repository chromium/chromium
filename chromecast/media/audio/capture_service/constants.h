// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_

namespace chromecast {
namespace media {
namespace capture_service {

constexpr char kDefaultUnixDomainSocketPath[] = "/tmp/capture-service";
constexpr int kDefaultTcpPort = 12855;

enum SampleFormat {
  INTERLEAVED_INT16 = 0,
  INTERLEAVED_INT32 = 1,
  INTERLEAVED_FLOAT = 2,
  PLANAR_INT16 = 3,
  PLANAR_INT32 = 4,
  PLANAR_FLOAT = 5,
};

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
