// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/push_buffer_pending_handler.h"

#include <utility>

#include "base/functional/callback.h"
#include "chromecast/media/cma/backend/proxy/push_buffer_queue.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/task_runner.h"

namespace chromecast {
namespace media {
namespace {

constexpr uint64_t kDelayBetweenDataPushAttemptsInMs = 10;

}  // namespace

PushBufferPendingHandler::PushBufferPendingHandler(
    TaskRunner* task_runner,
    AudioChannelPushBufferHandler::Client* client)
    : PushBufferPendingHandler(task_runner,
                               client,
                               std::make_unique<PushBufferQueue>()) {}

PushBufferPendingHandler::PushBufferPendingHandler(
    TaskRunner* task_runner,
    AudioChannelPushBufferHandler::Client* client,
    std::unique_ptr<AudioChannelPushBufferHandler> handler)
    : delegated_handler_(std::move(handler)),
      client_(client),
      task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(delegated_handler_.get());
  DCHECK(client_);
  DCHECK(task_runner_);
}

PushBufferPendingHandler::~PushBufferPendingHandler() = default;

CmaBackend::BufferStatus PushBufferPendingHandler::PushBuffer(
    const PushBufferRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try and push the data to |delegated_handler_|.
  if (!IsCallPending()) {
    auto delegated_result = delegated_handler_->PushBuffer(request);
    if (delegated_result != CmaBackend::BufferStatus::kBufferFailed) {
      return delegated_result;
    }
  }

  // Store the request to be processed later, and schedule that processing if it
  // isn't scheduled already.
  const bool had_data = IsCallPending();

  if (request.has_buffer()) {
    DCHECK(!has_push_buffer_queued_);
    has_push_buffer_queued_ = true;
  }

  pushed_data_.push(std::move(request));

  if (!had_data) {
    ScheduleDataPush();
  }

  return CmaBackend::BufferStatus::kBufferPending;
}

bool PushBufferPendingHandler::HasBufferedData() const {
  // The pending data is only considered by the producer sequence, so this
  // consumer sequence call does not consider it.
  return delegated_handler_->HasBufferedData();
}

std::optional<AudioChannelPushBufferHandler::PushBufferRequest>
PushBufferPendingHandler::GetBufferedData() {
  // The pending data is only considered by the producer sequence, so this
  // consumer sequence call does not consider it.
  return delegated_handler_->GetBufferedData();
}

bool PushBufferPendingHandler::IsCallPending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return !pushed_data_.empty();
}

void PushBufferPendingHandler::TryPushPendingData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Process as much
  for (auto push_result = CmaBackend::BufferStatus::kBufferSuccess;
       push_result == CmaBackend::BufferStatus::kBufferSuccess &&
       IsCallPending();) {
    push_result = delegated_handler_->PushBuffer(pushed_data_.front());

    if (push_result != CmaBackend::BufferStatus::kBufferFailed) {
      // If we removed the buffer containing data from a PushBuffer() call,
      // update the associated variable.
      if (pushed_data_.front().has_buffer()) {
        DCHECK(has_push_buffer_queued_);
        has_push_buffer_queued_ = false;
      }

      // If we didn't fail, pop the front element.
      pushed_data_.pop();
    }
  }

  if (IsCallPending()) {
    ScheduleDataPush();
  } else {
    client_->OnAudioChannelPushBufferComplete(
        CmaBackend::BufferStatus::kBufferSuccess);
  }
}

void PushBufferPendingHandler::ScheduleDataPush() {
  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(
      base::BindOnce(&PushBufferPendingHandler::TryPushPendingData,
                     weak_factory_.GetWeakPtr()));
  task_runner_->PostTask(task, kDelayBetweenDataPushAttemptsInMs);
}

}  // namespace media
}  // namespace chromecast
