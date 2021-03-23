// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/media_pipeline_buffer_extension.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/task_runner.h"

namespace chromecast {
namespace media {
namespace {

// TODO(b/180725465): Make this a BUILD flag for embedders to specify.
constexpr int kMicrosecondsDataToBuffer =
    5 * base::Time::kMicrosecondsPerSecond;

}  // namespace

MediaPipelineBufferExtension::MediaPipelineBufferExtension(
    TaskRunner* task_runner,
    CmaBackend::AudioDecoder* delegated_decoder)
    : AudioDecoderPipelineNode(delegated_decoder),
      task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(task_runner_);
}

MediaPipelineBufferExtension::MediaPipelineBufferExtension(
    TaskRunner* task_runner,
    std::unique_ptr<AudioDecoderPipelineNode> owned_delegated_decoder)
    : MediaPipelineBufferExtension(task_runner, owned_delegated_decoder.get()) {
  SetOwnedDecoder(std::move(owned_delegated_decoder));
}

MediaPipelineBufferExtension::~MediaPipelineBufferExtension() = default;

CmaBackend::BufferStatus
MediaPipelineBufferExtension::PushBufferToDelegatedDecoder(
    scoped_refptr<DecoderBufferBase> buffer) {
  last_buffer_pts_ = buffer->timestamp();
  return AudioDecoderPipelineNode::PushBuffer(std::move(buffer));
}

bool MediaPipelineBufferExtension::IsBufferFull() const {
  CheckCalledOnCorrectThread();
  return GetBufferDuration() >= kMicrosecondsDataToBuffer;
}

bool MediaPipelineBufferExtension::IsBufferEmpty() const {
  CheckCalledOnCorrectThread();
  return buffer_queue_.empty();
}

int64_t MediaPipelineBufferExtension::GetBufferDuration() const {
  if (IsBufferEmpty()) {
    return 0;
  }

  DCHECK_GE(last_buffer_pts_, 0);
  return buffer_queue_.back()->timestamp() - last_buffer_pts_;
}

void MediaPipelineBufferExtension::OnPushBufferComplete(BufferStatus status) {
  CheckCalledOnCorrectThread();

  // If the buffer was full and the call failed, inform the caller via callback
  // per method contract.
  if (status == BufferStatus::kBufferFailed && IsBufferFull()) {
    DCHECK(!IsBufferEmpty());
    AudioDecoderPipelineNode::OnPushBufferComplete(status);
    delegated_decoder_buffer_status_ = status;
    return;
  }

  // Else if there is no more work to do, return.
  if (status == BufferStatus::kBufferFailed || IsBufferEmpty()) {
    delegated_decoder_buffer_status_ = status;
    return;
  }

  // Dequeue and push buffers to the underlying AudioDecoder until a Pending or
  // Failure signal is returned. Rather than doing this in a loop, it is done
  // by posting sequential tasks to the task runner, to ensure that the current
  // thread is not blocked for the duration of this process.
  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(base::BindOnce(
      &MediaPipelineBufferExtension::PushToDecoderAfterPushBufferComplete,
      weak_factory_.GetWeakPtr()));

  // |task_runner_| takes ownership of |task|.
  task_runner_->PostTask(task, 0);
}

void MediaPipelineBufferExtension::PushToDecoderAfterPushBufferComplete() {
  CheckCalledOnCorrectThread();

  // Pull the front element off this instance's queue and process it.
  const bool is_buffer_full_before_push = IsBufferFull();
  const BufferStatus new_status =
      PushBufferToDelegatedDecoder(std::move(buffer_queue_.front()));
  buffer_queue_.pop();

  // If this queue is no longer blocked, inform the caller per method contract.
  if (is_buffer_full_before_push && !IsBufferFull()) {
    AudioDecoderPipelineNode::OnPushBufferComplete(
        new_status != BufferStatus::kBufferFailed
            ? BufferStatus::kBufferSuccess
            : BufferStatus::kBufferFailed);
  }

  // If the delegated decoder can't handle more data or there is no more data to
  // push, exit. Else, recurse.
  if (IsBufferEmpty() || new_status != BufferStatus::kBufferSuccess) {
    delegated_decoder_buffer_status_ = new_status;
  } else {
    auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(base::BindOnce(
        &MediaPipelineBufferExtension::PushToDecoderAfterPushBufferComplete,
        weak_factory_.GetWeakPtr()));
    task_runner_->PostTask(task, 0);
  }
}

CmaBackend::BufferStatus MediaPipelineBufferExtension::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  CheckCalledOnCorrectThread();

  // If the most recent call was a failure, inform the user.
  if (delegated_decoder_buffer_status_ == BufferStatus::kBufferFailed) {
    return BufferStatus::kBufferFailed;
  }

  // If the underlying decoder does not have pending data, push there directly.
  if (delegated_decoder_buffer_status_ == BufferStatus::kBufferSuccess) {
    delegated_decoder_buffer_status_ =
        PushBufferToDelegatedDecoder(std::move(buffer));
    return delegated_decoder_buffer_status_ == BufferStatus::kBufferFailed
               ? BufferStatus::kBufferFailed
               : BufferStatus::kBufferSuccess;
  }

  // Else, queue up the data for later processing.
  buffer_queue_.push(std::move(buffer));
  return IsBufferFull() ? BufferStatus::kBufferPending
                        : BufferStatus::kBufferSuccess;
}

CmaBackend::AudioDecoder::RenderingDelay
MediaPipelineBufferExtension::GetRenderingDelay() {
  CheckCalledOnCorrectThread();

  auto delegated_decoder_delay = AudioDecoderPipelineNode::GetRenderingDelay();
  delegated_decoder_delay.delay_microseconds += GetBufferDuration();
  return delegated_decoder_delay;
}

}  // namespace media
}  // namespace chromecast
