// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_

#include <memory>
#include <queue>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
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
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  RenderingDelay GetRenderingDelay() override;

 private:
  // AudioDecoderPipelineNode overrides.
  void OnPushBufferComplete(BufferStatus status) override;

  // Pushes the provided |buffer| to |delegated_decoder_|.
  BufferStatus PushBufferToDelegatedDecoder(
      scoped_refptr<DecoderBufferBase> buffer);

  // Returns the total amount of playback time stored in this instance.
  int64_t GetBufferDuration() const;

  // Helper method to be posted to the task runner as part of an
  // OnPushBufferComplete call.
  void PushToDecoderAfterPushBufferComplete();

  // PushBuffer data which has been queued locally but has not yet been
  // processed by the delegated decoder.
  std::queue<scoped_refptr<DecoderBufferBase>> buffer_queue_;

  // The playback timestamp (PTS) of the last buffer pushed to the underlying
  // decoder. Defaults to invalid state -1.
  int64_t last_buffer_pts_ = -1;

  // Holds the result of the last call to delegated decoder's PushBuffer
  // queue for which additional calls are not queued up (as is done by
  // PushToDecoderAfterPushBufferComplete()).
  BufferStatus delegated_decoder_buffer_status_ = BufferStatus::kBufferSuccess;

  TaskRunner* const task_runner_;

  base::WeakPtrFactory<MediaPipelineBufferExtension> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MEDIA_PIPELINE_BUFFER_EXTENSION_H_
