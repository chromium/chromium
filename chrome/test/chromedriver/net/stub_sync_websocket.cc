// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/stub_sync_websocket.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bool ParseCommand(const base::Value::Dict& command,
                  int* cmd_id,
                  std::string* method,
                  base::Value::Dict* params,
                  std::string* session_id) {
  std::optional<int> maybe_id = command.FindInt("id");
  EXPECT_TRUE(maybe_id);
  if (!maybe_id) {
    return false;
  }
  *cmd_id = *maybe_id;

  const std::string* maybe_method = command.FindString("method");
  EXPECT_NE(nullptr, maybe_method);
  if (!maybe_method) {
    return false;
  } else {
    session_id->clear();
  }
  *method = *maybe_method;

  // session id might miss, this if fine
  const std::string* maybe_session_id = command.FindString("sessionId");
  if (maybe_session_id) {
    *session_id = *maybe_session_id;
  }

  // params might miss, this is acceptable
  const base::Value::Dict* maybe_params = command.FindDict("params");
  if (maybe_params) {
    *params = maybe_params->Clone();
  }

  return true;
}

bool ParseMessage(const std::string& message,
                  int* cmd_id,
                  std::string* method,
                  base::Value::Dict* params,
                  std::string* session_id) {
  std::optional<base::Value> value = base::JSONReader::Read(message);
  EXPECT_TRUE(value);
  EXPECT_TRUE(value && value->is_dict());
  if (!value || !value->is_dict()) {
    return false;
  }

  return ParseCommand(value->GetDict(), cmd_id, method, params, session_id);
}

}  // namespace

StubSyncWebSocket::StubSyncWebSocket() {
  AddCommandHandler(
      "Inspector.enable",
      base::BindRepeating([](int cmd_id, const base::Value::Dict& params,
                             base::Value::Dict& response) {
        response.Set("id", cmd_id);
        response.Set("result", base::Value::Dict());
        return true;
      }));
}

StubSyncWebSocket::~StubSyncWebSocket() = default;

bool StubSyncWebSocket::IsConnected() {
  return connected_;
}

bool StubSyncWebSocket::Connect(const GURL& url) {
  EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
  connected_ = true;
  return true;
}

bool StubSyncWebSocket::Send(const std::string& message) {
  EXPECT_TRUE(connected_);
  if (!connected_) {
    return false;
  }
  int cmd_id;
  std::string method;
  base::Value::Dict params;
  std::string session_id;

  if (!ParseMessage(message, &cmd_id, &method, &params, &session_id)) {
    return false;
  }

  if (connect_complete_) {
    auto it = command_handlers_.find(method);
    base::Value::Dict response;
    if (it == command_handlers_.end() ||
        !it->second.Run(cmd_id, params, response)) {
      GenerateDefaultResponse(cmd_id, response);
    }
    std::string serialized_response;
    base::JSONWriter::Write(base::Value(std::move(response)),
                            &serialized_response);
    if (response_limit_ > 0) {
      --response_limit_;
      queued_response_.push(std::move(serialized_response));
    }
  } else {
    EnqueueHandshakeResponse(cmd_id, method);
  }
  return true;
}

SyncWebSocket::StatusCode StubSyncWebSocket::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  if (connected_ && queued_response_.empty() && !on_empty_queue_.is_null()) {
    on_empty_queue_.Run();
  }
  if (!queued_response_.empty()) {
    *message = std::move(queued_response_.front());
    queued_response_.pop();
    return SyncWebSocket::StatusCode::kOk;
  }
  if (!connected_) {
    return SyncWebSocket::StatusCode::kDisconnected;
  }
  // Further wait will either timeout or hang forever
  return SyncWebSocket::StatusCode::kTimeout;
}

bool StubSyncWebSocket::HasNextMessage() {
  return !queued_response_.empty();
}

bool StubSyncWebSocket::PopMessage(std::string* dest) {
  if (queued_response_.empty()) {
    return false;
  }
  *dest = std::move(queued_response_.front());
  queued_response_.pop();
  return true;
}

void StubSyncWebSocket::GenerateDefaultResponse(int cmd_id,
                                                base::Value::Dict& response) {
  response.Set("id", cmd_id);
  base::Value::Dict result;
  result.Set("param", 1);
  response.Set("result", std::move(result));
}

void StubSyncWebSocket::EnqueueHandshakeResponse(int cmd_id,
                                                 const std::string& method) {
  if (method == "Page.addScriptToEvaluateOnNewDocument") {
    EXPECT_FALSE(handshake_add_script_handled_);
    if (!handshake_add_script_handled_) {
      handshake_add_script_handled_ = true;
    } else {
      return;
    }
  } else if (method == "Runtime.evaluate") {
    EXPECT_FALSE(handshake_runtime_eval_handled_);
    if (!handshake_runtime_eval_handled_) {
      handshake_runtime_eval_handled_ = true;
    } else {
      return;
    }
  } else if (method == "Page.enable") {
    EXPECT_FALSE(handshake_page_enable_handled_);
    if (!handshake_page_enable_handled_) {
      handshake_page_enable_handled_ = true;
    } else {
      return;
    }
  } else {
    // Unexpected handshake command
    VLOG(0) << "unexpected handshake method: " << method;
    FAIL();
  }

  connect_complete_ =
      handshake_add_script_handled_ && handshake_runtime_eval_handled_;

  base::Value::Dict response;
  response.Set("id", cmd_id);
  base::Value::Dict result;
  result.Set("param", 1);
  response.Set("result", std::move(result));
  std::string message;
  base::JSONWriter::Write(base::Value(std::move(response)), &message);
  if (response_limit_ > 0) {
    --response_limit_;
    queued_response_.push(std::move(message));
  }
}

void StubSyncWebSocket::AddCommandHandler(const std::string& method,
                                          CommandHandler handler) {
  command_handlers_[method] = std::move(handler);
}

void StubSyncWebSocket::EnqueueResponse(const std::string& message) {
  if (response_limit_ > 0) {
    --response_limit_;
    queued_response_.push(message);
  }
}

void StubSyncWebSocket::Disconnect() {
  connected_ = false;
}

base::RepeatingClosure StubSyncWebSocket::DisconnectClosure() {
  return base::BindRepeating(&StubSyncWebSocket::Disconnect,
                             weak_ptr_factory_.GetWeakPtr());
}

void StubSyncWebSocket::NotifyOnEmptyQueue(base::RepeatingClosure callback) {
  on_empty_queue_ = std::move(callback);
}

void StubSyncWebSocket::SetResponseLimit(int count) {
  response_limit_ = count;
}
