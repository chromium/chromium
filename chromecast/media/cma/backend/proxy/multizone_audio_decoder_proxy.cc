// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy.h"

#include "base/notreached.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

void MultizoneAudioDecoderProxy::SetDelegate(Delegate* delegate) {
  NOTREACHED();
}

MultizoneAudioDecoderProxy::BufferStatus MultizoneAudioDecoderProxy::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  NOTREACHED();
  return BufferStatus::kBufferSuccess;
}

bool MultizoneAudioDecoderProxy::SetConfig(const AudioConfig& config) {
  NOTREACHED();
  return true;
}

bool MultizoneAudioDecoderProxy::SetVolume(float multiplier) {
  NOTREACHED();
  return true;
}

MultizoneAudioDecoderProxy::RenderingDelay
MultizoneAudioDecoderProxy::GetRenderingDelay() {
  NOTREACHED();
  return RenderingDelay{};
}

void MultizoneAudioDecoderProxy::GetStatistics(Statistics* statistics) {
  NOTREACHED();
}

bool MultizoneAudioDecoderProxy::RequiresDecryption() {
  NOTREACHED();
  return true;
}

void MultizoneAudioDecoderProxy::SetObserver(Observer* observer) {
  NOTREACHED();
}

}  // namespace media
}  // namespace chromecast
