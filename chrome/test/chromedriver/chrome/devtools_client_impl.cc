// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/i18n/message_formatter.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/util.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/command_id.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

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
  raw_ptr<int> count_;
};

Status ConditionIsMet(bool* is_condition_met) {
  *is_condition_met = true;
  return Status(kOk);
}

Status FakeCloseFrontends() {
  return Status(kOk);
}

struct SessionId {
  explicit SessionId(const std::string session_id) : session_id_(session_id) {}
  std::string session_id_;
};

std::ostream& operator<<(std::ostream& os, const SessionId& ses_manip) {
  return os << " (session_id=" << ses_manip.session_id_ << ")";
}

}  // namespace

namespace internal {

InspectorEvent::InspectorEvent() {}

InspectorEvent::~InspectorEvent() {}

InspectorCommandResponse::InspectorCommandResponse() {}

InspectorCommandResponse::~InspectorCommandResponse() {}

}  // namespace internal

const char DevToolsClientImpl::kBrowserwideDevToolsClientId[] = "browser";

DevToolsClientImpl::DevToolsClientImpl(const std::string& id,
                                       const std::string& session_id,
                                       const std::string& url,
                                       const SyncWebSocketFactory& factory)
    : socket_(factory.Run()),
      url_(url),
      owner_(nullptr),
      session_id_(session_id),
      parent_(nullptr),
      crashed_(false),
      detached_(false),
      id_(id),
      frontend_closer_func_(base::BindRepeating(&FakeCloseFrontends)),
      parser_func_(base::BindRepeating(&internal::ParseInspectorMessage)),
      unnotified_event_(nullptr),
      next_id_(1),
      stack_count_(0),
      is_remote_end_configured_(false),
      is_main_page_(false) {
  socket_->SetId(id_);
  // If error happens during proactive event consumption we ignore it
  // as there is no active user request where the error might be returned.
  // Unretained 'this' won't cause any problems as we reset the callback in the
  // .dtor.
  socket_->SetNotificationCallback(base::BindRepeating(
      base::IgnoreResult(&DevToolsClientImpl::HandleReceivedEvents),
      base::Unretained(this)));
}

DevToolsClientImpl::DevToolsClientImpl(const std::string& id,
                                       const std::string& session_id)
    : owner_(nullptr),
      session_id_(session_id),
      parent_(nullptr),
      crashed_(false),
      detached_(false),
      id_(id),
      frontend_closer_func_(base::BindRepeating(&FakeCloseFrontends)),
      parser_func_(base::BindRepeating(&internal::ParseInspectorMessage)),
      unnotified_event_(nullptr),
      next_id_(1),
      stack_count_(0),
      is_remote_end_configured_(false),
      is_main_page_(false) {}

DevToolsClientImpl::~DevToolsClientImpl() {
  if (parent_ != nullptr) {
    parent_->children_.erase(session_id_);
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

void DevToolsClientImpl::SetFrontendCloserFunc(
    const FrontendCloserFunc& frontend_closer_func) {
  frontend_closer_func_ = frontend_closer_func;
}

const std::string& DevToolsClientImpl::GetId() {
  return id_;
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

Status DevToolsClientImpl::AttachTo(DevToolsClientImpl* parent) {
  // checking the preconditions
  DCHECK(parent != nullptr);
  DCHECK(IsNull());
  DCHECK(parent->GetParentClient() == nullptr);

  if (!IsNull()) {
    return Status{
        kUnknownError,
        "Attaching non-null DevToolsClient to a new parent is prohibited"};
  }

  // Class invariant: the hierarchy is flat
  if (parent->GetParentClient() != nullptr) {
    return Status{kUnknownError,
                  "DevToolsClientImpl can be attached only to the root client"};
  }

  if (parent->IsConnected()) {
    ResetListeners();
    parent_ = parent;
    parent_->children_[session_id_] = this;
    Status status = OnConnected();
    if (status.IsError()) {
      return status;
    }
  } else {
    parent_ = parent;
    parent_->children_[session_id_] = this;
  }

  return Status{kOk};
}

Status DevToolsClientImpl::ConnectIfNecessary() {
  if (stack_count_)
    return Status(kUnknownError, "cannot connect when nested");

  if (parent_ == nullptr) {
    // This is the browser level DevToolsClient
    if (socket_->IsConnected())
      return Status(kOk);

    ResetListeners();

    if (!socket_->Connect(url_)) {
      // Try to close devtools frontend and then reconnect.
      Status status = frontend_closer_func_.Run();
      if (status.IsError())
        return status;
      if (!socket_->Connect(url_))
        return Status(kDisconnected, "unable to connect to renderer");
    }

    return OnConnected();

  } else {
    // This is a page or frame level DevToolsClient
    return parent_->ConnectIfNecessary();
  }
}

void DevToolsClientImpl::ResetListeners() {
  // checking the preconditions
  DCHECK(!IsConnected());

  // We are going to reconnect, therefore the remote end must be reconfigured
  is_remote_end_configured_ = false;

  // These lines must be before the SendCommandXxx calls in SetUpDevTools
  unnotified_connect_listeners_.clear();
  for (DevToolsEventListener* listener : listeners_) {
    if (listener->ListensToConnections()) {
      unnotified_connect_listeners_.push_back(listener);
    }
  }
  unnotified_event_listeners_.clear();
  response_info_map_.clear();

  for (auto child : children_) {
    child.second->ResetListeners();
  }
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

  for (auto child : children_) {
    status = child.second->OnConnected();
    if (status.IsError()) {
      break;
    }
  }

  return status;
}

Status DevToolsClientImpl::SetUpDevTools() {
  if (is_remote_end_configured_) {
    return Status{kOk};
  }

  if (id_ != kBrowserwideDevToolsClientId &&
      (GetOwner() == nullptr || !GetOwner()->IsServiceWorker())) {
    // This is a page or frame level DevToolsClient
    base::DictionaryValue params;
    std::string script =
        "(function () {"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Array = window.Array;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Promise = window.Promise;"
        "window.cdc_adoQpoasnfa76pfcZLmcfl_Symbol = window.Symbol;"
        "}) ();";
    params.SetString("source", script);
    Status status = SendCommandAndIgnoreResponse(
        "Page.addScriptToEvaluateOnNewDocument", params);
    if (status.IsError())
      return status;

    params.DictClear();
    params.SetString("expression", script);
    status = SendCommandAndIgnoreResponse("Runtime.evaluate", params);
    if (status.IsError())
      return status;
  }

  is_remote_end_configured_ = true;
  return Status{kOk};
}

Status DevToolsClientImpl::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  return SendCommandWithTimeout(method, params, nullptr);
}

Status DevToolsClientImpl::SendCommandFromWebSocket(
    const std::string& method,
    const base::DictionaryValue& params,
    int client_command_id) {
  return SendCommandInternal(method, params, nullptr, false, false,
                             client_command_id, nullptr);
}

Status DevToolsClientImpl::SendCommandWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout) {
  base::Value result;
  return SendCommandInternal(method, params, &result, true, true, 0, timeout);
}

Status DevToolsClientImpl::SendAsyncCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  base::Value result;
  return SendCommandInternal(method, params, &result, false, false, 0, nullptr);
}

Status DevToolsClientImpl::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    base::Value* result) {
  return SendCommandAndGetResultWithTimeout(method, params, nullptr, result);
}

Status DevToolsClientImpl::SendCommandAndGetResultWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout,
    base::Value* result) {
  base::Value intermediate_result;
  Status status = SendCommandInternal(method, params, &intermediate_result,
                                      true, true, 0, timeout);
  if (status.IsError())
    return status;
  if (!intermediate_result.is_dict())
    return Status(kUnknownError, "inspector response missing result");
  *result = std::move(intermediate_result);
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandAndIgnoreResponse(
    const std::string& method,
    const base::DictionaryValue& params) {
  return SendCommandInternal(method, params, nullptr, true, false, 0, nullptr);
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
}

Status DevToolsClientImpl::HandleReceivedEvents() {
  return HandleEventsUntil(base::BindRepeating(&ConditionIsMet),
                           Timeout(base::TimeDelta()));
}

Status DevToolsClientImpl::HandleEventsUntil(
    const ConditionalFunc& conditional_func, const Timeout& timeout) {
  SyncWebSocket* socket =
      static_cast<DevToolsClientImpl*>(GetRootClient())->socket_.get();
  DCHECK(socket);
  if (!socket->IsConnected()) {
    return Status(kDisconnected, "not connected to DevTools");
  }

  while (true) {
    if (!socket->HasNextMessage()) {
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

DevToolsClientImpl::ResponseInfo::~ResponseInfo() {}

DevToolsClient* DevToolsClientImpl::GetRootClient() {
  return parent_ ? parent_->GetRootClient() : this;
}

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
  return next_id_;
}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    base::Value* result,
    bool expect_response,
    bool wait_for_response,
    const int client_command_id,
    const Timeout* timeout) {
  DCHECK(IsConnected());
  if (parent_ == nullptr && !socket_->IsConnected())
    return Status(kDisconnected, "not connected to DevTools");

  // |client_command_id| will be 0 for commands sent by ChromeDriver
  int command_id = client_command_id ? client_command_id : next_id_++;
  base::DictionaryValue command;
  command.SetInteger("id", command_id);
  command.SetString("method", method);
  command.SetKey("params", params.Clone());
  if (!session_id_.empty()) {
    command.SetString("sessionId", session_id_);
  }
  std::string message = SerializeValue(&command);

  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Command: " << method << " (id=" << command_id
            << ")" << SessionId(session_id_) << " " << id_ << " "
            << FormatValueForDisplay(params);
  }
  SyncWebSocket* socket =
      static_cast<DevToolsClientImpl*>(GetRootClient())->socket_.get();
  if (!socket->Send(message)) {
    return Status(kDisconnected, "unable to send message to renderer");
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
        if (owner_) {
          std::string alert_text;
          Status status =
              owner_->GetJavaScriptDialogManager()->GetDialogMessage(
                  &alert_text);
          if (status.IsOk())
            return Status(kUnexpectedAlertOpen,
                          "{Alert text : " + alert_text + "}");
        }
        return Status(kUnexpectedAlertOpen);
      }
      CHECK_EQ(response_info->state, kReceived);
      internal::InspectorCommandResponse& response = response_info->response;
      if (!response.result) {
        return internal::ParseInspectorError(response.error);
      }
      *result = std::move(*response.result);
    }
  } else {
    CHECK(!wait_for_response);
    if (result)
      *result = base::Value(base::Value::Type::DICTIONARY);
  }
  return Status(kOk);
}

Status DevToolsClientImpl::ProcessNextMessage(int expected_id,
                                              bool log_timeout,
                                              const Timeout& timeout,
                                              DevToolsClientImpl* caller) {
  ScopedIncrementer increment_stack_count(&stack_count_);
  DCHECK(IsConnected());

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
      NOTREACHED();
      break;
  }

  return HandleMessage(expected_id, message, caller);
}

Status DevToolsClientImpl::HandleMessage(int expected_id,
                                         const std::string& message,
                                         DevToolsClientImpl* caller) {
  std::string session_id;
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  if (!parser_func_.Run(message, expected_id, &session_id, &type, &event,
                        &response)) {
    LOG(ERROR) << "Bad inspector message: " << message;
    return Status(kUnknownError, "bad inspector message: " + message);
  }
  DevToolsClientImpl* client = this;
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
  WebViewImplHolder client_holder(client->owner_);
  if (type == internal::kEventMessageType) {
    Status status = client->ProcessEvent(event);
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
  CHECK_EQ(type, internal::kCommandResponseMessageType);
  Status status = client->ProcessCommandResponse(response);
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

Status DevToolsClientImpl::ProcessEvent(const internal::InspectorEvent& event) {
  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Event: " << event.method
            << SessionId(session_id_) << " " << id_ << " "
            << FormatValueForDisplay(*event.params);
  }
  unnotified_event_listeners_ = listeners_;
  unnotified_event_ = &event;
  Status status = EnsureListenersNotifiedOfEvent();
  unnotified_event_ = nullptr;
  if (status.IsError())
    return status;
  if (event.method == "Inspector.detached")
    return Status(kDisconnected, "received Inspector.detached event");
  if (event.method == "Inspector.targetCrashed") {
    crashed_ = true;
    return Status(kTabCrashed);
  }
  if (event.method == "Page.javascriptDialogOpening") {
    // A command may have opened the dialog, which will block the response.
    // To find out which one (if any), do a round trip with a simple command
    // to the renderer and afterwards see if any of the commands still haven't
    // received a response.
    // This relies on the fact that DevTools commands are processed
    // sequentially. This may break if any of the commands are asynchronous.
    // If for some reason the round trip command fails, mark all the waiting
    // commands as blocked and return the error. This is better than risking
    // a hang.
    int max_id = next_id_;
    base::DictionaryValue enable_params;
    enable_params.SetString("purpose", "detect if alert blocked any cmds");
    Status enable_status = SendCommand("Inspector.enable", enable_params);
    for (auto iter = response_info_map_.begin();
         iter != response_info_map_.end(); ++iter) {
      if (iter->first > max_id)
        continue;
      if (iter->second->state == kWaiting)
        iter->second->state = kBlocked;
    }
    if (enable_status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::ProcessCommandResponse(
    const internal::InspectorCommandResponse& response) {
  auto iter = response_info_map_.find(response.id);
  if (IsVLogOn(1)) {
    std::string method, result;
    if (iter != response_info_map_.end())
      method = iter->second->method;
    if (response.result)
      result = FormatValueForDisplay(*response.result);
    else
      result = response.error;
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Response: " << method
            << " (id=" << response.id << ")" << SessionId(session_id_) << " "
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
      response_info->response.result = base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(response.result->Clone()));
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
  while (unnotified_connect_listeners_.size()) {
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
    Status status = listener->OnEvent(
        this, unnotified_event_->method, *unnotified_event_->params);
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
        unnotified_cmd_response_info_->response.result.get(),
        unnotified_cmd_response_info_->command_timeout);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

namespace internal {

bool ParseInspectorMessage(const std::string& message,
                           int expected_id,
                           std::string* session_id,
                           InspectorMessageType* type,
                           InspectorEvent* event,
                           InspectorCommandResponse* command_response) {
  // We want to allow invalid characters in case they are valid ECMAScript
  // strings. For example, webplatform tests use this to check string handling
  std::unique_ptr<base::Value> message_value = base::JSONReader::ReadDeprecated(
      message, base::JSON_REPLACE_INVALID_CHARACTERS);
  base::DictionaryValue* message_dict;
  if (!message_value || !message_value->GetAsDictionary(&message_dict))
    return false;
  session_id->clear();
  if (const std::string* str = message_dict->FindStringKey("sessionId"))
    *session_id = *str;

  base::Value* id_value = message_dict->FindKey("id");
  if (!id_value) {
    std::string method;
    if (!message_dict->GetString("method", &method))
      return false;
    base::DictionaryValue* params = nullptr;
    message_dict->GetDictionary("params", &params);

    *type = kEventMessageType;
    event->method = method;
    if (params) {
      event->params = base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(params->Clone()));
    } else {
      event->params = std::make_unique<base::DictionaryValue>();
    }
    return true;
  } else if (id_value->is_int()) {
    base::DictionaryValue* unscoped_error = nullptr;
    base::DictionaryValue* unscoped_result = nullptr;
    *type = kCommandResponseMessageType;
    command_response->id = id_value->GetInt();
    // As per Chromium issue 392577, DevTools does not necessarily return a
    // "result" dictionary for every valid response. In particular,
    // Tracing.start and Tracing.end command responses do not contain one.
    // So, if neither "error" nor "result" keys are present, just provide
    // a blank result dictionary.
    if (message_dict->GetDictionary("result", &unscoped_result)) {
      command_response->result = base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(unscoped_result->Clone()));
    } else if (message_dict->GetDictionary("error", &unscoped_error)) {
      base::JSONWriter::Write(*unscoped_error, &command_response->error);
    } else {
      command_response->result = std::make_unique<base::DictionaryValue>();
    }
    return true;
  }
  return false;
}

Status ParseInspectorError(const std::string& error_json) {
  std::unique_ptr<base::Value> error =
      base::JSONReader::ReadDeprecated(error_json);
  base::DictionaryValue* error_dict;
  if (!error || !error->GetAsDictionary(&error_dict))
    return Status(kUnknownError, "inspector error with no error message");

  absl::optional<int> maybe_code = error_dict->FindIntKey("code");
  std::string* maybe_message = error_dict->FindStringKey("message");

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
        error_message == kInspectorContextError) {
      return Status(kNoSuchWindow);
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
    }
    absl::optional<int> error_code = error_dict->FindIntPath("code");
    if (error_code == kInvalidParamsInspectorCode)
      return Status(kInvalidArgument, error_message);
  }
  return Status(kUnknownError, "unhandled inspector error: " + error_json);
}

}  // namespace internal
