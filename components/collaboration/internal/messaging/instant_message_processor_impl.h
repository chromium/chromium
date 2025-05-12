// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/instant_message_processor.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging {
using InstantMessageDelegate = MessagingBackendService::InstantMessageDelegate;

class InstantMessageProcessorImpl : public InstantMessageProcessor {
 public:
  InstantMessageProcessorImpl();
  ~InstantMessageProcessorImpl() override;

  // InstantMessageProcessor implementation.
  void SetMessagingBackendService(
      MessagingBackendService* messaging_backend_service) override;
  void SetInstantMessageDelegate(
      InstantMessageDelegate* instant_message_delegate) override;
  bool IsEnabled() const override;
  void DisplayInstantMessage(const InstantMessage& message) override;
  void HideInstantMessage(const std::set<base::Uuid>& message_ids) override;

 private:
  void ScheduleProcessing();
  void ProcessQueue();
  std::vector<InstantMessage> AggregateMessages(
      const std::vector<InstantMessage>& messages);
  void OnMessageDisplayed(const std::vector<base::Uuid>& message_ids,
                          bool success);
  InstantMessage CreateAggregatedMessage(
      const std::vector<InstantMessage>& messages);

  // The messaging backend service. Used for clearing the message from DB when
  // the message successfully shows in the UI.
  raw_ptr<MessagingBackendService> messaging_backend_service_;

  // The single delegate for when we need to inform the UI about instant
  // (one-off) messages.
  raw_ptr<InstantMessageDelegate> instant_message_delegate_;

  std::vector<InstantMessage> message_queue_;
  bool processing_scheduled_ = false;

  base::WeakPtrFactory<InstantMessageProcessorImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_IMPL_H_
