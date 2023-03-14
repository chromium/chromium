// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace internal {

enum InspectorMessageType {
  kEventMessageType = 0,
  kCommandResponseMessageType
};

struct InspectorEvent {
  InspectorEvent();
  ~InspectorEvent();
  std::string method;
  absl::optional<base::Value::Dict> params;
};

struct InspectorCommandResponse {
  InspectorCommandResponse();
  ~InspectorCommandResponse();
  int id;
  std::string error;
  absl::optional<base::Value::Dict> result;
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
  static const char kCdpTunnelChannel[];
  static const char kBidiChannelSuffix[];

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
  // Connect and configure the remote end.
  // The children are also connected and their remote ends are configured.
  // The listeners and the listeners of the children are notified appropriately.
  // Does nothing if the connection is already established.
  // Precondition: socket != nullptr
  // Postcondition: result.IsError() || IsConnected()
  Status Connect() override;
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
  DevToolsClient* GetRootClient() override;
  DevToolsClient* GetParentClient() const override;
  bool IsMainPage() const override;
  void SetMainPage(bool value);
  int NextMessageId() const;
  // Return NextMessageId and immediately increment it
  int AdvanceNextMessageId();
  void EnableEventTunnelingForTesting();

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
  raw_ptr<WebViewImpl> owner_ = nullptr;
  const std::string session_id_;
  std::string tunnel_session_id_;
  // parent_ / children_: it's a flat hierarchy - nesting is at most one level
  // deep. children_ holds child sessions - identified by their session id -
  // which send/receive messages via the socket_ of their parent.
  raw_ptr<DevToolsClientImpl> parent_ = nullptr;
  std::map<std::string, DevToolsClientImpl*> children_;
  bool crashed_ = false;
  bool detached_ = false;
  // For the top-level session, this is the target id.
  // For child sessions, it's the session id.
  const std::string id_;
  FrontendCloserFunc frontend_closer_func_;
  ParserFunc parser_func_;
  std::list<DevToolsEventListener*> listeners_;
  std::list<DevToolsEventListener*> unnotified_connect_listeners_;
  std::list<DevToolsEventListener*> unnotified_event_listeners_;
  raw_ptr<const internal::InspectorEvent> unnotified_event_ = nullptr;
  std::list<DevToolsEventListener*> unnotified_cmd_response_listeners_;
  scoped_refptr<ResponseInfo> unnotified_cmd_response_info_;
  std::map<int, scoped_refptr<ResponseInfo>> response_info_map_;
  int next_id_ = 1;  // The id identifying a particular request.
  int stack_count_ = 0;
  bool is_main_page_ = false;
  bool bidi_server_is_launched_ = false;
  // Event tunneling is temporarily disabled in production.
  // It is enabled only by the unit tests
  // TODO(chromedriver:4181): Enable CDP event tunneling
  bool event_tunneling_is_enabled_ = false;
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
