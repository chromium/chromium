// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h"

#include "base/notreached.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

MultizoneAudioDecoderProxyImpl::MultizoneAudioDecoderProxyImpl() = default;

MultizoneAudioDecoderProxyImpl::~MultizoneAudioDecoderProxyImpl() = default;

bool MultizoneAudioDecoderProxyImpl::Initialize() {
  NOTIMPLEMENTED();
  return true;
}

bool MultizoneAudioDecoderProxyImpl::Start(int64_t start_pts) {
  NOTIMPLEMENTED();
  return true;
}

void MultizoneAudioDecoderProxyImpl::Stop() {
  NOTIMPLEMENTED();
}

bool MultizoneAudioDecoderProxyImpl::Pause() {
  NOTIMPLEMENTED();
  return true;
}

bool MultizoneAudioDecoderProxyImpl::Resume() {
  NOTIMPLEMENTED();
  return true;
}

bool MultizoneAudioDecoderProxyImpl::SetPlaybackRate(float rate) {
  NOTIMPLEMENTED();
  return true;
}

void MultizoneAudioDecoderProxyImpl::LogicalPause() {
  NOTIMPLEMENTED();
}

void MultizoneAudioDecoderProxyImpl::LogicalResume() {
  NOTIMPLEMENTED();
}

int64_t MultizoneAudioDecoderProxyImpl::GetCurrentPts() const {
  return pts_offset_;
}

void MultizoneAudioDecoderProxyImpl::SetDelegate(Delegate* delegate) {
  NOTIMPLEMENTED();
}

MultizoneAudioDecoderProxy::BufferStatus
MultizoneAudioDecoderProxyImpl::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  NOTIMPLEMENTED();
  return BufferStatus::kBufferSuccess;
}

bool MultizoneAudioDecoderProxyImpl::SetConfig(const AudioConfig& config) {
  NOTIMPLEMENTED();
  return true;
}

bool MultizoneAudioDecoderProxyImpl::SetVolume(float multiplier) {
  NOTIMPLEMENTED();
  return true;
}

MultizoneAudioDecoderProxyImpl::RenderingDelay
MultizoneAudioDecoderProxyImpl::GetRenderingDelay() {
  NOTIMPLEMENTED();
  return RenderingDelay{};
}

void MultizoneAudioDecoderProxyImpl::GetStatistics(Statistics* statistics) {
  NOTIMPLEMENTED();
}

bool MultizoneAudioDecoderProxyImpl::RequiresDecryption() {
  NOTIMPLEMENTED();
  return true;
}

void MultizoneAudioDecoderProxyImpl::SetObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

}  // namespace media
}  // namespace chromecast
