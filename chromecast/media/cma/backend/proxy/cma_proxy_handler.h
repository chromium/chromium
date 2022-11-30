// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_PROXY_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_PROXY_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/backend/proxy/audio_channel_push_buffer_handler.h"
#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

namespace chromecast {

class TaskRunner;

namespace media {

struct AudioConfig;

class CmaProxyHandler {
 public:
  // The mode in which Cast Core should operate.
  enum class AudioDecoderOperationMode {
    // Both multiroom and audio rendering is enabled.
    kAll = 0,

    // Only multiroom is enabled and audio rendering is disabled.  This should
    // be used if the runtime is taking over responsibility for rendering audio.
    kMultiroomOnly = 1,

    // Only audio rendering is enabled and multiroom is disabled.
    kAudioOnly = 2
  };

  // The current state of the remote CMA backend.
  enum class PipelineState {
    kUninitialized = 0,
    kStopped = 1,
    kPlaying = 2,
    kPaused = 3,
  };

  // Observer for changes on the remote client.
  class Client {
   public:
    virtual ~Client() = default;

    // Called when an error occurs upon calling any gRPC Method.
    virtual void OnError() = 0;

    // Called when the Start(), Stop(), Pause(), or Resume() methods
    // successfully change the current pipeline state.
    virtual void OnPipelineStateChange(PipelineState state) = 0;

    // Called following the successful processing of a batch of PushBuffer
    // calls.
    virtual void OnBytesDecoded(int64_t decoded_byte_count) = 0;
  };

  virtual ~CmaProxyHandler() = default;

  // Create a new implementation-specific CmaProxyHandler. Each provided
  // object must exist for the duration of the created instance's lifetime, and
  // all callbacks for |client| will be called on |task_runner|.
  static std::unique_ptr<CmaProxyHandler> Create(
      TaskRunner* task_runner,
      Client* client,
      AudioChannelPushBufferHandler::Client* push_buffer_client);

  // Calls to the corresponding gRPC Methods. These functions may be called from
  // any thread.
  virtual void Initialize(const std::string& cast_session_id,
                          AudioDecoderOperationMode decoder_mode) = 0;
  virtual void Start(
      int64_t start_pts,
      const BufferIdManager::TargetBufferInfo& target_buffer) = 0;
  virtual void Stop() = 0;
  virtual void Pause() = 0;
  virtual void Resume(
      const BufferIdManager::TargetBufferInfo& target_buffer) = 0;
  virtual void SetPlaybackRate(float rate) = 0;
  virtual void SetVolume(float multiplier) = 0;
  virtual void UpdateTimestamp(
      const BufferIdManager::TargetBufferInfo& target_buffer) = 0;

  // Push the provided data or config to a queue, for processing at a later
  // point when resources are available. Returns true if the data was
  // successfully pushed to the queue and false otherwise. These functions may
  // be called from any thread.
  //
  // NOTES:
  // - SetConfig is expected to be called prior to any PushBuffer calls.
  // - SetConfig may be called later on as-well, after which time the new config
  //   will be used for all following PushBuffer calls.
  virtual bool SetConfig(const AudioConfig& config) = 0;
  virtual CmaBackend::BufferStatus PushBuffer(
      scoped_refptr<DecoderBufferBase> buffer,
      BufferIdManager::BufferId buffer_id) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_PROXY_HANDLER_H_
