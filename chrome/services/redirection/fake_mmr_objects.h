// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REDIRECTION_FAKE_MMR_OBJECTS_H_
#define CHROME_SERVICES_REDIRECTION_FAKE_MMR_OBJECTS_H_

#include <wrl/client.h>
#include <wrl/implements.h>

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "third_party/microsoft_multimedia_redirection/src/api/Mmr_h.h"

namespace redirection {

template <typename... Interfaces>
using FakeComObject = Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    Interfaces...>;

// Implements both session interfaces, as the real MMR session object does:
// IMMRSessionOpenscreen is reachable only by QueryInterface from an
// IMMRSession.
class FakeMmrSession final
    : public FakeComObject<IMMRSession, IMMRSessionOpenscreen> {
 public:
  FakeMmrSession();
  ~FakeMmrSession() override;

  const std::vector<std::vector<uint8_t>>& sent_messages() const {
    return sent_messages_;
  }
  IMMRResponseHandler* response_handler() { return response_handler_.Get(); }

  void set_send_message_result(HRESULT result) {
    send_message_result_ = result;
  }
  void set_response_handler_result(HRESULT result) {
    response_handler_result_ = result;
  }

  // Delivers `message` to whatever handler is currently registered.
  void SendResponse(base::span<uint8_t> message);

  // IMMRSession implementation.
  IFACEMETHODIMP GetSupportedCodecCount(UINT32* count) override;
  IFACEMETHODIMP GetSupportedCodec(UINT32 index, LPWSTR* mime_type) override;
  IFACEMETHODIMP CreateAudioStream(MMR_AUDIO_DECODER_CONFIG config,
                                   BYTE* extra_data,
                                   UINT32 extra_data_size,
                                   IMMRStream** stream) override;
  IFACEMETHODIMP CreateVideoStream(MMR_VIDEO_DECODER_CONFIG config,
                                   BYTE* extra_data,
                                   UINT32 extra_data_size,
                                   IMMRStream** stream) override;
  IFACEMETHODIMP BeginRemoting() override;
  IFACEMETHODIMP SetEventHandler(IMMREventHandler* handler) override;
  IFACEMETHODIMP Play() override;
  IFACEMETHODIMP Pause() override;
  IFACEMETHODIMP SetCurrentTime(LONGLONG timestamp) override;
  IFACEMETHODIMP SetPlaybackRate(double rate) override;
  IFACEMETHODIMP SetLoop(BOOL loop) override;
  IFACEMETHODIMP SetVolume(double volume) override;
  IFACEMETHODIMP GetExtendedStats(MMR_EXTENDED_STATS* stats) override;
  IFACEMETHODIMP GetVideoReplacementImage(BYTE* image_data,
                                          UINT32 width,
                                          UINT32 height,
                                          UINT32 stride) override;

  // IMMRSessionOpenscreen implementation.
  IFACEMETHODIMP CreateStream(UINT32 stream_id, IMMRStream** stream) override;
  IFACEMETHODIMP SetResponseHandler(IMMRResponseHandler* handler) override;
  IFACEMETHODIMP SendRawOpenscreenMessage(BYTE* message,
                                          UINT32 message_size) override;

 private:
  std::vector<std::vector<uint8_t>> sent_messages_;
  Microsoft::WRL::ComPtr<IMMRResponseHandler> response_handler_;
  HRESULT send_message_result_ = S_OK;
  HRESULT response_handler_result_ = S_OK;
};

// Records everything written to one elementary stream.
class FakeMmrStream final : public FakeComObject<IMMRStream> {
 public:
  struct Frame {
    std::vector<uint8_t> data;
    int64_t timestamp_hns = 0;
    int64_t duration_hns = 0;
    bool is_key_frame = false;
  };

  FakeMmrStream();
  ~FakeMmrStream() override;

  const std::vector<Frame>& frames() const { return frames_; }
  int end_of_stream_count() const { return end_of_stream_count_; }

  // Sets the HRESULT returned by AppendBuffer() and EndOfStream().
  void set_result(HRESULT result) { result_ = result; }

  // IMMRStream implementation.
  IFACEMETHODIMP AppendBuffer(BYTE* data,
                              UINT32 data_size,
                              LONGLONG timestamp,
                              LONGLONG duration,
                              BOOL is_key_frame) override;
  IFACEMETHODIMP EndOfStream() override;
  IFACEMETHODIMP SetVideoRect(HWND parent,
                              UINT32 x,
                              UINT32 y,
                              UINT32 width,
                              UINT32 height) override;
  IFACEMETHODIMP GetStats(MMR_STREAM_STATS* stats) override;

 private:
  std::vector<Frame> frames_;
  int end_of_stream_count_ = 0;
  HRESULT result_ = S_OK;
};

}  // namespace redirection

#endif  // CHROME_SERVICES_REDIRECTION_FAKE_MMR_OBJECTS_H_
