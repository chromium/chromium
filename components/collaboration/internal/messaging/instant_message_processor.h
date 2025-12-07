// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_H_

#include <set>

#include "base/uuid.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging {
using InstantMessageDelegate = MessagingBackendService::InstantMessageDelegate;

// The `InstantMessageProcessor` is an interface for a class that manages the
// queueing, aggregation, and display of instant messages.
//
// This class is responsible for:
// - Holding and managing a queue of pending instant messages.
// - Aggregating similar messages to create concise and informative
//   notifications.
// - Interacting with an `InstantMessageDelegate` to render the messages on the
//   UI.
// - Handling success callbacks from the delegate to clear messages from the
//   queue once they have been successfully displayed.
class InstantMessageProcessor {
 public:
  virtual ~InstantMessageProcessor() = default;

  // Must be invoked after construction.
  virtual void SetMessagingBackendService(
      MessagingBackendService* messaging_backend_service) = 0;

  // Must be invoked to set the platform specific delegate that is
  // responsible for displaying the message in the UI.
  virtual void SetInstantMessageDelegate(
      InstantMessageDelegate* instant_message_delegate) = 0;

  // Whether instant messaging is enabled for the platform yet.
  virtual bool IsEnabled() const = 0;

  // Notifies the InstantMessageDelegate to display the message for all the
  // provided levels.
  virtual void DisplayInstantMessage(const InstantMessage& message) = 0;

  // Notifies the InstantMessageDelegate to hide the messages with the provided
  // IDs.
  virtual void HideInstantMessage(const std::set<base::Uuid>& message_ids) = 0;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_INSTANT_MESSAGE_PROCESSOR_H_
