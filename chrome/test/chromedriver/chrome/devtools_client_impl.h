// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace internal {

enum InspectorMessageType {
  kEventMessageType = 0,
  kCommandResponseMessageType
};

struct InspectorEvent {
  InspectorEvent();
  ~InspectorEvent();
  std::string method;
  std::unique_ptr<base::DictionaryValue> params;
};

struct InspectorCommandResponse {
  InspectorCommandResponse();
  ~InspectorCommandResponse();
  int id;
  std::string error;
  std::unique_ptr<base::DictionaryValue> result;
};

}  // namespace internal

class DevToolsEventListener;
class Status;
class SyncWebSocket;

class DevToolsClientImpl : public DevToolsClient {
 public:
  static const char kBrowserwideDevToolsClientId[];

  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const std::string& id);

  typedef base::Callback<Status()> FrontendCloserFunc;
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const std::string& id,
                     const FrontendCloserFunc& frontend_closer_func);

  DevToolsClientImpl(DevToolsClientImpl* parent, const std::string& session_id);

  typedef base::Callback<bool(const std::string&,
                              int,
                              std::string*,
                              internal::InspectorMessageType*,
                              internal::InspectorEvent*,
                              internal::InspectorCommandResponse*)>
      ParserFunc;
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const std::string& id,
                     const FrontendCloserFunc& frontend_closer_func,
                     const ParserFunc& parser_func);

  ~DevToolsClientImpl() override;

  void SetParserFuncForTesting(const ParserFunc& parser_func);

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  bool WasCrashed() override;
  Status ConnectIfNecessary() override;
  Status SendCommand(
      const std::string& method,
      const base::DictionaryValue& params) override;
  Status SendCommandFromWebSocket(const std::string& method,
                                  const base::DictionaryValue& params,
                                  int client_command_id) override;
  Status SendCommandWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout) override;
  Status SendAsyncCommand(
      const std::string& method,
      const base::DictionaryValue& params) override;
  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override;
  Status SendCommandAndGetResultWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout,
      std::unique_ptr<base::DictionaryValue>* result) override;
  Status SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::DictionaryValue& params) override;
  void AddListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;
  DevToolsClientImpl* GetRootClient();

 private:
  enum ResponseState {
    // The client is waiting for the response.
    kWaiting,
    // The command response will not be received because it is blocked by an
    // alert that the command triggered.
    kBlocked,
    // The client no longer cares about the response.
    kIgnored,
    // The response has been received.
    kReceived
  };
  struct ResponseInfo : public base::RefCounted<ResponseInfo> {
   public:
    explicit ResponseInfo(const std::string& method);

    ResponseState state;
    std::string method;
    internal::InspectorCommandResponse response;
    Timeout command_timeout;

   private:
    friend class base::RefCounted<ResponseInfo>;
    ~ResponseInfo();
  };
  Status SendCommandInternal(const std::string& method,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::DictionaryValue>* result,
                             bool expect_response,
                             bool wait_for_response,
                             int client_command_id,
                             const Timeout* timeout);
  Status ProcessNextMessage(int expected_id, const Timeout& timeout);
  Status HandleMessage(int expected_id, const std::string& message);
  Status ProcessEvent(const internal::InspectorEvent& event);
  Status ProcessCommandResponse(
      const internal::InspectorCommandResponse& response);
  Status EnsureListenersNotifiedOfConnect();
  Status EnsureListenersNotifiedOfEvent();
  Status EnsureListenersNotifiedOfCommandResponse();

  std::unique_ptr<SyncWebSocket> socket_;
  GURL url_;
  // WebViewImpl that owns this instance; nullptr for browser-wide DevTools.
  WebViewImpl* owner_;
  const std::string session_id_;
  // parent_ / children_: it's a flat hierarchy - nesting is at most one level
  // deep. children_ holds child sessions - identified by their session id -
  // which send/receive messages via the socket_ of their parent.
  DevToolsClientImpl* parent_;
  std::map<std::string, DevToolsClientImpl*> children_;
  bool crashed_;
  bool detached_;
  // For the top-level session, this is the target id.
  // For child sessions, it's the session id.
  const std::string id_;
  FrontendCloserFunc frontend_closer_func_;
  ParserFunc parser_func_;
  std::list<DevToolsEventListener*> listeners_;
  std::list<DevToolsEventListener*> unnotified_connect_listeners_;
  std::list<DevToolsEventListener*> unnotified_event_listeners_;
  const internal::InspectorEvent* unnotified_event_;
  std::list<DevToolsEventListener*> unnotified_cmd_response_listeners_;
  scoped_refptr<ResponseInfo> unnotified_cmd_response_info_;
  std::map<int, scoped_refptr<ResponseInfo>> response_info_map_;
  int next_id_;  // The id identifying a particular request.
  int stack_count_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClientImpl);
};

namespace internal {

bool ParseInspectorMessage(const std::string& message,
                           int expected_id,
                           std::string* session_id,
                           InspectorMessageType* type,
                           InspectorEvent* event,
                           InspectorCommandResponse* command_response);

Status ParseInspectorError(const std::string& error_json);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
