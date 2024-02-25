// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_PENDING_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_PENDING_HANDLER_H_

#include <memory>
#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/audio_channel_push_buffer_handler.h"
#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

namespace chromecast {

class TaskRunner;

namespace media {

// Utility to queue up PushBuffer() and SetConfig() calls to be made
// asynchronously at a later time, when a synchronous call is not possible.
//
// Per the CmaBackend::Decoder::PushBuffer() method signature, an implementing
// class must continue to accept new buffers until returning the kBufferPending
// response, which then acts as a flow control mechanism. In some
// implementations of AudioChannelPushBufferHandler, kBufferPending is not a
// possible response code, so this class acts as a utility to add that
// functionality. Upon a failed PushBuffer call to the delegated instance,
// this class will call back asynchronously, either resulting in a successful
// push or retrying again at a later time.
//
// This class stores:
// - All AudioConfigs set while the underlying AudioChannelPushBufferHandler is
//   blocked.
// - A single PushBuffer call associated with the kBufferPending return value.
// Once any data is stored, it queues up a task on the task runner to try to
// call the associated function at a later time.
class PushBufferPendingHandler : public AudioChannelPushBufferHandler {
 public:
  // |task_runner| and |client| must persist for the lifetime of this instance.
  PushBufferPendingHandler(TaskRunner* task_runner,
                           AudioChannelPushBufferHandler::Client* client);
  ~PushBufferPendingHandler() override;

  // Returns whether or not this instance contains any data waiting to be sent
  // at a later point.
  bool IsCallPending() const;

  // AudioChannelPushBufferHandler overrides.
  //
  // Note that the pending data is considered only for the PushBuffer() call, as
  // it is only understood by the PRODUCER sequence. The CONSUMER sequence is
  // not aware of its existence, so the remaining methods will not consider it.
  CmaBackend::BufferStatus PushBuffer(
      const PushBufferRequest& request) override;
  bool HasBufferedData() const override;
  std::optional<PushBufferRequest> GetBufferedData() override;

 private:
  friend class PushBufferPendingHandlerTest;

  PushBufferPendingHandler(
      TaskRunner* task_runner,
      AudioChannelPushBufferHandler::Client* client,
      std::unique_ptr<AudioChannelPushBufferHandler> handler);

  // Attempts to push any pending data to |client_|, scheduling another call to
  // itself if any further pending data remains.
  void TryPushPendingData();

  // Schedules a call to TryPushPendingData() in the future.
  void ScheduleDataPush();

  // The AudioChannelPushBufferHandler to which calls to this instance's methods
  // should be delegated.
  std::unique_ptr<AudioChannelPushBufferHandler> delegated_handler_;

  // Client to use for callbacks.
  AudioChannelPushBufferHandler::Client* client_;

  // All data which has been pushed to this instance and not yet processed by
  // |delegated_handler_|.
  std::queue<PushBufferRequest> pushed_data_;

  // Signifies whether there is a buffer containing data from a PushBuffer()
  // call in the queue (of which there may only be one).
  bool has_push_buffer_queued_ = false;

  TaskRunner* const task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PushBufferPendingHandler> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_PENDING_HANDLER_H_
