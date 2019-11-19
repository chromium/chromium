// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

StubDevToolsClient::StubDevToolsClient() : id_("stub-id") {}

StubDevToolsClient::StubDevToolsClient(const std::string& id) : id_(id) {}

StubDevToolsClient::~StubDevToolsClient() {}

const std::string& StubDevToolsClient::GetId() {
  return id_;
}

bool StubDevToolsClient::WasCrashed() {
  return false;
}

Status StubDevToolsClient::ConnectIfNecessary() {
  return Status(kOk);
}

Status StubDevToolsClient::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  std::unique_ptr<base::DictionaryValue> result;
  return SendCommandAndGetResult(method, params, &result);
}

Status StubDevToolsClient::SendCommandFromWebSocket(
    const std::string& method,
    const base::DictionaryValue& params,
    const int client_command_id) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendCommandWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendAsyncCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  return SendCommand(method, params);
}

Status StubDevToolsClient::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    std::unique_ptr<base::DictionaryValue>* result) {
  result->reset(new base::DictionaryValue());
  return Status(kOk);
}

Status StubDevToolsClient::SendCommandAndGetResultWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout,
    std::unique_ptr<base::DictionaryValue>* result) {
  return SendCommandAndGetResult(method, params, result);
}

Status StubDevToolsClient::SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::DictionaryValue& params) {
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
