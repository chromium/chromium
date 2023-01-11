// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/media_pipeline_buffer_extension.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
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

MediaPipelineBufferExtension::PendingCommand::PendingCommand(
    scoped_refptr<DecoderBufferBase> buf)
    : buffer(std::move(buf)) {}

MediaPipelineBufferExtension::PendingCommand::PendingCommand(
    const AudioConfig& cfg)
    : config(cfg) {}

MediaPipelineBufferExtension::PendingCommand::PendingCommand(
    const PendingCommand& other) = default;
MediaPipelineBufferExtension::PendingCommand::PendingCommand(
    PendingCommand&& other) = default;

MediaPipelineBufferExtension::PendingCommand::~PendingCommand() = default;

MediaPipelineBufferExtension::PendingCommand&
MediaPipelineBufferExtension::PendingCommand::operator=(
    const PendingCommand& other) = default;
MediaPipelineBufferExtension::PendingCommand&
MediaPipelineBufferExtension::PendingCommand::operator=(
    PendingCommand&& other) = default;

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
  return command_queue_.empty();
}

int64_t MediaPipelineBufferExtension::GetBufferDuration() const {
  if (IsBufferEmpty()) {
    return 0;
  }

  DCHECK_GE(last_buffer_pts_, 0);
  DCHECK_GE(most_recent_buffer_pts_, last_buffer_pts_);

  return most_recent_buffer_pts_ - last_buffer_pts_;
}

bool MediaPipelineBufferExtension::IsDelegatedDecoderHealthy() const {
  return delegated_decoder_buffer_status_ != BufferStatus::kBufferFailed &&
         delegated_decoder_set_config_status_;
}

void MediaPipelineBufferExtension::OnPushBufferComplete(BufferStatus status) {
  CheckCalledOnCorrectThread();

  delegated_decoder_buffer_status_ = status;

  // If the buffer was full and the call failed, inform the caller via callback
  // per method contract.
  if (status == BufferStatus::kBufferFailed && IsBufferFull()) {
    DCHECK(!IsBufferEmpty());
    AudioDecoderPipelineNode::OnPushBufferComplete(status);
    return;
  }

  // Dequeue and push buffers to the underlying AudioDecoder until a Pending or
  // Failure signal is returned. Rather than doing this in a loop, it is done
  // by posting sequential tasks to the task runner, to ensure that the current
  // thread is not blocked for the duration of this process.
  SchedulePushToDecoder();
}

bool MediaPipelineBufferExtension::TryPushToDecoder() {
  CheckCalledOnCorrectThread();

  if (IsBufferEmpty()) {
    return true;
  }

  // Pull the front element off this instance's queue and process it.
  const bool is_buffer_full_before_push = IsBufferFull();
  PendingCommand& next_command = command_queue_.front();

  // Only one of the config or buffer may be set.
  DCHECK_NE(next_command.buffer.has_value(), next_command.config.has_value());

  // If the next command in the queue can be processed, do. Else, return true.
  if (next_command.buffer.has_value() &&
      delegated_decoder_buffer_status_ == BufferStatus::kBufferSuccess) {
    delegated_decoder_buffer_status_ =
        PushBufferToDelegatedDecoder(std::move(next_command.buffer.value()));

    // If this queue is no longer blocked, inform the caller per method
    // contract.
    if (is_buffer_full_before_push && !IsBufferFull()) {
      AudioDecoderPipelineNode::OnPushBufferComplete(
          delegated_decoder_buffer_status_ != BufferStatus::kBufferFailed
              ? BufferStatus::kBufferSuccess
              : BufferStatus::kBufferFailed);
    }

    if (delegated_decoder_buffer_status_ == BufferStatus::kBufferFailed) {
      return false;
    }
  } else if (next_command.config.has_value() &&
             delegated_decoder_set_config_status_) {
    delegated_decoder_set_config_status_ =
        AudioDecoderPipelineNode::SetConfig(next_command.config.value());
    if (!delegated_decoder_set_config_status_) {
      return false;
    }
  } else {
    return true;
  }

  // Pop the processed item from the queue and iterate as needed.
  command_queue_.pop();
  if (!IsBufferEmpty()) {
    SchedulePushToDecoder();
  }

  return true;
}

void MediaPipelineBufferExtension::SchedulePushToDecoder() {
  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(base::BindOnce(
      base::IgnoreResult(&MediaPipelineBufferExtension::TryPushToDecoder),
      weak_factory_.GetWeakPtr()));
  task_runner_->PostTask(task, 0);
}

bool MediaPipelineBufferExtension::TryProcessCommand(PendingCommand command) {
  // If the most recent call was a failure, inform the user.
  if (!IsDelegatedDecoderHealthy()) {
    return false;
  }

  // Queue up the item to be processed in the queue. Then, try to push the top
  // item of the queue to the underlying decoder. This may or may not be the
  // item that was just pushed, leading to two cases:
  // - If so, clearly the result of this call provides enough information to
  //   determine the correct response to the user's call.
  // - If not, then this result is true by assumption. But if the push failed,
  //   then the buffer is now in an unhealthy state and false should be
  //   returned.
  command_queue_.push(std::move(command));
  return TryPushToDecoder();
}

CmaBackend::BufferStatus MediaPipelineBufferExtension::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  CheckCalledOnCorrectThread();

  if (IsBufferFull()) {
    return BufferStatus::kBufferFailed;
  }

  most_recent_buffer_pts_ = buffer->timestamp();

  if (!TryProcessCommand(PendingCommand(std::move(buffer)))) {
    return BufferStatus::kBufferFailed;
  }

  return IsBufferFull() ? BufferStatus::kBufferPending
                        : BufferStatus::kBufferSuccess;
}

bool MediaPipelineBufferExtension::SetConfig(const AudioConfig& config) {
  CheckCalledOnCorrectThread();
  return TryProcessCommand(PendingCommand(config));
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
