// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_BROKER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_BROKER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.pb.h"
#include "third_party/protobuf/src/google/protobuf/duration.pb.h"

namespace chromecast {

class TaskRunner;

namespace media {
// Exposes the CastAudioDecoderChannel RPC client with a callback-based API.
//
// This class defines a thin abstraction on top of direct gRPC calls, with the
// intention of avoiding any dependency at higher layers on a specific gRPC
// implementation (e.g. gRPC Manager, vanilla gRPC< etc). Objects, enums, and
// values defined in |cast_audio_decoder_service.proto| will be exposed to
// layers above this class, but the specifics of any gRPC Implementation will
// not.
//
// NOTE: Implementations of this interface make NO assumptions either about the
// thread on which calls are made OR on which callbacks in |Handler| are called.
// Any such requirements must be enforced at higher layers.
class CastRuntimeAudioChannelBroker {
 public:
  using CastAudioDecoderMode = cast::media::CastAudioDecoderMode;
  using TimestampInfo = cast::media::TimestampInfo;

  // The valid response types for a gRPC Call.
  enum class StatusCode {
    kOk = 0,
    kCancelled = 1,
    kUnknown = 2,
    kInvalidArgument = 3,
    kDeadlineExceeded = 4,
    kNotFound = 5,
    kAlreadyExists = 6,
    kPermissionDenied = 7,
    kUnauthenticated = 16,
    kResourceExhausted = 8,
    kFailedPrecondition = 9,
    kAborted = 10,
    kOutOfRange = 11,
    kUnimplemented = 12,
    kInternal = 13,
    kUnavailable = 14,
    kDataLoss = 15,
    kDoNotUse = -1,
  };

  // Callbacks associated with RPC communication done by gRPC.
  //
  // The thread on which these callbacks are called is intentionally unspecified
  // by this interface. It is the responsibility of the caller to "jump" to
  // specific threads as required.
  class Handler {
   public:
    using MediaTime = cast::media::MediaTime;
    using PipelineState = cast::media::PipelineState;
    using PushBufferRequest = cast::media::PushBufferRequest;

    virtual ~Handler();

    // Returns a PushBufferRequest to be sent across the |PushBuffer| RPC. May
    // only be called if |HasBufferedData()| is true.
    virtual std::optional<PushBufferRequest> GetBufferedData() = 0;
    virtual bool HasBufferedData() = 0;

    // Handlers for the responses of the messages defined in
    // cast_audio_decoder_service.proto.
    //
    // Called when a |Initialize| RPC call completes. In the case of a
    // successful call, |status| will be kOK.
    virtual void HandleInitializeResponse(StatusCode status) = 0;

    // Called when a |GetMediaTime| RPC call completes. If |status| is kOK, then
    // |state| is the current pipeline state. Else, the call failed and |state|
    // is undefined.
    virtual void HandleStateChangeResponse(PipelineState state,
                                           StatusCode status) = 0;

    // Called when a |SetVolume| RPC call completes. In the case of a
    // successful call, |status| will be kOK.
    virtual void HandleSetVolumeResponse(StatusCode status) = 0;

    // Called when a |SetPlayback| RPC call completes. In the case of a
    // successful call, |status| will be kOK.
    virtual void HandleSetPlaybackResponse(StatusCode status) = 0;

    // Called when a |PushBuffer| RPC call completes. If |status| is kOK, then
    // |decoded_bytes| is the number of bytes which have been decoded so far.
    // Else, the RPC call failed and |decoded_byes| is undefined.
    virtual void HandlePushBufferResponse(int64_t decoded_bytes,
                                          StatusCode status) = 0;

    // Called when a |GetMediaTime| RPC call completes. If |status| is kOK, then
    // |time| is a valid non-empty MediaTime object. Else, |time| may be empty.
    virtual void HandleGetMediaTimeResponse(std::optional<MediaTime> time,
                                            StatusCode status) = 0;
  };

  static std::unique_ptr<CastRuntimeAudioChannelBroker> Create(
      TaskRunner* task_runner,
      Handler* handler);

  virtual ~CastRuntimeAudioChannelBroker();

  // Calls into the underlying RPC.
  virtual void InitializeAsync(const std::string& cast_session_id,
                               CastAudioDecoderMode decoder_mode) = 0;
  virtual void SetVolumeAsync(float multiplier) = 0;
  virtual void SetPlaybackAsync(double playback_rate) = 0;
  virtual void GetMediaTimeAsync() = 0;

  // Calls to StateChangeRequest RPC.
  virtual void StartAsync(int64_t pts_micros, TimestampInfo timestamp_info) = 0;
  virtual void StopAsync() = 0;
  virtual void PauseAsync() = 0;
  virtual void ResumeAsync(TimestampInfo timestamp_info) = 0;
  virtual void UpdateTimestampAsync(TimestampInfo timestamp_info) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CAST_RUNTIME_AUDIO_CHANNEL_BROKER_H_
