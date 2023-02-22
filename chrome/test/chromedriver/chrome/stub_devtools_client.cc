// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"

#include <memory>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

StubDevToolsClient::StubDevToolsClient() : id_("stub-id") {}

StubDevToolsClient::StubDevToolsClient(const std::string& id) : id_(id) {}

StubDevToolsClient::~StubDevToolsClient() {}

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

Status StubDevToolsClient::Connect() {
  is_connected_ = true;
  return Status(kOk);
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

Status StubDevToolsClient::HandleEventsUntil(
    const ConditionalFunc& conditional_func,
    const Timeout& timeout) {
  return Status(kOk);
}

Status StubDevToolsClient::HandleReceivedEvents() {
  return Status(kOk);
}

void StubDevToolsClient::SetDetached() {}

void StubDevToolsClient::SetOwner(WebViewImpl* owner) {}

WebViewImpl* StubDevToolsClient::GetOwner() const {
  return nullptr;
}

DevToolsClient* StubDevToolsClient::GetRootClient() {
  return this;
}

DevToolsClient* StubDevToolsClient::GetParentClient() const {
  return nullptr;
}

bool StubDevToolsClient::IsMainPage() const {
  return true;
}
