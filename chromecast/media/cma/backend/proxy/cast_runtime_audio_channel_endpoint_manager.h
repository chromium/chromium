// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_ENDPOINT_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_ENDPOINT_MANAGER_H_

#include <string>

namespace chromecast {
namespace media {

// Provides a global accessor for the endpoint to which the
// CastMediaAudioChannel should connect.
class CastRuntimeAudioChannelEndpointManager {
 public:
  // Returns the singleton instance of this class.
  static CastRuntimeAudioChannelEndpointManager* Get();

  // Returns the endpoint as described above.
  virtual const std::string& GetAudioChannelEndpoint() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_ENDPOINT_MANAGER_H_
