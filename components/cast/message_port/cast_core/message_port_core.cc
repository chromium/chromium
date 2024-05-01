// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/cast_core/message_port_core.h"

#include <string_view>

#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

namespace {
const uint32_t kInvalidChannelId = 0;
}  // namespace

MessagePortDescriptor::MessagePortDescriptor(uint32_t channel_id,
                                             bool peer_started)
    : channel_id(channel_id), peer_started(peer_started) {}

MessagePortDescriptor::MessagePortDescriptor(MessagePortDescriptor&& other)
    : MessagePortDescriptor(other.channel_id, other.peer_started) {}

Message::Message(Message&& other)
    : data(std::move(other.data)), ports(std::move(other.ports)) {}

Message& Message::operator=(Message&&) = default;

Message::Message(const std::string& data) : data(data) {}

Message::Message(const std::string& data,
                 std::vector<std::unique_ptr<MessagePortCore>> ports)
    : data(data), ports(std::move(ports)) {}

Message::~Message() = default;

MessagePortCore::MessagePortCore(uint32_t channel_id)
    : MessageConnector(channel_id) {}

MessagePortCore::MessagePortCore(MessagePortCore&& other) {
  Assign(std::move(other));
}

MessagePortCore::~MessagePortCore() = default;

void MessagePortCore::Assign(MessagePortCore&& other) {
  receiver_ = std::exchange(other.receiver_, nullptr);
  channel_id_ = other.channel_id_;
  peer_ = std::exchange(other.peer_, nullptr);
  errored_ = std::exchange(other.errored_, false);
  closed_ = std::exchange(other.closed_, true);

  if (peer_) {
    peer_->SetPeer(this);
  }
}

void MessagePortCore::SetReceiver(Receiver* receiver) {
  DCHECK(receiver);
  SetTaskRunner();
  receiver_ = receiver;
  if (!peer_) {
    OnPipeErrorOnSequence();
    return;
  }

  StartOnSequence();
  CheckPeerStartedOnSequence();
}

bool MessagePortCore::Accept(Message message) {
  if (closed_ || errored_) {
    return false;
  }

  if (HasTaskRunner()) {
    AcceptOnSequence(std::move(message));
  }

  return true;
}

void MessagePortCore::AcceptInternal(Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(receiver_);

  if (errored_) {
    return;
  }

  std::vector<std::unique_ptr<MessagePort>> transferables;
  for (auto& port : message.ports) {
    transferables.emplace_back(std::move(port));
  }

  bool result = receiver_->OnMessage(message.data, std::move(transferables));

  if (!(peer_ && peer_->AcceptResult(result))) {
    // The peer has closed and finished.
    OnPipeErrorInternal();
  }
}

bool MessagePortCore::AcceptResult(bool result) {
  // If it's closed_ but not errored_, we still want to post remaining queue.
  if (errored_) {
    return false;
  }

  if (HasTaskRunner()) {
    AcceptResultOnSequence(result);
  }

  // Whether we are closed and finished
  return message_queue_.size() || !closed_;
}

void MessagePortCore::AcceptResultInternal(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_response_);
  pending_response_ = false;
  if (!result) {
    OnPipeErrorInternal();
    return;
  }

  ProcessMessageQueue();
}

void MessagePortCore::OnPeerStarted() {
  // If it's closed_ but not errored_, we still want to post remaining queue.
  if (errored_ || peer_started_) {
    return;
  }

  if (HasTaskRunner()) {
    CheckPeerStartedOnSequence();
  }
}

void MessagePortCore::OnPeerError() {
  if (HasTaskRunner()) {
    OnPipeErrorOnSequence();
  }
}

void MessagePortCore::CheckPeerStartedInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (peer_started_ || !peer_) {
    return;
  }

  // We're also using this to check the initial state, so don't necessarily
  // assign |true|.
  peer_started_ = peer_->started();
  if (peer_started_) {
    ProcessMessageQueue();
  }
}

void MessagePortCore::ProcessMessageQueue() {
  if (errored_ || pending_response_)
    return;

  if (!message_queue_.empty()) {
    auto message = std::move(message_queue_.front());
    message_queue_.pop();
    PostMessageInternal(std::move(message));
  }
}

bool MessagePortCore::PostMessage(std::string_view message) {
  return PostMessageWithTransferables(message, {});
}

// static
MessagePortCore* MessagePortCore::FromMessagePort(MessagePort* port) {
  DCHECK(port);
  // This is safe because there is one MessagePortCore implementation per
  // platform and this is called internally to the implementation.
  return static_cast<MessagePortCore*>(port);
}

bool MessagePortCore::PostMessageWithTransferables(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  std::vector<std::unique_ptr<MessagePortCore>> transferables;
  for (auto& port : ports) {
    std::unique_ptr<MessagePortCore> port_cast(FromMessagePort(port.release()));
    transferables.emplace_back(std::move(port_cast));
  }

  auto msg = Message(std::string(message), std::move(transferables));
  PostMessageOnSequence(std::move(msg));
  return true;
}

void MessagePortCore::PostMessageInternal(Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (errored_ || !peer_) {
    return;
  }

  if (!peer_started_ || pending_response_) {
    message_queue_.emplace(std::move(message));
    return;
  }

  pending_response_ = peer_->Accept(std::move(message));
  if (!pending_response_) {
    OnPipeErrorInternal();
  }
}

void MessagePortCore::OnPipeErrorInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (errored_) {
    return;
  }

  errored_ = true;

  if (closed_) {
    return;
  }

  receiver_->OnPipeError();
}

void MessagePortCore::Close() {
  // Leave the receiver and sequence available for finishing up.
  closed_ = true;
}

bool MessagePortCore::IsValid() const {
  return !closed_ && !errored_ && peer_;
}

bool MessagePortCore::CanPostMessage() const {
  return receiver_ && peer_ && !errored_ && !closed_;
}

MessagePortDescriptor MessagePortCore::Transfer(MessageConnector* replacement) {
  DCHECK(replacement);
  DCHECK(peer_);
  MessagePortDescriptor desc(channel_id(), peer_->started());
  peer_->SetPeer(replacement);
  replacement->SetPeer(peer_);
  Close();
  peer_ = nullptr;
  receiver_ = nullptr;
  channel_id_ = kInvalidChannelId;
  errored_ = false;
  return desc;
}

}  // namespace cast_api_bindings
