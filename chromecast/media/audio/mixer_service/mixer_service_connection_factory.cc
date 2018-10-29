// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_service_connection_factory.h"

namespace chromecast {
namespace media {

std::unique_ptr<MixerServiceConnection>
MixerServiceConnectionFactory::CreateMixerServiceConnection(
    MixerServiceConnection::Delegate* delegate,
    const mixer_service::MixerStreamParams& params) {
  return std::make_unique<media::MixerServiceConnection>(delegate, params);
}

}  // namespace media
}  // namespace chromecast
