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
#include "base/memory/raw_ptr.h"
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

  DevToolsClientImpl(const std::string& id,
                     const std::string& session_id,
                     const std::string& url,
                     const SyncWebSocketFactory& factory);

  typedef base::RepeatingCallback<Status()> FrontendCloserFunc;

  DevToolsClientImpl(const std::string& id,
                     const std::string& session_id,
                     DevToolsClientImpl* parent);

  typedef base::RepeatingCallback<bool(const std::string&,
                                       int,
                                       std::string*,
                                       internal::InspectorMessageType*,
                                       internal::InspectorEvent*,
                                       internal::InspectorCommandResponse*)>
      ParserFunc;

  DevToolsClientImpl(const DevToolsClientImpl&) = delete;
  DevToolsClientImpl& operator=(const DevToolsClientImpl&) = delete;

  ~DevToolsClientImpl() override;

  void SetParserFuncForTesting(const ParserFunc& parser_func);
  void SetFrontendCloserFunc(const FrontendCloserFunc& frontend_closer_func);

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  bool WasCrashed() override;
  Status ConnectIfNecessary() override;
  Status SetUpDevTools() override;
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
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::DictionaryValue& params,
                                 base::Value* result) override;
  Status SendCommandAndGetResultWithTimeout(const std::string& method,
                                            const base::DictionaryValue& params,
                                            const Timeout* timeout,
                                            base::Value* result) override;
  Status SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::DictionaryValue& params) override;
  void AddListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;
  WebViewImpl* GetOwner() const override;
  DevToolsClient* GetRootClient() override;

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
                             base::Value* result,
                             bool expect_response,
                             bool wait_for_response,
                             int client_command_id,
                             const Timeout* timeout);
  Status ProcessNextMessage(int expected_id,
                            bool log_timeout,
                            const Timeout& timeout);
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
  raw_ptr<WebViewImpl> owner_;
  const std::string session_id_;
  // parent_ / children_: it's a flat hierarchy - nesting is at most one level
  // deep. children_ holds child sessions - identified by their session id -
  // which send/receive messages via the socket_ of their parent.
  raw_ptr<DevToolsClientImpl> parent_;
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
  raw_ptr<const internal::InspectorEvent> unnotified_event_;
  std::list<DevToolsEventListener*> unnotified_cmd_response_listeners_;
  scoped_refptr<ResponseInfo> unnotified_cmd_response_info_;
  std::map<int, scoped_refptr<ResponseInfo>> response_info_map_;
  int next_id_;  // The id identifying a particular request.
  int stack_count_;
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
