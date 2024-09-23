// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/blink_message_port_adapter.h"

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/cast/message_port/cast/message_port_cast.h"
#include "components/cast/message_port/platform_message_port.h"

namespace cast_api_bindings {
namespace {

// MessagePortAdapters are used to adapt between two different implementations
// of cast_api_bindings::MessagePort.
//
// PostMessageWithTransferables flow including adaptation:
//+---+     +-------+    +---------+        +---------+   +-------+   +---+
//| A |     | PortA |    | AdptrA  |        | AdptrB  |   | PortB |   | B |
//+---+     +-------+    +---------+        +---------+   +-------+   +---+
//  | Post      |             |                  |            |         |
//  |---------->|             |                  |            |         |
//  |           | OnMsg       |                  |            |         |
//  |           |------------>|                  |            |         |
//  |           |             | Adapt Ports      |            |         |
//  |           |             |-----------|      |            |         |
//  |           |             |<----------|      |            |         |
//  |           |             | Post             |            |         |
//  |           |             |----------------->|            |         |
//  |           |             |                  | OnMsg      |         |
//  |           |             |                  |----------->|         |
//  |           |             |                  |            | OnMsg   |
//  |           |             |                  |            |-------->|
//
// Error flow including deletion, for example when OnMessage fails
//  |           |             |                  |            |   false |
//  |           |             |                  |            |<--------|
//  |           |             |                  |      OnErr |         |
//  |           |             |                  |<-----------|         |
//  |           |             |           delete |            |         |
//  |           |             |<-----------------|            |         |
//  |           |      delete |                  |            |         |
//  |           |<------------|                  |            |         |
//  |     OnErr |             |                  |            |         |
//  |<----------|             |                  |            |         |
//  |           |             |                  | delete     |         |
//  |           |             |                  |------|     |         |
//  |           |             |                  |<-----|     |         |
class MessagePortAdapter : public MessagePort::Receiver {
 public:
  using CreatePairRepeatingCallback =
      base::RepeatingCallback<void(std::unique_ptr<MessagePort>*,
                                   std::unique_ptr<MessagePort>*)>;

  // PortType is used to track whether the held port represents a client or a
  // server port. Clients must respect client and server semantics because some
  // platforms have asymmetric port implementations.
  enum class PortType {
    CLIENT = 1,
    SERVER = 2,
  };

  // Two MessagePortAdapters are used to adapt between their respective
  // |port|s. Some implementations are directional, so |port_type| is used
  // determine how the |create_pair| function should be invoked when
  // adapting transferred ports. The adapter pair manages its own lifetime
  // and is destroyed on error.
  MessagePortAdapter(std::unique_ptr<MessagePort> port,
                     PortType port_type,
                     CreatePairRepeatingCallback create_pair_cb)
      : port_(std::move(port)),
        port_type_(port_type),
        create_pair_cb_(create_pair_cb) {
    DCHECK(port_);
    port_->SetReceiver(this);
  }
  ~MessagePortAdapter() override = default;
  MessagePortAdapter(const MessagePortAdapter&) = delete;
  MessagePortAdapter(MessagePortAdapter&&) = delete;

  // Pairs the port with a |peer|. Their lifetimes will be the same.
  void SetPeer(MessagePortAdapter* peer) {
    DCHECK(peer);
    peer_ = peer;
  }

  // Invokes |create_pair_|, which should create a new pair of ports with the
  // same type as |port_|.
  void CreatePair(std::unique_ptr<MessagePort>* client,
                  std::unique_ptr<MessagePort>* server) {
    create_pair_cb_.Run(client, server);
  }

 private:
  // MessagePort::Receiver implementation:
  void OnPipeError() override {
    DCHECK(peer_);

    // Ports are closed on error; when an error occurs, destroy the adapters.
    delete peer_;
    delete this;
  }

  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<MessagePort>> ports) override {
    DCHECK(peer_);
    std::vector<std::unique_ptr<MessagePort>> transferables;

    // Because we are adapting port types, an adapter to the peer's type is
    // needed for each transferred port.
    for (auto& port : ports) {
      // The incoming |port| is the same kind of port as |this|.
      MessagePortAdapter* incoming =
          new MessagePortAdapter(std::move(port), port_type_, create_pair_cb_);
      // The outgoing port is the same kind of port as |peer_|.
      std::unique_ptr<MessagePort> client;
      std::unique_ptr<MessagePort> server;
      peer_->CreatePair(&client, &server);

      // If |incoming| is a client port adapter, it will be talking to a
      // |server| of the port type of the |peer_|, and vice versa.
      MessagePortAdapter* outgoing = new MessagePortAdapter(
          incoming->port_type_ == PortType::CLIENT ? std::move(server)
                                                   : std::move(client),
          peer_->port_type_, peer_->create_pair_cb_);

      // Connect the adapters together.
      incoming->SetPeer(outgoing);
      outgoing->SetPeer(incoming);

      // If |incoming| is a client port adapter, the |client| port of the
      // |peer_| type will be transferred, and vice versa.
      // |server| of the port type of the |peer_|, and vice versa.
      transferables.push_back(port_type_ == PortType::CLIENT
                                  ? std::move(client)
                                  : std::move(server));
    }

    return peer_->port_->PostMessageWithTransferables(message,
                                                      std::move(transferables));
  }

  std::unique_ptr<MessagePort> port_;
  raw_ptr<MessagePortAdapter, DanglingUntriaged> peer_ = nullptr;
  const PortType port_type_;
  const CreatePairRepeatingCallback create_pair_cb_;
};
}  // namespace

// static
std::unique_ptr<MessagePort>
BlinkMessagePortAdapter::ToClientPlatformMessagePort(
    blink::WebMessagePort&& client) {
  MessagePortAdapter* blink =
      new MessagePortAdapter(MessagePortCast::Create(std::move(client)),
                             MessagePortAdapter::PortType::CLIENT,
                             base::BindRepeating(&MessagePortCast::CreatePair));

  std::unique_ptr<MessagePort> adapted_client;
  std::unique_ptr<MessagePort> server;
  CreatePlatformMessagePortPair(&adapted_client, &server);
  MessagePortAdapter* platform = new MessagePortAdapter(
      std::move(server), MessagePortAdapter::PortType::SERVER,
      base::BindRepeating(&CreatePlatformMessagePortPair));
  blink->SetPeer(platform);
  platform->SetPeer(blink);
  return adapted_client;
}

// static
blink::WebMessagePort BlinkMessagePortAdapter::FromServerPlatformMessagePort(
    std::unique_ptr<MessagePort> server) {
  MessagePortAdapter* platform = new MessagePortAdapter(
      std::move(server), MessagePortAdapter::PortType::SERVER,
      base::BindRepeating(&CreatePlatformMessagePortPair));

  std::unique_ptr<MessagePort> client;
  std::unique_ptr<MessagePort> adapted_server;
  MessagePortCast::CreatePair(&client, &adapted_server);
  MessagePortAdapter* blink = new MessagePortAdapter(
      std::move(client), MessagePortAdapter::PortType::CLIENT,
      base::BindRepeating(&MessagePortCast::CreatePair));
  blink->SetPeer(platform);
  platform->SetPeer(blink);
  return MessagePortCast::FromMessagePort(adapted_server.get())->TakePort();
}
}  // namespace cast_api_bindings
