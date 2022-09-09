// Copyright 2012 The Chromium Authors
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

// The next invariant means that the hierarchy must be flat,
// we can have two levels of DevToolsClientImpl maximum.
// Invariant: parent == nullptr || children.empty()
class DevToolsClientImpl : public DevToolsClient {
 public:
  static const char kBrowserwideDevToolsClientId[];

  // Postcondition: !IsNull()
  // Postcondition: !IsConnected()
  DevToolsClientImpl(const std::string& id,
                     const std::string& session_id,
                     const std::string& url,
                     const SyncWebSocketFactory& factory);

  typedef base::RepeatingCallback<Status()> FrontendCloserFunc;

  // Postcondition: IsNull()
  // Postcondition: !IsConnected()
  DevToolsClientImpl(const std::string& id, const std::string& session_id);

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
  // Make this DevToolsClient a child of 'parent'.
  // All the commands of the child are routed via the parent.
  // The parent demultiplexes the incoming events to the appropriate children.
  // If the parent->IsConnected() this object changes its state to connected,
  // it sets up the remote end and notifies the listeners about the connection.
  // Precondition: parent != nullptr
  // Precondition: IsNull()
  // The next precondition secures the class invariant about flat hierarchy
  // Precondition: parent->GetParentClient() == nullptr.
  // Postcondition: result.IsError() || !IsNull()
  Status AttachTo(DevToolsClientImpl* parent);

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  // If the object IsNull then it cannot be connected to the remote end.
  // Such an object needs to be attached to some !IsNull() parent first.
  // Postcondition: IsNull() == (socket == nullptr && parent == nullptr)
  bool IsNull() const override;
  bool IsConnected() const;
  bool WasCrashed() override;
  // Connect and configure the remote end.
  // The children are also connected and their remote ends are configured.
  // The listeners and the listeners of the children are notified appropriately.
  // Does nothing if the connection is already established.
  // Precondition: !IsNull()
  // Postcondition: result.IsError() || IsConnected()
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

  // Add a listener for connection and events.
  // Listeners cannot be added to the object that is already connected.
  // Precondition: !IsConnected() || !listener.ListensToConnections()
  // Precondition: listener != nullptr
  void AddListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;
  WebViewImpl* GetOwner() const override;
  DevToolsClient* GetRootClient() override;
  DevToolsClient* GetParentClient() const override;
  bool IsMainPage() const override;
  void SetMainPage(bool value);
  int NextMessageId() const;

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
                            const Timeout& timeout,
                            DevToolsClientImpl* caller);
  Status HandleMessage(int expected_id,
                       const std::string& message,
                       DevToolsClientImpl* caller);
  Status ProcessEvent(const internal::InspectorEvent& event);
  Status ProcessCommandResponse(
      const internal::InspectorCommandResponse& response);
  Status EnsureListenersNotifiedOfConnect();
  Status EnsureListenersNotifiedOfEvent();
  Status EnsureListenersNotifiedOfCommandResponse();
  void ResetListeners();
  Status OnConnected();
  Status SetUpDevTools();

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
  bool is_remote_end_configured_;
  bool is_main_page_;
  base::WeakPtrFactory<DevToolsClientImpl> weak_ptr_factory_{this};
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
