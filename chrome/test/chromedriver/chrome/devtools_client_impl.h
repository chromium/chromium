// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "url/gurl.h"

class DevToolsEventListener;
class Status;
class SyncWebSocket;

namespace internal {

enum InspectorMessageType {
  kEventMessageType = 0,
  kCommandResponseMessageType
};

}  // namespace internal

// The next invariant means that the hierarchy must be flat,
// we can have two levels of DevToolsClientImpl maximum.
// Invariant: parent == nullptr || children.empty()
class DevToolsClientImpl : public DevToolsClient {
 public:
  static const char kBrowserwideDevToolsClientId[];
  static const char kCdpTunnelChannel[];
  static const char kBidiChannelSuffix[];

  typedef base::RepeatingCallback<Status()> FrontendCloserFunc;

  // Postcondition: IsNull()
  // Postcondition: !IsConnected()
  DevToolsClientImpl(const std::string& id, const std::string& session_id);

  typedef base::RepeatingCallback<bool(const std::string&,
                                       int,
                                       std::string&,
                                       internal::InspectorMessageType&,
                                       InspectorEvent&,
                                       InspectorCommandResponse&)>
      ParserFunc;

  DevToolsClientImpl(const DevToolsClientImpl&) = delete;
  DevToolsClientImpl& operator=(const DevToolsClientImpl&) = delete;

  ~DevToolsClientImpl() override;

  void SetParserFuncForTesting(const ParserFunc& parser_func);
  // Make this DevToolsClient a child of 'parent'.
  // All the commands of the child are routed via the parent.
  // The parent demultiplexes the incoming events to the appropriate children.
  // Precondition: parent != nullptr
  // Precondition: parent.IsConnected()
  // Precondition: IsNull()
  // The next precondition secures the class invariant about flat hierarchy
  // Precondition: parent->GetParentClient() == nullptr.
  // Postcondition: result.IsError() || !IsNull()
  // Postcondition: result.IsError() || IsConnected()
  Status AttachTo(DevToolsClient* parent) override;

  // Set the socket for communication with the remote end.
  // All listeners are notified about the connection.
  // Precondition: socket != nullptr
  // Precondition: socket.IsConnected()
  // Precondition: IsNull()
  // Postcondition: result.IsError() || IsConnected()
  Status SetSocket(std::unique_ptr<SyncWebSocket> socket);

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  // Session id used to annotate the CDP commands.
  const std::string& SessionId() const override;
  // Session id used for CDP traffic tunneling
  const std::string& TunnelSessionId() const override;
  // Set the session id used for CDP traffic tunneling
  Status SetTunnelSessionId(std::string session_id) override;
  // Set the session id used for CDP traffic tunneling
  Status AppointAsBidiServerForTesting();
  // Start a BiDi Server in the connected target
  // Precondition: IsMainPage()
  // Precondition: IsConnected()
  // Precondition: BiDi tunnel for CDP traffic is not set.
  Status StartBidiServer(std::string bidi_mapper_script) override;
  Status StartBidiServer(std::string bidi_mapper_script,
                         const Timeout& timeout);
  // If the object IsNull then it cannot be connected to the remote end.
  // Such an object needs to be attached to some !IsNull() parent first.
  // Postcondition: IsNull() == (socket == nullptr && parent == nullptr)
  bool IsNull() const override;
  bool IsConnected() const override;
  bool WasCrashed() override;
  bool IsDialogOpen() const override;
  bool AutoAcceptsBeforeunload() const override;
  void SetAutoAcceptBeforeunload(bool value) override;
  Status PostBidiCommand(base::Value::Dict command) override;
  Status SendCommand(const std::string& method,
                     const base::Value::Dict& params) override;
  Status SendCommandFromWebSocket(const std::string& method,
                                  const base::Value::Dict& params,
                                  int client_command_id) override;
  Status SendCommandWithTimeout(const std::string& method,
                                const base::Value::Dict& params,
                                const Timeout* timeout) override;
  Status SendAsyncCommand(const std::string& method,
                          const base::Value::Dict& params) override;
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override;
  Status SendCommandAndGetResultWithTimeout(const std::string& method,
                                            const base::Value::Dict& params,
                                            const Timeout* timeout,
                                            base::Value::Dict* result) override;
  Status SendCommandAndIgnoreResponse(const std::string& method,
                                      const base::Value::Dict& params) override;

  // Add a listener for connection and events.
  // Listeners cannot be added to the object that is already connected.
  // Precondition: !IsConnected() || !listener.ListensToConnections()
  // Precondition: listener != nullptr
  void AddListener(DevToolsEventListener* listener) override;
  void RemoveListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;
  WebViewImpl* GetOwner() const override;
  DevToolsClient* GetParentClient() const override;
  bool IsMainPage() const override;
  void SetMainPage(bool value);
  int NextMessageId() const override;
  // Return NextMessageId and immediately increment it
  int AdvanceNextMessageId() override;
  void EnableEventTunnelingForTesting();

  Status SendRaw(const std::string& message) override;
  bool HasMessageForAnySession() const override;
  void RegisterSessionHandler(const std::string& session_id,
                              DevToolsClient* client) override;
  void UnregisterSessionHandler(const std::string& session_id) override;
  Status OnConnected() override;
  Status ProcessEvent(InspectorEvent event) override;
  Status ProcessCommandResponse(InspectorCommandResponse response) override;
  Status ProcessNextMessage(int expected_id,
                            bool log_timeout,
                            const Timeout& timeout,
                            DevToolsClient* caller) override;
  Status HandleMessage(int expected_id,
                       const std::string& message,
                       DevToolsClient* caller);
  Status GetDialogMessage(std::string& message) const override;
  Status GetTypeOfDialog(std::string& type) const override;
  Status HandleDialog(bool accept,
                      const std::optional<std::string>& text) override;

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
    InspectorCommandResponse response;
    Timeout command_timeout;

   private:
    friend class base::RefCounted<ResponseInfo>;
    ~ResponseInfo();
  };
  Status PostBidiCommandInternal(std::string channel,
                                 base::Value::Dict command);
  Status SendCommandInternal(const std::string& method,
                             const base::Value::Dict& params,
                             const std::string& session_id,
                             base::Value::Dict* result,
                             bool expect_response,
                             bool wait_for_response,
                             int client_command_id,
                             const Timeout* timeout);
  Status EnsureListenersNotifiedOfConnect();
  Status EnsureListenersNotifiedOfEvent();
  Status EnsureListenersNotifiedOfCommandResponse();
  Status SetUpDevTools();
  Status HandleDialogOpening(const base::Value::Dict& params);
  Status HandleDialogClosed(const base::Value::Dict& params);

  std::unique_ptr<SyncWebSocket> socket_;
  // WebViewImpl that owns this instance; nullptr for browser-wide DevTools.
  raw_ptr<WebViewImpl> owner_ = nullptr;
  const std::string session_id_;
  std::string tunnel_session_id_;
  // parent_ / children_: it's a flat hierarchy - nesting is at most one level
  // deep. children_ holds child sessions - identified by their session id -
  // which send/receive messages via the socket_ of their parent.
  raw_ptr<DevToolsClient> parent_ = nullptr;
  std::map<std::string, raw_ptr<DevToolsClient, CtnExperimental>> children_;
  bool crashed_ = false;
  bool detached_ = false;
  // For the top-level session, this is the target id.
  // For child sessions, it's the session id.
  const std::string id_;
  ParserFunc parser_func_;
  std::list<raw_ptr<DevToolsEventListener, CtnExperimental>> listeners_;
  std::list<raw_ptr<DevToolsEventListener, CtnExperimental>>
      unnotified_connect_listeners_;
  std::list<raw_ptr<DevToolsEventListener, CtnExperimental>>
      unnotified_event_listeners_;
  raw_ptr<const InspectorEvent> unnotified_event_ = nullptr;
  std::list<raw_ptr<DevToolsEventListener, CtnExperimental>>
      unnotified_cmd_response_listeners_;
  scoped_refptr<ResponseInfo> unnotified_cmd_response_info_;
  std::map<int, scoped_refptr<ResponseInfo>> response_info_map_;
  int next_id_ = 1;  // The id identifying a particular request.
  bool is_main_page_ = false;
  std::list<std::string> unhandled_dialog_queue_;
  std::list<std::string> dialog_type_queue_;
  std::string prompt_text_;
  bool autoaccept_beforeunload_ = false;
  // Event tunneling is temporarily disabled in production.
  // It is enabled only by the unit tests
  // TODO(chromedriver:4181): Enable CDP event tunneling
  bool event_tunneling_is_enabled_ = false;
  base::WeakPtrFactory<DevToolsClientImpl> weak_ptr_factory_{this};
};

namespace internal {

bool ParseInspectorMessage(const std::string& message,
                           int expected_id,
                           std::string& session_id,
                           InspectorMessageType& type,
                           InspectorEvent& event,
                           InspectorCommandResponse& command_response);

Status ParseInspectorError(const std::string& error_json);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
