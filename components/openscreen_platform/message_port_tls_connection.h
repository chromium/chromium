// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_MESSAGE_PORT_TLS_CONNECTION_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_MESSAGE_PORT_TLS_CONNECTION_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace openscreen {
class TaskRunner;
}  // namespace openscreen

namespace openscreen_platform {

// A TlsConnection implementation that is backed by a MessagePort. All messages
// received by the MessagePort are played out on the TlsConnection::Client
// that has been set, and all messages sent on the TlsConnection are forwarded
// to the MessagePort.
class MessagePortTlsConnection final
    : public openscreen::TlsConnection,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  MessagePortTlsConnection(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      openscreen::TaskRunner& task_runner);

  ~MessagePortTlsConnection() final;

  // TlsConnection overrides.
  void SetClient(TlsConnection::Client* client) final;
  bool Send(openscreen::ByteView data) final;
  openscreen::IPEndpoint GetRemoteEndpoint() const final;

 private:
  // MessagePort::Receiver overrides.
  bool OnMessage(
      std::string_view message,
      std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) final;
  void OnPipeError() final;

  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  const raw_ref<openscreen::TaskRunner> task_runner_;

  raw_ptr<TlsConnection::Client> client_ = nullptr;

  base::WeakPtrFactory<MessagePortTlsConnection> weak_ptr_factory_{this};
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_MESSAGE_PORT_TLS_CONNECTION_H_
