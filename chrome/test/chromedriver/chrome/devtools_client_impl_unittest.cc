// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <list>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

Status CloserFunc() {
  return Status(kOk);
}

class MockSyncWebSocket : public SyncWebSocket {
 public:
  MockSyncWebSocket() : connected_(false), id_(-1), queued_messages_(1) {}
  ~MockSyncWebSocket() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(message);
    base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(value->GetAsDictionary(&dict));
    if (!dict)
      return false;
    EXPECT_TRUE(dict->GetInteger("id", &id_));
    std::string method;
    EXPECT_TRUE(dict->GetString("method", &method));
    EXPECT_STREQ("method", method.c_str());
    base::DictionaryValue* params = NULL;
    EXPECT_TRUE(dict->GetDictionary("params", &params));
    if (!params)
      return false;
    int param = -1;
    EXPECT_TRUE(params->GetInteger("param", &param));
    EXPECT_EQ(1, param);
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (timeout.IsExpired())
      return SyncWebSocket::kTimeout;
    base::DictionaryValue response;
    response.SetInteger("id", id_);
    base::DictionaryValue result;
    result.SetInteger("param", 1);
    response.SetKey("result", result.Clone());
    base::JSONWriter::Write(response, message);
    --queued_messages_;
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return queued_messages_ > 0; }

 protected:
  bool connected_;
  int id_;
  int queued_messages_;
};

template <typename T>
std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket() {
  return std::unique_ptr<SyncWebSocket>(new T());
}

class DevToolsClientImplTest : public testing::Test {
 protected:
  DevToolsClientImplTest() : long_timeout_(base::TimeDelta::FromMinutes(5)) {}

  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommand) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
}

TEST_F(DevToolsClientImplTest, SendCommandAndGetResult) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client.SendCommandAndGetResult("method", params, &result);
  ASSERT_EQ(kOk, status.code());
  std::string json;
  base::JSONWriter::Write(*result, &json);
  ASSERT_STREQ("{\"param\":1}", json.c_str());
}

namespace {

class MockSyncWebSocket2 : public SyncWebSocket {
 public:
  MockSyncWebSocket2() {}
  ~MockSyncWebSocket2() override {}

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
    return SyncWebSocket::kDisconnected;
  }

  bool HasNextMessage() override { return true; }
};

}  // namespace

TEST_F(DevToolsClientImplTest, ConnectIfNecessaryConnectFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket2>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kDisconnected, client.ConnectIfNecessary().code());
}

namespace {

class MockSyncWebSocket3 : public SyncWebSocket {
 public:
  MockSyncWebSocket3() : connected_(false) {}
  ~MockSyncWebSocket3() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return false; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    EXPECT_TRUE(false);
    return SyncWebSocket::kDisconnected;
  }

  bool HasNextMessage() override { return true; }

 private:
  bool connected_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandSendFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket3>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class MockSyncWebSocket4 : public SyncWebSocket {
 public:
  MockSyncWebSocket4() : connected_(false) {}
  ~MockSyncWebSocket4() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return true; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    return SyncWebSocket::kDisconnected;
  }

  bool HasNextMessage() override { return true; }

 private:
  bool connected_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, SendCommandReceiveNextMessageFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket4>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class FakeSyncWebSocket : public SyncWebSocket {
 public:
  FakeSyncWebSocket() : connected_(false) {}
  ~FakeSyncWebSocket() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_FALSE(connected_);
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return true; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return true; }

 private:
  bool connected_;
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
  command_response->result.reset(new base::DictionaryValue());
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
  command_response->result.reset(new base::DictionaryValue());
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
  command_response->result.reset(new base::DictionaryValue());
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
    EXPECT_TRUE(params.HasKey("key"));
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
    event->params.reset(new base::DictionaryValue());
    event->params->SetInteger("key", 1);
  } else {
    *type = internal::kCommandResponseMessageType;
    command_response->id = expected_id;
    base::DictionaryValue params;
    command_response->result.reset(new base::DictionaryValue());
    command_response->result->SetInteger("key", 2);
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
  event->params.reset(new base::DictionaryValue());
  event->params->SetInteger("key", 1);
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
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  switch ((*recurse_count)++) {
    case 0:
      client->SendCommand("method", params);
      *type = internal::kEventMessageType;
      event->method = "method";
      event->params.reset(new base::DictionaryValue());
      event->params->SetInteger("key", 1);
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
  command_response->result.reset(new base::DictionaryValue());
  command_response->result->SetInteger("key", key);
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
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnCommand));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
}

TEST_F(DevToolsClientImplTest, SendCommandBadResponse) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnBadResponse));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandBadId) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnCommandBadId));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandResponseError) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnCommandError));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST_F(DevToolsClientImplTest, SendCommandEventBeforeResponse) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  MockListener listener;
  bool first = true;
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnEventThenResponse, &first));
  client.AddListener(&listener);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  ASSERT_TRUE(result);
  int key;
  ASSERT_TRUE(result->GetInteger("key", &key));
  ASSERT_EQ(2, key);
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
  int key;
  ASSERT_TRUE(event.params->GetInteger("key", &key));
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
  ASSERT_TRUE(response.result->empty());
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
  int key;
  ASSERT_TRUE(response.result->GetInteger("key", &key));
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

TEST(ParseInspectorError, UnknownError) {
  const std::string error("{\"code\": 10, \"message\": \"Error description\"}");
  Status status = internal::ParseInspectorError(error);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_EQ("unknown error: unhandled inspector error: " + error,
            status.message());
}

TEST_F(DevToolsClientImplTest, HandleEventsUntil) {
  MockListener listener;
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnEvent));
  client.AddListener(&listener);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::Bind(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kOk, status.code());
}

TEST_F(DevToolsClientImplTest, HandleEventsUntilTimeout) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnEvent));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::Bind(&AlwaysTrue),
                                           Timeout(base::TimeDelta()));
  ASSERT_EQ(kTimeout, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventCommand) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnCommand));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::Bind(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventError) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnError));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::Bind(&AlwaysTrue),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, WaitForNextEventConditionalFuncReturnsError) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc),
                            base::Bind(&ReturnEvent));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  Status status = client.HandleEventsUntil(base::Bind(&AlwaysError),
                                           Timeout(long_timeout_));
  ASSERT_EQ(kUnknownError, status.code());
}

TEST_F(DevToolsClientImplTest, NestedCommandsWithOutOfOrderResults) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  int recurse_count = 0;
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  client.SetParserFuncForTesting(
      base::Bind(&ReturnOutOfOrderResponses, &recurse_count, &client));
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  std::unique_ptr<base::DictionaryValue> result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  ASSERT_TRUE(result);
  int key;
  ASSERT_TRUE(result->GetInteger("key", &key));
  ASSERT_EQ(2, key);
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
    base::DictionaryValue params;
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
  DevToolsClient* client_;
  bool on_connected_called_;
  bool on_event_called_;
};

class OnConnectedSyncWebSocket : public SyncWebSocket {
 public:
  OnConnectedSyncWebSocket() : connected_(false) {}
  ~OnConnectedSyncWebSocket() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    EXPECT_TRUE(connected_);
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(message);
    base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(value->GetAsDictionary(&dict));
    if (!dict)
      return false;
    int id;
    EXPECT_TRUE(dict->GetInteger("id", &id));
    std::string method;
    EXPECT_TRUE(dict->GetString("method", &method));

    base::DictionaryValue response;
    response.SetInteger("id", id);
    response.Set("result", std::make_unique<base::DictionaryValue>());
    std::string json_response;
    base::JSONWriter::Write(response, &json_response);
    queued_response_.push_back(json_response);

    // Push one event.
    base::DictionaryValue event;
    event.SetString("method", "updateEvent");
    event.Set("params", std::make_unique<base::DictionaryValue>());
    std::string json_event;
    base::JSONWriter::Write(event, &json_event);
    queued_response_.push_back(json_event);

    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (queued_response_.empty())
      return SyncWebSocket::kDisconnected;
    *message = queued_response_.front();
    queued_response_.pop_front();
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return !queued_response_.empty(); }

 private:
  bool connected_;
  std::list<std::string> queued_response_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, ProcessOnConnectedFirstOnCommand) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<OnConnectedSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "onconnected-id",
                            base::Bind(&CloserFunc));
  OnConnectedListener listener1("DOM.getDocument", &client);
  OnConnectedListener listener2("Runtime.enable", &client);
  OnConnectedListener listener3("Page.enable", &client);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  EXPECT_EQ(kOk, client.SendCommand("Runtime.execute", params).code());
  listener1.VerifyCalled();
  listener2.VerifyCalled();
  listener3.VerifyCalled();
}

TEST_F(DevToolsClientImplTest, ProcessOnConnectedFirstOnHandleEventsUntil) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<OnConnectedSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", "onconnected-id",
                            base::Bind(&CloserFunc));
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
  MockSyncWebSocket5() : request_no_(0) {}
  ~MockSyncWebSocket5() override {}

  bool IsConnected() override { return true; }

  bool Connect(const GURL& url) override { return true; }

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
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return false; }

 private:
  int request_no_;
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
    client_->SendCommand("method", params);
    EXPECT_TRUE(other_listener_->received_event_);
    return Status(kOk);
  }

 private:
  DevToolsClient* client_;
  OtherEventListener* other_listener_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, ProcessOnEventFirst) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket5>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  OtherEventListener listener2;
  OnEventListener listener1(&client, &listener2);
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  base::DictionaryValue params;
  EXPECT_EQ(kOk, client.SendCommand("method", params).code());
}

namespace {

class DisconnectedSyncWebSocket : public MockSyncWebSocket {
 public:
  DisconnectedSyncWebSocket() : connection_count_(0), command_count_(0) {}
  ~DisconnectedSyncWebSocket() override {}

  bool Connect(const GURL& url) override {
    connection_count_++;
    connected_ = connection_count_ != 2;
    return connected_;
  }

  bool Send(const std::string& message) override {
    command_count_++;
    if (command_count_ == 1) {
      connected_ = false;
      return false;
    }
    return MockSyncWebSocket::Send(message);
  }

 private:
  int connection_count_;
  int command_count_;
};

Status CheckCloserFuncCalled(bool* is_called) {
  *is_called = true;
  return Status(kOk);
}

}  // namespace

TEST_F(DevToolsClientImplTest, Reconnect) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<DisconnectedSyncWebSocket>);
  bool is_called = false;
  DevToolsClientImpl client(factory,
                            "http://url",
                            "id",
                            base::Bind(&CheckCloserFuncCalled, &is_called));
  ASSERT_FALSE(is_called);
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  ASSERT_FALSE(is_called);
  base::DictionaryValue params;
  params.SetInteger("param", 1);
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

class MockSyncWebSocket6 : public SyncWebSocket {
 public:
  explicit MockSyncWebSocket6(std::list<std::string>* messages)
      : messages_(messages) {}
  ~MockSyncWebSocket6() override {}

  bool IsConnected() override { return true; }

  bool Connect(const GURL& url) override { return true; }

  bool Send(const std::string& message) override { return true; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (messages_->empty())
      return SyncWebSocket::kDisconnected;
    *message = messages_->front();
    messages_->pop_front();
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return messages_->size(); }

 private:
  std::list<std::string>* messages_;
};

class MockDevToolsEventListener : public DevToolsEventListener {
 public:
  MockDevToolsEventListener() : id_(1) {}
  ~MockDevToolsEventListener() override {}

  Status OnConnected(DevToolsClient* client) override { return Status(kOk); }

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override {
    id_++;
    Status status = client->SendCommand("hello", params);
    id_--;
    if (id_ == 3) {
      EXPECT_EQ(kUnexpectedAlertOpen, status.code());
    } else {
      EXPECT_EQ(kOk, status.code());
    }
    return Status(kOk);
  }

 private:
  int id_;
};

std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket6(
    std::list<std::string>* messages) {
  return std::make_unique<MockSyncWebSocket6>(messages);
}

}  // namespace

TEST_F(DevToolsClientImplTest, BlockedByAlert) {
  std::list<std::string> msgs;
  SyncWebSocketFactory factory = base::Bind(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client(
      factory, "http://url", "id", base::Bind(&CloserFunc));
  msgs.push_back(
      "{\"method\": \"Page.javascriptDialogOpening\", \"params\": {}}");
  msgs.push_back("{\"id\": 2, \"result\": {}}");
  base::DictionaryValue params;
  ASSERT_EQ(kUnexpectedAlertOpen,
            client.SendCommand("first", params).code());
}

TEST_F(DevToolsClientImplTest, CorrectlyDeterminesWhichIsBlockedByAlert) {
  // OUT                 | IN
  //                       FirstEvent
  // hello (id=1)
  //                       SecondEvent
  // hello (id=2)
  //                       ThirdEvent
  // hello (id=3)
  //                       FourthEvent
  // hello (id=4)
  //                       response for 1
  //                       alert
  // hello (id=5)
  // round trip command (id=6)
  //                       response for 2
  //                       response for 4
  //                       response for 5
  //                       response for 6
  std::list<std::string> msgs;
  SyncWebSocketFactory factory = base::Bind(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client(
      factory, "http://url", "id", base::Bind(&CloserFunc));
  MockDevToolsEventListener listener;
  client.AddListener(&listener);
  msgs.push_back("{\"method\": \"FirstEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"SecondEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"ThirdEvent\", \"params\": {}}");
  msgs.push_back("{\"method\": \"FourthEvent\", \"params\": {}}");
  msgs.push_back("{\"id\": 1, \"result\": {}}");
  msgs.push_back(
      "{\"method\": \"Page.javascriptDialogOpening\", \"params\": {}}");
  msgs.push_back("{\"id\": 2, \"result\": {}}");
  msgs.push_back("{\"id\": 4, \"result\": {}}");
  msgs.push_back("{\"id\": 5, \"result\": {}}");
  msgs.push_back("{\"id\": 6, \"result\": {}}");
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
                          const base::DictionaryValue& result,
                          const Timeout& command_timeout) override {
    msgs_.push_back(method);
    if (!callback_.is_null())
      callback_.Run(client);
    return Status(kOk);
  }

  base::Callback<void(DevToolsClient*)> callback_;
  std::list<std::string> msgs_;
};

void HandleReceivedEvents(DevToolsClient* client) {
  EXPECT_EQ(kOk, client->HandleReceivedEvents().code());
}

}  // namespace

TEST_F(DevToolsClientImplTest, ReceivesCommandResponse) {
  std::list<std::string> msgs;
  SyncWebSocketFactory factory = base::Bind(&CreateMockSyncWebSocket6, &msgs);
  DevToolsClientImpl client(
      factory, "http://url", "id", base::Bind(&CloserFunc));
  MockCommandListener listener1;
  listener1.callback_ = base::Bind(&HandleReceivedEvents);
  MockCommandListener listener2;
  client.AddListener(&listener1);
  client.AddListener(&listener2);
  msgs.push_back("{\"id\": 1, \"result\": {}}");
  msgs.push_back("{\"method\": \"event\", \"params\": {}}");
  base::DictionaryValue params;
  ASSERT_EQ(kOk, client.SendCommand("cmd", params).code());
  ASSERT_EQ(2u, listener2.msgs_.size());
  ASSERT_EQ("cmd", listener2.msgs_.front());
  ASSERT_EQ("event", listener2.msgs_.back());
}

namespace {

class MockSyncWebSocket7 : public SyncWebSocket {
 public:
  MockSyncWebSocket7() : id_(-1), sent_messages_(0), sent_responses_(0) {}
  ~MockSyncWebSocket7() override {}

  bool IsConnected() override { return true; }

  bool Connect(const GURL& url) override { return true; }

  bool Send(const std::string& message) override {
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(message);
    base::DictionaryValue* dict = nullptr;
    EXPECT_TRUE(value->GetAsDictionary(&dict));
    if (!dict)
      return false;
    EXPECT_TRUE(dict->GetInteger("id", &id_));
    std::string method;
    EXPECT_TRUE(dict->GetString("method", &method));
    EXPECT_STREQ("method", method.c_str());
    base::DictionaryValue* params = nullptr;
    EXPECT_TRUE(dict->GetDictionary("params", &params));
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
    base::DictionaryValue response;
    if (sent_responses_ == 0)
      response.SetInteger("id", 1);
    else
      response.SetInteger("id", 2);
    base::DictionaryValue result;
    result.SetInteger("param", 1);
    response.SetKey("result", result.Clone());
    base::JSONWriter::Write(response, message);
    sent_responses_++;
    return SyncWebSocket::kOk;
  }

  bool HasNextMessage() override { return sent_messages_ > sent_responses_; }

private:
  int id_;
  int sent_messages_;
  int sent_responses_;
};

} // namespace

TEST_F(DevToolsClientImplTest, SendCommandAndIgnoreResponse) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket7>);
  DevToolsClientImpl client(factory, "http://url", "id",
                            base::Bind(&CloserFunc));
  ASSERT_EQ(kOk, client.ConnectIfNecessary().code());
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  ASSERT_EQ(kOk, client.SendCommandAndIgnoreResponse("method", params).code());
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
}
