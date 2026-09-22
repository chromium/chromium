// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/fake_mmr_objects.h"

#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"

namespace redirection {

FakeMmrSession::FakeMmrSession() = default;

FakeMmrSession::~FakeMmrSession() = default;

void FakeMmrSession::SendResponse(base::span<uint8_t> message) {
  response_handler_->OnResponse(message.data(),
                                static_cast<UINT32>(message.size()));
}

IFACEMETHODIMP FakeMmrSession::SetResponseHandler(
    IMMRResponseHandler* handler) {
  if (FAILED(response_handler_result_)) {
    return response_handler_result_;
  }
  response_handler_ = handler;
  return S_OK;
}

IFACEMETHODIMP FakeMmrSession::SendRawOpenscreenMessage(BYTE* message,
                                                        UINT32 message_size) {
  if (FAILED(send_message_result_)) {
    return send_message_result_;
  }
  // SAFETY: SendRawOpenscreenMessage() documents `message_size` as the number
  // of valid bytes in `message`.
  sent_messages_.push_back(base::ToVector(
      UNSAFE_BUFFERS(base::span(message, size_t{message_size}))));
  return S_OK;
}

IFACEMETHODIMP FakeMmrSession::GetSupportedCodecCount(UINT32* count) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::GetSupportedCodec(UINT32 index,
                                                 LPWSTR* mime_type) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::CreateAudioStream(
    MMR_AUDIO_DECODER_CONFIG config,
    BYTE* extra_data,
    UINT32 extra_data_size,
    IMMRStream** stream) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::CreateVideoStream(
    MMR_VIDEO_DECODER_CONFIG config,
    BYTE* extra_data,
    UINT32 extra_data_size,
    IMMRStream** stream) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::BeginRemoting() {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::SetEventHandler(IMMREventHandler* handler) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::Play() {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::Pause() {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::SetCurrentTime(LONGLONG timestamp) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::SetPlaybackRate(double rate) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::SetLoop(BOOL loop) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::SetVolume(double volume) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::GetExtendedStats(MMR_EXTENDED_STATS* stats) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::GetVideoReplacementImage(BYTE* image_data,
                                                        UINT32 width,
                                                        UINT32 height,
                                                        UINT32 stride) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeMmrSession::CreateStream(UINT32 stream_id,
                                            IMMRStream** stream) {
  return E_NOTIMPL;
}

}  // namespace redirection
