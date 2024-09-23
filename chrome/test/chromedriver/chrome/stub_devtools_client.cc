// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"

#include <memory>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

StubDevToolsClient::StubDevToolsClient() : id_("stub-id") {}

StubDevToolsClient::StubDevToolsClient(const std::string& id) : id_(id) {}

StubDevToolsClient::~StubDevToolsClient() = default;

const std::string& StubDevToolsClient::GetId() {
  return id_;
}

const std::string& StubDevToolsClient::SessionId() const {
  return session_id_;
}

const std::string& StubDevToolsClient::TunnelSessionId() const {
  return tunnel_session_id_;
}

Status StubDevToolsClient::SetTunnelSessionId(std::string session_id) {
  tunnel_session_id_ = std::move(session_id);
  return Status{kOk};
}

Status StubDevToolsClient::StartBidiServer(std::string bidi_mapper_script) {
  return Status{kOk};
}

bool StubDevToolsClient::IsNull() const {
  return false;
}

bool StubDevToolsClient::IsConnected() const {
  return is_connected_;
}

bool StubDevToolsClient::WasCrashed() {
  return false;
}

bool StubDevToolsClient::IsDialogOpen() const {
  return false;
}

bool StubDevToolsClient::AutoAcceptsBeforeunload() const {
  return autoaccept_beforeunload_;
}

void StubDevToolsClient::SetAutoAcceptBeforeunload(bool value) {
  autoaccept_beforeunload_ = value;
}

Status StubDevToolsClient::PostBidiCommand(base::Value::Dict command) {
  return Status{kOk};
}

Status StubDevToolsClient::SendCommand(const std::string& method,
                                       const base::Value::Dict& params) {
  base::Value::Dict result;
  return SendCommandAndGetResult(method, params, &result);
}

Status StubDevToolsClient::SendCommandFromWebSocket(
    const std::string& method,
    const base::Value::Dict& params,
    const int client_command_id) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendCommandWithTimeout(
    const std::string& method,
    const base::Value::Dict& params,
    const Timeout* timeout) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendAsyncCommand(const std::string& method,
                                            const base::Value::Dict& params) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendCommandAndGetResult(
    const std::string& method,
    const base::Value::Dict& params,
    base::Value::Dict* result) {
  return Status(kOk);
}

Status StubDevToolsClient::SendCommandAndGetResultWithTimeout(
    const std::string& method,
    const base::Value::Dict& params,
    const Timeout* timeout,
    base::Value::Dict* result) {
  return SendCommandAndGetResult(method, params, result);
}

Status StubDevToolsClient::SendCommandAndIgnoreResponse(
    const std::string& method,
    const base::Value::Dict& params) {
  return SendCommand(method, params);
}

void StubDevToolsClient::AddListener(DevToolsEventListener* listener) {
  listeners_.push_back(listener);
}

void StubDevToolsClient::RemoveListener(DevToolsEventListener* listener) {
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
}

Status StubDevToolsClient::HandleEventsUntil(
    const ConditionalFunc& conditional_func,
    const Timeout& timeout) {
  return Status(kOk);
}

Status StubDevToolsClient::HandleReceivedEvents() {
  return Status(kOk);
}

void StubDevToolsClient::SetDetached() {}

void StubDevToolsClient::SetOwner(WebViewImpl* owner) {
  owner_ = owner;
}

WebViewImpl* StubDevToolsClient::GetOwner() const {
  return owner_;
}

DevToolsClient* StubDevToolsClient::GetParentClient() const {
  return nullptr;
}

bool StubDevToolsClient::IsMainPage() const {
  return true;
}

Status StubDevToolsClient::SendRaw(const std::string& message) {
  return Status{kOk};
}

bool StubDevToolsClient::HasMessageForAnySession() const {
  return false;
}

Status StubDevToolsClient::AttachTo(DevToolsClient* parent) {
  return Status{kOk};
}

void StubDevToolsClient::RegisterSessionHandler(const std::string& session_id,
                                                DevToolsClient* client) {}

void StubDevToolsClient::UnregisterSessionHandler(
    const std::string& session_id) {}

Status StubDevToolsClient::OnConnected() {
  return Status{kOk};
}

Status StubDevToolsClient::ProcessEvent(InspectorEvent event) {
  return Status{kOk};
}

Status StubDevToolsClient::ProcessCommandResponse(
    InspectorCommandResponse response) {
  return Status{kOk};
}

int StubDevToolsClient::NextMessageId() const {
  return 0;
}

int StubDevToolsClient::AdvanceNextMessageId() {
  return 0;
}

Status StubDevToolsClient::ProcessNextMessage(int expected_id,
                                              bool log_timeout,
                                              const Timeout& timeout,
                                              DevToolsClient* caller) {
  return Status{kOk};
}

Status StubDevToolsClient::GetDialogMessage(std::string& message) const {
  return Status{kOk};
}

Status StubDevToolsClient::GetTypeOfDialog(std::string& type) const {
  return Status{kOk};
}

Status StubDevToolsClient::HandleDialog(
    bool accept,
    const std::optional<std::string>& text) {
  return Status{kOk};
}
