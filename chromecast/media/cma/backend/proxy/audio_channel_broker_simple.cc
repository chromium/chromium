// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h"

#include "base/notreached.h"

namespace chromecast {
namespace media {

// static
std::unique_ptr<CastRuntimeAudioChannelBroker>
CastRuntimeAudioChannelBroker::Create(
    CastRuntimeAudioChannelBroker::Handler* handler) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
