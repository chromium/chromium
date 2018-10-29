// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_FACTORY_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_FACTORY_H_

#include <memory>

#include "chromecast/media/audio/mixer_service/mixer_service_connection.h"

namespace chromecast {
namespace media {

class MixerServiceConnectionFactory {
 public:
  MixerServiceConnectionFactory() = default;
  ~MixerServiceConnectionFactory() = default;

  std::unique_ptr<MixerServiceConnection> CreateMixerServiceConnection(
      MixerServiceConnection::Delegate* delegate,
      const mixer_service::MixerStreamParams& params);

  DISALLOW_COPY_AND_ASSIGN(MixerServiceConnectionFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SERVICE_CONNECTION_FACTORY_H_