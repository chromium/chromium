// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_client.h"

#include "components/ui_devtools/devtools_protocol_encoding.h"
#include "components/ui_devtools/devtools_server.h"

namespace ui_devtools {

UiDevToolsClient::UiDevToolsClient(const std::string& name,
                                   UiDevToolsServer* server)
    : name_(name),
      connection_id_(kNotConnected),
      dispatcher_(this),
      server_(server) {
  DCHECK(server_);
}

UiDevToolsClient::~UiDevToolsClient() {}

void UiDevToolsClient::AddAgent(std::unique_ptr<UiDevToolsAgent> agent) {
  agent->Init(&dispatcher_);
  agents_.push_back(std::move(agent));
}

void UiDevToolsClient::Disconnect() {
  connection_id_ = kNotConnected;
  DisableAllAgents();
}

void UiDevToolsClient::Dispatch(const std::string& data) {
  int call_id;
  std::string method;
  std::unique_ptr<protocol::Value> protocolCommand =
      protocol::StringUtil::parseMessage(data, false);
  if (dispatcher_.parseCommand(protocolCommand.get(), &call_id, &method)) {
    dispatcher_.dispatch(call_id, method, std::move(protocolCommand), data);
  }
}

bool UiDevToolsClient::connected() const {
  return connection_id_ != kNotConnected;
}

void UiDevToolsClient::set_connection_id(int connection_id) {
  connection_id_ = connection_id;
}

const std::string& UiDevToolsClient::name() const {
  return name_;
}

void UiDevToolsClient::DisableAllAgents() {
  for (std::unique_ptr<UiDevToolsAgent>& agent : agents_)
    agent->Disable();
}

namespace {
std::string SerializeToJSON(std::unique_ptr<protocol::Serializable> message) {
  std::vector<uint8_t> cbor = std::move(*message).TakeSerialized();
  std::string json;
  crdtp::Status status = ConvertCBORToJSON(crdtp::SpanFrom(cbor), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  return json;
}
}  // namespace

void UiDevToolsClient::sendProtocolResponse(
    int callId,
    std::unique_ptr<protocol::Serializable> message) {
  if (connected()) {
    server_->SendOverWebSocket(
        connection_id_, base::StringPiece(SerializeToJSON(std::move(message))));
  }
}

void UiDevToolsClient::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  if (connected()) {
    server_->SendOverWebSocket(
        connection_id_, base::StringPiece(SerializeToJSON(std::move(message))));
  }
}

void UiDevToolsClient::flushProtocolNotifications() {
  NOTIMPLEMENTED();
}

void UiDevToolsClient::fallThrough(int call_id,
                                   const std::string& method,
                                   const std::string& message) {
  NOTIMPLEMENTED();
}

}  // namespace ui_devtools
