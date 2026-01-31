// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/test/command_injecting_socket.h"

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

CommandInjectingSocket::CommandInjectingSocket(
    std::unique_ptr<SyncWebSocket> wrapped_socket)
    : SyncWebSocketWrapper(std::move(wrapped_socket)) {}

void CommandInjectingSocket::SetSkipCount(int count) {
  skip_count_ = count;
}

void CommandInjectingSocket::SetMethod(std::string method) {
  method_ = std::move(method);
}

void CommandInjectingSocket::SetSessionId(std::string session_id) {
  session_id_ = std::move(session_id);
}

void CommandInjectingSocket::SetParams(base::DictValue params) {
  params_ = std::move(params);
}

bool CommandInjectingSocket::IsSaturated() const {
  return skip_count_ < 0;
}

bool CommandInjectingSocket::Send(const std::string& message) {
  if (skip_count_ == 0) {
    base::DictValue command;
    command.Set("id", next_cmd_id++);
    command.Set("method", method_);
    command.Set("params", params_.Clone());
    if (!session_id_.empty()) {
      command.Set("sessionId", session_id_);
    }
    std::string json;
    if (!base::JSONWriter::Write(command, &json)) {
      return false;
    }
    if (!wrapped_socket_->Send(json)) {
      return false;
    }
  }
  --skip_count_;
  return wrapped_socket_->Send(message);
}

bool CommandInjectingSocket::InterceptResponse(const std::string& message) {
  std::optional<base::Value> maybe_response =
      base::JSONReader::Read(message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!maybe_response.has_value() || !maybe_response->is_dict()) {
    return false;
  }
  std::optional<int> maybe_id = maybe_response->GetDict().FindInt("id");
  return maybe_id.value_or(0) >= 1000'000'000;
}

SyncWebSocket::StatusCode CommandInjectingSocket::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  StatusCode code = StatusCode::kOk;
  std::string received_message;
  // This loop tries to remove the response to the injected command. Otherwise
  // DevToolsClientImpl gets confused.
  do {
    received_message.clear();
    code = wrapped_socket_->ReceiveNextMessage(&received_message, timeout);
  } while (code == StatusCode::kOk && InterceptResponse(received_message));

  if (code == StatusCode::kOk) {
    *message = std::move(received_message);
  }
  return code;
}
