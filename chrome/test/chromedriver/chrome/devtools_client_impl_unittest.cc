// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <algorithm>
#include <list>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

bool ParseCommand(const base::Value::Dict& command,
                  int* cmd_id,
                  std::string* method,
                  base::Value::Dict* params,
                  std::string* session_id) {
  absl::optional<int> maybe_id = command.FindInt("id");
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
  absl::optional<base::Value> value = base::JSONReader::Read(message);
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

Status CreateCdpCommand(int cmd_id,
                        std::string method,
                        base::Value::Dict params,
                        std::string session_id,
                        base::Value::Dict* cmd) {
  base::Value::Dict dict;
  dict.Set("id", cmd_id);
  dict.Set("method", std::move(method));
  dict.Set("params", std::move(params));
  if (!session_id.empty()) {
    dict.Set("sessionId", std::move(session_id));
  }
  *cmd = std::move(dict);
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
  dict.Set("id", std::move(cmd_id));
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
                         base::Value::Dict* cmd) {
  return CreateCdpCommand(cmd_id, std::move(method), std::move(params),
                          std::string(), cmd);
}

Status CreateBidiResponse(int cmd_id,
                          base::Value::Dict result,
                          base::Value::Dict* resp) {
  return CreateCdpResponse(cmd_id, std::move(result), std::string(), resp);
}

Status WrapBidiCommandInCdpCommand(int cdp_cmd_id,
                                   const base::Value::Dict& bidi_cmd,
                                   std::string mapper_session_id,
                                   base::Value::Dict* cmd) {
  std::string bidi_cmd_str;
  if (!base::JSONWriter::Write(bidi_cmd, &bidi_cmd_str)) {
    return Status(kUnknownError, "cannot serialize a BiDi command");
  }
  std::string msg;
  if (!base::JSONWriter::Write(bidi_cmd_str, &msg)) {
    return Status(kUnknownError,
                  "cannot serialize the string: " + bidi_cmd_str);
  }
  std::string expression = "onBidiMessage(" + msg + ")";
  base::Value::Dict params;
  params.Set("expression", expression);
  return CreateCdpCommand(cdp_cmd_id, "Runtime.evaluate", std::move(params),
                          std::move(mapper_session_id), cmd);
}

Status WrapBidiEventInCdpEvent(const base::Value::Dict& bidi_resp,
                               std::string mapper_session_id,
                               base::Value::Dict* evt) {
  std::string payload;
  if (!base::JSONWriter::Write(bidi_resp, &payload)) {
    return Status(kUnknownError, "cannot serialize the string: " + payload);
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
  // response also arrives wrapped into a CDP event
  return WrapBidiEventInCdpEvent(bidi_resp, std::move(mapper_session_id), evt);
}

class MockSyncWebSocket : public SyncWebSocket {
 public:
  MockSyncWebSocket() = default;
  ~MockSyncWebSocket() override = default;

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

    if (connect_complete_) {
      EXPECT_STREQ("method", method.c_str());
      int param = params.FindInt("param").value_or(-1);
      EXPECT_EQ(1, param);
      EnqueueDefaultResponse(cmd_id);
    } else {
      EnqueueHandshakeResponse(cmd_id, method);
    }
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (timeout.IsExpired())
      return SyncWebSocket::StatusCode::kTimeout;
    EXPECT_TRUE(HasNextMessage());
    if (PopMessage(message)) {
      return SyncWebSocket::StatusCode::kOk;
    } else {
      return SyncWebSocket::StatusCode::kDisconnected;
    }
  }

  bool HasNextMessage() override { return !queued_response_.empty(); }

  void EnqueueDefaultResponse(int cmd_id) {
    base::Value::Dict response;
    response.Set("id", cmd_id);
    base::Value::Dict result;
    result.Set("param", 1);
    response.Set("result", std::move(result));
    std::string message;
    base::JSONWriter::Write(base::Value(std::move(response)), &message);
    queued_response_.push(std::move(message));
  }

  void EnqueueHandshakeResponse(int cmd_id, const std::string& method) {
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
    queued_response_.push(std::move(message));
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
  bool handshake_add_script_handled_ = false;
  bool handshake_runtime_eval_handled_ = false;
  bool connect_complete_ = false;
  std::queue<std::string> queued_response_;
};

template <typename T>
std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket() {
  return std::unique_ptr<SyncWebSocket>(new T());
}

class DevToolsClientImplTest : public testing::Test {
 protected:
  DevToolsClientImplTest() : long_timeout_(base::Minutes(5)) {}

  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, Ctor1) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  const std::string expected_id = "E2F4";
  const std::string expected_session_id = "BC80031";
  DevToolsClientImpl client(expected_id, expected_session_id);
  EXPECT_EQ(expected_id, client.GetId());
  EXPECT_EQ(expected_session_id, client.SessionId());
  EXPECT_FALSE(client.IsMainPage());
  EXPECT_FALSE(client.IsConnected());
  EXPECT_TRUE(client.IsNull());
  EXPECT_FALSE(client.WasCrashed());
  EXPECT_EQ(1, client.NextMessageId());
  EXPECT_EQ(nullptr, client.GetOwner());
  EXPECT_EQ(nullptr, client.GetParentClient());
  EXPECT_EQ(&client, client.GetRootClient());
}

TEST_F(DevToolsClientImplTest, Ctor2) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  const std::string expected_id = "E2F4";
  const std::string expected_session_id = "BC80031";
  DevToolsClientImpl client(expected_id, expected_session_id, "http://url",
                            factory);
  EXPECT_EQ(expected_id, client.GetId());
  EXPECT_EQ(expected_session_id, client.SessionId());
  EXPECT_FALSE(client.IsMainPage());
  EXPECT_FALSE(client.IsConnected());
  EXPECT_FALSE(client.IsNull());
  EXPECT_FALSE(client.WasCrashed());
  EXPECT_EQ(1, client.NextMessageId());
  EXPECT_EQ(nullptr, client.GetOwner());
  EXPECT_EQ(nullptr, client.GetParentClient());
  EXPECT_EQ(&client, client.GetRootClient());
}

TEST_F(DevToolsClientImplTest, SendCommand) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  params.Set("param", 1);
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
}

TEST_F(DevToolsClientImplTest, SendCommandAndGetResult) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  params.Set("param", 1);
  base::Value result;
  Status status = client.SendCommandAndGetResult("method", params, &result);
  ASSERT_EQ(kOk, status.code());
  std::string json;
  base::JSONWriter::Write(result, &json);
  ASSERT_STREQ("{\"param\":1}", json.c_str());
}

TEST_F(DevToolsClientImplTest, SetMainPage) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("E2F4", "BC80031", "http://url", factory);
  client.SetMainPage(true);
  EXPECT_TRUE(client.IsMainPage());
}

namespace {

class MockSyncWebSocket2 : public SyncWebSocket {
 public:
  MockSyncWebSocket2() = default;
  ~MockSyncWebSocket2() override = default;

  bool IsConnected() override { return false; }

  bool Connect(const GURL& url) override { return false; }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(false);
    return false;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    EXPECT_TRUE(false);
    return SyncWebSocket::StatusCode::kDisconnected;
  }

  bool HasNextMessage() override { return true; }
};

}  // namespace

TEST_F(DevToolsClientImplTest, ConnectIfNecessaryConnectFails) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket2>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kDisconnected, client.ConnectIfNecessary().code());
}

namespace {

class MockSyncWebSocket3 : public MockSyncWebSocket {
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

template <typename T>
std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket_B(bool b1) {
  return std::unique_ptr<SyncWebSocket>(new T(b1));
}

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandSendFails) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket_B<MockSyncWebSocket3>, false);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandReceiveNextMessageFails) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket_B<MockSyncWebSocket3>, true);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class FakeSyncWebSocket : public MockSyncWebSocket {
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
                   std::string* session_id,
                   internal::InspectorMessageType* type,
                   internal::InspectorEvent* event,
                   internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  session_id->clear();
  command_response->id = expected_id;
  command_response->result = std::make_unique<base::DictionaryValue>();
  return true;
}

bool ReturnBadResponse(const std::string& message,
                       int expected_id,
                       std::string* session_id,
                       internal::InspectorMessageType* type,
                       internal::InspectorEvent* event,
                       internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  session_id->clear();
  command_response->id = expected_id;
  command_response->result = std::make_unique<base::DictionaryValue>();
  return false;
}

bool ReturnCommandBadId(const std::string& message,
                        int expected_id,
                        std::string* session_id,
                        internal::InspectorMessageType* type,
                        internal::InspectorEvent* event,
                        internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  session_id->clear();
  command_response->id = expected_id + 100;
  command_response->result = std::make_unique<base::DictionaryValue>();
  return true;
}

bool ReturnUnexpectedIdThenResponse(
    bool* first,
    const std::string& message,
    int expected_id,
    std::string* session_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  session_id->clear();
  if (*first) {
    *type = internal::kCommandResponseMessageType;
    command_response->id = expected_id + 100;
    command_response->error = "{\"code\":-32001,\"message\":\"ERR\"}";
  } else {
    *type = internal::kCommandResponseMessageType;
    command_response->id = expected_id;
    base::DictionaryValue params;
    command_response->result = std::make_unique<base::DictionaryValue>();
    command_response->result->GetDict().Set("key", 2);
  }
  *first = false;
  return true;
}

bool ReturnCommandError(const std::string& message,
                        int expected_id,
                        std::string* session_id,
                        internal::InspectorMessageType* type,
                        internal::InspectorEvent* event,
                        internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  session_id->clear();
  command_response->id = expected_id;
  command_response->error = "err";
  return true;
}

class MockListener : public DevToolsEventListener {
 public:
  MockListener() : called_(false) {}
  ~MockListener() override { EXPECT_TRUE(called_); }

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    called_ = true;
    EXPECT_STREQ("method", method.c_str());
    EXPECT_TRUE(params.GetDict().Find("key"));
    return Status(kOk);
  }

 private:
  bool called_;
};

bool ReturnEventThenResponse(
    bool* first,
    const std::string& message,
    int expected_id,
    std::string* session_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  session_id->clear();
  if (*first) {
    *type = internal::kEventMessageType;
    event->method = "method";
    event->params = std::make_unique<base::DictionaryValue>();
    event->params->GetDict().Set("key", 1);
  } else {
    *type = internal::kCommandResponseMessageType;
    command_response->id = expected_id;
    base::DictionaryValue params;
    command_response->result = std::make_unique<base::DictionaryValue>();
    command_response->result->GetDict().Set("key", 2);
  }
  *first = false;
  return true;
}

bool ReturnEvent(const std::string& message,
                 int expected_id,
                 std::string* session_id,
                 internal::InspectorMessageType* type,
                 internal::InspectorEvent* event,
                 internal::InspectorCommandResponse* command_response) {
  *type = internal::kEventMessageType;
  event->method = "method";
  event->params = std::make_unique<base::DictionaryValue>();
  event->params->GetDict().Set("key", 1);
  return true;
}

bool ReturnOutOfOrderResponses(
    int* recurse_count,
    DevToolsClient* client,
    const std::string& message,
    int expected_id,
    std::string* session_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  int key = 0;
  base::Value::Dict params;
  params.Set("param", 1);
  switch ((*recurse_count)++) {
    case 0:
      client->SendCommand("method", params);
      *type = internal::kEventMessageType;
      event->method = "method";
      event->params = std::make_unique<base::DictionaryValue>();
      event->params->GetDict().Set("key", 1);
      return true;
    case 1:
      command_response->id = expected_id - 1;
      key = 2;
      break;
    case 2:
      command_response->id = expected_id;
      key = 3;
      break;
  }
  *type = internal::kCommandResponseMessageType;
  command_response->result = std::make_unique<base::DictionaryValue>();
  command_response->result->GetDict().Set("key", key);
  return true;
}

bool ReturnError(const std::string& message,
                 int expected_id,
                 std::string* session_id,
                 internal::InspectorMessageType* type,
                 internal::InspectorEvent* event,
                 internal::InspectorCommandResponse* command_response) {
  return false;
}

Status AlwaysTrue(bool* is_met) {
  *is_met = true;
  return Status(kOk);
}

Status AlwaysError(bool* is_met) {
  return Status(kUnknownError);
}

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandOnlyConnectsOnce) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommand));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
}

TEST_F(DevToolsClientImplTest, SendCommandBadResponse) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnBadResponse));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandBadId) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommandBadId));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandUnexpectedId) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  bool first = true;
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnUnexpectedIdThenResponse, &first));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
}

TEST_F(DevToolsClientImplTest, SendCommandResponseError) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommandError));
  base::Value::Dict params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandEventBeforeResponse) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  MockListener listener;
  bool first = true;
  DevToolsClientImpl client("id", "", "http://url", factory);
  client.AddListener(&listener);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnEventThenResponse, &first));
  base::Value::Dict params;
  base::Value result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  ASSERT_TRUE(result.is_dict());
  absl::optional<int> key = result.GetDict().FindInt("key");
  ASSERT_TRUE(key);
  ASSERT_EQ(2, key.value());
}

TEST(ParseInspectorMessage, NonJson) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage("hi", 0, &session_id, &type,
                                               &event, &response));
}

TEST(ParseInspectorMessage, NeitherCommandNorEvent) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_FALSE(internal::ParseInspectorMessage("{}", 0, &session_id, &type,
                                               &event, &response));
}

TEST(ParseInspectorMessage, EventNoParams) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\"}", 0, &session_id, &type, &event, &response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  ASSERT_TRUE(event.params->is_dict());
}

TEST(ParseInspectorMessage, EventNoParamsWithSessionId) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\",\"sessionId\":\"B221AF2\"}", 0, &session_id,
      &type, &event, &response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  ASSERT_TRUE(event.params->is_dict());
  EXPECT_EQ("B221AF2", session_id);
}

TEST(ParseInspectorMessage, EventWithParams) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\",\"params\":{\"key\":100},\"sessionId\":\"AB3A\"}",
      0, &session_id, &type, &event, &response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  int key = event.params->GetDict().FindInt("key").value_or(-1);
  ASSERT_EQ(100, key);
  EXPECT_EQ("AB3A", session_id);
}

TEST(ParseInspectorMessage, CommandNoErrorOrResult) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  // As per Chromium issue 392577, DevTools does not necessarily return a
  // "result" dictionary for every valid response. If neither "error" nor
  // "result" keys are present, a blank result dictionary should be inferred.
  ASSERT_TRUE(
      internal::ParseInspectorMessage("{\"id\":1,\"sessionId\":\"AB2AF3C\"}", 0,
                                      &session_id, &type, &event, &response));
  ASSERT_TRUE(response.result->DictEmpty());
  EXPECT_EQ("AB2AF3C", session_id);
}

TEST(ParseInspectorMessage, CommandError) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"id\":1,\"error\":{}}", 0, &session_id, &type, &event, &response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_TRUE(response.error.length());
  ASSERT_FALSE(response.result);
}

TEST(ParseInspectorMessage, Command) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  std::string session_id;
  ASSERT_TRUE(
      internal::ParseInspectorMessage("{\"id\":1,\"result\":{\"key\":1}}", 0,
                                      &session_id, &type, &event, &response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_FALSE(response.error.length());
  int key = response.result->GetDict().FindInt("key").value_or(-1);
  ASSERT_EQ(1, key);
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

TEST_F(DevToolsClientImplTest, HandleEventsUntil) {
  MockListener listener;
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  client.AddListener(&listener);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kOk, status.code());
}

TEST_F(DevToolsClientImplTest, HandleEventsUntilTimeout) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(base::TimeDelta()));
  ASSERT_EQ(kTimeout, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventCommand) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnCommand));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventError) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnError));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventConditionalFuncReturnsError) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(base::BindRepeating(&ReturnEvent));
  Status status = client.HandleEventsUntil(base::BindRepeating(&AlwaysError),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, NestedCommandsWithOutOfOrderResults) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  int recurse_count = 0;
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(
      base::BindRepeating(&ReturnOutOfOrderResponses, &recurse_count, &client));
  base::Value::Dict params;
  params.Set("param", 1);
  base::Value result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  ASSERT_TRUE(result.is_dict());
  absl::optional<int> key = result.GetDict().FindInt("key");
  ASSERT_TRUE(key);
  ASSERT_EQ(2, key.value());
}

namespace {

class OnConnectedListener : public DevToolsEventListener {
 public:
  OnConnectedListener(const std::string& method, DevToolsClient* client)
      : method_(method),
        client_(client),
        on_connected_called_(false),
        on_event_called_(false) {
    client_->AddListener(this);
  }
  ~OnConnectedListener() override {}

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
                 const base::DictionaryValue& params) override {
    EXPECT_EQ(client_, client);
    EXPECT_STREQ("onconnected-id", client->GetId().c_str());
    EXPECT_TRUE(on_connected_called_);
    on_event_called_ = true;
    return Status(kOk);
  }

 private:
  std::string method_;
  raw_ptr<DevToolsClient> client_;
  bool on_connected_called_;
  bool on_event_called_;
};

class OnConnectedSyncWebSocket : public MockSyncWebSocket {
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
      response.Set("result", base::DictionaryValue());
      std::string json_response;
      base::JSONWriter::Write(base::Value(std::move(response)), &json_response);
      queued_response_.push(std::move(json_response));

      // Push one event.
      base::Value::Dict event;
      event.Set("method", "updateEvent");
      event.Set("params", base::DictionaryValue());
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
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<OnConnectedSyncWebSocket>);
  DevToolsClientImpl client("onconnected-id", "", "http://url", factory);
  OnConnectedListener listener1("DOM.getDocument", &client);
  OnConnectedListener listener2("Runtime.enable", &client);
  OnConnectedListener listener3("Page.enable", &client);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  EXPECT_EQ(kOk, client.SendCommand("Runtime.execute", params).code());
  listener1.VerifyCalled();
  listener2.VerifyCalled();
  listener3.VerifyCalled();
}

TEST_F(DevToolsClientImplTest, ProcessOnConnectedFirstOnHandleEventsUntil) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<OnConnectedSyncWebSocket>);
  DevToolsClientImpl client("onconnected-id", "", "http://url", factory);
  OnConnectedListener listener1("DOM.getDocument", &client);
  OnConnectedListener listener2("Runtime.enable", &client);
  OnConnectedListener listener3("Page.enable", &client);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  EXPECT_EQ(kOk, client.HandleReceivedEvents().code());
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
  OtherEventListener() : received_event_(false) {}
  ~OtherEventListener() override {}

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    received_event_ = true;
    return Status(kOk);
  }

  bool received_event_;
};

class OnEventListener : public DevToolsEventListener {
 public:
  OnEventListener(DevToolsClient* client,
                  OtherEventListener* other_listener)
      : client_(client),
        other_listener_(other_listener) {}
  ~OnEventListener() override {}

  Status OnConnected(DevToolsClient* client) override {
    EXPECT_EQ(client_, client);
    return Status(kOk);
  }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    EXPECT_EQ(client_, client);
    client_->SendCommand("method", params.GetDict());
    EXPECT_TRUE(other_listener_->received_event_);
    return Status(kOk);
  }

 private:
  raw_ptr<DevToolsClient> client_;
  raw_ptr<OtherEventListener> other_listener_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, ProcessOnEventFirst) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket5>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  OtherEventListener listener2;
  OnEventListener listener1(&client, &listener2);
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  Status status = client.ConnectIfNecessary();
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict params;
  EXPECT_EQ(kOk, client.SendCommand("method", params).code());
}

namespace {

class DisconnectedSyncWebSocket : public MockSyncWebSocket {
 public:
  DisconnectedSyncWebSocket() = default;
  ~DisconnectedSyncWebSocket() override = default;

  bool Connect(const GURL& url) override {
    connection_count_++;
    connected_ = connection_count_ != 2;
    return connected_;
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
      command_count_++;
      if (command_count_ == 1) {
        connected_ = false;
        handshake_add_script_handled_ = false;
        handshake_runtime_eval_handled_ = false;
        connect_complete_ = false;
        while (!queued_response_.empty()) {
          queued_response_.pop();
        }
        return false;
      }
      return MockSyncWebSocket::Send(message);
    } else {
      EnqueueHandshakeResponse(cmd_id, method);
    }
    return true;
  }

 private:
  int connection_count_ = 0;
  int command_count_ = 0;
};

Status CheckCloserFuncCalled(bool* is_called) {
  *is_called = true;
  return Status(kOk);
}

}  // namespace

TEST_F(DevToolsClientImplTest, Reconnect) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<DisconnectedSyncWebSocket>);
  bool is_called = false;
  DevToolsClientImpl client("id", "", "http://url", factory);
  client.SetFrontendCloserFunc(
      base::BindRepeating(&CheckCloserFuncCalled, &is_called));
  ASSERT_FALSE(is_called);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  ASSERT_FALSE(is_called);
  base::Value::Dict params;
  params.Set("param", 1);
  is_called = false;
  ASSERT_EQ(kDisconnected, client.SendCommand("method", params).code());
  ASSERT_FALSE(is_called);
  ASSERT_EQ(kDisconnected, client.HandleReceivedEvents().code());
  ASSERT_FALSE(is_called);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  ASSERT_TRUE(is_called);
  is_called = false;
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
  ASSERT_FALSE(is_called);
}

namespace {

class MockSyncWebSocket6 : public MockSyncWebSocket {
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
  raw_ptr<std::list<std::string>> messages_;
  bool connected_ = false;
};

class MockDevToolsEventListener : public DevToolsEventListener {
 public:
  MockDevToolsEventListener() = default;
  ~MockDevToolsEventListener() override = default;

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    DevToolsClientImpl* client_impl = static_cast<DevToolsClientImpl*>(client);
    int msg_id = client_impl->NextMessageId();

    Status status = client->SendCommand("hello", params.GetDict());

    if (msg_id == expected_blocked_id_) {
      EXPECT_EQ(kUnexpectedAlertOpen, status.code());
    } else {
      EXPECT_EQ(kOk, status.code());
    }
    return Status(kOk);
  }

  void SetExpectedBlockedId(int value) { expected_blocked_id_ = value; }

 private:
  int expected_blocked_id_ = -1;
};

std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket6(
    std::list<std::string>* messages) {
  return std::make_unique<MockSyncWebSocket6>(messages);
}

}  // namespace

TEST_F(DevToolsClientImplTest, BlockedByAlert) {
  std::list<std::string> msgs;
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client("id", "", "http://url", factory);
  Status status = client.ConnectIfNecessary();
  ASSERT_EQ(kOk, status.code()) << status.message();
  msgs.push_back(
      "{\"method\": \"Page.javascriptDialogOpening\", \"params\": {}}");
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
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client("id", "", "http://url", factory);
  MockDevToolsEventListener listener;
  client.AddListener(&listener);
  Status status = client.ConnectIfNecessary();
  ASSERT_EQ(kOk, status.code()) << status.message();
  int next_msg_id = client.NextMessageId();
  msgs.push_back("{\"method\": \"FirstEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"SecondEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"ThirdEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"FourthEvent\", \"params\": {}}");
  msgs.push_back((std::stringstream()
                  << "{\"id\": " << next_msg_id++ << ", \"result\": {}}")
                     .str());
  msgs.push_back(
      "{\"method\": \"Page.javascriptDialogOpening\", \"params\": {}}");
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

namespace {

class MockCommandListener : public DevToolsEventListener {
 public:
  MockCommandListener() {}
  ~MockCommandListener() override {}

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    msgs_.push_back(method);
    return Status(kOk);
  }

  Status OnCommandSuccess(DevToolsClient* client,
                          const std::string& method,
                          const base::DictionaryValue* result,
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
  EXPECT_EQ(kOk, client->HandleReceivedEvents().code());
}

}  // namespace

TEST_F(DevToolsClientImplTest, ReceivesCommandResponse) {
  std::list<std::string> msgs;
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client("id", "", "http://url", factory);
  MockCommandListener listener1;
  listener1.callback_ = base::BindRepeating(&HandleReceivedEvents);
  MockCommandListener listener2;
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  Status status = client.ConnectIfNecessary();
  ASSERT_EQ(kOk, status.code()) << status.message();
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

namespace {

class MockSyncWebSocket7 : public SyncWebSocket {
 public:
  MockSyncWebSocket7() = default;
  ~MockSyncWebSocket7() override = default;

  bool IsConnected() override { return true; }

  bool Connect(const GURL& url) override { return true; }

  bool Send(const std::string& message) override {
    absl::optional<base::Value> value = base::JSONReader::Read(message);
    base::Value::Dict* dict = value->GetIfDict();
    EXPECT_TRUE(dict);
    if (!dict)
      return false;
    absl::optional<int> maybe_id = dict->FindInt("id");
    EXPECT_TRUE(maybe_id);
    if (!maybe_id)
      return false;
    id_ = *maybe_id;
    std::string* method = dict->FindString("method");
    EXPECT_TRUE(method);
    EXPECT_STREQ("method", method->c_str());
    base::Value::Dict* params = dict->FindDict("params");
    if (!params)
      return false;
    sent_messages_++;
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    EXPECT_LE(sent_responses_, 1);
    EXPECT_EQ(sent_messages_, 2);
    base::Value::Dict response;
    response.Set("id", (sent_responses_ == 0) ? 1 : 2);
    base::Value result{base::Value::Type::DICT};
    result.GetDict().Set("param", 1);
    response.Set("result", result.Clone());
    base::JSONWriter::Write(base::Value(std::move(response)), message);
    sent_responses_++;
    return SyncWebSocket::StatusCode::kOk;
  }

  bool HasNextMessage() override { return sent_messages_ > sent_responses_; }

 private:
  int id_ = -1;
  int sent_messages_ = 0;
  int sent_responses_ = 0;
};

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandAndIgnoreResponse) {
  SyncWebSocketFactory factory =
      base::BindRepeating(&CreateMockSyncWebSocket<MockSyncWebSocket7>);
  DevToolsClientImpl client("id", "", "http://url", factory);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  params.Set("param", 1);
  ASSERT_EQ(kOk, client.SendCommandAndIgnoreResponse("method", params).code());
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
}

namespace {

struct SessionState {
  bool handshake_add_script_handled = false;
  bool handshake_runtime_eval_handled = false;
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
    if (!HasNextMessage() && timeout.IsExpired())
      return SyncWebSocket::StatusCode::kTimeout;
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
    absl::optional<int> ping = params.FindInt("ping");
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
                 const base::DictionaryValue& event_params) override {
    if (event_handled_) {
      return Status{kOk};
    }

    EXPECT_EQ(client_, client);
    EXPECT_EQ(method, "event");

    base::Value::Dict params;
    params.Set("ping", ping_);
    base::Value result;
    event_handled_ = true;
    Status status = client_->SendCommandAndGetResult("method", params, &result);
    EXPECT_EQ(kOk, status.code());
    if (!status.IsOk()) {
      return status;
    }
    EXPECT_TRUE(result.is_dict());
    if (!result.is_dict()) {
      return Status{kUnknownError, "result is not a dictionary"};
    }
    absl::optional<int> pong = result.GetDict().FindInt("pong");
    EXPECT_TRUE(pong);
    if (pong) {
      pong_ = *pong;
    } else {
      return Status{kUnknownError, "result does not contain 'pong' field"};
    }
    return Status(kOk);
  }

 private:
  raw_ptr<DevToolsClient> client_;
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

template <class T>
std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket_S(
    const std::string& arg) {
  return std::make_unique<T>(arg);
}

}  // namespace

TEST_F(DevToolsClientImplTest, RoutingChildParent) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket<MultiSessionMockSyncWebSocket>);
  DevToolsClientImpl root_client("root", "root_session", "http://url", factory);
  DevToolsClientImpl client("child", "child_session");
  client.AttachTo(&root_client);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::Value::Dict params;
  params.Set("param", 1);
  EXPECT_EQ(kOk, client.SendCommand("method", params).code());
}

TEST_F(DevToolsClientImplTest, RoutingTwoChildren) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket<MultiSessionMockSyncWebSocket>);
  DevToolsClientImpl root_client("root", "root_session", "http://url", factory);
  DevToolsClientImpl red_client("red_client", "red_session");
  DevToolsClientImpl blue_client("blue_client", "blue_session");
  red_client.AttachTo(&root_client);
  blue_client.AttachTo(&root_client);
  ASSERT_EQ(kOk, blue_client.ConnectIfNecessary().code());
  ASSERT_EQ(kOk, red_client.ConnectIfNecessary().code());
  {
    base::Value::Dict params;
    params.Set("ping", 2);
    base::Value result;
    ASSERT_EQ(
        kOk,
        red_client.SendCommandAndGetResult("method", params, &result).code());
    ASSERT_TRUE(result.is_dict());
    absl::optional<int> pong = result.GetDict().FindInt("pong");
    ASSERT_TRUE(pong);
    EXPECT_EQ(2, *pong);
  }
  {
    base::Value::Dict params;
    params.Set("ping", 3);
    base::Value result;
    ASSERT_EQ(
        kOk,
        blue_client.SendCommandAndGetResult("method", params, &result).code());
    ASSERT_TRUE(result.is_dict());
    absl::optional<int> pong = result.GetDict().FindInt("pong");
    ASSERT_TRUE(pong);
    EXPECT_EQ(3, *pong);
  }
}

TEST_F(DevToolsClientImplTest, RoutingWithEvent) {
  const std::string blue_session = "blue_session";
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket_S<MultiSessionMockSyncWebSocket2>, blue_session);
  DevToolsClientImpl root_client("root", "root_session", "http://url", factory);
  DevToolsClientImpl red_client("red_client", "red_session");
  DevToolsClientImpl blue_client("blue_client", blue_session);
  red_client.AttachTo(&root_client);
  blue_client.AttachTo(&root_client);
  PingingListener blue_listener;
  blue_listener.SetPing(71);
  EXPECT_EQ(71, blue_listener.Ping());
  EXPECT_NE(71, blue_listener.Pong());
  blue_listener.AttachTo(&blue_client);
  ASSERT_EQ(kOk, blue_client.ConnectIfNecessary().code());
  ASSERT_EQ(kOk, red_client.ConnectIfNecessary().code());
  {
    base::Value::Dict params;
    params.Set("ping", 12);
    base::Value result;
    ASSERT_EQ(
        kOk,
        red_client.SendCommandAndGetResult("method", params, &result).code());
    ASSERT_TRUE(result.is_dict());
    absl::optional<int> pong = result.GetDict().FindInt("pong");
    ASSERT_TRUE(pong);
    EXPECT_EQ(12, *pong);
  }

  EXPECT_EQ(71, blue_listener.Ping());
  EXPECT_EQ(71, blue_listener.Pong());
}

namespace {

class BidiMockSyncWebSocket : public MultiSessionMockSyncWebSocket {
 public:
  explicit BidiMockSyncWebSocket(std::string wrapper_session)
      : wrapper_session_(wrapper_session) {}
  ~BidiMockSyncWebSocket() override = default;

  Status CreateDefaultBidiResponse(int cmd_id,
                                   std::string method,
                                   base::Value::Dict params,
                                   base::Value::Dict* response) {
    base::Value::Dict result;
    absl::optional<int> ping = params.FindInt("ping");
    if (ping) {
      result.Set("pong", *ping);
    } else {
      result.Set("param", 1);
    }

    return CreateBidiResponse(cmd_id, std::move(result), response);
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
                                           std::string method,
                                           base::Value::Dict params,
                                           std::string session_id,
                                           base::Value::Dict* response) {
    return CreateDefaultCdpResponse(cmd_id, std::move(method),
                                    std::move(params), std::move(session_id),
                                    response);
  }

  virtual bool OnCdpOverBidiCommand(SessionState* session_state,
                                    int cmd_id,
                                    std::string method,
                                    base::Value::Dict params,
                                    std::string session_id) {
    EXPECT_STREQ("method", method.c_str());
    EXPECT_GE(cmd_id, 0);
    if (method != "method" || cmd_id < 0) {
      return false;
    }
    base::Value::Dict response;
    Status status = CreateCdpOverBidiResponse(
        session_state, -cmd_id, std::move(method), std::move(params),
        std::move(session_id), &response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    base::Value::Dict evt;
    status = WrapBidiResponseInCdpEvent(response, wrapper_session_, &evt);
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
                                 base::Value::Dict params) {
    EXPECT_STREQ("method", method.c_str());
    base::Value::Dict bidi_response;
    Status status = CreateDefaultBidiResponse(
        cmd_id, std::move(method), std::move(params), &bidi_response);
    EXPECT_TRUE(status.IsOk()) << status.message();
    if (status.IsError()) {
      return false;
    }

    base::Value::Dict evt;
    status = WrapBidiResponseInCdpEvent(bidi_response, wrapper_session_, &evt);
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
                             base::Value::Dict params) {
    if (method == "PROTO.cdp.sendCommand") {
      const std::string* cdp_method = params.FindString("cdpMethod");
      const std::string* cdp_session = params.FindString("cdpSession");
      const base::Value::Dict* cdp_params = params.FindDict("cdpParams");
      EXPECT_NE(cdp_method, nullptr);
      EXPECT_NE(cdp_session, nullptr);
      EXPECT_TRUE(cdp_params);
      if (!cdp_method || !cdp_session || !cdp_params) {
        return false;
      }
      // The service bidi commands must always have negative id
      EXPECT_LT(cmd_id, 0);
      if (cmd_id >= 0) {
        return false;
      }
      return OnCdpOverBidiCommand(session_state, -cmd_id, *cdp_method,
                                  cdp_params->Clone(), *cdp_session);
    } else {
      return OnPureBidiCommand(session_state, cmd_id, std::move(method),
                               std::move(params));
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

    EXPECT_EQ(session_id, wrapper_session_);

    size_t count = expression->size() - expected_exression_start.size() - 1;
    std::string bidi_arg_str =
        expression->substr(expected_exression_start.size(), count);
    absl::optional<base::Value> bidi_arg = base::JSONReader::Read(bidi_arg_str);
    EXPECT_TRUE(bidi_arg->is_string()) << bidi_arg_str;
    if (!bidi_arg->is_string()) {
      return false;
    }
    const std::string& bidi_expr_msg = bidi_arg->GetString();
    absl::optional<base::Value> bidi_expr =
        base::JSONReader::Read(bidi_expr_msg);

    EXPECT_TRUE(bidi_expr) << bidi_expr_msg;
    EXPECT_TRUE(bidi_expr->is_dict()) << bidi_expr_msg;
    if (!bidi_expr || !bidi_expr->is_dict()) {
      return false;
    }

    const base::Value::Dict& bidi_dict = bidi_expr->GetDict();

    absl::optional<int> bidi_cmd_id = bidi_dict.FindInt("id");
    const std::string* bidi_method = bidi_dict.FindString("method");
    const base::Value::Dict* bidi_params = bidi_dict.FindDict("params");
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
                         bidi_params->Clone());
  }

  std::string wrapper_session_;
};

class BidiEventListener : public DevToolsEventListener {
 public:
  BidiEventListener() = default;
  ~BidiEventListener() override = default;

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params_dv) override {
    if (method != "Runtime.bindingCalled") {
      return Status(kOk);
    }

    const base::Value::Dict& params = params_dv.GetDict();

    const std::string* name = params.FindString("name");
    EXPECT_NE(name, nullptr);
    if (name == nullptr) {
      return Status{kUnknownError,
                    "name is missing in the Runtime.bindingCalled params"};
    }
    if (*name != "sendBidiResponse") {
      return Status{kOk};
    }

    const std::string* payload = params.FindString("payload");
    EXPECT_NE(payload, nullptr);
    if (payload == nullptr) {
      return Status{kUnknownError,
                    "payload is missing in the Runtime.bindingCalled params"};
    }

    absl::optional<base::Value> value = base::JSONReader::Read(*payload);
    EXPECT_TRUE(value);
    if (!value) {
      return Status{kUnknownError, "unable to deserialize the event payload"};
    }

    EXPECT_TRUE(value->is_dict());
    if (!value->is_dict()) {
      return Status{kUnknownError, "event payload is not a dictionary"};
    }

    payload_list.push_back(value->GetDict().Clone());

    return Status(kOk);
  }

  std::vector<base::Value::Dict> payload_list;
};

}  // namespace

TEST_F(DevToolsClientImplTest, BidiCommand) {
  std::string mapper_session = "mapper_session";
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket_S<BidiMockSyncWebSocket>, mapper_session);
  DevToolsClientImpl root_client("root", "root_session", "http://url", factory);
  DevToolsClientImpl red_client("red_client", mapper_session);
  BidiEventListener bidi_listener;
  red_client.AddListener(&bidi_listener);
  red_client.AttachTo(&root_client);
  root_client.ConnectIfNecessary();
  red_client.ConnectIfNecessary();
  base::Value::Dict params;
  params.Set("ping", 196);
  base::Value::Dict bidi_cmd;
  Status status =
      CreateBidiCommand(111, "method", std::move(params), &bidi_cmd);
  ASSERT_TRUE(status.IsOk()) << status.message();
  base::Value::Dict cmd;
  status = WrapBidiCommandInCdpCommand(225, bidi_cmd, mapper_session, &cmd);
  ASSERT_TRUE(status.IsOk()) << status.message();

  int cdp_cmd_id;
  std::string cdp_method;
  base::Value::Dict cdp_params;
  std::string cdp_session_id;
  ASSERT_TRUE(ParseCommand(cmd, &cdp_cmd_id, &cdp_method, &cdp_params,
                           &cdp_session_id));

  base::Value result;
  status = red_client.SendCommandAndGetResult(cdp_method, cdp_params, &result);
  ASSERT_TRUE(status.IsOk()) << status.message();
  ASSERT_TRUE(result.is_dict());
  const std::string* result_type =
      result.GetDict().FindStringByDottedPath("result.type");
  ASSERT_NE(nullptr, result_type);
  ASSERT_EQ("undefined", *result_type);

  status = red_client.HandleReceivedEvents();
  ASSERT_TRUE(status.IsOk()) << status.message();

  ASSERT_EQ(static_cast<size_t>(1), bidi_listener.payload_list.size());
  const base::Value::Dict& payload = bidi_listener.payload_list.front();
  ASSERT_EQ(111, payload.FindInt("id").value_or(-1));
  ASSERT_EQ(196, payload.FindIntByDottedPath("result.pong").value_or(-1));
}
