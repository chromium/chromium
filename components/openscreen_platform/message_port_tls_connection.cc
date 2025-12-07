// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/message_port_tls_connection.h"

#include <string_view>

#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace openscreen_platform {

MessagePortTlsConnection::MessagePortTlsConnection(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    openscreen::TaskRunner& task_runner)
    : message_port_(std::move(message_port)), task_runner_(task_runner) {
  DCHECK(message_port_);
  message_port_->SetReceiver(this);
}

MessagePortTlsConnection::~MessagePortTlsConnection() = default;

// TlsConnection overrides.
void MessagePortTlsConnection::SetClient(TlsConnection::Client* client) {
  DCHECK(task_runner_->IsRunningOnTaskRunner());
  client_ = client;
}

bool MessagePortTlsConnection::Send(openscreen::ByteView data) {
  return message_port_->PostMessage(std::string_view(
      reinterpret_cast<const char*>(data.data()), data.size()));
}

openscreen::IPEndpoint MessagePortTlsConnection::GetRemoteEndpoint() const {
  return openscreen::IPEndpoint{openscreen::IPAddress::kV6LoopbackAddress()};
}

bool MessagePortTlsConnection::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DCHECK(ports.empty());

  if (client_) {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([ptr = weak_ptr_factory_.GetWeakPtr(), message]() {
        if (ptr) {
          ptr->OnMessage(
              message,
              std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>());
        }
      });

      return true;
    }

    client_->OnRead(this, std::vector<uint8_t>(message.begin(), message.end()));
  }

  return true;
}

void MessagePortTlsConnection::OnPipeError() {
  if (client_) {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([ptr = weak_ptr_factory_.GetWeakPtr()]() {
        if (ptr) {
          ptr->OnPipeError();
        }
      });
      return;
    }

    client_->OnError(
        this, openscreen::Error(openscreen::Error::Code::kSocketFailure));
  }
}

}  // namespace openscreen_platform
