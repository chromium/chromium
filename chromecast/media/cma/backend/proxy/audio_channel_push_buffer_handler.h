// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_PUSH_BUFFER_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_PUSH_BUFFER_HANDLER_H_

#include <optional>

#include "chromecast/media/api/cma_backend.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.pb.h"

namespace chromecast {
namespace media {

// This class is responsible for buffering both DecoderBuffer and AudioConfig
// data, which are pushed together over gRPC using the PushData() API call.
// Two sequences are expected to access this object:
// - A PRODUCER sequence, which will push new data in.
// - A CONSUMER sequence which will pull this data back out.
class AudioChannelPushBufferHandler {
 public:
  // NOTE: This is needed to avoid introducing extra DEPS-file dependencies on
  // third_party/openscreen.
  using PushBufferRequest = cast::media::PushBufferRequest;

  class Client {
   public:
    // Called following a PushBuffer() call which returns kBufferPending, to
    // provide the result of that call and signal that this instance is ready
    // to handle more PushBuffer calls.
    virtual void OnAudioChannelPushBufferComplete(
        CmaBackend::BufferStatus status) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~AudioChannelPushBufferHandler() = default;

  // Writes the next available PushBufferRequest to be processed. Return values
  // are as follows:
  // - kBufferSuccess: The data has been processed successfully
  // - kBufferPending: The data will be processed at a later time. Do not call
  //   this method again until the associated client has provided a callback
  //   with type kBufferSuccess.
  // - kBufferFailed: The data was NOT processed successfully, but the pipeline
  //   remains in a healthy state and you may try again at a later point
  // Note that no call to this method will ever put the implementation into an
  // undefined or unhealthy state.
  //
  // May only be called by the PRODUCER.
  virtual CmaBackend::BufferStatus PushBuffer(
      const PushBufferRequest& request) = 0;

  // Returns true if there is data available for reading.
  //
  // May only be called by the CONSUMER.
  virtual bool HasBufferedData() const = 0;

  // Attempts to read the next available PushBufferRequest, returning the
  // instance on success and empty. May only be called when HasBufferedData() is
  // true.
  //
  // May only be called by the CONSUMER.
  virtual std::optional<PushBufferRequest> GetBufferedData() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_CHANNEL_PUSH_BUFFER_HANDLER_H_
