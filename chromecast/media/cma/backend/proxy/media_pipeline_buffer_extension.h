// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_

#include <memory>
#include <optional>
#include <queue>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/backend/proxy/audio_decoder_pipeline_node.h"
#include "chromecast/public/media/cast_key_status.h"

namespace chromecast {

class TaskRunner;

namespace media {

// This class is intended to act as a layer on top of another CmaBackend,
// increasing the number of PushBuffer frames that may be queued up with
// PushBuffer() calls, while delegating all other functionality directly to the
// CmaBackend on top of which it sits.
class MediaPipelineBufferExtension : public AudioDecoderPipelineNode {
 public:
  // |task_runner| and |delegated_decoder| must persist for the duration of this
  // class's lifetime.
  MediaPipelineBufferExtension(TaskRunner* task_runner,
                               CmaBackend::AudioDecoder* delegated_decoder);
  // Takes ownership of |owned_delegated_decoder|.
  MediaPipelineBufferExtension(
      TaskRunner* task_runner,
      std::unique_ptr<AudioDecoderPipelineNode> owned_delegated_decoder);

  ~MediaPipelineBufferExtension() override;

  bool IsBufferFull() const;
  bool IsBufferEmpty() const;

  // AudioDecoderPipelineNode overrides.
  RenderingDelay GetRenderingDelay() override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;

 private:
  struct PendingCommand {
    // NOTE: Ctors needed to support emplace calls.
    explicit PendingCommand(scoped_refptr<DecoderBufferBase> buf);
    explicit PendingCommand(const AudioConfig& cfg);
    PendingCommand(const PendingCommand& other);
    PendingCommand(PendingCommand&& other);

    ~PendingCommand();

    PendingCommand& operator=(const PendingCommand& other);
    PendingCommand& operator=(PendingCommand&& other);

    std::optional<scoped_refptr<DecoderBufferBase>> buffer;
    std::optional<AudioConfig> config;
  };

  // AudioDecoderPipelineNode overrides.
  void OnPushBufferComplete(BufferStatus status) override;

  // Pushes the provided |buffer| to |delegated_decoder_|.
  BufferStatus PushBufferToDelegatedDecoder(
      scoped_refptr<DecoderBufferBase> buffer);

  // Returns the total amount of playback time stored in this instance.
  int64_t GetBufferDuration() const;

  // Returns whether the last call to either PushBuffer() or SetConfig() on the
  // delegated decoder returned an unhealthy response code.
  bool IsDelegatedDecoderHealthy() const;

  // Helper method to try to push the top item in the |command_queue_| to
  // |delegated_decoder_|. Returns false if the underlying decoder's operation
  // both was called and that call failed, and true in all other cases.
  //
  // If either there is no data to process or no work that can be done at this
  // time, this operation is a no op which returns true.
  bool TryPushToDecoder();

  // Pushes the |command| to |command_queue_| and tries to process it, returning
  // whether or not the delegated decoder is in a healthy state following this
  // process.
  bool TryProcessCommand(PendingCommand command);

  // Schedules the TryPushToDecoder() method to run at a later time on
  // |task_runner_|.
  void SchedulePushToDecoder();

  // PushBuffer and SetConfig data which has been queued locally but has not yet
  // been processed by the delegated decoder.
  std::queue<PendingCommand> command_queue_;

  // The playback timestamp (PTS) of the last buffer pushed to the underlying
  // decoder.
  int64_t last_buffer_pts_ = 0;

  // The playback timestamp (PTS) of the most recently supplied by the caller.
  int64_t most_recent_buffer_pts_ = 0;

  // Holds the result of the last call to delegated decoder's PushBuffer
  // queue for which additional calls are not queued up (as is done by
  // PushToDecoderAfterPushBufferComplete()).
  BufferStatus delegated_decoder_buffer_status_ = BufferStatus::kBufferSuccess;

  // Holds the result of the last call to  delegated decoder's SetConfig method.
  bool delegated_decoder_set_config_status_ = true;

  TaskRunner* const task_runner_;

  base::WeakPtrFactory<MediaPipelineBufferExtension> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_
