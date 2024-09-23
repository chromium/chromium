// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/stub_sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::Eq;
using testing::Optional;
using testing::Pointee;

const char kTestMapperScript[] = "Lorem ipsum dolor sit amet";

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

testing::AssertionResult StatusOk(const Status& status) {
  return StatusCodeIs<kOk>(status);
}

bool ParseCommand(const base::Value::Dict& command,
                  int* cmd_id,
                  std::string* method,
                  base::Value::Dict* params,
                  std::string* session_id) {
  std::optional<int> maybe_id = command.FindInt("id");
  EXPECT_TRUE(maybe_id);
  if (!maybe_id)
    return false;
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

template <class T>
Status SerializeAsJson(const T& value, std::string* json) {
  if (!base::JSONWriter::Write(value, json)) {
    return Status(kUnknownError, "cannot serialize the argument as JSON");
  }
  return Status{kOk};
}

Status CreateCdpEvent(std::string method,
                      base::Value::Dict params,
                      std::string session_id,
                      base::Value::Dict* evt) {
  base::Value::Dict dict;
  dict.Set("method", std::move(method));
  dict.Set("params", std::move(params));
  if (!session_id.empty()) {
    dict.Set("sessionId", std::move(session_id));
  }
  *evt = std::move(dict);
  return Status{kOk};
}

Status CreateCdpResponse(int cmd_id,
                         base::Value::Dict result,
                         std::string session_id,
                         base::Value::Dict* resp) {
  base::Value::Dict dict;
  dict.Set("id", cmd_id);
  dict.Set("result", std::move(result));
  if (!session_id.empty()) {
    dict.Set("sessionId", std::move(session_id));
  }
  *resp = std::move(dict);
  return Status{kOk};
}

Status CreateBidiCommand(int cmd_id,
                         std::string method,
                         base::Value::Dict params,
                         const std::string* channel,
                         base::Value::Dict* cmd) {
  base::Value::Dict dict;
  dict.Set("id", cmd_id);
  dict.Set("method", std::move(method));
  dict.Set("params", std::move(params));
  if (channel) {
    dict.Set("channel", *channel);
  }
  *cmd = std::move(dict);
  return Status{kOk};
}

Status WrapBidiEventInCdpEvent(const base::Value::Dict& bidi_evt,
                               std::string mapper_session_id,
                               base::Value::Dict* evt) {
  std::string payload;
  Status status = SerializeAsJson(bidi_evt, &payload);
  if (status.IsError()) {
    return status;
  }
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", payload);
  return CreateCdpEvent("Runtime.bindingCalled", std::move(params),
                        std::move(mapper_session_id), evt);
}

Status WrapBidiResponseInCdpEvent(const base::Value::Dict& bidi_resp,
                                  std::string mapper_session_id,
                                  base::Value::Dict* evt) {
  std::string payload;
  Status status = SerializeAsJson(bidi_resp, &payload);
  if (status.IsError()) {
    return status;
  }
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", payload);
  return CreateCdpEvent("Runtime.bindingCalled", std::move(params),
                        std::move(mapper_session_id), evt);
}

class SyncWebSocketWrapper : public SyncWebSocket {
 public:
  explicit SyncWebSocketWrapper(SyncWebSocket* socket) : socket_(socket) {}
  ~SyncWebSocketWrapper() override = default;

  bool IsConnected() override { return socket_->IsConnected(); }

  bool Connect(const GURL& url) override { return socket_->Connect(url); }

  bool Send(const std::string& message) override {
    return socket_->Send(message);
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    return socket_->ReceiveNextMessage(message, timeout);
  }

  bool HasNextMessage() override { return socket_->HasNextMessage(); }

 private:
  raw_ptr<SyncWebSocket> socket_;
};

template <typename TSocket>
class SocketHolder {
 public:
  template <typename... Args>
  explicit SocketHolder(Args&&... args) : socket_{args...} {}

  std::unique_ptr<SyncWebSocket> Wrapper() {
    return std::unique_ptr<SyncWebSocket>(new SyncWebSocketWrapper(&socket_));
  }

  TSocket& Socket() { return socket_; }

  bool ConnectSocket() { return socket_.Connect(GURL("http://url/")); }

 private:
  TSocket socket_;
};

struct SessionState {
  bool handshake_add_script_handled = false;
  bool handshake_runtime_eval_handled = false;
  bool handshake_page_enable_handled_ = false;
  bool connect_complete = false;
};

class MultiSessionMockSyncWebSocket : public SyncWebSocket {
 public:
  MultiSessionMockSyncWebSocket() = default;
  ~MultiSessionMockSyncWebSocket() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    int cmd_id;
    std::string method;
    base::Value::Dict params;
    std::string session_id;

    if (!ParseMessage(message, &cmd_id, &method, &params, &session_id)) {
      return false;
    }

    SessionState& session_state = sesison_states_[session_id];

    if (session_state.connect_complete) {
      return OnUserCommand(&session_state, cmd_id, std::move(method),
                           std::move(params), std::move(session_id));
    } else {
      return EnqueueHandshakeResponse(&session_state, cmd_id, std::move(method),
                                      std::move(session_id));
    }
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (!HasNextMessage() && timeout.IsExpired()) {
      return SyncWebSocket::StatusCode::kTimeout;
    }
    EXPECT_TRUE(HasNextMessage());
    if (PopMessage(message)) {
      return SyncWebSocket::StatusCode::kOk;
    } else {
      return SyncWebSocket::StatusCode::kDisconnected;
    }
  }

  bool HasNextMessage() override { return !queued_response_.empty(); }

  virtual bool OnUserCommand(SessionState* session_state,
                             int cmd_id,
                             std::string method,
                             base::Value::Dict params,
                             std::string session_id) {
    EXPECT_STREQ("method", method.c_str());
    base::Value::Dict response;
    Status status =
        CreateDefaultCdpResponse(cmd_id, std::move(method), std::move(params),
                                 std::move(session_id), &response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    std::string message;
    status = SerializeAsJson(response, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(std::move(message));
    return true;
  }

  Status CreateDefaultCdpResponse(int cmd_id,
                                  std::string method,
                                  base::Value::Dict params,
                                  std::string session_id,
                                  base::Value::Dict* response) {
    base::Value::Dict result;
    std::optional<int> ping = params.FindInt("ping");
    if (ping) {
      result.Set("pong", *ping);
    } else {
      result.Set("param", 1);
    }

    return CreateCdpResponse(cmd_id, std::move(result), std::move(session_id),
                             response);
  }

  bool EnqueueHandshakeResponse(SessionState* session_state,
                                int cmd_id,
                                std::string method,
                                std::string session_id) {
    if (method == "Page.addScriptToEvaluateOnNewDocument") {
      EXPECT_FALSE(session_state->handshake_add_script_handled);
      if (!session_state->handshake_add_script_handled) {
        session_state->handshake_add_script_handled = true;
      } else {
        return false;
      }
    } else if (method == "Runtime.evaluate") {
      EXPECT_FALSE(session_state->handshake_runtime_eval_handled);
      if (!session_state->handshake_runtime_eval_handled) {
        session_state->handshake_runtime_eval_handled = true;
      } else {
        return false;
      }
    } else if (method == "Page.enable") {
      EXPECT_FALSE(session_state->handshake_page_enable_handled_);
      if (!session_state->handshake_page_enable_handled_) {
        session_state->handshake_page_enable_handled_ = true;
      } else {
        return false;
      }
    } else {
      // Unexpected handshake command
      VLOG(0) << "unexpected handshake method: " << method;
      ADD_FAILURE();
      return false;
    }

    session_state->connect_complete =
        session_state->handshake_add_script_handled &&
        session_state->handshake_runtime_eval_handled;

    base::Value::Dict result;
    result.Set("param", 1);
    base::Value::Dict response;
    Status status =
        CreateCdpResponse(cmd_id, std::move(result), session_id, &response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    std::string message;
    status = SerializeAsJson(base::Value(std::move(response)), &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(std::move(message));
    return true;
  }

  bool PopMessage(std::string* dest) {
    if (queued_response_.empty()) {
      return false;
    }
    *dest = std::move(queued_response_.front());
    queued_response_.pop();
    return true;
  }

 protected:
  bool connected_ = false;
  std::map<std::string, SessionState> sesison_states_;
  std::queue<std::string> queued_response_;
};

class DevToolsClientImplTest : public testing::Test {
 protected:
  DevToolsClientImplTest() : long_timeout_(base::Minutes(5)) {}

  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, Ctor) {
  const std::string expected_id = "E2F4";
  const std::string expected_session_id = "BC80031";
  DevToolsClientImpl client(expected_id, expected_session_id);
  EXPECT_EQ(expected_id, client.GetId());
  EXPECT_EQ(expected_session_id, client.SessionId());
  EXPECT_EQ(std::string(), client.TunnelSessionId());
  EXPECT_FALSE(client.IsMainPage());
  EXPECT_FALSE(client.IsConnected());
  EXPECT_TRUE(client.IsNull());
  EXPECT_FALSE(client.WasCrashed());
  EXPECT_EQ(1, client.NextMessageId());
  EXPECT_EQ(nullptr, client.GetOwner());
  EXPECT_EQ(nullptr, client.GetParentClient());
  EXPECT_FALSE(client.AutoAcceptsBeforeunload());
  // No dialog
  std::string message("old message");
  std::string type("old type");
  ASSERT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  ASSERT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  ASSERT_FALSE(client.IsDialogOpen());
  ASSERT_STREQ("old message", message.c_str());
  ASSERT_STREQ("old type", type.c_str());
  ASSERT_TRUE(
      StatusCodeIs<kNoSuchAlert>(client.HandleDialog(false, std::nullopt)));
}

TEST_F(DevToolsClientImplTest, SendCommand) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  params.Set("param", 1);
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
}

TEST_F(DevToolsClientImplTest, SendCommandAndGetResult) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  params.Set("param", 1);
  base::Value::Dict result;
  ASSERT_TRUE(
      StatusOk(client.SendCommandAndGetResult("method", params, &result)));
  std::string json;
  base::JSONWriter::Write(base::Value(std::move(result)), &json);
  ASSERT_STREQ("{\"param\":1}", json.c_str());
}

TEST_F(DevToolsClientImplTest, SetMainPage) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("E2F4", "BC80031");
  client.SetMainPage(true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  EXPECT_TRUE(client.IsMainPage());
}

TEST_F(DevToolsClientImplTest, SetTunnelSessionId) {
  DevToolsClientImpl client("E2F4", "BC80031");
  ASSERT_TRUE(client.TunnelSessionId().empty());
  ASSERT_TRUE(StatusOk(client.SetTunnelSessionId("bidi_session")));
  EXPECT_EQ("bidi_session", client.TunnelSessionId());
}

TEST_F(DevToolsClientImplTest, ChangeTunnelSessionId) {
  DevToolsClientImpl client("E2F4", "BC80031");
  ASSERT_TRUE(client.TunnelSessionId().empty());
  ASSERT_TRUE(StatusOk(client.SetTunnelSessionId("bidi_session")));
  EXPECT_TRUE(client.SetTunnelSessionId("another_bidi_session").IsError());
}

TEST_F(DevToolsClientImplTest, SetNullSocket) {
  DevToolsClientImpl client("page_client", "page_session");
  EXPECT_TRUE(client.SetSocket(nullptr).IsError());
}

TEST_F(DevToolsClientImplTest, AttachToNull) {
  DevToolsClientImpl client("client", "session");
  EXPECT_TRUE(client.AttachTo(nullptr).IsError());
}

TEST_F(DevToolsClientImplTest, AttachToClientWithNoSocket) {
  DevToolsClientImpl client1("client1", "session_1");
  DevToolsClientImpl client2("client2", "session_2");
  EXPECT_TRUE(client1.AttachTo(&client2).IsError());
}

TEST_F(DevToolsClientImplTest, AttachToAnotherRoot) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder1;
  DevToolsClientImpl root_client1("root_client_1", "root_session_1");
  ASSERT_TRUE(socket_holder1.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client1.SetSocket(socket_holder1.Wrapper())));
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder2;
  DevToolsClientImpl root_client2("root_client_2", "root_session_2");
  ASSERT_TRUE(socket_holder2.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client2.SetSocket(socket_holder2.Wrapper())));
  DevToolsClientImpl client("page_client", "page_session");
  ASSERT_TRUE(StatusOk(client.AttachTo(&root_client1)));
  // Client cannot be re-attached
  EXPECT_TRUE(client.AttachTo(&root_client2).IsError());
}

TEST_F(DevToolsClientImplTest, AttachRootToRoot) {
  SocketHolder<StubSyncWebSocket> socket_holder1;
  DevToolsClientImpl root_client1("root_client_1", "root_session_1");
  ASSERT_TRUE(socket_holder1.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client1.SetSocket(socket_holder1.Wrapper())));
  SocketHolder<StubSyncWebSocket> socket_holder2;
  DevToolsClientImpl root_client2("root_client_2", "root_session_2");
  ASSERT_TRUE(socket_holder2.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client2.SetSocket(socket_holder2.Wrapper())));
  EXPECT_TRUE(root_client2.AttachTo(&root_client1).IsError());
}

TEST_F(DevToolsClientImplTest, AttachAsGrandChild) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder;
  DevToolsClientImpl root_client("root_client", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl child_client("child_client", "child_session");
  ASSERT_TRUE(StatusOk(child_client.AttachTo(&root_client)));
  DevToolsClientImpl grand_child_client("grand_child_client",
                                        "grand_child_session");
  EXPECT_TRUE(grand_child_client.AttachTo(&child_client).IsError());
}

namespace {

class MockSyncWebSocket3 : public StubSyncWebSocket {
 public:
  explicit MockSyncWebSocket3(bool send_returns_after_connect)
      : send_returns_after_connect_(send_returns_after_connect) {}
  ~MockSyncWebSocket3() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    int cmd_id;
    std::string method;
    base::Value::Dict params;
    std::string session_id;

    if (!ParseMessage(message, &cmd_id, &method, &params, &session_id)) {
      return false;
    }

    if (connect_complete_) {
      return send_returns_after_connect_;
    } else {
      EnqueueHandshakeResponse(cmd_id, method);
    }
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (PopMessage(message)) {
      // Handshake response only
      return SyncWebSocket::StatusCode::kOk;
    } else {
      return SyncWebSocket::StatusCode::kDisconnected;
    }
  }

  bool HasNextMessage() override { return true; }

 private:
  bool connected_ = false;
  bool send_returns_after_connect_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandSendFails) {
  SocketHolder<MockSyncWebSocket3> socket_holder{false};
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandReceiveNextMessageFails) {
  SocketHolder<MockSyncWebSocket3> socket_holder{true};
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class FakeSyncWebSocket : public StubSyncWebSocket {
 public:
  FakeSyncWebSocket() = default;
  ~FakeSyncWebSocket() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_FALSE(connected_);
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    int cmd_id;
    std::string method;
    base::Value::Dict params;
    std::string session_id;

    if (!ParseMessage(message, &cmd_id, &method, &params, &session_id)) {
      return false;
    }

    if (!connect_complete_) {
      EnqueueHandshakeResponse(cmd_id, method);
    }
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    PopMessage(message);
    return SyncWebSocket::StatusCode::kOk;
  }

  bool HasNextMessage() override { return true; }

 private:
  bool connected_ = false;
};

bool ReturnCommand(const std::string& message,
                   int expected_id,
                   std::string& session_id,
                   internal::InspectorMessageType& type,
                   InspectorEvent& event,
                   InspectorCommandResponse& command_response) {
  type = internal::kCommandResponseMessageType;
  session_id.clear();
  command_response.id = expected_id;
  command_response.result = base::Value::Dict();
  return true;
}

bool ReturnBadResponse(const std::string& message,
                       int expected_id,
                       std::string& session_id,
                       internal::InspectorMessageType& type,
                       InspectorEvent& event,
                       InspectorCommandResponse& command_response) {
  type = internal::kCommandResponseMessageType;
  session_id.clear();
  command_response.id = expected_id;
  command_response.result = base::Value::Dict();
  return false;
}

bool ReturnCommandBadId(const std::string& message,
                        int expected_id,
                        std::string& session_id,
                        internal::InspectorMessageType& type,
                        InspectorEvent& event,
                        InspectorCommandResponse& command_response) {
  type = internal::kCommandResponseMessageType;
  session_id.clear();
  command_response.id = expected_id + 100;
  command_response.result = base::Value::Dict();
  return true;
}

bool ReturnUnexpectedIdThenResponse(
    bool* first,
    const std::string& message,
    int expected_id,
    std::string& session_id,
    internal::InspectorMessageType& type,
    InspectorEvent& event,
    InspectorCommandResponse& command_response) {
  session_id.clear();
  if (*first) {
    type = internal::kCommandResponseMessageType;
    command_response.id = expected_id + 100;
    command_response.error = "{\"code\":-32001,\"message\":\"ERR\"}";
  } else {
    type = internal::kCommandResponseMessageType;
    command_response.id = expected_id;
    command_response.result = base::Value::Dict();
    command_response.result->Set("key", 2);
  }
  *first = false;
  return true;
}

bool ReturnCommandError(const std::string& message,
                        int expected_id,
                        std::string& session_id,
                        internal::InspectorMessageType& type,
                        InspectorEvent& event,
                        InspectorCommandResponse& command_response) {
  type = internal::kCommandResponseMessageType;
  session_id.clear();
  command_response.id = expected_id;
  command_response.error = "err";
  return true;
}

class MockListener : public DevToolsEventListener {
 public:
  MockListener() = default;
  ~MockListener() override { EXPECT_TRUE(called_); }

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    called_ = true;
    EXPECT_STREQ("method", method.c_str());
    EXPECT_TRUE(params.Find("key"));
    return Status(kOk);
  }

 private:
  bool called_ = false;
};

bool ReturnEventThenResponse(bool* first,
                             const std::string& message,
                             int expected_id,
                             std::string& session_id,
                             internal::InspectorMessageType& type,
                             InspectorEvent& event,
                             InspectorCommandResponse& command_response) {
  session_id.clear();
  if (*first) {
    type = internal::kEventMessageType;
    event.method = "method";
    event.params = base::Value::Dict();
    event.params->Set("key", 1);
  } else {
    type = internal::kCommandResponseMessageType;
    command_response.id = expected_id;
    command_response.result = base::Value::Dict();
    command_response.result->Set("key", 2);
  }
  *first = false;
  return true;
}

bool ReturnEvent(const std::string& message,
                 int expected_id,
                 std::string& session_id,
                 internal::InspectorMessageType& type,
                 InspectorEvent& event,
                 InspectorCommandResponse& command_response) {
  type = internal::kEventMessageType;
  event.method = "method";
  event.params = base::Value::Dict();
  event.params->Set("key", 1);
  return true;
}

bool ReturnOutOfOrderResponses(int* recurse_count,
                               DevToolsClient* client,
                               const std::string& message,
                               int expected_id,
                               std::string& session_id,
                               internal::InspectorMessageType& type,
                               InspectorEvent& event,
                               InspectorCommandResponse& command_response) {
  int key = 0;
  base::Value::Dict params;
  params.Set("param", 1);
  switch ((*recurse_count)++) {
    case 0:
      client->SendCommand("method", params);
      type = internal::kEventMessageType;
      event.method = "method";
      event.params = base::Value::Dict();
      event.params->Set("key", 1);
      return true;
    case 1:
      command_response.id = expected_id - 1;
      key = 2;
      break;
    case 2:
      command_response.id = expected_id;
      key = 3;
      break;
  }
  type = internal::kCommandResponseMessageType;
  command_response.result = base::Value::Dict();
  command_response.result->Set("key", key);
  return true;
}

bool ReturnError(const std::string& message,
                 int expected_id,
                 std::string& session_id,
                 internal::InspectorMessageType& type,
                 InspectorEvent& event,
                 InspectorCommandResponse& command_response) {
  return false;
}

Status AlwaysTrue(bool* is_met) {
  *is_met = true;
  return Status(kOk);
}

Status AlwaysFalse(bool* is_met) {
  *is_met = false;
  return Status(kOk);
}

Status AlwaysError(bool* is_met) {
  return Status(kUnknownError);
}

}  // namespace

TEST_F(DevToolsClientImplTest, FakeSyncWebSocketSelfTest) {
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommand));
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
}

TEST_F(DevToolsClientImplTest, SendCommandBadResponse) {
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnBadResponse));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandBadId) {
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommandBadId));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandUnexpectedId) {
  bool first = true;
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnUnexpectedIdThenResponse, &first));
  base::Value::Dict params;
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
}

TEST_F(DevToolsClientImplTest, SendCommandResponseError) {
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommandError));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandEventBeforeResponse) {
  MockListener listener;
  bool first = true;
  SocketHolder<FakeSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  client.AddListener(&listener);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnEventThenResponse, &first));
  base::Value::Dict params;
  base::Value::Dict result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  std::optional<int> key = result.FindInt("key");
  ASSERT_TRUE(key);
  ASSERT_EQ(2, key.value());
}

TEST(ParseInspectorMessage, NonJson) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage("hi", 0, session_id, type, event,
                                               response));
}

TEST(ParseInspectorMessage, NeitherCommandNorEvent) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage("{}", 0, session_id, type, event,
                                               response));
}

TEST(ParseInspectorMessage, EventNoParams) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\"}", 0, session_id, type, event, response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
}

TEST(ParseInspectorMessage, EventNoParamsWithSessionId) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\",\"sessionId\":\"B221AF2\"}", 0, session_id, type,
      event, response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  EXPECT_EQ("B221AF2", session_id);
}

TEST(ParseInspectorMessage, EventWithParams) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\",\"params\":{\"key\":100},\"sessionId\":\"AB3A\"}",
      0, session_id, type, event, response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  int key = event.params->FindInt("key").value_or(-1);
  ASSERT_EQ(100, key);
  EXPECT_EQ("AB3A", session_id);
}

TEST(ParseInspectorMessage, CommandNoErrorOrResult) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  // As per Chromium issue 392577, DevTools does not necessarily return a
  // "result" dictionary for every valid response. If neither "error" nor
  // "result" keys are present, a blank result dictionary should be inferred.
  ASSERT_TRUE(
      internal::ParseInspectorMessage("{\"id\":1,\"sessionId\":\"AB2AF3C\"}", 0,
                                      session_id, type, event, response));
  ASSERT_TRUE(response.result->empty());
  EXPECT_EQ("AB2AF3C", session_id);
}

TEST(ParseInspectorMessage, CommandError) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"id\":1,\"error\":{}}", 0, session_id, type, event, response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_TRUE(response.error.length());
  ASSERT_FALSE(response.result);
}

TEST(ParseInspectorMessage, Command) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(
      internal::ParseInspectorMessage("{\"id\":1,\"result\":{\"key\":1}}", 0,
                                      session_id, type, event, response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_FALSE(response.error.length());
  int key = response.result->FindInt("key").value_or(-1);
  ASSERT_EQ(1, key);
}

TEST(ParseInspectorMessage, NoBindingName) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(
      internal::ParseInspectorMessage("{\"method\":\"Runtime.bindingCalled\","
                                      "\"params\":{\"key\":100},"
                                      "\"sessionId\":\"AB3A\"}",
                                      -1, session_id, type, event, response));
}

TEST(ParseInspectorMessage, UnknownBindingName) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"Runtime.bindingCalled\","
      "\"params\":{\"name\":\"helloWorld\", \"payload\": \"{}\"},"
      "\"sessionId\":\"AB3A\"}",
      -1, session_id, type, event, response));
  ASSERT_EQ(internal::kEventMessageType, type);
}

TEST(ParseInspectorMessage, BidiMessageNoPayload) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage(
      "{\"method\":\"Runtime.bindingCalled\","
      "\"params\":{\"name\":\"sendBidiResponse\"},"
      "\"sessionId\":\"AB3A\"}",
      -1, session_id, type, event, response));
}

TEST(ParseInspectorMessage, BidiMessagePayloadNotADict) {
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage(
      "{\"method\":\"Runtime.bindingCalled\","
      "\"params\":{\"name\":\"sendBidiResponse\", \"payload\": \"7\"},"
      "\"sessionId\":\"AB3A\"}",
      -1, session_id, type, event, response));
}

TEST(ParseInspectorMessage, TunneledCdpEvent) {
  base::Value::Dict cdp_params;
  cdp_params.Set("data", "hello");
  // payload_params.
  base::Value::Dict params;
  params.Set("cdpMethod", "event");
  params.Set("cdpSession", "ABC");
  params.Set("cdpParams", std::move(cdp_params));
  base::Value::Dict payload;
  payload.Set("method", "cdp.eventReceived");
  payload.Set("params", std::move(params));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict evt;
  ASSERT_TRUE(
      StatusOk(WrapBidiEventInCdpEvent(std::move(payload), "333", &evt)));
  std::string message;
  SerializeAsJson(evt, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kEventMessageType, type);
  EXPECT_EQ("ABC", session_id);
  EXPECT_EQ("event", event.method);
  ASSERT_TRUE(event.params);
  EXPECT_THAT(event.params->FindString("data"), Pointee(Eq("hello")));
}

TEST(ParseInspectorMessage, TunneledCdpEventNoCdpSession) {
  base::Value::Dict cdp_params;
  cdp_params.Set("data", "hello");
  // payload_params.
  base::Value::Dict params;
  params.Set("cdpMethod", "event");
  params.Set("cdpParams", std::move(cdp_params));
  base::Value::Dict payload;
  payload.Set("method", "cdp.eventReceived");
  payload.Set("params", std::move(params));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict evt;
  ASSERT_TRUE(
      StatusOk(WrapBidiEventInCdpEvent(std::move(payload), "333", &evt)));
  std::string message;
  SerializeAsJson(evt, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kEventMessageType, type);
  EXPECT_EQ("", session_id);
  EXPECT_EQ("event", event.method);
  ASSERT_TRUE(event.params);
  EXPECT_THAT(event.params->FindString("data"), Pointee(Eq("hello")));
}

TEST(ParseInspectorMessage, TunneledCdpEventNoCdpParams) {
  // payload_params.
  base::Value::Dict params;
  params.Set("cdpMethod", "event");
  params.Set("cdpSession", "ABC");
  base::Value::Dict payload;
  payload.Set("method", "cdp.eventReceived");
  payload.Set("params", std::move(params));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict evt;
  ASSERT_TRUE(
      StatusOk(WrapBidiEventInCdpEvent(std::move(payload), "333", &evt)));
  std::string message;
  SerializeAsJson(evt, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kEventMessageType, type);
  EXPECT_EQ("ABC", session_id);
  EXPECT_EQ("event", event.method);
  ASSERT_TRUE(event.params);
}

TEST(ParseInspectorMessage, TunneledCdpEventNoCdpMethod) {
  // payload_params.
  base::Value::Dict params;
  params.Set("cdpSession", "ABC");
  base::Value::Dict payload;
  payload.Set("method", "cdp.eventReceived");
  payload.Set("params", std::move(params));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict evt;
  ASSERT_TRUE(
      StatusOk(WrapBidiEventInCdpEvent(std::move(payload), "333", &evt)));
  std::string message;
  SerializeAsJson(evt, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                               event, response));
}

TEST(ParseInspectorMessage, TunneledCdpEventNoPayloadParams) {
  base::Value::Dict payload;
  payload.Set("method", "cdp.eventReceived");
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict evt;
  ASSERT_TRUE(
      StatusOk(WrapBidiEventInCdpEvent(std::move(payload), "333", &evt)));
  std::string message;
  SerializeAsJson(evt, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                               event, response));
}

TEST(ParseInspectorMessage, TunneledCdpResponse) {
  base::Value::Dict result;
  result.Set("data", "hola");
  base::Value::Dict payload;
  payload.Set("id", 11);
  payload.Set("cdpSession", "ABC");
  payload.Set("result", std::move(result));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict resp;
  ASSERT_TRUE(
      StatusOk(WrapBidiResponseInCdpEvent(std::move(payload), "333", &resp)));
  std::string message;
  SerializeAsJson(resp, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kCommandResponseMessageType, type);
  EXPECT_EQ("ABC", session_id);
  EXPECT_EQ(11, response.id);
  ASSERT_TRUE(response.result);
  EXPECT_THAT(response.result->FindString("data"), Pointee(Eq("hola")));
}

TEST(ParseInspectorMessage, TunneledCdpResponseNoSession) {
  base::Value::Dict result;
  result.Set("data", "hola");
  base::Value::Dict payload;
  payload.Set("id", 11);
  payload.Set("result", std::move(result));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict resp;
  ASSERT_TRUE(
      StatusOk(WrapBidiResponseInCdpEvent(std::move(payload), "333", &resp)));
  std::string message;
  SerializeAsJson(resp, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kCommandResponseMessageType, type);
  EXPECT_EQ("", session_id);
  EXPECT_EQ(11, response.id);
  ASSERT_TRUE(response.result);
  EXPECT_THAT(response.result->FindString("data"), Pointee(Eq("hola")));
}

TEST(ParseInspectorMessage, TunneledCdpResponseNoId) {
  base::Value::Dict result;
  result.Set("data", "hola");
  base::Value::Dict payload;
  payload.Set("cdpSession", "ABC");
  payload.Set("result", std::move(result));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict resp;
  ASSERT_TRUE(
      StatusOk(WrapBidiResponseInCdpEvent(std::move(payload), "333", &resp)));
  std::string message;
  SerializeAsJson(resp, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                               event, response));
}

TEST(ParseInspectorMessage, TunneledCdpResponseNoResult) {
  base::Value::Dict payload;
  payload.Set("id", 11);
  payload.Set("cdpSession", "ABC");
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict resp;
  ASSERT_TRUE(
      StatusOk(WrapBidiResponseInCdpEvent(std::move(payload), "333", &resp)));
  std::string message;
  SerializeAsJson(resp, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kCommandResponseMessageType, type);
  EXPECT_EQ("ABC", session_id);
  EXPECT_EQ(11, response.id);
  ASSERT_TRUE(response.result);
}

TEST(ParseInspectorMessage, TunneledCdpResponseError) {
  base::Value::Dict error;
  error.Set("data", "hola");
  base::Value::Dict payload;
  payload.Set("id", 11);
  payload.Set("cdpSession", "ABC");
  payload.Set("error", std::move(error));
  payload.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  base::Value::Dict resp;
  ASSERT_TRUE(
      StatusOk(WrapBidiResponseInCdpEvent(std::move(payload), "333", &resp)));
  std::string message;
  SerializeAsJson(resp, &message);
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(message, -1, session_id, type,
                                              event, response));
  EXPECT_EQ(internal::kCommandResponseMessageType, type);
  EXPECT_EQ("ABC", session_id);
  EXPECT_EQ(11, response.id);
  ASSERT_TRUE(!response.error.empty());
}

TEST(ParseInspectorError, EmptyError) {
  Status status = internal::ParseInspectorError("");
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_EQ("unknown error: inspector error with no error message",
            status.message());
}

TEST(ParseInspectorError, InvalidUrlError) {
  Status status = internal::ParseInspectorError(
      "{\"message\": \"Cannot navigate to invalid URL\"}");
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(ParseInspectorError, InvalidArgumentCode) {
  Status status = internal::ParseInspectorError(
      "{\"code\": -32602, \"message\": \"Error description\"}");
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_EQ("invalid argument: Error description", status.message());
}

TEST(ParseInspectorError, NoTargetWithGivenIdFound) {
  Status status = internal::ParseInspectorError(
      "{\"code\": -32602, \"message\": \"No target with given id found\"}");
  ASSERT_EQ(kNoSuchWindow, status.code());
  ASSERT_EQ("no such window: No target with given id found", status.message());
}

TEST(ParseInspectorError, UnknownError) {
  const std::string error("{\"code\": 10, \"message\": \"Error description\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_EQ("unknown error: unhandled inspector error: " + error,
            status.message());
}

TEST(ParseInspectorError, CdpNotImplementedError) {
  const std::string error("{\"code\":-32601,\"message\":\"SOME MESSAGE\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kUnknownCommand, status.code());
  ASSERT_EQ("unknown command: SOME MESSAGE", status.message());
}

TEST(ParseInspectorError, NoSuchFrameError) {
  // As the server returns the generic error code: SERVER_ERROR = -32000
  // we have to rely on the error message content.
  // A real scenario where this error message occurs is WPT test:
  // 'cookies/samesite/iframe-reload.https.html'
  // The error is thrown by InspectorDOMAgent::getFrameOwner
  // (inspector_dom_agent.cc).
  const std::string error(
      "{\"code\":-32000,"
      "\"message\":\"Frame with the given id was not found.\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kNoSuchFrame, status.code());
  ASSERT_EQ("no such frame: Frame with the given id was not found.",
            status.message());
}

TEST(ParseInspectorError, SessionNotFoundError) {
  const std::string error("{\"code\":-32001,\"message\":\"SOME MESSAGE\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kNoSuchFrame, status.code());
  ASSERT_EQ("no such frame: SOME MESSAGE", status.message());
}

TEST(ParseInspectorError, ExecutionContextWasDestroyed) {
  const std::string error(
      "{\"code\":-32000,\"message\":\"Execution context was destroyed.\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kNavigationDetectedByRemoteEnd, status.code());
  ASSERT_EQ(
      "navigation detected by remote end: Execution context was destroyed.",
      status.message());
}

TEST(ParseInspectorError, InspectedTargetNavigatedOrClosed) {
  const std::string error(
      "{\"code\":-32000,\"message\":\"Inspected target navigated or closed\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kNavigationDetectedByRemoteEnd, status.code());
  ASSERT_EQ(
      "navigation detected by remote end: Inspected target navigated or closed",
      status.message());
}

TEST_F(DevToolsClientImplTest, HandleEventsUntil) {
  MockListener listener;
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  client.AddListener(&listener);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kOk, status.code());
}

TEST_F(DevToolsClientImplTest, HandleEventsUntilTimeout) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysFalse),
                                           Timeout(base::TimeDelta()));
  ASSERT_EQ(kTimeout, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventCommand) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommand));
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventError) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnError));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventConditionalFuncReturnsError) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysError),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, NestedCommandsWithOutOfOrderResults) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  int recurse_count = 0;
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnOutOfOrderResponses, &recurse_count, &client));
  base::Value::Dict params;
  params.Set("param", 1);
  base::Value::Dict result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  std::optional<int> key = result.FindInt("key");
  ASSERT_TRUE(key);
  ASSERT_EQ(2, key.value());
}

namespace {

class OnConnectedListener : public DevToolsEventListener {
 public:
  OnConnectedListener(const std::string& method, DevToolsClient* client)
      : method_(method), client_(client) {
    client_->AddListener(this);
  }
  ~OnConnectedListener() override = default;

  void VerifyCalled() {
    EXPECT_TRUE(on_connected_called_);
    EXPECT_TRUE(on_event_called_);
  }

  Status OnConnected(DevToolsClient* client) override {
    EXPECT_EQ(client_, client);
    EXPECT_STREQ("onconnected-id", client->GetId().c_str());
    EXPECT_FALSE(on_connected_called_);
    EXPECT_FALSE(on_event_called_);
    on_connected_called_ = true;
    base::Value::Dict params;
    return client_->SendCommand(method_, params);
  }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    EXPECT_EQ(client_, client);
    EXPECT_STREQ("onconnected-id", client->GetId().c_str());
    EXPECT_TRUE(on_connected_called_);
    on_event_called_ = true;
    return Status(kOk);
  }

 private:
  std::string method_;
  raw_ptr<DevToolsClient> client_ = nullptr;
  bool on_connected_called_ = false;
  bool on_event_called_ = false;
};

class OnConnectedSyncWebSocket : public StubSyncWebSocket {
 public:
  OnConnectedSyncWebSocket() = default;
  ~OnConnectedSyncWebSocket() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    int cmd_id;
    std::string method;
    base::Value::Dict params;
    std::string session_id;

    if (!ParseMessage(message, &cmd_id, &method, &params, &session_id)) {
      return false;
    }

    if (connect_complete_) {
      base::Value::Dict response;
      response.Set("id", cmd_id);
      response.Set("result", base::Value::Dict());
      std::string json_response;
      base::JSONWriter::Write(base::Value(std::move(response)), &json_response);
      queued_response_.push(std::move(json_response));

      // Push one event.
      base::Value::Dict event;
      event.Set("method", "updateEvent");
      event.Set("params", base::Value::Dict());
      std::string json_event;
      base::JSONWriter::Write(base::Value(std::move(event)), &json_event);
      queued_response_.push(std::move(json_event));
    } else {
      EnqueueHandshakeResponse(cmd_id, method);
    }
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (PopMessage(message)) {
      return SyncWebSocket::StatusCode::kOk;
    } else {
      return SyncWebSocket::StatusCode::kDisconnected;
    }
  }

 private:
  bool connected_ = false;
};

}  // namespace

TEST_F(DevToolsClientImplTest, ProcessOnConnectedFirstOnCommand) {
  SocketHolder<OnConnectedSyncWebSocket> socket_holder;
  DevToolsClientImpl client("onconnected-id", "");
  OnConnectedListener listener1("DOM.getDocument", &client);
  OnConnectedListener listener2("Runtime.enable", &client);
  OnConnectedListener listener3("Page.enable", &client);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  EXPECT_TRUE(StatusOk(client.SendCommand("Runtime.execute", params)));
  listener1.VerifyCalled();
  listener2.VerifyCalled();
  listener3.VerifyCalled();
}

TEST_F(DevToolsClientImplTest, ProcessOnConnectedFirstOnHandleEventsUntil) {
  SocketHolder<OnConnectedSyncWebSocket> socket_holder;
  DevToolsClientImpl client("onconnected-id", "");
  OnConnectedListener listener1("DOM.getDocument", &client);
  OnConnectedListener listener2("Runtime.enable", &client);
  OnConnectedListener listener3("Page.enable", &client);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));
  listener1.VerifyCalled();
  listener2.VerifyCalled();
  listener3.VerifyCalled();
}

namespace {

class MockSyncWebSocket5 : public SyncWebSocket {
 public:
  MockSyncWebSocket5() = default;
  ~MockSyncWebSocket5() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return true; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (request_no_ == 0) {
      *message = "{\"method\": \"m\", \"params\": {}}";
    } else {
      *message = base::StringPrintf(
          "{\"result\": {}, \"id\": %d}", request_no_);
    }
    request_no_++;
    return SyncWebSocket::StatusCode::kOk;
  }

  bool HasNextMessage() override { return false; }

 private:
  int request_no_ = 0;
  bool connected_ = false;
};

class OtherEventListener : public DevToolsEventListener {
 public:
  OtherEventListener() = default;
  ~OtherEventListener() override = default;

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    received_event_ = true;
    return Status(kOk);
  }

  bool received_event_ = false;
};

class OnEventListener : public DevToolsEventListener {
 public:
  OnEventListener(DevToolsClient* client,
                  OtherEventListener* other_listener)
      : client_(client),
        other_listener_(other_listener) {}
  ~OnEventListener() override = default;

  Status OnConnected(DevToolsClient* client) override {
    EXPECT_EQ(client_, client);
    return Status(kOk);
  }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    EXPECT_EQ(client_, client);
    client_->SendCommand("method", params);
    EXPECT_TRUE(other_listener_->received_event_);
    return Status(kOk);
  }

 private:
  raw_ptr<DevToolsClient> client_ = nullptr;
  raw_ptr<OtherEventListener> other_listener_ = nullptr;
};

}  // namespace

TEST_F(DevToolsClientImplTest, ProcessOnEventFirst) {
  SocketHolder<MockSyncWebSocket5> socket_holder;
  DevToolsClientImpl client("id", "");
  OtherEventListener listener2;
  OnEventListener listener1(&client, &listener2);
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  EXPECT_TRUE(StatusOk(client.SendCommand("method", params)));
}

namespace {

class MockSyncWebSocket6 : public StubSyncWebSocket {
 public:
  explicit MockSyncWebSocket6(std::list<std::string>* messages)
      : messages_(messages) {}
  ~MockSyncWebSocket6() override = default;

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return true; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (messages_->empty())
      return SyncWebSocket::StatusCode::kDisconnected;
    *message = messages_->front();
    messages_->pop_front();
    return SyncWebSocket::StatusCode::kOk;
  }

  bool HasNextMessage() override { return messages_->size(); }

 private:
  raw_ptr<std::list<std::string>> messages_ = nullptr;
  bool connected_ = false;
};

class MockDevToolsEventListener : public DevToolsEventListener {
 public:
  MockDevToolsEventListener() = default;
  ~MockDevToolsEventListener() override = default;

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    DevToolsClientImpl* client_impl = static_cast<DevToolsClientImpl*>(client);
    int msg_id = client_impl->NextMessageId();

    Status status = client->SendCommand("hello", params);

    if (msg_id == expected_blocked_id_) {
      EXPECT_TRUE(StatusCodeIs<kUnexpectedAlertOpen>(status));
    } else {
      EXPECT_TRUE(StatusOk(status));
    }
    return Status(kOk);
  }

  void SetExpectedBlockedId(int value) { expected_blocked_id_ = value; }

 private:
  int expected_blocked_id_ = -1;
};

std::string JavaScriptDialogOpeningEvent(const std::string& message,
                                         const std::string& type,
                                         const std::string& default_prompt) {
  return base::StringPrintf(
      "{\"method\": \"Page.javascriptDialogOpening\", \"params\": {"
      "\"message\": \"%s\","
      "\"type\": \"%s\","
      "\"defaultPrompt\": \"%s\""
      "}}",
      message.c_str(), type.c_str(), default_prompt.c_str());
}

std::string JavaScriptDialogClosedEvent(bool result,
                                        const std::string& user_input) {
  return base::StringPrintf(
      "{\"method\": \"Page.javascriptDialogClosed\", \"params\": {"
      "\"result\": %s,"
      "\"userInput\": \"%s\""
      "}}",
      (result ? "true" : "false"), user_input.c_str());
}

}  // namespace

TEST_F(DevToolsClientImplTest, BlockedByAlert) {
  std::list<std::string> msgs;
  SocketHolder<MockSyncWebSocket6> socket_holder{&msgs};
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  msgs.push_back(
      JavaScriptDialogOpeningEvent("irrelevant", "irrelevant", "irrelevant"));
  msgs.push_back("{\"id\": 2, \"result\": {}}");
  base::Value::Dict params;
  ASSERT_EQ(kUnexpectedAlertOpen,
            client.SendCommand("first", params).code());
}

TEST_F(DevToolsClientImplTest, CorrectlyDeterminesWhichIsBlockedByAlert) {
  // OUT                 | IN
  //                       FirstEvent
  // hello (id1)
  //                       SecondEvent
  // hello (id2)
  //                       ThirdEvent
  // hello (id3)
  //                       FourthEvent
  // hello (id4)
  //                       response for id1
  //                       alert
  // hello (id5)
  // round trip command (id6)
  //                       response for id2
  //                       response for id4
  //                       response for id5
  //                       response for id6
  std::list<std::string> msgs;
  SocketHolder<MockSyncWebSocket6> socket_holder{&msgs};
  DevToolsClientImpl client("id", "");
  MockDevToolsEventListener listener;
  client.AddListener(&listener);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  int next_msg_id = client.NextMessageId();
  msgs.push_back("{\"method\": \"FirstEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"SecondEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"ThirdEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"FourthEvent\", \"params\": {}}");
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  msgs.push_back(
      JavaScriptDialogOpeningEvent("irrelevant", "irrelevant", "irrelevant"));
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  listener.SetExpectedBlockedId(next_msg_id++);
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  ASSERT_EQ(kOk, client.HandleReceivedEvents().code());
}

TEST_F(DevToolsClientImplTest, AutoAcceptsBeforeunloadInheritance) {
  SocketHolder<OnConnectedSyncWebSocket> socket_holder;
  DevToolsClientImpl parent("parent", "parent_session");
  DevToolsClientImpl child("child", "child_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(parent.SetSocket(socket_holder.Wrapper())));
  ASSERT_TRUE(StatusOk(child.AttachTo(&parent)));

  // both clients must have the default value
  EXPECT_EQ(false, parent.AutoAcceptsBeforeunload());
  EXPECT_EQ(false, child.AutoAcceptsBeforeunload());

  // The property must not be inherited by the child from the parent.
  for (int mask = 0; mask < 4; ++mask) {
    const bool parent_is_on = (mask & 2) == 2;
    const bool child_is_on = (mask & 1) == 1;
    parent.SetAutoAcceptBeforeunload(parent_is_on);
    child.SetAutoAcceptBeforeunload(child_is_on);
    EXPECT_EQ(parent_is_on, parent.AutoAcceptsBeforeunload());
    EXPECT_EQ(child_is_on, child.AutoAcceptsBeforeunload());
  }
}

TEST_F(DevToolsClientImplTest, NoDialog) {
  SocketHolder<OnConnectedSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  // Check if the setup is correct
  EXPECT_TRUE(StatusOk(client.SendCommand("Runtime.execute", params)));

  std::string message("old message");
  std::string type("old type");
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_STREQ("old message", message.c_str());
  EXPECT_STREQ("old type", type.c_str());
  EXPECT_TRUE(
      StatusCodeIs<kNoSuchAlert>(client.HandleDialog(false, std::nullopt)));
}

TEST_F(DevToolsClientImplTest, HandleDialogPassesParams) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "prompt", ""));
  EXPECT_TRUE(socket_holder.Socket().HasNextMessage());
  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));
  EXPECT_TRUE(client.IsDialogOpen());

  bool is_handled = false;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](bool& is_handled, int cmd_id, const base::Value::Dict& params,
             base::Value::Dict& response) {
            EXPECT_THAT(params.FindBool("accept"), Optional(Eq(false)));
            EXPECT_THAT(params.FindString("promptText"), Pointee(Eq("text")));
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            is_handled = true;
            return true;
          },
          std::ref(is_handled)));

  std::string given_text("text");
  EXPECT_TRUE(
      StatusOk(client.HandleDialog(false, std::make_optional(given_text))));
  EXPECT_TRUE(is_handled);
}

TEST_F(DevToolsClientImplTest, HandleDialogNullPrompt) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "prompt", "from-event"));
  EXPECT_TRUE(socket_holder.Socket().HasNextMessage());
  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));
  EXPECT_TRUE(client.IsDialogOpen());

  bool is_handled = false;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](bool& is_handled, int cmd_id, const base::Value::Dict& params,
             base::Value::Dict& response) {
            EXPECT_THAT(params.FindBool("accept"), Optional(Eq(false)));
            EXPECT_THAT(params.FindString("promptText"),
                        Pointee(Eq("from-event")));
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            is_handled = true;
            return true;
          },
          std::ref(is_handled)));

  EXPECT_TRUE(StatusOk(client.HandleDialog(false, std::nullopt)));
  EXPECT_TRUE(is_handled);
}

TEST_F(DevToolsClientImplTest, OneDialog) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "alert", "from-event"));
  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));
  EXPECT_TRUE(client.IsDialogOpen());
  EXPECT_TRUE(StatusOk(client.GetDialogMessage(message)));
  EXPECT_EQ("hi", message);
  EXPECT_TRUE(StatusOk(client.GetTypeOfDialog(type)));
  EXPECT_EQ("alert", type);

  int handle_dialog_counter = 0;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](StubSyncWebSocket& socket, int& counter, int cmd_id,
             const base::Value::Dict& params, base::Value::Dict& response) {
            socket.EnqueueResponse(JavaScriptDialogClosedEvent(true, ""));
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            ++counter;
            return true;
          },
          std::ref(socket_holder.Socket()), std::ref(handle_dialog_counter)));

  EXPECT_TRUE(StatusOk(client.HandleDialog(false, std::nullopt)));
  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_TRUE(
      StatusCodeIs<kNoSuchAlert>(client.HandleDialog(false, std::nullopt)));
  EXPECT_EQ(1, handle_dialog_counter);
}

TEST_F(DevToolsClientImplTest, TwoDialogs) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("1", "confirm", ""));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("2", "alert", ""));

  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  // Consume the Page.javaScriptDialogOpening events
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_TRUE(StatusOk(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusOk(client.GetTypeOfDialog(type)));
  EXPECT_TRUE(client.IsDialogOpen());
  EXPECT_EQ("1", message);
  EXPECT_EQ("confirm", type);

  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating([](int cmd_id, const base::Value::Dict& params,
                             base::Value::Dict& response) {
        response.Set("id", cmd_id);
        response.Set("result", base::Value::Dict());
        return true;
      }));

  EXPECT_TRUE(StatusOk(client.HandleDialog(false, std::nullopt)));

  EXPECT_TRUE(client.IsDialogOpen());
  EXPECT_TRUE(StatusOk(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusOk(client.GetTypeOfDialog(type)));
  EXPECT_EQ("2", message);
  EXPECT_EQ("alert", type);

  int handle_dialog_counter = 0;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](StubSyncWebSocket& socket, int& counter, int cmd_id,
             const base::Value::Dict& params, base::Value::Dict& response) {
            socket.EnqueueResponse(JavaScriptDialogClosedEvent(true, ""));
            socket.EnqueueResponse(JavaScriptDialogClosedEvent(true, ""));
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            ++counter;
            return true;
          },
          std::ref(socket_holder.Socket()), std::ref(handle_dialog_counter)));

  EXPECT_TRUE(StatusOk(client.HandleDialog(false, std::nullopt)));

  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_TRUE(
      StatusCodeIs<kNoSuchAlert>(client.HandleDialog(false, std::nullopt)));
  EXPECT_EQ(1, handle_dialog_counter);
}

TEST_F(DevToolsClientImplTest, OneDialogManualClose) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "alert", ""));

  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  // Consume the Page.javaScriptDialogOpening events
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_TRUE(client.IsDialogOpen());
  EXPECT_TRUE(StatusOk(client.GetDialogMessage(message)));
  EXPECT_EQ("hi", message);
  EXPECT_TRUE(StatusOk(client.GetTypeOfDialog(type)));
  EXPECT_EQ("alert", type);

  socket_holder.Socket().EnqueueResponse(JavaScriptDialogClosedEvent(true, ""));

  // Consume the Page.javascriptDialogClosed events
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_TRUE(
      StatusCodeIs<kNoSuchAlert>(client.HandleDialog(false, std::nullopt)));
}

TEST_F(DevToolsClientImplTest, BeforeunloadIsNotAutoAccepted) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  // Prohibit auto-acceptance of beforeunload dialogs
  client.SetAutoAcceptBeforeunload(false);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "beforeunload", ""));
  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  bool is_handled = false;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](bool& is_handled, int cmd_id, const base::Value::Dict& params,
             base::Value::Dict& response) {
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            is_handled = true;
            return true;
          },
          std::ref(is_handled)));

  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_TRUE(client.IsDialogOpen());
  EXPECT_TRUE(StatusOk(client.GetDialogMessage(message)));
  EXPECT_EQ("hi", message);
  EXPECT_TRUE(StatusOk(client.GetTypeOfDialog(type)));
  EXPECT_EQ("beforeunload", type);
  EXPECT_FALSE(is_handled);
}

TEST_F(DevToolsClientImplTest, BeforeunloadIsAutoAccepted) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  client.SetAutoAcceptBeforeunload(true);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("hi", "beforeunload", ""));
  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  bool is_handled = false;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](bool& is_handled, int cmd_id, const base::Value::Dict& params,
             base::Value::Dict& response) {
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            is_handled = true;
            return true;
          },
          std::ref(is_handled)));

  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_TRUE(is_handled);
}

TEST_F(DevToolsClientImplTest, AlertAndBeforeunloadAreAutoAccepted) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  DevToolsClientImpl client("", "");
  client.SetAutoAcceptBeforeunload(true);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("1", "alert", ""));
  socket_holder.Socket().EnqueueResponse(
      JavaScriptDialogOpeningEvent("2", "beforeunload", ""));
  EXPECT_FALSE(client.IsDialogOpen());
  std::string message;
  std::string type;
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));

  int handle_dialog_counter = 0;
  socket_holder.Socket().AddCommandHandler(
      "Page.handleJavaScriptDialog",
      base::BindRepeating(
          [](StubSyncWebSocket& socket, int& counter, int cmd_id,
             const base::Value::Dict& params, base::Value::Dict& response) {
            socket.EnqueueResponse(JavaScriptDialogClosedEvent(true, ""));
            response.Set("id", cmd_id);
            response.Set("result", base::Value::Dict());
            ++counter;
            return true;
          },
          std::ref(socket_holder.Socket()), std::ref(handle_dialog_counter)));

  // Consume the Page.javaScriptDialogOpening event
  EXPECT_TRUE(StatusOk(client.HandleReceivedEvents()));

  EXPECT_FALSE(client.IsDialogOpen());
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetDialogMessage(message)));
  EXPECT_TRUE(StatusCodeIs<kNoSuchAlert>(client.GetTypeOfDialog(type)));
  EXPECT_EQ(1, handle_dialog_counter);
}

namespace {

class MockCommandListener : public DevToolsEventListener {
 public:
  MockCommandListener() = default;
  ~MockCommandListener() override = default;

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    msgs_.push_back(method);
    return Status(kOk);
  }

  Status OnCommandSuccess(DevToolsClient* client,
                          const std::string& method,
                          const base::Value::Dict* result,
                          const Timeout& command_timeout) override {
    msgs_.push_back(method);
    if (!callback_.is_null())
      callback_.Run(client);
    return Status(kOk);
  }

  base::RepeatingCallback<void(DevToolsClient*)> callback_;
  std::list<std::string> msgs_;
};

void HandleReceivedEvents(DevToolsClient* client) {
  EXPECT_TRUE(StatusOk(client->HandleReceivedEvents()));
}

}  // namespace

TEST_F(DevToolsClientImplTest, ReceivesCommandResponse) {
  std::list<std::string> msgs;
  SocketHolder<MockSyncWebSocket6> socket_holder{&msgs};
  DevToolsClientImpl client("id", "");
  MockCommandListener listener1;
  listener1.callback_ = base::BindRepeating(&HandleReceivedEvents);
  MockCommandListener listener2;
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  int next_msg_id = client.NextMessageId();
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  msgs.push_back("{\"method\": \"event\", \"params\": {}}");
  base::Value::Dict params;
  ASSERT_EQ(kOk, client.SendCommand("cmd", params).code());
  ASSERT_EQ(2u, listener2.msgs_.size());
  ASSERT_EQ("cmd", listener2.msgs_.front());
  ASSERT_EQ("event", listener2.msgs_.back());
}

TEST_F(DevToolsClientImplTest, SendCommandAndIgnoreResponse) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder;
  DevToolsClientImpl client("id", "");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(client.SetSocket(socket_holder.Wrapper())));
  base::Value::Dict params;
  params.Set("param", 1);
  ASSERT_TRUE(StatusOk(client.SendCommandAndIgnoreResponse("method", params)));
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
}

namespace {

class PingingListener : public DevToolsEventListener {
 public:
  PingingListener() = default;
  ~PingingListener() override = default;

  int Ping() const { return ping_; }

  int Pong() const { return pong_; }

  void SetPing(int ping) {
    ping_ = ping;
    pong_ = ping + 1;  // make them different
  }

  void AttachTo(DevToolsClient* client) {
    client_ = client;
    client_->AddListener(this);
  }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& event_params) override {
    if (event_handled_) {
      return Status{kOk};
    }

    EXPECT_EQ(client_, client);
    EXPECT_EQ(method, "event");

    base::Value::Dict params;
    params.Set("ping", ping_);
    base::Value::Dict result;
    event_handled_ = true;
    Status status = client_->SendCommandAndGetResult("method", params, &result);
    EXPECT_TRUE(StatusOk(status));
    if (!status.IsOk()) {
      return status;
    }
    std::optional<int> pong = result.FindInt("pong");
    EXPECT_TRUE(pong);
    if (pong) {
      pong_ = *pong;
    } else {
      return Status{kUnknownError, "result does not contain 'pong' field"};
    }
    return Status(kOk);
  }

 private:
  raw_ptr<DevToolsClient> client_ = nullptr;
  int ping_ = -1;
  int pong_ = 0;
  bool event_handled_ = false;
};

class MultiSessionMockSyncWebSocket2 : public MultiSessionMockSyncWebSocket {
 public:
  explicit MultiSessionMockSyncWebSocket2(const std::string& event_session)
      : event_session_(event_session) {}
  ~MultiSessionMockSyncWebSocket2() override = default;

  bool OnUserCommand(SessionState* session_state,
                     int cmd_id,
                     std::string method,
                     base::Value::Dict params,
                     std::string session_id) override {
    EXPECT_STREQ("method", method.c_str());

    {
      base::Value::Dict evt;
      Status status =
          CreateCdpEvent("event", base::Value::Dict{}, event_session_, &evt);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      std::string message;
      status = SerializeAsJson(std::move(evt), &message);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      queued_response_.push(std::move(message));
    }
    {
      base::Value::Dict response;
      Status status =
          CreateDefaultCdpResponse(cmd_id, std::move(method), std::move(params),
                                   std::move(session_id), &response);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      std::string message;
      status = SerializeAsJson(std::move(response), &message);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      queued_response_.push(std::move(message));
    }
    return true;
  }

 private:
  std::string event_session_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, AttachToConnected) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder;
  DevToolsClientImpl root_client("root_client", "root_session");
  DevToolsClientImpl client("page_client", "page_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  ASSERT_TRUE(root_client.IsConnected());
  EXPECT_TRUE(StatusOk(client.AttachTo(&root_client)));
  EXPECT_TRUE(client.IsConnected());
}

TEST_F(DevToolsClientImplTest, RoutingChildParent) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder;
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl client("child", "child_session");
  ASSERT_TRUE(StatusOk(client.AttachTo(&root_client)));
  base::Value::Dict params;
  params.Set("param", 1);
  ASSERT_TRUE(StatusOk(client.SendCommand("method", params)));
}

TEST_F(DevToolsClientImplTest, RoutingTwoChildren) {
  SocketHolder<MultiSessionMockSyncWebSocket> socket_holder;
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl red_client("red_client", "red_session");
  DevToolsClientImpl blue_client("blue_client", "blue_session");
  ASSERT_TRUE(StatusOk(red_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(blue_client.AttachTo(&root_client)));
  {
    base::Value::Dict params;
    params.Set("ping", 2);
    base::Value::Dict result;
    ASSERT_TRUE(StatusOk(
        red_client.SendCommandAndGetResult("method", params, &result)));
    EXPECT_EQ(result.FindInt("pong").value_or(-1), 2);
  }
  {
    base::Value::Dict params;
    params.Set("ping", 3);
    base::Value::Dict result;
    ASSERT_TRUE(StatusOk(
        blue_client.SendCommandAndGetResult("method", params, &result)));
    EXPECT_EQ(result.FindInt("pong").value_or(-1), 3);
  }
}

TEST_F(DevToolsClientImplTest, RoutingWithEvent) {
  const std::string blue_session = "blue_session";
  SocketHolder<MultiSessionMockSyncWebSocket2> socket_holder{blue_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl red_client("red_client", "red_session");
  DevToolsClientImpl blue_client("blue_client", blue_session);
  PingingListener blue_listener;
  blue_listener.SetPing(71);
  ASSERT_EQ(71, blue_listener.Ping());
  ASSERT_NE(71, blue_listener.Pong());
  blue_listener.AttachTo(&blue_client);
  ASSERT_TRUE(StatusOk(red_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(blue_client.AttachTo(&root_client)));
  {
    base::Value::Dict params;
    params.Set("ping", 12);
    base::Value::Dict result;
    ASSERT_TRUE(StatusOk(
        red_client.SendCommandAndGetResult("method", params, &result)));
    EXPECT_EQ(result.FindInt("pong").value_or(-1), 12);
  }

  EXPECT_EQ(71, blue_listener.Ping());
  EXPECT_EQ(71, blue_listener.Pong());
}

namespace {

class BidiMockSyncWebSocket : public MultiSessionMockSyncWebSocket {
 public:
  explicit BidiMockSyncWebSocket(std::string mapper_session)
      : mapper_session_(mapper_session) {}
  ~BidiMockSyncWebSocket() override = default;

  Status CreateDefaultBidiResponse(int cmd_id,
                                   std::string method,
                                   base::Value::Dict params,
                                   const std::string* channel,
                                   base::Value::Dict* response) {
    base::Value::Dict result;
    std::optional<int> ping = params.FindInt("ping");
    if (ping) {
      result.Set("pong", *ping);
    } else {
      result.Set("param", 1);
    }

    base::Value::Dict dict;
    dict.Set("id", std::move(cmd_id));
    dict.Set("result", std::move(result));
    if (channel) {
      dict.Set("channel", *channel);
    }
    *response = std::move(dict);
    return Status{kOk};
  }

  virtual bool OnPureCdpCommand(SessionState* session_state,
                                int cmd_id,
                                std::string method,
                                base::Value::Dict params,
                                std::string session_id) {
    return MultiSessionMockSyncWebSocket::OnUserCommand(
        session_state, cmd_id, method, std::move(params), session_id);
  }

  virtual Status CreateCdpOverBidiResponse(SessionState* session_state,
                                           int cmd_id,
                                           std::string cdp_method,
                                           base::Value::Dict cdp_params,
                                           std::string cdp_session,
                                           const std::string* channel,
                                           base::Value::Dict* response) {
    base::Value::Dict result;
    std::optional<int> ping = cdp_params.FindInt("ping");
    if (ping) {
      result.Set("pong", *ping);
    } else {
      result.Set("param", 1);
    }

    base::Value::Dict dict;
    dict.Set("id", cmd_id);
    dict.Set("result", std::move(result));
    if (channel) {
      dict.Set("channel", *channel);
    }
    if (!cdp_session.empty()) {
      dict.Set("cdpSession", std::move(cdp_session));
    }
    *response = std::move(dict);
    return Status{kOk};
  }

  virtual bool OnCdpOverBidiCommand(SessionState* session_state,
                                    int cmd_id,
                                    std::string cdp_method,
                                    base::Value::Dict cdp_params,
                                    std::string cdp_session,
                                    const std::string* channel) {
    EXPECT_STREQ("method", cdp_method.c_str());
    if (cdp_method != "method") {
      return false;
    }
    base::Value::Dict response;
    Status status = CreateCdpOverBidiResponse(
        session_state, cmd_id, std::move(cdp_method), std::move(cdp_params),
        std::move(cdp_session), channel, &response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    base::Value::Dict evt;
    status = WrapBidiResponseInCdpEvent(response, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    std::string message;
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    queued_response_.push(message);
    return true;
  }

  virtual bool OnPureBidiCommand(SessionState* session_state,
                                 int cmd_id,
                                 std::string method,
                                 base::Value::Dict params,
                                 const std::string* channel) {
    EXPECT_STREQ("method", method.c_str());
    base::Value::Dict bidi_response;
    Status status = CreateDefaultBidiResponse(
        cmd_id, std::move(method), std::move(params), channel, &bidi_response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    base::Value::Dict evt;
    status = WrapBidiResponseInCdpEvent(bidi_response, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    std::string message;
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(message);
    return true;
  }

  virtual bool OnBidiCommand(SessionState* session_state,
                             int cmd_id,
                             std::string method,
                             base::Value::Dict params,
                             const std::string* channel) {
    if (method == "cdp.sendCommand") {
      const std::string* cdp_method = params.FindString("cdpMethod");
      const std::string* cdp_session = params.FindString("cdpSession");
      const base::Value::Dict* cdp_params = params.FindDict("cdpParams");
      EXPECT_NE(cdp_method, nullptr);
      EXPECT_NE(cdp_session, nullptr);
      EXPECT_TRUE(cdp_params);
      if (!cdp_method || !cdp_session || !cdp_params) {
        return false;
      }
      return OnCdpOverBidiCommand(session_state, cmd_id, *cdp_method,
                                  cdp_params->Clone(), *cdp_session, channel);
    } else {
      return OnPureBidiCommand(session_state, cmd_id, std::move(method),
                               std::move(params), channel);
    }
  }

  Status CreateRuntimeEvaluateResponse(int cmd_id,
                                       std::string session_id,
                                       base::Value::Dict* response) {
    base::Value::Dict inner_result;
    inner_result.Set("type", "undefined");
    base::Value::Dict result;
    result.Set("result", std::move(inner_result));

    return CreateCdpResponse(cmd_id, std::move(result), session_id, response);
  }

  bool OnUserCommand(SessionState* session_state,
                     int cmd_id,
                     std::string method,
                     base::Value::Dict params,
                     std::string session_id) override {
    if (method != "Runtime.evaluate") {
      return OnPureCdpCommand(session_state, cmd_id, method, std::move(params),
                              session_id);
    }

    const std::string* expression = params.FindString("expression");
    EXPECT_NE(nullptr, expression);
    if (!expression) {
      return false;
    }

    static const std::string expected_exression_start = "onBidiMessage(";
    if (expression->size() <= expected_exression_start.size() + 1 ||
        expression->substr(0, expected_exression_start.size()) !=
            expected_exression_start ||
        expression->back() != ')') {
      return OnPureCdpCommand(session_state, cmd_id, method, std::move(params),
                              session_id);
    }

    EXPECT_EQ(session_id, mapper_session_);

    size_t count = expression->size() - expected_exression_start.size() - 1;
    std::string bidi_arg_str =
        expression->substr(expected_exression_start.size(), count);
    std::optional<base::Value> bidi_arg = base::JSONReader::Read(bidi_arg_str);
    EXPECT_TRUE(bidi_arg->is_string()) << bidi_arg_str;
    if (!bidi_arg->is_string()) {
      return false;
    }
    const std::string& bidi_expr_msg = bidi_arg->GetString();
    std::optional<base::Value> bidi_expr =
        base::JSONReader::Read(bidi_expr_msg);

    EXPECT_TRUE(bidi_expr) << bidi_expr_msg;
    EXPECT_TRUE(bidi_expr->is_dict()) << bidi_expr_msg;
    if (!bidi_expr || !bidi_expr->is_dict()) {
      return false;
    }

    const base::Value::Dict& bidi_dict = bidi_expr->GetDict();

    std::optional<int> bidi_cmd_id = bidi_dict.FindInt("id");
    const std::string* bidi_method = bidi_dict.FindString("method");
    const base::Value::Dict* bidi_params = bidi_dict.FindDict("params");
    const std::string* bidi_channel = bidi_dict.FindString("channel");
    EXPECT_TRUE(bidi_cmd_id);
    EXPECT_NE(bidi_method, nullptr);
    EXPECT_NE(bidi_params, nullptr);
    if (!bidi_cmd_id || !bidi_method || !bidi_params) {
      return false;
    }

    {
      // Runtime evaluate result
      base::Value::Dict response;
      Status status =
          CreateRuntimeEvaluateResponse(cmd_id, session_id, &response);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      std::string message;
      status = SerializeAsJson(response, &message);
      EXPECT_TRUE(status.IsOk()) << status.message();
      if (status.IsError()) {
        return false;
      }
      queued_response_.push(std::move(message));
    }

    return OnBidiCommand(session_state, *bidi_cmd_id, *bidi_method,
                         bidi_params->Clone(), bidi_channel);
  }

  std::string mapper_session_;
};

class MultiSessionMockSyncWebSocket3 : public BidiMockSyncWebSocket {
 public:
  explicit MultiSessionMockSyncWebSocket3(std::string mapper_session,
                                          int* wrapped_ping_counter)
      : BidiMockSyncWebSocket(mapper_session),
        wrapped_ping_counter_(wrapped_ping_counter) {}
  ~MultiSessionMockSyncWebSocket3() override = default;

  Status CreateCdpOverBidiResponse(SessionState* session_state,
                                   int cmd_id,
                                   std::string cdp_method,
                                   base::Value::Dict cdp_params,
                                   std::string cdp_session,
                                   const std::string* channel,
                                   base::Value::Dict* response) override {
    base::Value::Dict result;
    std::optional<int> ping = cdp_params.FindInt("wrapped-ping");
    EXPECT_TRUE(ping);
    if (!ping) {
      return Status{kUnknownError, "wrapped-ping is missing"};
    }
    result.Set("wrapped-pong", *ping);
    if (wrapped_ping_counter_) {
      ++(*wrapped_ping_counter_);
    }

    base::Value::Dict dict;
    dict.Set("id", cmd_id);
    dict.Set("result", std::move(result));
    if (channel) {
      dict.Set("channel", *channel);
    }
    if (!cdp_session.empty()) {
      dict.Set("cdpSession", std::move(cdp_session));
    }
    *response = std::move(dict);
    return Status{kOk};
  }

  raw_ptr<int> wrapped_ping_counter_ = nullptr;
};

class BidiEventListener : public DevToolsEventListener {
 public:
  BidiEventListener() = default;
  ~BidiEventListener() override = default;

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override {
    if (method != "Runtime.bindingCalled") {
      return Status(kOk);
    }

    const std::string* name = params.FindString("name");
    EXPECT_NE(name, nullptr);
    if (name == nullptr) {
      return Status{kUnknownError,
                    "name is missing in the Runtime.bindingCalled params"};
    }
    if (*name != "sendBidiResponse") {
      return Status{kOk};
    }

    const base::Value::Dict* payload = params.FindDict("payload");
    EXPECT_NE(payload, nullptr);
    if (payload == nullptr) {
      return Status{kUnknownError,
                    "payload is missing in the Runtime.bindingCalled params"};
    }

    payload_list.push_back(payload->Clone());

    return Status(kOk);
  }

  std::vector<base::Value::Dict> payload_list;
};

}  // namespace

TEST_F(DevToolsClientImplTest, BidiCommand) {
  std::string mapper_session = "mapper_session";
  SocketHolder<BidiMockSyncWebSocket> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  BidiEventListener bidi_listener;
  mapper_client.AddListener(&bidi_listener);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));
  base::Value::Dict params;
  params.Set("ping", 196);
  base::Value::Dict bidi_cmd;

  ASSERT_TRUE(StatusOk(
      CreateBidiCommand(111, "method", std::move(params), nullptr, &bidi_cmd)));
  ASSERT_TRUE(StatusOk(mapper_client.PostBidiCommand(std::move(bidi_cmd))));
  ASSERT_TRUE(StatusOk(mapper_client.HandleReceivedEvents()));

  ASSERT_EQ(static_cast<size_t>(1), bidi_listener.payload_list.size());
  const base::Value::Dict& payload = bidi_listener.payload_list.front();
  ASSERT_EQ(111, payload.FindInt("id").value_or(-1));
  ASSERT_EQ(196, payload.FindIntByDottedPath("result.pong").value_or(-1));
}

TEST_F(DevToolsClientImplTest, BidiCommandIds) {
  // DevToolsClientImpl changes command ids internally.
  // In this test we check that the response ids are restored in accordance to
  // the original command ids.
  std::string mapper_session = "mapper_session";
  SocketHolder<BidiMockSyncWebSocket> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  BidiEventListener bidi_listener;
  mapper_client.AddListener(&bidi_listener);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));

  for (int cmd_id : {2, 3, 11, 1000021, 1000022, 1000023}) {
    base::Value::Dict bidi_cmd;
    ASSERT_TRUE(StatusOk(CreateBidiCommand(
        cmd_id, "method", base::Value::Dict(), nullptr, &bidi_cmd)));
    ASSERT_TRUE(StatusOk(mapper_client.PostBidiCommand(std::move(bidi_cmd))));
    ASSERT_TRUE(StatusOk(mapper_client.HandleReceivedEvents()));
    ASSERT_EQ(static_cast<size_t>(1), bidi_listener.payload_list.size());
    const base::Value::Dict& payload = bidi_listener.payload_list.front();
    ASSERT_EQ(cmd_id, payload.FindInt("id").value_or(-1));
    bidi_listener.payload_list.clear();
  }
}

TEST_F(DevToolsClientImplTest, CdpCommandTunneling) {
  int wrapped_counter = 0;
  SocketHolder<MultiSessionMockSyncWebSocket3> socket_holder{"mapper_session",
                                                             &wrapped_counter};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl page_client("page_client", "blue_session");
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  ASSERT_TRUE(StatusOk(page_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  // Set the tunnel session after all connections to avoid handshake mocking
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));
  ASSERT_TRUE(
      StatusOk(page_client.SetTunnelSessionId(mapper_client.SessionId())));
  {
    base::Value::Dict params;
    params.Set("wrapped-ping", 13);
    base::Value::Dict result;
    ASSERT_TRUE(StatusOk(
        page_client.SendCommandAndGetResult("method", params, &result)));
    EXPECT_EQ(wrapped_counter, 1);
    EXPECT_EQ(result.FindInt("wrapped-pong").value_or(-1), 13);
  }
}

namespace {
class MultiSessionMockSyncWebSocket4 : public BidiMockSyncWebSocket {
 public:
  explicit MultiSessionMockSyncWebSocket4(std::string mapper_session)
      : BidiMockSyncWebSocket(mapper_session) {}
  ~MultiSessionMockSyncWebSocket4() override = default;

  bool OnCdpOverBidiCommand(SessionState* session_state,
                            int cmd_id,
                            std::string cdp_method,
                            base::Value::Dict cdp_params,
                            std::string cdp_session,
                            const std::string* channel) override {
    EXPECT_STREQ("method", cdp_method.c_str());
    if (cdp_method != "method") {
      return false;
    }

    base::Value::Dict cdp_evt_params;
    cdp_evt_params.Set("source", "cdp-over-bidi");
    base::Value::Dict bidi_params;
    bidi_params.Set("cdpParams", std::move(cdp_evt_params));
    bidi_params.Set("cdpSession", cdp_session);
    bidi_params.Set("cdpMethod", "event");
    base::Value::Dict bidi_evt;
    bidi_evt.Set("method", "cdp.eventReceived");
    bidi_evt.Set("params", std::move(bidi_params));
    if (channel) {
      bidi_evt.Set("channel", *channel);
    }
    base::Value::Dict evt;
    Status status = WrapBidiEventInCdpEvent(bidi_evt, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    std::string message;
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(message);

    base::Value::Dict response;
    status = CreateCdpOverBidiResponse(
        session_state, cmd_id, std::move(cdp_method), std::move(cdp_params),
        std::move(cdp_session), channel, &response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    status = WrapBidiResponseInCdpEvent(response, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(std::move(message));

    return true;
  }

  bool OnPureBidiCommand(SessionState* session_state,
                         int cmd_id,
                         std::string method,
                         base::Value::Dict params,
                         const std::string* channel) override {
    EXPECT_STREQ("method", method.c_str());
    if (method != "method") {
      return false;
    }

    base::Value::Dict bidi_evt;
    bidi_evt.Set("method", "event");
    base::Value::Dict bidi_evt_params;
    bidi_evt_params.Set("source", "bidi");
    bidi_evt.Set("params", std::move(bidi_evt_params));
    if (channel) {
      bidi_evt.Set("channel", *channel);
    }
    base::Value::Dict evt;
    Status status = WrapBidiEventInCdpEvent(bidi_evt, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    std::string message;
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(message);

    base::Value::Dict bidi_response;
    status = CreateDefaultBidiResponse(
        cmd_id, std::move(method), std::move(params), channel, &bidi_response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    status = WrapBidiResponseInCdpEvent(bidi_response, mapper_session_, &evt);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    status = SerializeAsJson(evt, &message);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }
    queued_response_.push(message);
    return true;
  }
};

class CdpEventListener : public DevToolsEventListener {
 public:
  CdpEventListener() = default;
  ~CdpEventListener() override = default;

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params_dv) override {
    base::Value::Dict data;
    data.Set("method", method);
    data.Set("params", params_dv.Clone());
    event_list.push_back(std::move(data));
    return Status(kOk);
  }

  std::vector<base::Value::Dict> event_list;
};

}  // namespace

TEST_F(DevToolsClientImplTest, BidiEvent) {
  std::string mapper_session = "mapper_session";
  SocketHolder<MultiSessionMockSyncWebSocket4> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  BidiEventListener bidi_listener;
  mapper_client.AddListener(&bidi_listener);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));
  base::Value::Dict bidi_cmd;
  ASSERT_TRUE(StatusOk(CreateBidiCommand(37, "method", base::Value::Dict(),
                                         nullptr, &bidi_cmd)));
  ASSERT_TRUE(StatusOk(mapper_client.PostBidiCommand(std::move(bidi_cmd))));
  ASSERT_TRUE(StatusOk(mapper_client.HandleReceivedEvents()));

  ASSERT_EQ(static_cast<size_t>(2), bidi_listener.payload_list.size());
  const base::Value::Dict& payload = bidi_listener.payload_list.front();
  EXPECT_THAT(payload.FindString("method"), Pointee(Eq("event")));
  EXPECT_THAT(payload.FindStringByDottedPath("params.source"),
              Pointee(Eq("bidi")));
}

TEST_F(DevToolsClientImplTest, BidiEventCrossRouting) {
  std::string mapper_session = "mapper_session";
  SocketHolder<MultiSessionMockSyncWebSocket4> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  // Green is the BiDiMapper in this test
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  DevToolsClientImpl page_client("page_client", "blue_session");
  BidiEventListener bidi_listener;
  mapper_client.AddListener(&bidi_listener);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(page_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));
  ASSERT_TRUE(
      StatusOk(page_client.SetTunnelSessionId(mapper_client.SessionId())));

  base::Value::Dict bidi_cmd;
  ASSERT_TRUE(StatusOk(CreateBidiCommand(414, "method", base::Value::Dict(),
                                         nullptr, &bidi_cmd)));
  // The messasge is dispatched from a non-BiDiMapper client
  ASSERT_TRUE(StatusOk(page_client.PostBidiCommand(std::move(bidi_cmd))));
  ASSERT_TRUE(StatusOk(page_client.HandleReceivedEvents()));

  // BiDi commands can be sent from any client
  // but events and responses must arrive to the BiDiMapper client
  ASSERT_EQ(static_cast<size_t>(2), bidi_listener.payload_list.size());
  const base::Value::Dict& payload = bidi_listener.payload_list.front();
  EXPECT_THAT(payload.FindString("method"), Pointee(Eq("event")));
  EXPECT_THAT(payload.FindStringByDottedPath("params.source"),
              Pointee(Eq("bidi")));
}

TEST_F(DevToolsClientImplTest, CdpEventTunneling) {
  std::string mapper_session = "mapper_session";
  SocketHolder<MultiSessionMockSyncWebSocket4> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl page_client("page_client", "red_session");
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  BidiEventListener mapper_bidi_listener;
  mapper_client.AddListener(&mapper_bidi_listener);
  CdpEventListener red_cdp_listener;
  page_client.AddListener(&red_cdp_listener);
  ASSERT_TRUE(StatusOk(page_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));
  ASSERT_TRUE(
      StatusOk(page_client.SetTunnelSessionId(mapper_client.SessionId())));
  base::Value::Dict result;
  page_client.SendCommandAndGetResult("method", base::Value::Dict(), &result);

  ASSERT_EQ(static_cast<size_t>(0), mapper_bidi_listener.payload_list.size());
  ASSERT_EQ(static_cast<size_t>(1), red_cdp_listener.event_list.size());
  const base::Value::Dict& payload = red_cdp_listener.event_list.front();
  EXPECT_THAT(payload.FindString("method"), Pointee(Eq("event")));
  EXPECT_THAT(payload.FindStringByDottedPath("params.source"),
              Pointee(Eq("cdp-over-bidi")));
}

TEST_F(DevToolsClientImplTest, BidiChannels) {
  // Corner cases for channels
  std::string mapper_session = "mapper_session";
  SocketHolder<MultiSessionMockSyncWebSocket4> socket_holder{mapper_session};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", mapper_session);
  BidiEventListener mapper_bidi_listener;
  mapper_client.AddListener(&mapper_bidi_listener);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AppointAsBidiServerForTesting()));

  for (std::string channel : {DevToolsClientImpl::kCdpTunnelChannel,
                              DevToolsClientImpl::kBidiChannelSuffix, ""}) {
    base::Value::Dict bidi_cmd;
    ASSERT_TRUE(StatusOk(CreateBidiCommand(1, "method", base::Value::Dict(),
                                           &channel, &bidi_cmd)));
    ASSERT_TRUE(StatusOk(mapper_client.PostBidiCommand(std::move(bidi_cmd))));
    ASSERT_TRUE(StatusOk(mapper_client.HandleReceivedEvents()));
    ASSERT_EQ(static_cast<size_t>(2), mapper_bidi_listener.payload_list.size());
    for (const base::Value::Dict& payload : mapper_bidi_listener.payload_list) {
      EXPECT_THAT(payload.FindString("channel"), Pointee(Eq(channel)));
    }
    mapper_bidi_listener.payload_list.clear();
  }

  // no channel case
  {
    base::Value::Dict bidi_cmd;
    ASSERT_TRUE(StatusOk(CreateBidiCommand(1, "method", base::Value::Dict(),
                                           nullptr, &bidi_cmd)));
    ASSERT_TRUE(StatusOk(mapper_client.PostBidiCommand(std::move(bidi_cmd))));
    ASSERT_TRUE(StatusOk(mapper_client.HandleReceivedEvents()));
    ASSERT_EQ(static_cast<size_t>(2), mapper_bidi_listener.payload_list.size());
    for (const base::Value::Dict& payload : mapper_bidi_listener.payload_list) {
      EXPECT_EQ(nullptr, payload.FindString("channel"));
    }
    mapper_bidi_listener.payload_list.clear();
  }
}

namespace {

struct BidiMapperState {
  // input
  bool fail_on_expose_devtools = false;
  bool fail_on_add_bidi_response_binding = false;
  bool fail_on_mapper_init = false;
  bool fail_on_mapper_run_instnace = false;
  bool fail_on_subscribe_to_cdp = false;
  // output
  bool devtools_exposed = false;
  bool send_bidi_response_binding_added = false;
  bool mapper_is_initiated = false;
  bool mapper_instance_is_running = false;
  bool subscribed_to_cdp = false;
};

class BidiServerMockSyncWebSocket : public BidiMockSyncWebSocket {
 public:
  explicit BidiServerMockSyncWebSocket(BidiMapperState* mapper_state)
      : BidiMockSyncWebSocket("mapper_session"), mapper_state_(mapper_state) {}
  ~BidiServerMockSyncWebSocket() override = default;

  bool OnPureCdpCommand(SessionState* session_state,
                        int cmd_id,
                        std::string method,
                        base::Value::Dict params,
                        std::string session_id) override {
    if (method == "Target.exposeDevToolsProtocol") {
      EXPECT_EQ("root_session", session_id);
      EXPECT_THAT(params.FindString("bindingName"), Pointee(Eq("cdp")));
      mapper_state_->devtools_exposed = true;
      if (mapper_state_->fail_on_expose_devtools) {
        return false;
      }
    } else if (method == "Runtime.addBinding") {
      EXPECT_EQ(mapper_session_, session_id);
      EXPECT_THAT(params.FindString("name"), Pointee(Eq("sendBidiResponse")));
      mapper_state_->send_bidi_response_binding_added = true;
      if (mapper_state_->fail_on_add_bidi_response_binding) {
        return false;
      }
    } else if (method == "Runtime.evaluate") {
      std::string* expression = params.FindString("expression");
      EXPECT_TRUE(expression != nullptr);
      if (expression == nullptr) {
        return false;
      }

      if (*expression == kTestMapperScript) {
        mapper_state_->mapper_is_initiated = true;
        if (mapper_state_->fail_on_mapper_init) {
          return false;
        }
      } else if (*expression == "window.runMapperInstance(\"mapper_client\")") {
        mapper_state_->mapper_instance_is_running = true;
        if (mapper_state_->fail_on_mapper_run_instnace) {
          return false;
        }
      }
    }
    base::Value::Dict response;
    EXPECT_TRUE(StatusOk(CreateCdpResponse(cmd_id, base::Value::Dict(),
                                           std::move(session_id), &response)));
    std::string message;
    EXPECT_TRUE(StatusOk(SerializeAsJson(response, &message)));
    queued_response_.push(std::move(message));
    return true;
  }

  bool OnPureBidiCommand(SessionState* session_state,
                         int cmd_id,
                         std::string method,
                         base::Value::Dict params,
                         const std::string* channel) override {
    EXPECT_NE(nullptr, channel);
    if (!channel) {
      return false;
    }
    EXPECT_EQ(DevToolsClientImpl::kCdpTunnelChannel, *channel);
    EXPECT_EQ("session.subscribe", method);
    EXPECT_THAT(params.FindString("events"), Pointee(Eq("cdp.eventReceived")));
    mapper_state_->subscribed_to_cdp = true;
    return !mapper_state_->fail_on_subscribe_to_cdp;
  }

  raw_ptr<BidiMapperState> mapper_state_ = nullptr;
};

}  // namespace

TEST_F(DevToolsClientImplTest, StartBidiServer) {
  BidiMapperState mapper_state;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(StatusOk(mapper_client.StartBidiServer(kTestMapperScript)));
  EXPECT_TRUE(mapper_state.devtools_exposed);
  EXPECT_TRUE(mapper_state.mapper_is_initiated);
  EXPECT_TRUE(mapper_state.mapper_instance_is_running);
  EXPECT_TRUE(mapper_state.send_bidi_response_binding_added);
  EXPECT_TRUE(mapper_state.subscribed_to_cdp);
}

TEST_F(DevToolsClientImplTest, StartBidiServerNotConnected) {
  BidiMapperState mapper_state;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(mapper_client.AttachTo(&root_client).IsError());

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerNotAPageClient) {
  BidiMapperState mapper_state;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerTunnelIsAlreadySet) {
  BidiMapperState mapper_state;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl pink_client("pink_client", "pink_session");
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(pink_client.AttachTo(&root_client)));
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));
  mapper_client.SetTunnelSessionId(pink_client.SessionId());

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerFailOnAddBidiResponseBinding) {
  BidiMapperState mapper_state;
  mapper_state.fail_on_add_bidi_response_binding = true;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerFailOnRunMapperInstnace) {
  BidiMapperState mapper_state;
  mapper_state.fail_on_mapper_run_instnace = true;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerFailOnExposeDevTools) {
  BidiMapperState mapper_state;
  mapper_state.fail_on_expose_devtools = true;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerFailOnMapperInit) {
  BidiMapperState mapper_state;
  mapper_state.fail_on_mapper_init = true;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}

TEST_F(DevToolsClientImplTest, StartBidiServerFailOnSubscribeToCdp) {
  BidiMapperState mapper_state;
  mapper_state.fail_on_subscribe_to_cdp = true;
  SocketHolder<BidiServerMockSyncWebSocket> socket_holder{&mapper_state};
  DevToolsClientImpl root_client("root", "root_session");
  ASSERT_TRUE(socket_holder.ConnectSocket());
  ASSERT_TRUE(StatusOk(root_client.SetSocket(socket_holder.Wrapper())));
  DevToolsClientImpl mapper_client("mapper_client", "mapper_session");
  mapper_client.EnableEventTunnelingForTesting();
  mapper_client.SetMainPage(true);
  ASSERT_TRUE(StatusOk(mapper_client.AttachTo(&root_client)));

  EXPECT_TRUE(mapper_client.StartBidiServer(kTestMapperScript).IsError());
}
