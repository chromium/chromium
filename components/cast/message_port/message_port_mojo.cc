// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/message_port_mojo.h"

#include <vector>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace cast_api_bindings {

// static
void MessagePort::CreatePair(std::unique_ptr<MessagePort>* client,
                             std::unique_ptr<MessagePort>* server) {
  mojo::PendingRemote<mojom::UnidirectionalMessagePort> client_remote;
  mojo::PendingRemote<mojom::UnidirectionalMessagePort> server_remote;

  auto client_receiver = server_remote.InitWithNewPipeAndPassReceiver();
  auto server_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  *client = std::make_unique<MessagePortMojo>(std::move(client_receiver),
                                              std::move(client_remote));
  *server = std::make_unique<MessagePortMojo>(std::move(server_receiver),
                                              std::move(server_remote));
}

MessagePortMojo::MessagePortMojo(
    mojo::PendingReceiver<mojom::UnidirectionalMessagePort> receiver,
    mojo::PendingRemote<mojom::UnidirectionalMessagePort> remote)
    : mojo_remote_(std::move(remote)),
      mojo_receiver_(this, std::move(receiver)),
      weak_factory_(this) {
  mojo_receiver_.set_disconnect_handler(base::BindOnce(
      &MessagePortMojo::OnMojoDisconnected, weak_factory_.GetWeakPtr()));
  mojo_remote_.set_disconnect_handler(base::BindOnce(
      &MessagePortMojo::OnMojoDisconnected, weak_factory_.GetWeakPtr()));
}

MessagePortMojo::~MessagePortMojo() = default;

bool MessagePortMojo::PostMessage(base::StringPiece message) {
  return PostMessageWithTransferables(std::move(message), {});
}

bool MessagePortMojo::PostMessageWithTransferables(
    base::StringPiece message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  if (!CanPostMessage()) {
    return false;
  }

  std::vector<mojom::MessagePortPtr> mojo_ports;
  mojo_ports.reserve(ports.size());
  for (std::unique_ptr<MessagePort>& port : ports) {
    DCHECK(port);

    // The |mojo_remote_| object used below would be invalidated in this
    // process.
    DCHECK(port.get() != this);

    // This is safe because there is one MessagePort implementation used at a
    // time.
    MessagePortMojo* casted_port = static_cast<MessagePortMojo*>(port.get());

    mojo_ports.push_back(casted_port->Unbind());
  }

  mojo_remote_->PostMessageWithTransferables(
      std::string(message.begin(), message.end()), std::move(mojo_ports));
  return true;
}

void MessagePortMojo::SetReceiver(MessagePort::Receiver* receiver) {
  receiver_ = receiver;
}

void MessagePortMojo::Close() {
  mojo_remote_.reset();
  mojo_receiver_.reset();
}

bool MessagePortMojo::CanPostMessage() const {
  return mojo_remote_.is_bound();
}

void MessagePortMojo::PostMessageWithTransferables(
    const std::string& message,
    std::vector<mojom::MessagePortPtr> ports) {
  if (!receiver_) {
    return;
  }

  std::vector<std::unique_ptr<MessagePort>> mojo_ports;
  mojo_ports.reserve(ports.size());
  for (mojom::MessagePortPtr& port : ports) {
    mojo_ports.push_back(std::make_unique<MessagePortMojo>(
        std::move(port.get()->receiver), std::move(port.get()->remote)));
  }

  receiver_->OnMessage(message, std::move(mojo_ports));
}

mojom::MessagePortPtr MessagePortMojo::Unbind() {
  if (!mojo_remote_.is_bound() || !mojo_receiver_.is_bound()) {
    return nullptr;
  }

  return mojom::MessagePort::New(mojo_receiver_.Unbind(),
                                 mojo_remote_.Unbind());
}

void MessagePortMojo::OnMojoDisconnected() {
  if (!receiver_) {
    return;
  }

  receiver_->OnPipeError();
  Close();
}

}  // namespace cast_api_bindings
