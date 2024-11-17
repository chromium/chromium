// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/cast/message_port_cast.h"

#include <string_view>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace cast_api_bindings {

// static
void MessagePortCast::CreatePair(std::unique_ptr<MessagePort>* client,
                                 std::unique_ptr<MessagePort>* server) {
  auto pair_raw = blink::WebMessagePort::CreatePair();
  *client = MessagePortCast::Create(std::move(pair_raw.first));
  *server = MessagePortCast::Create(std::move(pair_raw.second));
}

// static
std::unique_ptr<MessagePort> MessagePortCast::Create(
    blink::WebMessagePort&& port) {
  return std::make_unique<MessagePortCast>(std::move(port));
}

// static
std::unique_ptr<MessagePort> MessagePortCast::Create(
    blink::MessagePortDescriptor&& port_descriptor) {
  return std::make_unique<MessagePortCast>(
      blink::WebMessagePort::Create(std::move(port_descriptor)));
}

// static
MessagePortCast* MessagePortCast::FromMessagePort(MessagePort* port) {
  DCHECK(port);
  // This is safe because there is one MessagePort implementation per platform
  // and this is called internally to the implementation.
  return static_cast<MessagePortCast*>(port);
}

bool MessagePortCast::OnMessage(blink::WebMessagePort::Message message) {
  DCHECK(receiver_);
  std::string message_str;
  if (!base::UTF16ToUTF8(message.data.data(), message.data.size(),
                         &message_str)) {
    return false;
  }

  std::vector<std::unique_ptr<MessagePort>> transferables;
  for (blink::WebMessagePort& port : message.ports) {
    transferables.push_back(Create(std::move(port)));
  }

  return receiver_->OnMessage(message_str, std::move(transferables));
}

MessagePortCast::MessagePortCast(blink::WebMessagePort&& port)
    : receiver_(nullptr), port_(std::move(port)) {}

MessagePortCast::~MessagePortCast() = default;

void MessagePortCast::OnPipeError() {
  DCHECK(receiver_);
  receiver_->OnPipeError();
}

blink::WebMessagePort MessagePortCast::TakePort() {
  return std::move(port_);
}

bool MessagePortCast::PostMessage(std::string_view message) {
  return PostMessageWithTransferables(message, {});
}

bool MessagePortCast::PostMessageWithTransferables(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  DCHECK(port_.IsValid());
  std::vector<blink::WebMessagePort> transferables;

  for (auto& port : ports) {
    MessagePortCast* port_cast = FromMessagePort(port.get());
    transferables.push_back(port_cast->TakePort());
  }

  blink::WebMessagePort::Message msg = blink::WebMessagePort::Message(
      base::UTF8ToUTF16(message), std::move(transferables));
  return port_.PostMessage(std::move(msg));
}

void MessagePortCast::SetReceiver(
    cast_api_bindings::MessagePort::Receiver* receiver) {
  DCHECK(receiver);
  DCHECK(!receiver_);
  receiver_ = receiver;
  port_.SetReceiver(this, base::SequencedTaskRunner::GetCurrentDefault());
}

void MessagePortCast::Close() {
  return port_.Close();
}

bool MessagePortCast::CanPostMessage() const {
  return port_.CanPostMessage();
}

}  // namespace cast_api_bindings
