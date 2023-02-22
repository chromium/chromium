// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <list>
#include <memory>
#include <queue>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  FakeDevToolsClient() : status_(kOk) {}
  ~FakeDevToolsClient() override = default;

  void set_status(const Status& status) {
    status_ = status;
  }
  void set_result(const base::Value::Dict& result) { result_ = result.Clone(); }

  // Overridden from DevToolsClient:
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override {
    if (status_.IsError())
      return status_;
    *result = result_.Clone();
    return Status(kOk);
  }

 private:
  Status status_;
  base::Value::Dict result_;
};

void AssertEvalFails(const base::Value::Dict& command_result) {
  base::Value::Dict result;
  FakeDevToolsClient client;
  client.set_result(command_result);
  Status status = internal::EvaluateScript(
      &client, "context", std::string(), internal::ReturnByValue,
      base::TimeDelta::Max(), false, result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_TRUE(result.empty());
}

}  // namespace

TEST(EvaluateScript, CommandError) {
  base::Value::Dict result;
  FakeDevToolsClient client;
  client.set_status(Status(kUnknownError));
  Status status = internal::EvaluateScript(
      &client, "context", std::string(), internal::ReturnByValue,
      base::TimeDelta::Max(), false, result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_TRUE(result.empty());
}

TEST(EvaluateScript, MissingResult) {
  base::Value::Dict dict;
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(EvaluateScript, Throws) {
  base::Value::Dict dict;
  dict.SetByDottedPath("exceptionDetails.exception.className", "SyntaxError");
  dict.SetByDottedPath("result.type", "object");
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(EvaluateScript, Ok) {
  base::Value::Dict result;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.key", 100);
  FakeDevToolsClient client;
  client.set_result(dict);
  ASSERT_TRUE(internal::EvaluateScript(&client, "context", std::string(),
                                       internal::ReturnByValue,
                                       base::TimeDelta::Max(), false, result)
                  .IsOk());
  ASSERT_TRUE(result.contains("key"));
}

TEST(EvaluateScriptAndGetValue, MissingType) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.value", 1);
  client.set_result(dict);
  ASSERT_TRUE(internal::EvaluateScriptAndGetValue(
                  &client, "context", std::string(), base::TimeDelta::Max(),
                  false, &result)
                  .IsError());
}

TEST(EvaluateScriptAndGetValue, Undefined) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.type", "undefined");
  client.set_result(dict);
  Status status = internal::EvaluateScriptAndGetValue(
      &client, "context", std::string(), base::TimeDelta::Max(), false,
      &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->is_none());
}

TEST(EvaluateScriptAndGetValue, Ok) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.type", "integer");
  dict.SetByDottedPath("result.value", 1);
  client.set_result(dict);
  Status status = internal::EvaluateScriptAndGetValue(
      &client, "context", std::string(), base::TimeDelta::Max(), false,
      &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->is_int());
  ASSERT_EQ(1, result->GetInt());
}

TEST(EvaluateScriptAndGetObject, NoObject) {
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.type", "integer");
  client.set_result(dict);
  bool got_object;
  std::string object_id;
  ASSERT_TRUE(internal::EvaluateScriptAndGetObject(
                  &client, "context", std::string(), base::TimeDelta::Max(),
                  false, &got_object, &object_id)
                  .IsOk());
  ASSERT_FALSE(got_object);
  ASSERT_TRUE(object_id.empty());
}

TEST(EvaluateScriptAndGetObject, Ok) {
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.objectId", "id");
  client.set_result(dict);
  bool got_object;
  std::string object_id;
  ASSERT_TRUE(internal::EvaluateScriptAndGetObject(
                  &client, "context", std::string(), base::TimeDelta::Max(),
                  false, &got_object, &object_id)
                  .IsOk());
  ASSERT_TRUE(got_object);
  ASSERT_STREQ("id", object_id.c_str());
}

TEST(ParseCallFunctionResult, NotDict) {
  std::unique_ptr<base::Value> result;
  base::Value value(1);
  ASSERT_NE(kOk, internal::ParseCallFunctionResult(value, &result).code());
}

TEST(ParseCallFunctionResult, Ok) {
  std::unique_ptr<base::Value> result;
  base::Value::Dict dict;
  dict.Set("status", 0);
  dict.Set("value", 1);
  Status status =
      internal::ParseCallFunctionResult(base::Value(std::move(dict)), &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->is_int());
  ASSERT_EQ(1, result->GetInt());
}

TEST(ParseCallFunctionResult, ScriptError) {
  std::unique_ptr<base::Value> result;
  base::Value::Dict dict;
  dict.Set("status", 1);
  dict.Set("value", 1);
  Status status =
      internal::ParseCallFunctionResult(base::Value(std::move(dict)), &result);
  ASSERT_EQ(1, status.code());
  ASSERT_FALSE(result);
}

namespace {

class MockSyncWebSocket : public SyncWebSocket {
 public:
  explicit MockSyncWebSocket(SyncWebSocket::StatusCode next_status)
      : connected_(false), id_(-1), next_status_(next_status) {}
  MockSyncWebSocket() : MockSyncWebSocket(SyncWebSocket::StatusCode::kOk) {}
  ~MockSyncWebSocket() override = default;

  void SetNexStatusCode(SyncWebSocket::StatusCode status_code) {
    next_status_ = status_code;
  }

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override {
    absl::optional<base::Value> value = base::JSONReader::Read(message);
    if (!value) {
      return false;
    }

    absl::optional<int> id = value->GetDict().FindInt("id");
    if (!id) {
      return false;
    }

    std::string response_str;
    base::Value::Dict response;
    response.Set("id", *id);
    base::Value::Dict result;
    result.Set("param", 1);
    response.Set("result", std::move(result));
    base::JSONWriter::Write(response, &response_str);
    messages_.push(response_str);
    return true;
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    if (next_status_ == SyncWebSocket::StatusCode::kOk && !messages_.empty()) {
      *message = messages_.front();
      messages_.pop();
    }
    return next_status_;
  }

  bool HasNextMessage() override { return !messages_.empty(); }

 protected:
  bool connected_;
  int id_;
  std::queue<std::string> messages_;
  SyncWebSocket::StatusCode next_status_;
};

std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket(
    SyncWebSocket::StatusCode next_status) {
  return std::make_unique<MockSyncWebSocket>(next_status);
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

}  // namespace

TEST(CreateChild, MultiLevel) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket, SyncWebSocket::StatusCode::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "", "http://url", factory);
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl level1(client_ptr->GetId(), true, nullptr, &browser_info,
                     std::move(client_uptr), nullptr, PageLoadStrategy::kEager);
  Status status = client_ptr->Connect();
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> level2 =
      std::unique_ptr<WebViewImpl>(level1.CreateChild(sessionid, "1234"));
  level2->AttachTo(client_ptr);
  sessionid = "3";
  std::unique_ptr<WebViewImpl> level3 =
      std::unique_ptr<WebViewImpl>(level2->CreateChild(sessionid, "3456"));
  level3->AttachTo(client_ptr);
  sessionid = "4";
  std::unique_ptr<WebViewImpl> level4 =
      std::unique_ptr<WebViewImpl>(level3->CreateChild(sessionid, "5678"));
  level4->AttachTo(client_ptr);
}

TEST(CreateChild, IsNonBlocking_NoErrors) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket, SyncWebSocket::StatusCode::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "", "http://url", factory);
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kEager);
  Status status = client_ptr->Connect();
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_FALSE(parent_view.IsNonBlocking());

  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);
  ASSERT_NO_FATAL_FAILURE(child_view->IsNonBlocking());
  ASSERT_FALSE(child_view->IsNonBlocking());
}

TEST(CreateChild, Load_NoErrors) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket, SyncWebSocket::StatusCode::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "", "http://url", factory);
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNone);
  Status status = client_ptr->Connect();
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);

  ASSERT_NO_FATAL_FAILURE(child_view->Load("chrome://version", nullptr));
}

TEST(CreateChild, WaitForPendingNavigations_NoErrors) {
  std::unique_ptr<MockSyncWebSocket> socket =
      std::make_unique<MockSyncWebSocket>(SyncWebSocket::StatusCode::kOk);
  SyncWebSocketFactory factory = base::BindRepeating(
      [](SyncWebSocket* socket) {
        return std::unique_ptr<SyncWebSocket>(new SyncWebSocketWrapper(socket));
      },
      socket.get());
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "", "http://url", factory);
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNone);
  Status status = client_ptr->Connect();
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);

  // child_view gets no socket...
  socket->SetNexStatusCode(SyncWebSocket::StatusCode::kTimeout);
  ASSERT_NO_FATAL_FAILURE(child_view->WaitForPendingNavigations(
      "1234", Timeout(base::Milliseconds(10)), true));
}

TEST(CreateChild, IsPendingNavigation_NoErrors) {
  SyncWebSocketFactory factory = base::BindRepeating(
      &CreateMockSyncWebSocket, SyncWebSocket::StatusCode::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "", "http://url", factory);
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNormal);
  Status status = client_ptr->Connect();
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);

  Timeout timeout(base::Milliseconds(10));
  bool result;
  ASSERT_NO_FATAL_FAILURE(child_view->IsPendingNavigation(&timeout, &result));
}

TEST(ManageCookies, AddCookie_SameSiteTrue) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>();
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), nullptr, PageLoadStrategy::kEager);
  std::string samesite = "Strict";
  base::Value::Dict dict;
  dict.Set("success", true);
  client_ptr->set_result(dict);
  Status status = view.AddCookie("utest", "chrome://version", "value", "domain",
                                 "path", samesite, true, true, 123456789);
  ASSERT_EQ(kOk, status.code());
}
