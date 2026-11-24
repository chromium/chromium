// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/pending_outgoing_message.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/sharing_message/sharing_constants.h"

namespace sharing_message {

// static
std::unique_ptr<PendingOutgoingMessage>
PendingOutgoingMessage::CreatePendingMessage(
    std::string message_guid,
    base::TimeDelta time_to_live,
    components_sharing_message::SharingMessage message,
    SendTarget target) {
  CHECK(!message.has_ack_message());
  return base::WrapUnique(new PendingOutgoingMessage(
      std::move(message_guid), time_to_live, std::move(message),
      std::move(target), /*ack_callback=*/SendAckCallback()));
}

// static
std::unique_ptr<PendingOutgoingMessage>
PendingOutgoingMessage::CreatePendingAckMessage(
    components_sharing_message::SharingMessage message,
    SendTarget target,
    SendAckCallback ack_callback) {
  CHECK(message.has_ack_message());
  return base::WrapUnique(new PendingOutgoingMessage(
      /*message_guid=*/std::string(), kSharingAckMessageTTL, std::move(message),
      std::move(target), std::move(ack_callback)));
}

// static
components_sharing_message::SharingMessage PendingOutgoingMessage::TakeMessage(
    std::unique_ptr<PendingOutgoingMessage> pending_message) {
  CHECK(pending_message);
  return std::move(pending_message->message_);
}

PendingOutgoingMessage::PendingOutgoingMessage(
    std::string message_guid,
    base::TimeDelta time_to_live,
    components_sharing_message::SharingMessage message,
    SendTarget target,
    SendAckCallback ack_callback)
    : message_guid_(std::move(message_guid)),
      time_to_live_(time_to_live),
      message_(std::move(message)),
      target_(std::move(target)),
      ack_callback_(std::move(ack_callback)) {}

PendingOutgoingMessage::~PendingOutgoingMessage() = default;

bool PendingOutgoingMessage::IsAckMessage() const {
  return message_.has_ack_message();
}

PendingOutgoingMessage::SendAckCallback
PendingOutgoingMessage::TakeAckCallback() {
  CHECK(IsAckMessage());
  return std::move(ack_callback_);
}

}  // namespace sharing_message
