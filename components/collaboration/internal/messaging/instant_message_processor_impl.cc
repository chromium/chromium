// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"

#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging {
namespace {
constexpr base::TimeDelta kQueueInterval = base::Seconds(1);
}  // namespace

InstantMessageProcessorImpl::InstantMessageProcessorImpl() = default;

InstantMessageProcessorImpl::~InstantMessageProcessorImpl() = default;

void InstantMessageProcessorImpl::SetMessagingBackendService(
    MessagingBackendService* messaging_backend_service) {
  CHECK(!messaging_backend_service_);
  CHECK(messaging_backend_service);
  messaging_backend_service_ = messaging_backend_service;
}

void InstantMessageProcessorImpl::SetInstantMessageDelegate(
    InstantMessageDelegate* instant_message_delegate) {
  // We must be either setting a delegate where there was none before or
  // we should be resetting a non-null delegate.
  CHECK((instant_message_delegate_ == nullptr &&
         instant_message_delegate != nullptr) ||
        (instant_message_delegate_ != nullptr &&
         instant_message_delegate == nullptr));
  instant_message_delegate_ = instant_message_delegate;
}

bool InstantMessageProcessorImpl::IsEnabled() const {
  return !!instant_message_delegate_;
}

void InstantMessageProcessorImpl::DisplayInstantMessage(
    const InstantMessage& message) {
  message_queue_.push_back(message);
  ScheduleProcessing();
}

void InstantMessageProcessorImpl::ScheduleProcessing() {
  if (processing_scheduled_) {
    return;
  }
  processing_scheduled_ = true;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstantMessageProcessorImpl::ProcessQueue,
                     weak_ptr_factory_.GetWeakPtr()),
      kQueueInterval);
}

void InstantMessageProcessorImpl::ProcessQueue() {
  processing_scheduled_ = false;
  std::vector<InstantMessage> aggregated_messages =
      AggregateMessages(message_queue_);

  for (const auto& message : aggregated_messages) {
    // TODO(crbug.com/400794347): Aggregate message ids for callback.
    CHECK(message.attribution.has_value() &&
          message.attribution->id.has_value());
    std::vector<base::Uuid> message_ids{message.attribution->id.value()};
    instant_message_delegate_->DisplayInstantaneousMessage(
        message,
        base::BindOnce(&InstantMessageProcessorImpl::OnMessageDisplayed,
                       weak_ptr_factory_.GetWeakPtr(), message_ids));
  }

  message_queue_.clear();
}

std::vector<InstantMessage> InstantMessageProcessorImpl::AggregateMessages(
    const std::vector<InstantMessage>& messages) {
  // TODO(crbug.com/400794347): Aggregate messages.
  return messages;
}

void InstantMessageProcessorImpl::OnMessageDisplayed(
    const std::vector<base::Uuid>& message_ids,
    bool success) {
  if (!success) {
    return;
  }

  for (const base::Uuid& message_id : message_ids) {
    messaging_backend_service_->ClearPersistentMessage(
        message_id, PersistentNotificationType::INSTANT_MESSAGE);
  }
}

}  // namespace collaboration::messaging
