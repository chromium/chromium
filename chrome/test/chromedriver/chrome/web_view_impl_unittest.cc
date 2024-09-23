// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/net/stub_sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kElementKey[] = "ELEMENT";
const char kElementKeyW3C[] = "element-6066-11e4-a52e-4f735466cecf";
const char kShadowRootKey[] = "shadow-6066-11e4-a52e-4f735466cecf";
const int kNonExistingBackendNodeId = 1000'000'001;

using testing::Eq;
using testing::Optional;
using testing::Pointee;

std::string ElementReference(const char* frame_id,
                             const char* loader_id,
                             int backend_node_id) {
  return base::StringPrintf("f.%s.d.%s.e.%d", frame_id, loader_id,
                            backend_node_id);
}

std::string ElementReference(const char* frame_id,
                             const char* loader_id,
                             const char* backend_node_id) {
  return base::StringPrintf("f.%s.d.%s.e.%s", frame_id, loader_id,
                            backend_node_id);
}

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

base::Value::Dict CreateElementPlaceholder(
    int index,
    std::string element_key = kElementKeyW3C) {
  base::Value::Dict placeholder;
  placeholder.Set(element_key, index);
  return placeholder;
}

// Create weak local object reference on a node
base::Value::Dict CreateWeakNodeReference(int weak_local_object_reference) {
  base::Value::Dict node;
  node.Set("type", "node");
  node.Set("weakLocalObjectReference", weak_local_object_reference);
  return node;
}

base::Value::Dict CreateNode(int backend_node_id,
                             int weak_local_object_reference = -1) {
  base::Value::Dict node;
  node.Set("type", "node");
  node.SetByDottedPath("value.backendNodeId", backend_node_id);
  if (weak_local_object_reference >= 0) {
    node.Set("weakLocalObjectReference", weak_local_object_reference);
  }
  return node;
}

std::string WrapToJson(base::Value value, int status = 0) {
  base::Value::Dict result;
  result.Set("value", std::move(value));
  result.Set("status", 0);
  std::string json;
  base::JSONWriter::Write(result, &json);
  return json;
}

base::Value::Dict GenerateResponse(int backend_node_id) {
  base::Value::Dict dict;
  dict.Set("value", WrapToJson(base::Value(CreateElementPlaceholder(0)), 0));
  base::Value::Dict node = CreateNode(backend_node_id);
  base::Value::List serialized_list;
  serialized_list.Append(std::move(dict));
  serialized_list.Append(std::move(node));
  base::Value::Dict response;
  response.SetByDottedPath("result.deepSerializedValue.value",
                           std::move(serialized_list));
  return response;
}

base::Value::Dict GenerateResponseWithScriptArguments(
    base::Value::List args,
    const std::string& element_key) {
  base::Value::List arr;
  base::Value::List nodes;
  for (base::Value& arg : args) {
    if (!arg.is_dict()) {
      arr.Append(std::move(arg));
      continue;
    }
    std::string* maybe_object_id = arg.GetDict().FindString("objectId");
    int object_id = 0xdeadbeef;
    if (!maybe_object_id || !base::StringToInt(*maybe_object_id, &object_id)) {
      arr.Append(std::move(arg));
      continue;
    }

    arr.Append(CreateElementPlaceholder(nodes.size(), element_key));
    nodes.Append(CreateNode(object_id));
  }

  base::Value::Dict dict;
  dict.Set("value", WrapToJson(base::Value(std::move(arr)), 0));
  base::Value::List serialized_list;
  serialized_list.Append(std::move(dict));

  for (base::Value& node : nodes) {
    serialized_list.Append(std::move(node));
  }

  base::Value::Dict response;
  response.SetByDottedPath("result.deepSerializedValue.value",
                           std::move(serialized_list));
  return response;
}

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  explicit FakeDevToolsClient(std::string id)
      : StubDevToolsClient(id), status_(kOk) {}
  FakeDevToolsClient() : status_(kOk) {}
  ~FakeDevToolsClient() override = default;

  void SetStatus(const Status& status) { status_ = status; }
  void SetResult(const base::Value::Dict& result) { result_ = result.Clone(); }

  void SetElementKey(std::string element_key) {
    element_key_ = std::move(element_key);
  }

  // Overridden from DevToolsClient:
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override {
    if (status_.IsError())
      return status_;

    if (method == "Page.getFrameTree") {
      // Unused padding frame
      base::Value::Dict unused_child;
      unused_child.SetByDottedPath("frame.id", "unused");
      unused_child.SetByDottedPath("frame.loaderId", "unused_loader");
      // Used by the dedicated tests
      base::Value::Dict good_child;
      good_child.SetByDottedPath("frame.id", "good");
      good_child.SetByDottedPath("frame.loaderId", "good_loader");
      // Default constructed WebViewImpl will point here
      // Needed for the tests that are neutral to getFramTree
      base::Value::Dict default_child;
      default_child.SetByDottedPath("frame.id", GetOwner()->GetId());
      default_child.SetByDottedPath("frame.loaderId", "default_loader");
      // root
      base::Value::List children;
      children.Append(std::move(good_child));
      children.Append(std::move(default_child));
      for (base::Value& frame : extra_child_frames_) {
        children.Append(frame.Clone());
      }
      result->SetByDottedPath("frameTree.frame.id", "root");
      result->SetByDottedPath("frameTree.frame.loaderId", "root_loader");
      result->SetByDottedPath("frameTree.childFrames", std::move(children));
    } else if (method == "DOM.resolveNode") {
      std::optional<int> maybe_backend_node_id =
          params.FindInt("backendNodeId");
      if (!maybe_backend_node_id.has_value()) {
        return Status{
            kUnknownError,
            "backend node id is missing in DOM.resolveNode parameters"};
      }
      if (maybe_backend_node_id.value() == kNonExistingBackendNodeId) {
        return Status{kNoSuchElement, "element with such id not found"};
      }
      result->SetByDottedPath("object.objectId",
                              base::NumberToString(*maybe_backend_node_id));
    } else if (method == "Runtime.callFunctionOn" && result_.empty()) {
      const base::Value::List* args = params.FindList("arguments");
      if (args == nullptr) {
        return Status{kInvalidArgument,
                      "arguments are not provided to Runtime.callFunctionOn"};
      }
      *result =
          GenerateResponseWithScriptArguments(args->Clone(), element_key_);
    } else {
      *result = result_.Clone();
    }

    return Status(kOk);
  }

  void AddExtraChildFrame(base::Value::Dict frame) {
    extra_child_frames_.Append(std::move(frame));
  }

  void ClearExtraChildFrames() { extra_child_frames_.clear(); }

 private:
  Status status_;
  base::Value::Dict result_;
  base::Value::List extra_child_frames_;
  std::string element_key_ = kElementKeyW3C;
};

void AssertEvalFails(const base::Value::Dict& command_result) {
  base::Value::Dict result;
  FakeDevToolsClient client;
  client.SetResult(command_result);
  Status status = internal::EvaluateScript(
      &client, "context", std::string(), base::TimeDelta::Max(), false, result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_TRUE(result.empty());
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

}  // namespace

TEST(EvaluateScript, CommandError) {
  base::Value::Dict result;
  FakeDevToolsClient client;
  client.SetStatus(Status(kUnknownError));
  Status status = internal::EvaluateScript(
      &client, "context", std::string(), base::TimeDelta::Max(), false, result);
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
  client.SetResult(dict);
  ASSERT_TRUE(internal::EvaluateScript(&client, "context", std::string(),
                                       base::TimeDelta::Max(), false, result)
                  .IsOk());
  ASSERT_TRUE(result.contains("key"));
}

TEST(EvaluateScriptAndGetValue, MissingType) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::Value::Dict dict;
  dict.SetByDottedPath("result.value", 1);
  client.SetResult(dict);
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
  client.SetResult(dict);
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
  client.SetResult(dict);
  Status status = internal::EvaluateScriptAndGetValue(
      &client, "context", std::string(), base::TimeDelta::Max(), false,
      &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->is_int());
  ASSERT_EQ(1, result->GetInt());
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
    std::optional<base::Value> value = base::JSONReader::Read(message);
    if (!value) {
      return false;
    }

    std::optional<int> id = value->GetDict().FindInt("id");
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
    if (timeout.IsExpired()) {
      return SyncWebSocket::StatusCode::kTimeout;
    }
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

}  // namespace

TEST(CreateChild, MultiLevel) {
  SocketHolder<MockSyncWebSocket> socket_holder{SyncWebSocket::StatusCode::kOk};
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl level1(client_ptr->GetId(), true, nullptr, &browser_info,
                     std::move(client_uptr), std::nullopt,
                     PageLoadStrategy::kEager, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));
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
  SocketHolder<MockSyncWebSocket> socket_holder{SyncWebSocket::StatusCode::kOk};
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), std::nullopt,
                          PageLoadStrategy::kEager, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));
  ASSERT_FALSE(parent_view.IsNonBlocking());

  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);
  ASSERT_NO_FATAL_FAILURE(child_view->IsNonBlocking());
  ASSERT_FALSE(child_view->IsNonBlocking());
}

TEST(CreateChild, Load_NoErrors) {
  SocketHolder<MockSyncWebSocket> socket_holder{SyncWebSocket::StatusCode::kOk};
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), std::nullopt,
                          PageLoadStrategy::kNone, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);

  ASSERT_NO_FATAL_FAILURE(child_view->Load("chrome://version", nullptr));
}

TEST(CreateChild, WaitForPendingNavigations_NoErrors) {
  SocketHolder<MockSyncWebSocket> socket_holder{SyncWebSocket::StatusCode::kOk};
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), std::nullopt,
                          PageLoadStrategy::kNone, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  child_view->AttachTo(client_ptr);

  // child_view gets no socket...
  socket_holder.Socket().SetNexStatusCode(SyncWebSocket::StatusCode::kTimeout);
  ASSERT_NO_FATAL_FAILURE(child_view->WaitForPendingNavigations(
      "1234", Timeout(base::Milliseconds(10)), true));
}

TEST(CreateChild, IsPendingNavigation_NoErrors) {
  SocketHolder<MockSyncWebSocket> socket_holder{SyncWebSocket::StatusCode::kOk};
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("id", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), std::nullopt,
                          PageLoadStrategy::kNormal, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));
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
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kEager, true);
  std::string samesite = "Strict";
  base::Value::Dict dict;
  dict.Set("success", true);
  client_ptr->SetResult(dict);
  Status status = view.AddCookie("utest", "chrome://version", "value", "domain",
                                 "path", samesite, true, true, 123456789);
  ASSERT_EQ(kOk, status.code());
}

TEST(GetBackendNodeId, ElementW3C) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kEager, true);
  {
    // Good 1
    base::Value::Dict node_ref;
    node_ref.Set(kElementKey, ElementReference("root", "root_loader", 25));
    node_ref.Set(kElementKeyW3C, ElementReference("root", "root_loader", 13));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(13, backend_node_id);
  }
  {
    // Good 2
    base::Value::Dict node_ref;
    node_ref.Set(kElementKey, ElementReference("good", "good_loader", 25));
    node_ref.Set(kElementKeyW3C, ElementReference("good", "good_loader", 13));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "good", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(13, backend_node_id);
  }
  {
    // Stale
    base::Value::Dict node_ref;
    node_ref.Set(kElementKey, ElementReference("root", "past_loader", 25));
    node_ref.Set(kElementKeyW3C, ElementReference("root", "past_loader", 13));
    int backend_node_id = -1;
    EXPECT_EQ(kStaleElementReference,
              view.GetBackendNodeIdByElement(
                      "", base::Value(std::move(node_ref)), &backend_node_id)
                  .code());
  }
  {
    // Unknown
    base::Value::Dict node_ref;
    node_ref.Set(kElementKey, ElementReference("root", "root_loader", 25));
    node_ref.Set(kElementKeyW3C, ElementReference("root", "root_loader", 13));
    int backend_node_id = -1;
    EXPECT_EQ(kNoSuchElement, view.GetBackendNodeIdByElement(
                                      "good", base::Value(std::move(node_ref)),
                                      &backend_node_id)
                                  .code());
  }
}

TEST(GetBackendNodeId, ShadowRootW3C) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kEager, true);
  {
    // Good 1
    base::Value::Dict node_ref;
    node_ref.Set(kShadowRootKey, ElementReference("root", "root_loader", 11));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(11, backend_node_id);
  }
  {
    // Good 2
    base::Value::Dict node_ref;
    node_ref.Set(kShadowRootKey, ElementReference("good", "good_loader", 11));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "good", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(11, backend_node_id);
  }
  {
    // Stale
    base::Value::Dict node_ref;
    node_ref.Set(kShadowRootKey, ElementReference("root", "past_loader", 11));
    int backend_node_id = -1;
    EXPECT_EQ(kDetachedShadowRoot,
              view.GetBackendNodeIdByElement(
                      "", base::Value(std::move(node_ref)), &backend_node_id)
                  .code());
  }
  {
    // Unknown
    base::Value::Dict node_ref;
    node_ref.Set(kShadowRootKey, ElementReference("root", "root_loader", 11));
    int backend_node_id = -1;
    EXPECT_EQ(
        kNoSuchShadowRoot,
        view.GetBackendNodeIdByElement("good", base::Value(std::move(node_ref)),
                                       &backend_node_id)
            .code());
  }
}

TEST(GetBackendNodeId, NonW3C) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), false, nullptr, &browser_info,
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kEager, true);
  {
    base::Value::Dict node_ref;
    node_ref.Set(kElementKey, ElementReference("root", "root_loader", 25));
    node_ref.Set(kElementKeyW3C, ElementReference("root", "root_loader", 13));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(25, backend_node_id);
  }
  {
    base::Value::Dict node_ref;
    node_ref.Set(kShadowRootKey, ElementReference("root", "root_loader", 11));
    int backend_node_id = -1;
    EXPECT_TRUE(StatusOk(view.GetBackendNodeIdByElement(
        "", base::Value(std::move(node_ref)), &backend_node_id)));
    EXPECT_EQ(11, backend_node_id);
  }
}

TEST(CallUserSyncScript, ElementIdAsResultRootFrame) {
  BrowserInfo browser_info;
  std::unique_ptr<base::Value> result;
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  client_uptr->SetResult(GenerateResponse(4321));
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");
  view.GetFrameTracker()->SetContextIdForFrame("good", "irrelevant");
  {
    EXPECT_TRUE(StatusOk(
        view.CallUserSyncScript("root", "some_code", base::Value::List(),
                                base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("root", "root_loader", "4321"))));
  }
  result.reset();
  {
    EXPECT_TRUE(
        StatusOk(view.CallUserSyncScript("", "some_code", base::Value::List(),
                                         base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("root", "root_loader", 4321))));
  }
  result.reset();
  {
    EXPECT_TRUE(StatusOk(
        view.CallUserSyncScript("good", "some_code", base::Value::List(),
                                base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("good", "good_loader", 4321))));
  }
}

TEST(CallUserSyncScript, ElementIdAsResultChildFrame) {
  BrowserInfo browser_info;
  std::unique_ptr<base::Value> result;
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("good");
  client_uptr->SetResult(GenerateResponse(4321));
  WebViewImpl view("good", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");
  view.GetFrameTracker()->SetContextIdForFrame("good", "irrelevant");
  {
    EXPECT_TRUE(StatusOk(
        view.CallUserSyncScript("root", "some_code", base::Value::List(),
                                base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("root", "root_loader", 4321))));
  }
  result.reset();
  {
    EXPECT_TRUE(
        StatusOk(view.CallUserSyncScript("", "some_code", base::Value::List(),
                                         base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("good", "good_loader", 4321))));
  }
  result.reset();
  {
    EXPECT_TRUE(StatusOk(
        view.CallUserSyncScript("good", "some_code", base::Value::List(),
                                base::TimeDelta::Max(), &result)));
    ASSERT_TRUE(result->is_dict());
    EXPECT_THAT(result->GetDict().FindString(kElementKeyW3C),
                Pointee(Eq(ElementReference("good", "good_loader", 4321))));
  }
}

TEST(CallUserSyncScript, ElementIdAsResultChildFrameErrors) {
  BrowserInfo browser_info;
  std::unique_ptr<base::Value> result;
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  FakeDevToolsClient* client_ptr = client_uptr.get();
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");
  view.GetFrameTracker()->SetContextIdForFrame("good", "irrelevant");
  view.GetFrameTracker()->SetContextIdForFrame("bad", "irrelevant");
  {
    // missing frame.id case
    base::Value::Dict bad_child;
    bad_child.SetByDottedPath("frame.loaderId", "bad_loader");
    client_ptr->AddExtraChildFrame(std::move(bad_child));
    EXPECT_FALSE(view.CallUserSyncScript("bad", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
    client_ptr->ClearExtraChildFrames();
  }
  result.reset();
  {
    // missing loaderId case
    base::Value::Dict bad_child;
    bad_child.SetByDottedPath("frame.id", "bad");
    client_ptr->AddExtraChildFrame(std::move(bad_child));
    EXPECT_FALSE(view.CallUserSyncScript("bad", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
    client_ptr->ClearExtraChildFrames();
  }
  result.reset();
  {
    // empty frame description
    base::Value::Dict bad_child;
    client_ptr->AddExtraChildFrame(std::move(bad_child));
    EXPECT_FALSE(view.CallUserSyncScript("bad", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
    client_ptr->ClearExtraChildFrames();
  }
  result.reset();
  {
    // frame is not a dictionary
    base::Value::Dict bad_child;
    bad_child.Set("frame", "bad");
    client_ptr->AddExtraChildFrame(std::move(bad_child));
    EXPECT_FALSE(view.CallUserSyncScript("bad", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
    client_ptr->ClearExtraChildFrames();
  }
  result.reset();
  {
    // frame does not exist
    EXPECT_FALSE(view.CallUserSyncScript("non_existent_frame", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
  }
  result.reset();
  {
    // no execution context information
    base::Value::Dict bad_child;
    bad_child.SetByDottedPath("frame.id", "no_execution_context");
    bad_child.SetByDottedPath("frame.loaderId", "no_execution_context_loader");
    client_ptr->AddExtraChildFrame(std::move(bad_child));
    EXPECT_FALSE(view.CallUserSyncScript("no_execution_context", "some_code",
                                         base::Value::List(),
                                         base::TimeDelta::Max(), &result)
                     .IsOk());
    client_ptr->ClearExtraChildFrames();
  }
  result.reset();
  {
    // Test self-check: make sure that one shot frames work correctly
    base::Value::Dict another_good_child;
    another_good_child.SetByDottedPath("frame.id", "bad");
    another_good_child.SetByDottedPath("frame.loaderId", "bad_loader");
    client_ptr->AddExtraChildFrame(std::move(another_good_child));
    EXPECT_TRUE(StatusOk(
        view.CallUserSyncScript("bad", "some_code", base::Value::List(),
                                base::TimeDelta::Max(), &result)));
    client_ptr->ClearExtraChildFrames();
  }
}

TEST(GetFedCmTracker, OK) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>();
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kEager, true);
  FedCmTracker* tracker = nullptr;
  Status status = view.GetFedCmTracker(&tracker);
  EXPECT_TRUE(StatusOk(status));
  EXPECT_NE(nullptr, tracker);
}

class CallUserSyncScriptArgs
    : public testing::TestWithParam<std::pair<std::string, bool>> {
 public:
  void SetUp() override {
    std::unique_ptr<FakeDevToolsClient> client_uptr =
        std::make_unique<FakeDevToolsClient>("root");
    client_ptr = client_uptr.get();
    view = std::make_unique<WebViewImpl>(
        "root", IsW3C(), nullptr, &browser_info, std::move(client_uptr),
        std::nullopt, PageLoadStrategy::kEager, true);
    view->GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");
    view->GetFrameTracker()->SetContextIdForFrame("good", "irrelevant");
    view->GetFrameTracker()->SetContextIdForFrame("bad", "irrelevant");
  }

  void TearDown() override {
    client_ptr = nullptr;
    view.reset();
  }

  bool IsW3C() { return GetParam().second; }

  std::string ElementKey() { return GetParam().first; }

  BrowserInfo browser_info;
  std::unique_ptr<WebViewImpl> view;
  raw_ptr<FakeDevToolsClient> client_ptr;
};

TEST_P(CallUserSyncScriptArgs, Root) {
  // Expecting success as the frame and loader_id match each other.
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("root", "root_loader", 99));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  EXPECT_TRUE(StatusOk(view->CallUserSyncScript(
      "root", "some_code", std::move(args), base::TimeDelta::Max(), &result)));
}

TEST_P(CallUserSyncScriptArgs, GoodChild) {
  // Expecting success as the frame and loader_id match each other.
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("good", "good_loader", 99));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  EXPECT_TRUE(StatusOk(view->CallUserSyncScript(
      "good", "some_code", std::move(args), base::TimeDelta::Max(), &result)));
}

TEST_P(CallUserSyncScriptArgs, DeepElement) {
  std::string element_id = ElementReference("good", "good_loader", 99);
  base::Value::Dict ref;
  ref.Set(ElementKey(), element_id);
  base::Value::List list;
  list.Append(1);
  list.Append(std::move(ref));
  list.Append("xyz");
  base::Value::Dict arg;
  arg.SetByDottedPath("uno.a", "b");
  arg.SetByDottedPath("uno.dos", std::move(list));
  arg.SetByDottedPath("dos.b", 7.7);
  base::Value::List nodes;
  base::Value::List args;
  args.Append(std::move(arg));
  std::unique_ptr<base::Value> result;
  client_ptr->SetElementKey(ElementKey());
  EXPECT_TRUE(
      StatusOk(view->CallUserSyncScript("good", "return nodes", std::move(args),
                                        base::TimeDelta::Max(), &result)));

  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(result->GetList().size(), size_t(1));
  base::Value::Dict* received_ref = result->GetList()[0].GetIfDict();
  ASSERT_NE(received_ref, nullptr);
  std::string* maybe_backend_node_id = received_ref->FindString(ElementKey());
  EXPECT_THAT(maybe_backend_node_id, Pointee(Eq(element_id)));
}

TEST_P(CallUserSyncScriptArgs, FrameAndLoaderMismatch) {
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("root", "root_loader", 99));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  const int expected_error =
      ElementKey() == kShadowRootKey ? kNoSuchShadowRoot : kNoSuchElement;
  EXPECT_EQ(expected_error,
            view->CallUserSyncScript("good", "some_code", std::move(args),
                                     base::TimeDelta::Max(), &result)
                .code());
}

TEST_P(CallUserSyncScriptArgs, NoSuchFrame) {
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("unknown", "root_loader", 99));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  const int expected_error =
      ElementKey() == kShadowRootKey ? kNoSuchShadowRoot : kNoSuchElement;
  EXPECT_EQ(expected_error,
            view->CallUserSyncScript("root", "some_code", std::move(args),
                                     base::TimeDelta::Max(), &result)
                .code());
}

TEST_P(CallUserSyncScriptArgs, NoSuchLoader) {
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("root", "bad_loader", 99));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  const int expected_error = ElementKey() == kShadowRootKey
                                 ? kDetachedShadowRoot
                                 : kStaleElementReference;
  EXPECT_EQ(expected_error,
            view->CallUserSyncScript("root", "some_code", std::move(args),
                                     base::TimeDelta::Max(), &result)
                .code());
}

TEST_P(CallUserSyncScriptArgs, NoSuchBackendNodeId) {
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(),
          ElementReference("good", "good_loader", kNonExistingBackendNodeId));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  const int expected_error = ElementKey() == kShadowRootKey
                                 ? kDetachedShadowRoot
                                 : kStaleElementReference;
  EXPECT_EQ(expected_error,
            view->CallUserSyncScript("good", "some_code", std::move(args),
                                     base::TimeDelta::Max(), &result)
                .code());
}

TEST_P(CallUserSyncScriptArgs, MalformedReference) {
  std::vector<std::string> components = {"f", "good", "d",      "good_loader",
                                         "e", "99",   "trailer"};
  std::string ref_prefix;
  std::vector<std::string> bad_refs;
  for (size_t k = 0; k < components.size(); ++k) {
    ref_prefix += components[k];
    if (k != 5) {
      // exclude the only good reference
      bad_refs.push_back(ref_prefix);
    }
    ref_prefix += ".";
    bad_refs.push_back(ref_prefix);
  }
  for (const std::string& ref_str : bad_refs) {
    base::Value::List args;
    base::Value::Dict ref;
    ref.Set(ElementKey(), ref_str);
    args.Append(std::move(ref));
    std::unique_ptr<base::Value> result;
    EXPECT_FALSE(view->CallUserSyncScript("good", "some_code", std::move(args),
                                          base::TimeDelta::Max(), &result)
                     .IsOk());
  }
}

TEST_P(CallUserSyncScriptArgs, NoBackendNodeId) {
  base::Value::List args;
  base::Value::Dict ref;
  ref.Set(ElementKey(), ElementReference("good", "good_loader", "xx"));
  args.Append(std::move(ref));
  std::unique_ptr<base::Value> result;
  EXPECT_FALSE(view->CallUserSyncScript("good", "some_code", std::move(args),
                                        base::TimeDelta::Max(), &result)
                   .IsOk());
}

INSTANTIATE_TEST_SUITE_P(References,
                         CallUserSyncScriptArgs,
                         ::testing::Values(std::make_pair(kElementKeyW3C, true),
                                           std::make_pair(kShadowRootKey, true),
                                           std::make_pair(kElementKey, false),
                                           std::make_pair(kShadowRootKey,
                                                          false)));

TEST(CallUserSyncScript, WeakReference) {
  base::Value::Dict node = CreateNode(557, 13);
  base::Value::Dict weak_ref = CreateWeakNodeReference(13);
  base::Value::Dict dict;
  base::Value::List placeholder_list;
  placeholder_list.Append(CreateElementPlaceholder(0));
  placeholder_list.Append(CreateElementPlaceholder(1));
  dict.Set("value", WrapToJson(base::Value(std::move(placeholder_list)), 0));
  base::Value::List serialized_list;
  serialized_list.Append(std::move(dict));
  serialized_list.Append(std::move(node));
  serialized_list.Append(std::move(weak_ref));
  base::Value::Dict response;
  response.SetByDottedPath("result.deepSerializedValue.value",
                           std::move(serialized_list));

  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  client_uptr->SetResult(response);

  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");

  std::unique_ptr<base::Value> result;
  EXPECT_TRUE(
      StatusOk(view.CallUserSyncScript("root", "some_code", base::Value::List(),
                                       base::TimeDelta::Max(), &result)));

  ASSERT_TRUE(result->is_list());
  const base::Value::List& result_list = result->GetList();
  ASSERT_EQ(2u, result_list.size());
  ASSERT_TRUE(result_list[0].is_dict());
  EXPECT_THAT(result_list[0].GetDict().FindString(kElementKeyW3C),
              Pointee(Eq(ElementReference("root", "root_loader", 557))));
  ASSERT_TRUE(result_list[1].is_dict());
  EXPECT_THAT(result_list[1].GetDict().FindString(kElementKeyW3C),
              Pointee(Eq(ElementReference("root", "root_loader", 557))));
}

TEST(CallUserSyncScript, WeakReferenceOrderInsensitive) {
  base::Value::Dict node = CreateNode(557, 13);
  base::Value::Dict weak_ref = CreateWeakNodeReference(13);
  base::Value::Dict dict;
  base::Value::List placeholder_list;
  placeholder_list.Append(CreateElementPlaceholder(0));
  placeholder_list.Append(CreateElementPlaceholder(1));
  dict.Set("value", WrapToJson(base::Value(std::move(placeholder_list)), 0));
  base::Value::List serialized_list;
  serialized_list.Append(std::move(dict));
  serialized_list.Append(std::move(weak_ref));
  serialized_list.Append(std::move(node));
  base::Value::Dict response;
  response.SetByDottedPath("result.deepSerializedValue.value",
                           std::move(serialized_list));

  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  client_uptr->SetResult(response);

  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");

  std::unique_ptr<base::Value> result;
  EXPECT_TRUE(
      StatusOk(view.CallUserSyncScript("root", "some_code", base::Value::List(),
                                       base::TimeDelta::Max(), &result)));

  ASSERT_TRUE(result->is_list());
  const base::Value::List& result_list = result->GetList();
  ASSERT_EQ(2u, result_list.size());
  ASSERT_TRUE(result_list[0].is_dict());
  EXPECT_THAT(result_list[0].GetDict().FindString(kElementKeyW3C),
              Pointee(Eq(ElementReference("root", "root_loader", 557))));
  ASSERT_TRUE(result_list[1].is_dict());
  EXPECT_THAT(result_list[1].GetDict().FindString(kElementKeyW3C),
              Pointee(Eq(ElementReference("root", "root_loader", 557))));
}

TEST(CallUserSyncScript, WeakReferenceNotResolved) {
  base::Value::Dict weak_ref = CreateWeakNodeReference(13);
  base::Value::Dict dict;
  base::Value::List placeholder_list;
  placeholder_list.Append(CreateElementPlaceholder(0));
  dict.Set("value", WrapToJson(base::Value(std::move(placeholder_list)), 0));
  base::Value::List serialized_list;
  serialized_list.Append(std::move(dict));
  serialized_list.Append(std::move(weak_ref));
  base::Value::Dict response;
  response.SetByDottedPath("result.deepSerializedValue.value",
                           std::move(serialized_list));

  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>("root");
  client_uptr->SetResult(response);

  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);
  view.GetFrameTracker()->SetContextIdForFrame("root", "irrelevant");

  std::unique_ptr<base::Value> result;
  Status status =
      view.CallUserSyncScript("root", "some_code", base::Value::List(),
                              base::TimeDelta::Max(), &result);
  EXPECT_TRUE(status.IsError());
}

namespace {

bool ReturnError(int code,
                 const std::string& message,
                 int cmd_id,
                 const base::Value::Dict& params,
                 base::Value::Dict& response) {
  response.Set("id", cmd_id);
  base::Value::Dict error;
  error.Set("code", code);
  error.Set("message", message);
  response.Set("error", std::move(error));
  return true;
}

}  // namespace

class WaitForPendingNavigations : public testing::TestWithParam<std::string> {
 public:
  const std::string& Message() { return GetParam(); }
};

TEST_P(WaitForPendingNavigations, NavigationDetection) {
  SocketHolder<StubSyncWebSocket> socket_holder;
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>("", "");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), std::nullopt,
                   PageLoadStrategy::kNormal, true);
  EXPECT_TRUE(socket_holder.ConnectSocket());
  EXPECT_TRUE(StatusOk(client_ptr->SetSocket(socket_holder.Wrapper())));

  Timeout timeout{base::Milliseconds(100)};
  // Pretend waiting for new responses after 3 handled commands.
  socket_holder.Socket().SetResponseLimit(3);
  socket_holder.Socket().AddCommandHandler(
      "Runtime.evaluate", base::BindRepeating(&ReturnError, -32000, Message()));
  Status status = view.WaitForPendingNavigations("", timeout, false);
  EXPECT_EQ(kTimeout, status.code());
}

INSTANTIATE_TEST_SUITE_P(
    Timeout,
    WaitForPendingNavigations,
    ::testing::Values("uniqueContextId not found",
                      "Cannot find default execution context",
                      "Cannot find context with specified id",
                      "Execution context was destroyed.",
                      "Inspected target navigated or closed"));

namespace {

#if defined(MEMORY_SANITIZER)
base::TimeDelta kErrorWaitDuration = base::Seconds(100);
#elif defined(NDEBUG)
base::TimeDelta kErrorWaitDuration = base::Seconds(3);
#else
base::TimeDelta kErrorWaitDuration = base::Seconds(100);
#endif

class BidiDevToolsClient : public StubDevToolsClient {
 public:
  explicit BidiDevToolsClient(const std::string& id) : StubDevToolsClient(id) {}

  Status PostBidiCommand(base::Value::Dict command) override {
    base::Value* id = command.Find("id");
    EXPECT_NE(nullptr, id);
    if (id == nullptr) {
      return Status{kTestError,
                    "[BidiDevToolsClient], no 'id' in the BiDi command"};
    }
    std::string* method = command.FindString("method");
    EXPECT_NE(nullptr, method);
    if (method == nullptr) {
      return Status{kTestError,
                    "[BidiDevToolsClient], no 'method' in the BiDi command"};
    }
    base::Value::Dict* params = command.FindDict("param");
    EXPECT_NE(nullptr, params);
    if (params == nullptr) {
      return Status{kTestError,
                    "[BidiDevToolsClient], no 'params' in the BiDi command"};
    }

    std::string* channel = command.FindString("channel");

    base::Value::Dict result;
    std::optional<int> ping = params->FindInt("ping");
    if (ping) {
      result.Set("pong", *ping);
    } else {
      result.Set("param", 1);
    }

    base::Value::Dict payload;
    payload.Set("id", std::move(*id));
    payload.Set("result", std::move(result));
    if (channel != nullptr) {
      payload.Set("channel", std::move(*channel));
    }

    base::Value::Dict event_params;
    event_params.Set("name", "sendBidiResponse");
    event_params.Set("payload", std::move(payload));

    for (DevToolsEventListener* listener : listeners_) {
      listener->OnEvent(this, "Runtime.bindingCalled", event_params);
    }

    return Status{kOk};
  }

  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override {
    bool is_condition_met = false;
    Status status{kOk};
    do {
      status = conditional_func.Run(&is_condition_met);
    } while (status.IsOk() && !is_condition_met && !timeout.IsExpired());
    if (!is_condition_met && status.IsOk()) {
      return Status{kTimeout, "timed out by BidiDevToolsClient"};
    }
    return status;
  }

  int EventListenerCount() const { return static_cast<int>(listeners_.size()); }
};

}  // namespace

TEST(SendBidiCommandTest, Success) {
  std::unique_ptr<BidiDevToolsClient> client_uptr =
      std::make_unique<BidiDevToolsClient>("id");
  BidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("id", 1);
  command.Set("channel", "/test");
  command.Set("method", "some");
  command.Set("param", std::move(param));

  Timeout timeout{base::Seconds(1)};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  EXPECT_TRUE(
      StatusOk(view.SendBidiCommand(std::move(command), timeout, response)));
  EXPECT_THAT(response.FindIntByDottedPath("result.pong"), Optional(Eq(123)));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}

TEST(SendBidiCommandTest, MaxJsUintId) {
  // This test verifies that non-int32 ids are supported by the method.
  std::unique_ptr<BidiDevToolsClient> client_uptr =
      std::make_unique<BidiDevToolsClient>("id");
  BidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("id", 9007199254740991.0);
  command.Set("channel", "/test");
  command.Set("method", "some");
  command.Set("param", std::move(param));

  Timeout timeout{base::Seconds(1)};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  EXPECT_TRUE(
      StatusOk(view.SendBidiCommand(std::move(command), timeout, response)));
  EXPECT_THAT(response.FindIntByDottedPath("result.pong"), Optional(Eq(123)));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}

TEST(SendBidiCommandTest, NoId) {
  // This test verifies that the method won't loop forever or until the time is
  // out waiting for the response if the command id is missing.
  std::unique_ptr<BidiDevToolsClient> client_uptr =
      std::make_unique<BidiDevToolsClient>("id");
  BidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("channel", "/test");
  command.Set("method", "some");
  command.Set("param", std::move(param));

  Timeout timeout{kErrorWaitDuration};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(
      view.SendBidiCommand(std::move(command), timeout, response)));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}

class SendBidiCommandBadChannelTest
    : public testing::TestWithParam<std::optional<std::string>> {
 public:
  const std::optional<std::string>& Channel() { return GetParam(); }
};

TEST_P(SendBidiCommandBadChannelTest, BadChannel) {
  // This test checks that the command responds with kUnknownError to the
  // violation of the precondition that "channel" must be set.
  std::unique_ptr<BidiDevToolsClient> client_uptr =
      std::make_unique<BidiDevToolsClient>("id");
  BidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("id", 1);
  command.Set("method", "some");
  command.Set("param", std::move(param));
  if (Channel().has_value()) {
    command.Set("channel", *Channel());
  }

  Timeout timeout{kErrorWaitDuration};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  Status status = view.SendBidiCommand(std::move(command), timeout, response);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(),
              ::testing::ContainsRegex("non-empty string 'channel'"));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}

INSTANTIATE_TEST_SUITE_P(BadChannels,
                         SendBidiCommandBadChannelTest,
                         ::testing::Values(std::nullopt,
                                           "",
                                           "no-leading-slash"));

class SendBidiCommandSpecialChannelTest
    : public testing::TestWithParam<std::string> {
 public:
  const std::string& Channel() { return GetParam(); }
};

TEST_P(SendBidiCommandSpecialChannelTest, ChannelValues) {
  // This test verifies that any well formed channel string is supported
  std::unique_ptr<BidiDevToolsClient> client_uptr =
      std::make_unique<BidiDevToolsClient>("id");
  BidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("id", 1);
  command.Set("channel", Channel());
  command.Set("method", "some");
  command.Set("param", std::move(param));

  Timeout timeout{base::Seconds(1)};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  EXPECT_TRUE(
      StatusOk(view.SendBidiCommand(std::move(command), timeout, response)));
  EXPECT_THAT(response.FindIntByDottedPath("result.pong"), Optional(Eq(123)));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}

INSTANTIATE_TEST_SUITE_P(
    Channels,
    SendBidiCommandSpecialChannelTest,
    ::testing::Values(DevToolsClientImpl::kBidiChannelSuffix,
                      DevToolsClientImpl::kCdpTunnelChannel));

namespace {

class NeverReturningBidiDevToolsClient : public BidiDevToolsClient {
 public:
  explicit NeverReturningBidiDevToolsClient(const std::string& id)
      : BidiDevToolsClient(id) {}

  Status PostBidiCommand(base::Value::Dict command) override {
    return Status{kOk};
  }
};

}  // namespace

TEST(SendBidiCommandTest, NoResponse) {
  std::unique_ptr<NeverReturningBidiDevToolsClient> client_uptr =
      std::make_unique<NeverReturningBidiDevToolsClient>("id");
  NeverReturningBidiDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view("root", true, nullptr, &browser_info, std::move(client_uptr),
                   std::nullopt, PageLoadStrategy::kEager, true);

  base::Value::Dict param;
  param.Set("ping", 123);

  base::Value::Dict command;
  command.Set("id", 1);
  command.Set("channel", "/test");
  command.Set("method", "some");
  command.Set("param", std::move(param));

  Timeout timeout{base::Milliseconds(10)};
  base::Value::Dict response;
  const int initial_listener_count = client_ptr->EventListenerCount();
  EXPECT_TRUE(StatusCodeIs<kTimeout>(
      view.SendBidiCommand(std::move(command), timeout, response)));
  EXPECT_EQ(initial_listener_count, client_ptr->EventListenerCount());
}
