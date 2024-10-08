// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <cstring>
#include <memory>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/util.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"

namespace {

const char kInspectorDefaultContextError[] =
    "Cannot find default execution context";
const char kInspectorContextError[] = "Cannot find context with specified id";
const char kInspectorInvalidURL[] = "Cannot navigate to invalid URL";
const char kInspectorInsecureContext[] =
    "Permission can't be granted in current context.";
const char kInspectorOpaqueOrigins[] =
    "Permission can't be granted to opaque origins.";
const char kInspectorPushPermissionError[] =
    "Push Permission without userVisibleOnly:true isn't supported";
const char kInspectorNoSuchFrameError[] =
    "Frame with the given id was not found.";
const char kNoTargetWithGivenIdError[] = "No target with given id found";
const char kUniqueContextIdNotFoundError[] = "uniqueContextId not found";
const char kNoNodeForBackendNodeIdError[] =
    "No node found for given backend id";
const char kNoNodeWithGivenIdFoundError[] = "No node with given id found";
const char kExecutionContextWasDestroyed[] = "Execution context was destroyed.";
const char kInspectedTargetNavigatedOrClosed[] =
    "Inspected target navigated or closed";

static constexpr int kSessionNotFoundInspectorCode = -32001;
static constexpr int kCdpMethodNotFoundCode = -32601;
static constexpr int kInvalidParamsInspectorCode = -32602;

class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(int* count) : count_(count) {
    (*count_)++;
  }
  ~ScopedIncrementer() {
    (*count_)--;
  }

 private:
  raw_ptr<int> count_ = nullptr;
};

Status ConditionIsMet(bool* is_condition_met) {
  *is_condition_met = true;
  return Status(kOk);
}

struct SessionId {
  explicit SessionId(std::string session_id)
      : session_id_(std::move(session_id)) {}
  std::string session_id_;
};

std::ostream& operator<<(std::ostream& os, const SessionId& ses_manip) {
  return os << " (session_id=" << ses_manip.session_id_ << ")";
}

Status IsBidiMessage(const std::string& method,
                     const base::Value::Dict& params,
                     bool& is_bidi_message) {
  is_bidi_message = false;
  if (method != "Runtime.bindingCalled") {
    return Status{kOk};
  }
  const std::string* name = params.FindString("name");
  if (name == nullptr) {
    return Status{kUnknownError,
                  "name is missing in the Runtime.bindingCalled params"};
  }
  if (*name != "sendBidiResponse" && *name != "sendDebugMessage") {
    return Status{kOk};
  }

  is_bidi_message = true;
  return Status{kOk};
}

Status DeserializePayload(const base::Value::Dict& params,
                          base::Value::Dict* result) {
  result->clear();
  const std::string* payload = params.FindString("payload");
  if (!payload) {
    return Status{kUnknownError,
                  "payload is missing in the Runtime.bindingCalled params"};
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(*payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value || !value->is_dict()) {
    return Status{kUnknownError, "unable to deserialize the BiDi payload"};
  }

  *result = std::move(value->GetDict());
  return Status{kOk};
}

Status WrapCdpCommandInBidiCommand(base::Value::Dict cdp_cmd,
                                   base::Value::Dict* bidi_cmd) {
  std::optional<int> cdp_cmd_id = cdp_cmd.FindInt("id");
  if (!cdp_cmd_id) {
    return Status(kUnknownError, "CDP command has no 'id' field");
  }

  std::string* cdp_method = cdp_cmd.FindString("method");
  if (!cdp_method) {
    return Status(kUnknownError, "CDP command has no 'method' field");
  }

  std::string* cdp_session_id = cdp_cmd.FindString("sessionId");
  base::Value::Dict* cdp_params = cdp_cmd.FindDict("params");

  base::Value::Dict params;
  params.Set("cdpMethod", std::move(*cdp_method));
  if (cdp_session_id) {
    params.Set("cdpSession", std::move(*cdp_session_id));
  }
  if (cdp_params) {
    params.Set("cdpParams", std::move(*cdp_params));
  }

  base::Value::Dict dict;
  dict.Set("id", *cdp_cmd_id);
  dict.Set("method", "cdp.sendCommand");
  dict.Set("params", std::move(params));
  dict.Set("channel", DevToolsClientImpl::kCdpTunnelChannel);
  *bidi_cmd = std::move(dict);
  return Status{kOk};
}

Status WrapBidiCommandInMapperCdpCommand(int cdp_cmd_id,
                                         const base::Value::Dict& bidi_cmd,
                                         std::string mapper_session_id,
                                         base::Value::Dict* cmd) {
  std::string json;
  Status status = SerializeAsJson(bidi_cmd, &json);
  if (status.IsError()) {
    return status;
  }
  std::string arg;
  status = SerializeAsJson(json, &arg);
  if (status.IsError()) {
    return status;
  }
  std::string expression = "onBidiMessage(" + arg + ")";
  base::Value::Dict params;
  params.Set("expression", std::move(expression));

  base::Value::Dict dict;
  dict.Set("id", cdp_cmd_id);
  dict.Set("method", "Runtime.evaluate");
  dict.Set("params", std::move(params));
  DCHECK(!mapper_session_id.empty());
  dict.Set("sessionId", std::move(mapper_session_id));
  *cmd = std::move(dict);
  return Status{kOk};
}

bool ParseCdpTunnelMessage(base::Value::Dict payload,
                           std::string& session_id,
                           internal::InspectorMessageType& type,
                           InspectorEvent& event,
                           InspectorCommandResponse& command_response) {
  // handle CDP over BiDi events and responses
  const std::string* payload_method = payload.FindString("method");

  if (payload_method && *payload_method == "cdp.eventReceived") {  // CDP event
    base::Value::Dict* payload_params = payload.FindDict("params");
    if (!payload_params) {
      LOG(WARNING) << "params field is missing in the payload of "
                      "Runtime.bindingCalled message";
      return false;
    }
    const std::string* cdp_method = payload_params->FindString("cdpMethod");
    if (!cdp_method) {
      LOG(WARNING) << "params.cdpMethod is missing in the payload of "
                      "Runtime.bindingCalled message";
      return false;
    }

    type = internal::kEventMessageType;
    event.method = *cdp_method;
    const std::string* cdp_session = payload_params->FindString("cdpSession");
    session_id = cdp_session ? *cdp_session : "";

    base::Value::Dict* cdp_params = payload_params->FindDict("cdpParams");
    if (cdp_params) {
      event.params = std::move(*cdp_params);
    } else {
      event.params = base::Value::Dict();
    }
  } else {  // CDP command response

    std::optional<int> cdp_id = payload.FindInt("id");
    if (!cdp_id) {
      LOG(WARNING) << "tunneled CDP response has no id";
      return false;
    }

    const std::string* cdp_session = payload.FindString("cdpSession");
    session_id = cdp_session ? *cdp_session : "";

    base::Value::Dict* cdp_result = payload.FindDict("result");
    const base::Value::Dict* cdp_error = payload.FindDict("error");

    type = internal::kCommandResponseMessageType;
    command_response.id = *cdp_id;
    // As per Chromium issue 392577, DevTools does not necessarily return
    // a "result" dictionary for every valid response. In particular,
    // Tracing.start and Tracing.end command responses do not contain one.
    // So, if neither "error" nor "result" keys are present, just provide
    // a blank result dictionary.
    if (cdp_result) {
      command_response.result = std::move(*cdp_result);
    } else if (cdp_error) {
      base::JSONWriter::Write(*cdp_error, &command_response.error);
    } else {
      command_response.result = base::Value::Dict();
    }
  }
  return true;
}

}  // namespace

const char DevToolsClientImpl::kBrowserwideDevToolsClientId[] = "browser";
const char DevToolsClientImpl::kCdpTunnelChannel[] = "/cdp";
const char DevToolsClientImpl::kBidiChannelSuffix[] = "/bidi";

DevToolsClientImpl::DevToolsClientImpl(const std::string& id,
                                       const std::string& session_id)
    : session_id_(session_id),
      id_(id),
      parser_func_(base::BindRepeating(&internal::ParseInspectorMessage)) {}

DevToolsClientImpl::~DevToolsClientImpl() {
  if (IsNull()) {
    return;
  }
  if (parent_ != nullptr) {
    parent_->UnregisterSessionHandler(session_id_);
  } else {
    // Resetting the callback is redundant as we assume
    // that .dtor won't start a nested message loop.
    // Doing this just in case.
    socket_->SetNotificationCallback(base::RepeatingClosure());
  }
}

void DevToolsClientImpl::SetParserFuncForTesting(
    const ParserFunc& parser_func) {
  parser_func_ = parser_func;
}

const std::string& DevToolsClientImpl::GetId() {
  return id_;
}

const std::string& DevToolsClientImpl::SessionId() const {
  return session_id_;
}

const std::string& DevToolsClientImpl::TunnelSessionId() const {
  return tunnel_session_id_;
}

Status DevToolsClientImpl::SetTunnelSessionId(std::string session_id) {
  if (!tunnel_session_id_.empty()) {
    return Status{kUnknownError,
                  "BiDi tunnel is already set up in this client"};
  }
  tunnel_session_id_ = std::move(session_id);
  return Status{kOk};
}

Status DevToolsClientImpl::StartBidiServer(std::string bidi_mapper_script) {
  // Give BiDiMapper generous amount of time to start.
  // If the wait times out then we likely have a bug in BiDiMapper.
  // There is no need to make this timeout user configurable.
  // We use the default page load timeout (the biggest in the standard).
  Timeout timeout = Timeout(base::Seconds(300));
  return StartBidiServer(std::move(bidi_mapper_script), timeout);
}

Status DevToolsClientImpl::StartBidiServer(
    std::string bidi_mapper_script,
    const Timeout& timeout) {
  if (!is_main_page_) {
    // Later we might want to start the BiDiMapper an another type of targets
    // however for the moment being we support pages only.
    return Status{kUnknownError,
                  "BiDi server can only be started by a page client"};
  }
  if (!IsConnected()) {
    return Status{kUnknownError,
                  "BiDi server setup requires existing connection"};
  }
  if (!tunnel_session_id_.empty()) {
    return Status{kUnknownError,
                  "BiDi tunnel is already set up in this client"};
  }
  Status status{kOk};
  // Page clients have target_id coinciding with id
  std::string target_id = id_;
  {
    base::Value::Dict params;
    params.Set("bindingName", "cdp");
    params.Set("targetId", target_id);
    DevToolsClient* root_client = this;
    while (root_client->GetParentClient() != nullptr) {
      root_client = root_client->GetParentClient();
    }
    status = root_client->SendCommandAndIgnoreResponse(
        "Target.exposeDevToolsProtocol", std::move(params));
    if (status.IsError()) {
      return status;
    }
  }
  {
    base::Value::Dict params;
    params.Set("name", "sendBidiResponse");
    status =
        SendCommandAndIgnoreResponse("Runtime.addBinding", std::move(params));
    if (status.IsError()) {
      return status;
    }
  }
  if (IsVLogOn(Log::kDebug)) {
    // If debug logs are on, provide a channel for Mapper debug logs.
    base::Value::Dict params;
    params.Set("name", "sendDebugMessage");
    status =
        SendCommandAndIgnoreResponse("Runtime.addBinding", std::move(params));
    if (status.IsError()) {
      return status;
    }
  }
  {
    // Click on the Mapper tab to interact with the page in order to
    // "beforeunload" being triggered when the tab is closed.
    base::Value::Dict params;
    params.Set("expression", "document.body.click()");
    params.Set("userGesture", true);
    status =
        SendCommandAndIgnoreResponse("Runtime.evaluate", std::move(params));
    if (status.IsError()) {
      return status;
    }
  }
  {
    base::Value::Dict params;
    params.Set("expression", std::move(bidi_mapper_script));
    base::Value::Dict result;
    status = SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", std::move(params), &timeout, &result);

    if (result.contains("exceptionDetails")) {
      std::string description = "unknown";
      if (const std::string* maybe_description =
              result.FindStringByDottedPath("result.description")) {
        description = *maybe_description;
      }
      return Status(kUnknownError,
                    "Failed to initialize BiDi Mapper: " + description);
    }

    if (status.IsError()) {
      return status;
    }
  }
  {
    base::Value::Dict result;
    base::Value::Dict params;
    std::string window_id;
    status = SerializeAsJson(target_id, &window_id);
    if (status.IsError()) {
      return status;
    }

    params.Set("expression", base::StringPrintf("window.runMapperInstance(%s)",
                                                window_id.c_str()));
    params.Set("awaitPromise", true);
    status = SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", std::move(params), &timeout, &result);
    if (result.contains("exceptionDetails")) {
      std::string description = "unknown";
      if (const std::string* maybe_description =
              result.FindStringByDottedPath("result.description")) {
        description = *maybe_description;
      }
      return Status(kUnknownError,
                    "Failed to initialize BiDi Mapper: " + description);
    }
    if (status.IsError()) {
      return status;
    }
  }
  // We know that the current DevToolsClient is a CDP tunnel now
  tunnel_session_id_ = session_id_;

  if (event_tunneling_is_enabled_) {
    base::Value::Dict params;
    params.Set("events", "cdp.eventReceived");
    base::Value::Dict bidi_cmd;
    bidi_cmd.Set("id", AdvanceNextMessageId());
    bidi_cmd.Set("method", "session.subscribe");
    bidi_cmd.Set("params", std::move(params));
    status = PostBidiCommandInternal(DevToolsClientImpl::kCdpTunnelChannel,
                                     std::move(bidi_cmd));
  }

  return status;
}

Status DevToolsClientImpl::AppointAsBidiServerForTesting() {
  is_main_page_ = true;
  tunnel_session_id_ = session_id_;
  return Status{kOk};
}

bool DevToolsClientImpl::WasCrashed() {
  return crashed_;
}

bool DevToolsClientImpl::IsNull() const {
  return parent_.get() == nullptr && socket_.get() == nullptr;
}

bool DevToolsClientImpl::IsConnected() const {
  return parent_ ? parent_->IsConnected()
                 : (socket_ ? socket_->IsConnected() : false);
}

Status DevToolsClientImpl::AttachTo(DevToolsClient* parent) {
  // checking the preconditions
  if (parent == nullptr) {
    return Status{kUnknownError, "parent cannot be nullptr"};
  }
  if (!IsNull()) {
    return Status{
        kUnknownError,
        "attaching non-null DevToolsClient to a new parent is prohibited"};
  }
  // Class invariant: the hierarchy is flat
  if (parent->GetParentClient() != nullptr) {
    return Status{kUnknownError,
                  "DevToolsClientImpl can be attached only to a root client"};
  }
  if (!parent->IsConnected()) {
    return Status{kUnknownError, "cannot attach to a disconnected parent"};
  }

  Status status{kOk};

  parent_ = parent;
  parent_->RegisterSessionHandler(session_id_, this);

  if (parent->IsConnected())
    status = OnConnected();

  return status;
}

void DevToolsClientImpl::RegisterSessionHandler(const std::string& session_id,
                                                DevToolsClient* client) {
  children_[session_id] = client;
}

void DevToolsClientImpl::UnregisterSessionHandler(
    const std::string& session_id) {
  children_.erase(session_id);
}

Status DevToolsClientImpl::SetSocket(std::unique_ptr<SyncWebSocket> socket) {
  if (!socket) {
    return Status{kUnknownError, "socket cannot be nullptr"};
  }
  if (!socket->IsConnected()) {
    return Status{kUnknownError, "socket must be connected"};
  }
  socket_ = std::move(socket);
  socket_->SetId(id_);
  // If error happens during proactive event consumption we ignore it
  // as there is no active user request where the error might be returned.
  // Unretained 'this' won't cause any problems as we reset the callback in the
  // .dtor.
  socket_->SetNotificationCallback(base::BindRepeating(
      base::IgnoreResult(&DevToolsClientImpl::HandleReceivedEvents),
      base::Unretained(this)));

  return OnConnected();
}

Status DevToolsClientImpl::OnConnected() {
  // checking the preconditions
  DCHECK(IsConnected());
  if (!IsConnected()) {
    return Status{kUnknownError,
                  "The remote end can be configured only if the connection is "
                  "established"};
  }

  Status status = SetUpDevTools();
  if (status.IsError()) {
    return status;
  }

  // Notify all listeners of the new connection. Do this now so that any errors
  // that occur are reported now instead of later during some unrelated call.
  // Also gives listeners a chance to send commands before other clients.
  status = EnsureListenersNotifiedOfConnect();
  if (status.IsError()) {
    return status;
  }

  for (auto& [session_id, client] : children_) {
    status = client->OnConnected();
    if (status.IsError()) {
      break;
    }
  }

  return status;
}

Status DevToolsClientImpl::SetUpDevTools() {
  if (id_ != kBrowserwideDevToolsClientId &&
      (GetOwner() == nullptr || !GetOwner()->IsServiceWorker())) {
    // The method Page.addScriptToEvaluateOnNewDocument used below has no effect
    // until Page domain is enabled.
    // In majority of cases the event listeners like NavigationTracker enable
    // this domain quickly enough to mask the problem.
    // In rare occasions (2% of cases) they kick in too late and therefore the
    // tests like testExecuteScriptWithDeletedGlobalJSON can fail.
    // To avoid such flakiness we enable the Page domain first.
    SendCommandAndIgnoreResponse("Page.enable", base::Value::Dict());

    // This is a page or frame level DevToolsClient
    base::Value::Dict params;
    std::string script =
        "(function () {"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Array = window.Array;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Object = window.Object;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Promise = window.Promise;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Proxy = window.Proxy;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Symbol = window.Symbol;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_JSON = window.JSON;"
        "}) ();";
    params.Set("source", script);
    Status status = SendCommandAndIgnoreResponse(
        "Page.addScriptToEvaluateOnNewDocument", params);
    if (status.IsError())
      return status;

    params.clear();
    params.Set("expression", script);
    status = SendCommandAndIgnoreResponse("Runtime.evaluate", params);
    if (status.IsError())
      return status;
  }

  return Status{kOk};
}

Status DevToolsClientImpl::PostBidiCommand(base::Value::Dict command) {
  std::string* maybe_channel = command.FindString("channel");
  std::string channel =
      maybe_channel ? *maybe_channel + DevToolsClientImpl::kBidiChannelSuffix
                    : std::string();
  // Corner cases:
  // In user message channel=nullptr
  //    -> the posted command has no channel
  //    -> the message from the BiDiMapper has no channel
  //    -> the response has no channel
  // In user message channel=""
  //     -> the posted command has channel="/bidi"
  //     -> the message from the BiDiMapper has channel="/bidi"
  //     -> the response has channel=""
  // The user names their channel the same way as our infra cdp channel
  // channel="/cdp"
  //     -> the posted command has channel="/cdp/bidi"
  //     -> the message from the BiDiMapper has channel="/cdp/bidi"
  //     -> the response has channel="/cdp"
  // The user names their channel as our user channel suffix
  // channel="/bidi"
  //     -> the posted command has channel="/bidi/bidi"
  //     -> the message from the BiDiMapper has channel="/bidi/bidi"
  //     -> the response has channel="/bidi"

  return PostBidiCommandInternal(std::move(channel), std::move(command));
}

Status DevToolsClientImpl::SendCommand(const std::string& method,
                                       const base::Value::Dict& params) {
  return SendCommandWithTimeout(method, params, nullptr);
}

Status DevToolsClientImpl::SendCommandFromWebSocket(
    const std::string& method,
    const base::Value::Dict& params,
    int client_command_id) {
  return SendCommandInternal(method, params, session_id_, nullptr, false, false,
                             client_command_id, nullptr);
}

Status DevToolsClientImpl::SendCommandWithTimeout(
    const std::string& method,
    const base::Value::Dict& params,
    const Timeout* timeout) {
  base::Value::Dict result;
  return SendCommandInternal(method, params, session_id_, &result, true, true,
                             0, timeout);
}

Status DevToolsClientImpl::SendAsyncCommand(const std::string& method,
                                            const base::Value::Dict& params) {
  base::Value::Dict result;
  return SendCommandInternal(method, params, session_id_, &result, false, false,
                             0, nullptr);
}

Status DevToolsClientImpl::SendCommandAndGetResult(
    const std::string& method,
    const base::Value::Dict& params,
    base::Value::Dict* result) {
  return SendCommandAndGetResultWithTimeout(method, params, nullptr, result);
}

Status DevToolsClientImpl::SendCommandAndGetResultWithTimeout(
    const std::string& method,
    const base::Value::Dict& params,
    const Timeout* timeout,
    base::Value::Dict* result) {
  base::Value::Dict intermediate_result;
  Status status =
      SendCommandInternal(method, params, session_id_, &intermediate_result,
                          true, true, 0, timeout);
  if (status.IsError())
    return status;
  *result = std::move(intermediate_result);
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandAndIgnoreResponse(
    const std::string& method,
    const base::Value::Dict& params) {
  return SendCommandInternal(method, params, session_id_, nullptr, true, false,
                             0, nullptr);
}

void DevToolsClientImpl::AddListener(DevToolsEventListener* listener) {
  DCHECK(listener);
  DCHECK(!IsConnected() || !listener->ListensToConnections());
  if (IsConnected() && listener->ListensToConnections()) {
    LOG(WARNING)
        << __PRETTY_FUNCTION__
        << " subscribing a listener to the already connected DevToolsClient."
        << " Connection notification will not arrive.";
  }
  listeners_.push_back(listener);
  if (listener->ListensToConnections()) {
    unnotified_connect_listeners_.push_back(listener);
  }
}

void DevToolsClientImpl::RemoveListener(DevToolsEventListener* listener) {
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
  it = std::find(unnotified_connect_listeners_.begin(),
                 unnotified_connect_listeners_.end(), listener);
  if (it != unnotified_connect_listeners_.end()) {
    unnotified_connect_listeners_.erase(it);
  }
  it = std::find(unnotified_event_listeners_.begin(),
                 unnotified_event_listeners_.end(), listener);
  if (it != unnotified_event_listeners_.end()) {
    unnotified_event_listeners_.erase(it);
  }
}

Status DevToolsClientImpl::HandleReceivedEvents() {
  return HandleEventsUntil(base::BindRepeating(&ConditionIsMet),
                           Timeout(base::TimeDelta()));
}

Status DevToolsClientImpl::HandleEventsUntil(
    const ConditionalFunc& conditional_func, const Timeout& timeout) {
  if (!IsConnected()) {
    return Status(kDisconnected, "not connected to DevTools");
  }

  while (true) {
    if (!HasMessageForAnySession()) {
      bool is_condition_met = false;
      Status status = conditional_func.Run(&is_condition_met);
      if (status.IsError())
        return status;
      if (is_condition_met)
        return Status(kOk);
    }

    // Create a small timeout so conditional_func can be retried
    // when only funcinterval has expired, continue while loop
    // but return timeout status if primary timeout has expired
    // This supports cases when loading state is updated by a different client
    Timeout funcinterval = Timeout(base::Milliseconds(500), &timeout);
    Status status = ProcessNextMessage(-1, false, funcinterval, this);
    if (status.code() == kTimeout) {
      if (timeout.IsExpired()) {
        // Build status message based on timeout parameter, not funcinterval
        std::string err =
            "Timed out receiving message from renderer: " +
            base::StringPrintf("%.3lf", timeout.GetDuration().InSecondsF());
        LOG(ERROR) << err;
        return Status(kTimeout, err);
      }
    } else if (status.IsError()) {
      return status;
    }
  }
}

void DevToolsClientImpl::SetDetached() {
  detached_ = true;
}

void DevToolsClientImpl::SetOwner(WebViewImpl* owner) {
  owner_ = owner;
}

WebViewImpl* DevToolsClientImpl::GetOwner() const {
  return owner_;
}

DevToolsClientImpl::ResponseInfo::ResponseInfo(const std::string& method)
    : state(kWaiting), method(method) {}

DevToolsClientImpl::ResponseInfo::~ResponseInfo() = default;

DevToolsClient* DevToolsClientImpl::GetParentClient() const {
  return parent_.get();
}

bool DevToolsClientImpl::IsMainPage() const {
  return is_main_page_;
}

void DevToolsClientImpl::SetMainPage(bool value) {
  DCHECK(!IsConnected());
  is_main_page_ = value;
}

int DevToolsClientImpl::NextMessageId() const {
  if (parent_) {
    return parent_->NextMessageId();
  }
  return next_id_;
}

// Return NextMessageId and immediately increment it
int DevToolsClientImpl::AdvanceNextMessageId() {
  if (parent_) {
    return parent_->AdvanceNextMessageId();
  }
  return next_id_++;
}

Status DevToolsClientImpl::PostBidiCommandInternal(std::string channel,
                                                   base::Value::Dict command) {
  if (tunnel_session_id_.empty()) {
    return Status{
        kUnknownError,
        "uanble to send BiDi commands without BiDi server session id"};
  }
  if (!channel.empty()) {
    command.Set("channel", std::move(channel));
  }

  std::string json;
  Status status = SerializeAsJson(command, &json);
  if (status.IsError()) {
    return status;
  }

  std::string arg;
  status = SerializeAsJson(json, &arg);
  if (status.IsError()) {
    return status;
  }

  std::string expression = "onBidiMessage(" + arg + ")";

  base::Value::Dict params;
  params.Set("expression", expression);

  // Send command and ignore the response
  return SendCommandInternal("Runtime.evaluate", params, tunnel_session_id_,
                             nullptr, true, false, 0, nullptr);
}

Status DevToolsClientImpl::SendRaw(const std::string& message) {
  if (socket_ && socket_->Send(message)) {
    return Status{kOk};
  }
  if (parent_) {
    return parent_->SendRaw(message);
  }
  return Status(kDisconnected, "unable to send message to renderer");
}

bool DevToolsClientImpl::HasMessageForAnySession() const {
  if (socket_) {
    return socket_->HasNextMessage();
  }
  if (parent_) {
    return parent_->HasMessageForAnySession();
  }
  return false;
}

Status DevToolsClientImpl::SendCommandInternal(const std::string& method,
                                               const base::Value::Dict& params,
                                               const std::string& session_id,
                                               base::Value::Dict* result,
                                               bool expect_response,
                                               bool wait_for_response,
                                               const int client_command_id,
                                               const Timeout* timeout) {
  if (parent_ == nullptr && !(socket_ && socket_->IsConnected())) {
    // The browser has crashed or closed the connection, e.g. due to
    // DeveloperToolsAvailability policy change.
    return Status(kDisconnected, "not connected to DevTools");
  }

  // |client_command_id| will be 0 for commands sent by ChromeDriver
  int command_id =
      client_command_id ? client_command_id : AdvanceNextMessageId();
  base::Value::Dict command;
  command.Set("id", command_id);
  command.Set("method", method);
  command.Set("params", params.Clone());
  if (!session_id.empty()) {
    command.Set("sessionId", session_id);
  }

  // if BiDi session id is known
  // and if the command is not already sent within the BiDi session
  if (!tunnel_session_id_.empty() && tunnel_session_id_ != session_id) {
    base::Value::Dict bidi_command;
    Status status =
        WrapCdpCommandInBidiCommand(std::move(command), &bidi_command);
    if (status.IsError()) {
      return status;
    }
    status = WrapBidiCommandInMapperCdpCommand(
        AdvanceNextMessageId(), bidi_command, tunnel_session_id_, &command);
    if (status.IsError()) {
      return status;
    }
  }

  std::string message;
  {
    Status status = SerializeAsJson(command, &message);
    if (status.IsError()) {
      return status;
    }
  }

  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Command: " << method << " (id=" << command_id
            << ")" << ::SessionId(session_id) << " " << id_ << " "
            << FormatValueForDisplay(base::Value(params.Clone()));
  }
  {
    Status status = SendRaw(message);
    if (status.IsError()) {
      return status;
    }
  }

  if (expect_response) {
    scoped_refptr<ResponseInfo> response_info =
        base::MakeRefCounted<ResponseInfo>(method);
    if (timeout)
      response_info->command_timeout = *timeout;
    response_info_map_[command_id] = response_info;

    if (wait_for_response) {
      while (response_info->state == kWaiting) {
        // Use a long default timeout if user has not requested one.
        Status status = ProcessNextMessage(
            command_id, true,
            timeout != nullptr ? *timeout : Timeout(base::Minutes(10)), this);
        if (status.IsError()) {
          if (response_info->state == kReceived)
            response_info_map_.erase(command_id);
          return status;
        }
      }
      if (response_info->state == kBlocked) {
        response_info->state = kIgnored;
        {
          std::string alert_text;
          Status status = GetDialogMessage(alert_text);
          if (status.IsOk())
            return Status(kUnexpectedAlertOpen,
                          "{Alert text : " + alert_text + "}");
        }
        return Status(kUnexpectedAlertOpen);
      }
      CHECK_EQ(response_info->state, kReceived);
      InspectorCommandResponse& response = response_info->response;
      if (!response.result) {
        return internal::ParseInspectorError(response.error);
      }
      *result = std::move(*response.result);
    }
  } else {
    CHECK(!wait_for_response);
  }
  return Status(kOk);
}

Status DevToolsClientImpl::ProcessNextMessage(int expected_id,
                                              bool log_timeout,
                                              const Timeout& timeout,
                                              DevToolsClient* caller) {
  if (!IsConnected()) {
    LOG(WARNING) << "Processing messages while being disconnected";
  }

  Status status = EnsureListenersNotifiedOfConnect();
  if (status.IsError())
    return status;
  status = EnsureListenersNotifiedOfEvent();
  if (status.IsError())
    return status;
  status = EnsureListenersNotifiedOfCommandResponse();
  if (status.IsError())
    return status;

  // The command response may have already been received (in which case it will
  // have been deleted from |response_info_map_|) or blocked while notifying
  // listeners.
  if (expected_id != -1) {
    auto iter = response_info_map_.find(expected_id);
    if (iter == response_info_map_.end() || iter->second->state != kWaiting)
      return Status(kOk);
  }

  if (crashed_)
    return Status(kTabCrashed);

  if (detached_)
    return Status(kTargetDetached);

  if (parent_ != nullptr)
    return parent_->ProcessNextMessage(-1, log_timeout, timeout, caller);

  std::string message;

  switch (socket_->ReceiveNextMessage(&message, timeout)) {
    case SyncWebSocket::StatusCode::kOk:
      break;
    case SyncWebSocket::StatusCode::kDisconnected: {
      std::string err = "Unable to receive message from renderer";
      LOG(ERROR) << err;
      return Status(kDisconnected, err);
    }
    case SyncWebSocket::StatusCode::kTimeout: {
      std::string err =
          "Timed out receiving message from renderer: " +
          base::StringPrintf("%.3lf", timeout.GetDuration().InSecondsF());
      if (log_timeout)
        LOG(ERROR) << err;
      return Status(kTimeout, err);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return HandleMessage(expected_id, message, caller);
}

Status DevToolsClientImpl::HandleMessage(int expected_id,
                                         const std::string& message,
                                         DevToolsClient* caller) {
  std::string session_id;
  internal::InspectorMessageType type;
  InspectorEvent event;
  InspectorCommandResponse response;
  if (!parser_func_.Run(message, expected_id, session_id, type, event,
                        response)) {
    LOG(ERROR) << "Bad inspector message: " << message;
    return Status(kUnknownError, "bad inspector message: " + message);
  }
  DevToolsClient* client = this;
  if (session_id != session_id_) {
    auto it = children_.find(session_id);
    if (it == children_.end()) {
      // ChromeDriver only cares about iframe targets, but uses
      // Target.setAutoAttach in FrameTracker. If we don't know about this
      // sessionId, then it must be of a different target type and should be
      // ignored.
      return Status(kOk);
    }
    client = it->second;
  }
  WebViewImplHolder client_holder(client->GetOwner());
  Status status{kOk};
  if (type == internal::kEventMessageType) {
    status = client->ProcessEvent(std::move(event));
  } else {
    CHECK_EQ(type, internal::kCommandResponseMessageType);
    status = client->ProcessCommandResponse(std::move(response));
  }
  if (caller == client || this == client) {
    // In either case we are in the root.
    // 'this == client' means that the error has happened in the browser
    // session. Any errors happening here are global and most likely will lead
    // to the session termination. Forward them to the caller!
    // 'caller == client' means that the message must be routed to the
    // same client that invoked the current root. Sending the errors
    // to the caller is the proper behavior in this case as well.
    return status;
  } else {
    // We support active event consumption meaning that the whole session
    // makes progress independently from the active WebDriver Classic target.
    // This is needed for timely delivery of bidi events to the user.
    // If something wrong happens in the different target the corresponding
    // WebView must update its state accordingly to notify the user
    // about the issue on the next HTTP request.
    return Status{kOk};
  }
}

Status DevToolsClientImpl::HandleDialogOpening(
    const base::Value::Dict& params) {
  const std::string* message = params.FindString("message");
  if (!message) {
    return Status(kUnknownError, "dialog event missing or invalid 'message'");
  }

  unhandled_dialog_queue_.push_back(*message);

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kUnknownError, "dialog has invalid 'type'");
  }

  dialog_type_queue_.push_back(*type);

  const std::string* prompt_text = params.FindString("defaultPrompt");
  if (!prompt_text) {
    return Status(kUnknownError,
                  "dialog event missing or invalid 'defaultPrompt'");
  }
  prompt_text_ = *prompt_text;

  if (*type == "beforeunload" && AutoAcceptsBeforeunload()) {
    return HandleDialog(true, std::nullopt);
  }
  return Status{kOk};
}

Status DevToolsClientImpl::HandleDialogClosed(const base::Value::Dict& params) {
  // Inspector only sends this event when all dialogs have been closed.
  // Clear the unhandled queue in case the user closed a dialog manually.
  unhandled_dialog_queue_.clear();
  dialog_type_queue_.clear();
  return Status{kOk};
}

Status DevToolsClientImpl::ProcessEvent(InspectorEvent event) {
  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Event: " << event.method
            << ::SessionId(session_id_) << " " << id_ << " "
            << FormatValueForDisplay(base::Value(event.params->Clone()));
  }

  Status status{kOk};

  // The default parser ensures that event.params is never nullptr.
  // The unit tests however can set different parsers that not necessarily
  // provide such a guarantee.
  // Therefore we perform this nullptr check here.
  if (event.params) {
    if (event.method == "Page.javascriptDialogOpening") {
      status = HandleDialogOpening(*event.params);
    } else if (event.method == "Page.javascriptDialogClosed") {
      status = HandleDialogClosed(*event.params);
    }
    if (status.IsError()) {
      return status;
    }
  }

  unnotified_event_listeners_ = listeners_;
  unnotified_event_ = &event;
  status = EnsureListenersNotifiedOfEvent();
  unnotified_event_ = nullptr;
  if (status.IsError())
    return status;
  if (event.method == "Inspector.detached")
    return Status(kTargetDetached, "received Inspector.detached event");
  if (event.method == "Inspector.targetCrashed") {
    crashed_ = true;
    return Status(kTabCrashed);
  }
  if (IsDialogOpen()) {
    // A command may have opened the dialog, which will block the response.
    // To find out which one (if any), do a round trip with a simple command
    // to the renderer and afterwards see if any of the commands still haven't
    // received a response.
    // This relies on the fact that DevTools commands are processed
    // sequentially. This may break if any of the commands are asynchronous.
    // If for some reason the round trip command fails, mark all the waiting
    // commands as blocked and return the error. This is better than risking
    // a hang.
    int max_id = NextMessageId();
    base::Value::Dict enable_params;
    enable_params.Set("purpose", "detect if alert blocked any cmds");
    Status enable_status = SendCommand("Inspector.enable", enable_params);
    for (const auto& [cmd_id, response] : response_info_map_) {
      if (cmd_id > max_id) {
        continue;
      }
      if (response->state == kWaiting) {
        response->state = kBlocked;
      }
    }
    if (enable_status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::ProcessCommandResponse(
    InspectorCommandResponse response) {
  auto iter = response_info_map_.find(response.id);
  if (IsVLogOn(1)) {
    std::string method, result;
    if (iter != response_info_map_.end())
      method = iter->second->method;
    if (response.result)
      result = FormatValueForDisplay(base::Value(response.result->Clone()));
    else
      result = response.error;
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Response: " << method
            << " (id=" << response.id << ")" << ::SessionId(session_id_) << " "
            << id_ << " " << result;
  }

  if (iter == response_info_map_.end()) {
    // A CDP session may become detached while a command sent to that session
    // is still pending. When the browser eventually tries to process this
    // command, it sends a response with an error and no session ID. Since
    // there is no session ID, this message will be routed here to the root
    // DevToolsClientImpl. If we receive such a response, just ignore it
    // since the session it belongs to is already detached.
    if (parent_ == nullptr) {
      if (!response.result) {
        const Status status = internal::ParseInspectorError(response.error);
        if (status.code() == StatusCode::kNoSuchFrame) {
          return Status(kOk);
        }
      }
    }
    return Status(kUnknownError, "unexpected command response");
  }

  scoped_refptr<ResponseInfo> response_info = response_info_map_[response.id];
  response_info_map_.erase(response.id);

  if (response_info->state != kIgnored) {
    response_info->state = kReceived;
    response_info->response.id = response.id;
    response_info->response.error = response.error;
    if (response.result) {
      response_info->response.result = response.result->Clone();
    }
  }

  if (response.result) {
    unnotified_cmd_response_listeners_ = listeners_;
    unnotified_cmd_response_info_ = response_info;
    Status status = EnsureListenersNotifiedOfCommandResponse();
    unnotified_cmd_response_info_.reset();
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::EnsureListenersNotifiedOfConnect() {
  while (!unnotified_connect_listeners_.empty()) {
    DevToolsEventListener* listener = unnotified_connect_listeners_.front();
    unnotified_connect_listeners_.pop_front();
    Status status = listener->OnConnected(this);
    if (status.IsError())
      return status;
  }

  return Status(kOk);
}

Status DevToolsClientImpl::EnsureListenersNotifiedOfEvent() {
  while (unnotified_event_listeners_.size()) {
    DevToolsEventListener* listener = unnotified_event_listeners_.front();
    unnotified_event_listeners_.pop_front();
    const base::Value::Dict& dict = *unnotified_event_->params;
    Status status = listener->OnEvent(this, unnotified_event_->method, dict);
    if (status.IsError()) {
      unnotified_event_listeners_.clear();
      return status;
    }
  }
  return Status(kOk);
}

Status DevToolsClientImpl::EnsureListenersNotifiedOfCommandResponse() {
  while (unnotified_cmd_response_listeners_.size()) {
    DevToolsEventListener* listener =
        unnotified_cmd_response_listeners_.front();
    unnotified_cmd_response_listeners_.pop_front();
    Status status = listener->OnCommandSuccess(
        this, unnotified_cmd_response_info_->method,
        base::OptionalToPtr(unnotified_cmd_response_info_->response.result),
        unnotified_cmd_response_info_->command_timeout);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

void DevToolsClientImpl::EnableEventTunnelingForTesting() {
  event_tunneling_is_enabled_ = true;
}

bool DevToolsClientImpl::IsDialogOpen() const {
  return !unhandled_dialog_queue_.empty();
}

bool DevToolsClientImpl::AutoAcceptsBeforeunload() const {
  return autoaccept_beforeunload_;
}

void DevToolsClientImpl::SetAutoAcceptBeforeunload(bool value) {
  autoaccept_beforeunload_ = value;
}

Status DevToolsClientImpl::GetDialogMessage(std::string& message) const {
  if (!IsDialogOpen()) {
    return Status(kNoSuchAlert);
  }

  message = unhandled_dialog_queue_.front();
  return Status(kOk);
}

Status DevToolsClientImpl::GetTypeOfDialog(std::string& type) const {
  if (!IsDialogOpen()) {
    return Status(kNoSuchAlert);
  }

  type = dialog_type_queue_.front();
  return Status(kOk);
}

Status DevToolsClientImpl::HandleDialog(
    bool accept,
    const std::optional<std::string>& text) {
  if (!IsDialogOpen()) {
    return Status(kNoSuchAlert);
  }

  base::Value::Dict params;
  params.Set("accept", accept);
  if (text) {
    params.Set("promptText", *text);
  } else {
    params.Set("promptText", prompt_text_);
  }
  Status status = SendCommand("Page.handleJavaScriptDialog", params);
  if (status.IsError()) {
    // Retry once to work around
    // https://bugs.chromium.org/p/chromedriver/issues/detail?id=1500
    status = SendCommand("Page.handleJavaScriptDialog", params);
    if (status.IsError()) {
      return status;
    }
  }
  // Remove a dialog from the queue. Need to check the queue is not empty here,
  // because it could have been cleared during waiting for the command
  // response.
  if (unhandled_dialog_queue_.size()) {
    unhandled_dialog_queue_.pop_front();
  }

  if (dialog_type_queue_.size()) {
    dialog_type_queue_.pop_front();
  }

  return Status(kOk);
}

namespace internal {

bool ParseInspectorMessage(const std::string& message,
                           int expected_id,
                           std::string& session_id,
                           InspectorMessageType& type,
                           InspectorEvent& event,
                           InspectorCommandResponse& command_response) {
  // We want to allow invalid characters in case they are valid ECMAScript
  // strings. For example, webplatform tests use this to check string handling
  std::optional<base::Value> message_value =
      base::JSONReader::Read(message, base::JSON_REPLACE_INVALID_CHARACTERS);
  base::Value::Dict* message_dict =
      message_value ? message_value->GetIfDict() : nullptr;
  if (!message_dict)
    return false;
  session_id.clear();
  if (const std::string* str = message_dict->FindString("sessionId"))
    session_id = *str;

  base::Value* id_value = message_dict->Find("id");
  if (!id_value) {
    const std::string* method = message_dict->FindString("method");
    if (!method)
      return false;
    bool is_bidi_message = false;
    base::Value::Dict* params = message_dict->FindDict("params");
    if (params) {
      Status status = IsBidiMessage(*method, *params, is_bidi_message);
      if (status.IsError()) {
        LOG(WARNING) << status.message();
        return false;
      }
    }

    if (is_bidi_message) {
      base::Value::Dict payload;
      Status status = DeserializePayload(*params, &payload);
      if (status.IsError()) {
        LOG(WARNING) << status.message();
        return false;
      }

      std::string* channel = payload.FindString("channel");

      if (channel && *channel == DevToolsClientImpl::kCdpTunnelChannel) {
        return ParseCdpTunnelMessage(std::move(payload), session_id, type,
                                     event, command_response);
      }  // Infra CDP tunnel

      if (channel &&
          base::EndsWith(*channel, DevToolsClientImpl::kBidiChannelSuffix)) {
        size_t pos = channel->size() -
                     std::strlen(DevToolsClientImpl::kBidiChannelSuffix);
        // Update the channel value of the payload in-place.
        channel->erase(std::next(channel->begin(), pos), channel->end());
      }

      // Replace the payload string with the deserialized value to avoid
      // double deserialization in the BidiTracker.
      params->Set("payload", std::move(payload));
    }  // BiDi message

    type = kEventMessageType;
    event.method = *method;
    if (params) {
      event.params = params->Clone();
    } else {
      event.params = base::Value::Dict();
    }
    return true;
  } else if (id_value->is_int()) {
    type = kCommandResponseMessageType;
    command_response.id = id_value->GetInt();
    // As per Chromium issue 392577, DevTools does not necessarily return a
    // "result" dictionary for every valid response. In particular,
    // Tracing.start and Tracing.end command responses do not contain one.
    // So, if neither "error" nor "result" keys are present, just provide
    // a blank result dictionary.
    if (base::Value::Dict* unscoped_result = message_dict->FindDict("result")) {
      command_response.result = std::move(*unscoped_result);
    } else if (base::Value::Dict* unscoped_error =
                   message_dict->FindDict("error")) {
      base::JSONWriter::Write(*unscoped_error, &command_response.error);
    } else {
      command_response.result = base::Value::Dict();
    }
    return true;
  }
  return false;
}

Status ParseInspectorError(const std::string& error_json) {
  std::optional<base::Value> error = base::JSONReader::Read(error_json);
  base::Value::Dict* error_dict = error ? error->GetIfDict() : nullptr;
  if (!error_dict)
    return Status(kUnknownError, "inspector error with no error message");

  std::optional<int> maybe_code = error_dict->FindInt("code");
  std::string* maybe_message = error_dict->FindString("message");

  if (maybe_code.has_value()) {
    if (maybe_code.value() == kCdpMethodNotFoundCode) {
      return Status(kUnknownCommand,
                    maybe_message ? *maybe_message : "UnknownCommand");
    } else if (maybe_code.value() == kSessionNotFoundInspectorCode) {
      return Status(kNoSuchFrame,
                    maybe_message ? *maybe_message : "inspector detached");
    }
  }

  if (maybe_message) {
    std::string error_message = *maybe_message;
    if (error_message == kInspectorDefaultContextError ||
        error_message == kInspectorContextError ||
        error_message == kUniqueContextIdNotFoundError) {
      // The error messages that can arise during a call to
      // Runtime.evaluate and Runtime.callFunctionOn if the provided
      // context does no longer exist.
      return Status(kNoSuchExecutionContext);
    } else if (error_message == kInspectorInvalidURL) {
      return Status(kInvalidArgument);
    } else if (error_message == kInspectorInsecureContext) {
      return Status(kInvalidArgument,
                    "feature cannot be used in insecure context");
    } else if (error_message == kInspectorPushPermissionError ||
               error_message == kInspectorOpaqueOrigins) {
      return Status(kInvalidArgument, error_message);
    } else if (error_message == kInspectorNoSuchFrameError) {
      // As the server returns the generic error code: SERVER_ERROR = -32000
      // we have to rely on the error message content.
      return Status(kNoSuchFrame, error_message);
    } else if (error_message == kNoNodeForBackendNodeIdError ||
               error_message == kNoNodeWithGivenIdFoundError) {
      // The error message that arises during DOM.resolveNode code.
      // This means that the node with given BackendNodeId is not found.
      return Status{kNoSuchElement, error_message};
    } else if (error_message == kExecutionContextWasDestroyed ||
               error_message == kInspectedTargetNavigatedOrClosed) {
      // The error messages that arise if navigation was started by the
      // asynchronous script before the script execution was finished..
      return Status{kNavigationDetectedByRemoteEnd, error_message};
    }
    std::optional<int> error_code = error_dict->FindInt("code");
    if (error_code == kInvalidParamsInspectorCode) {
      if (error_message == kNoTargetWithGivenIdError) {
        return Status(kNoSuchWindow, error_message);
      }
      return Status(kInvalidArgument, error_message);
    }
  }
  return Status(kUnknownError, "unhandled inspector error: " + error_json);
}

}  // namespace internal
