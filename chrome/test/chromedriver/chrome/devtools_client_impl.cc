// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/util.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

namespace {

const char kInspectorDefaultContextError[] =
    "Cannot find default execution context";
const char kInspectorContextError[] =
    "Cannot find execution context with given id";
// Builds older than commit position 353387 return a different error message.
// TODO(samuong): Remove this once we stop supporting Chrome 47.
const char kOldInspectorContextError[] =
    "Execution context with given id not found.";

Status ParseInspectorError(const std::string& error_json) {
  std::unique_ptr<base::Value> error = base::JSONReader::Read(error_json);
  base::DictionaryValue* error_dict;
  if (!error || !error->GetAsDictionary(&error_dict))
    return Status(kUnknownError, "inspector error with no error message");
  std::string error_message;
  if (error_dict->GetString("message", &error_message) &&
      (error_message == kInspectorDefaultContextError ||
       error_message == kInspectorContextError ||
       error_message == kOldInspectorContextError)) {
    return Status(kNoSuchExecutionContext);
  }
  return Status(kUnknownError, "unhandled inspector error: " + error_json);
}

class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(int* count) : count_(count) {
    (*count_)++;
  }
  ~ScopedIncrementer() {
    (*count_)--;
  }

 private:
  int* count_;
};

Status ConditionIsMet(bool* is_condition_met) {
  *is_condition_met = true;
  return Status(kOk);
}

Status FakeCloseFrontends() {
  return Status(kOk);
}

}  // namespace

namespace internal {

InspectorEvent::InspectorEvent() {}

InspectorEvent::~InspectorEvent() {}

InspectorCommandResponse::InspectorCommandResponse() {}

InspectorCommandResponse::~InspectorCommandResponse() {}

}  // namespace internal

const char DevToolsClientImpl::kBrowserwideDevToolsClientId[] = "browser";

DevToolsClientImpl::DevToolsClientImpl(const SyncWebSocketFactory& factory,
                                       const std::string& url,
                                       const std::string& id)
    : socket_(factory.Run()),
      url_(url),
      parent_(nullptr),
      owner_(nullptr),
      crashed_(false),
      detached_(false),
      id_(id),
      frontend_closer_func_(base::Bind(&FakeCloseFrontends)),
      parser_func_(base::Bind(&internal::ParseInspectorMessage)),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {
  socket_->SetId(id_);
}

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url,
    const std::string& id,
    const FrontendCloserFunc& frontend_closer_func)
    : socket_(factory.Run()),
      url_(url),
      parent_(nullptr),
      owner_(nullptr),
      crashed_(false),
      detached_(false),
      id_(id),
      frontend_closer_func_(frontend_closer_func),
      parser_func_(base::Bind(&internal::ParseInspectorMessage)),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {
  socket_->SetId(id_);
}

DevToolsClientImpl::DevToolsClientImpl(DevToolsClientImpl* parent,
                                       const std::string& session_id)
    : parent_(parent),
      owner_(nullptr),
      session_id_(session_id),
      crashed_(false),
      detached_(false),
      id_(session_id),
      frontend_closer_func_(base::BindRepeating(&FakeCloseFrontends)),
      parser_func_(base::BindRepeating(&internal::ParseInspectorMessage)),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {
  parent->children_[session_id] = this;
}

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url,
    const std::string& id,
    const FrontendCloserFunc& frontend_closer_func,
    const ParserFunc& parser_func)
    : socket_(factory.Run()),
      url_(url),
      parent_(nullptr),
      owner_(nullptr),
      crashed_(false),
      detached_(false),
      id_(id),
      frontend_closer_func_(frontend_closer_func),
      parser_func_(parser_func),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {
  socket_->SetId(id_);
}

DevToolsClientImpl::~DevToolsClientImpl() {
  if (parent_ != nullptr)
    parent_->children_.erase(session_id_);
}

void DevToolsClientImpl::SetParserFuncForTesting(
    const ParserFunc& parser_func) {
  parser_func_ = parser_func;
}

const std::string& DevToolsClientImpl::GetId() {
  return id_;
}

bool DevToolsClientImpl::WasCrashed() {
  return crashed_;
}

Status DevToolsClientImpl::ConnectIfNecessary() {
  if (stack_count_)
    return Status(kUnknownError, "cannot connect when nested");

  if (parent_ == nullptr) {
    if (socket_->IsConnected())
      return Status(kOk);

    if (!socket_->Connect(url_)) {
      // Try to close devtools frontend and then reconnect.
      Status status = frontend_closer_func_.Run();
      if (status.IsError())
        return status;
      if (!socket_->Connect(url_))
        return Status(kDisconnected, "unable to connect to renderer");
    }
  }

  unnotified_connect_listeners_ = listeners_;
  unnotified_event_listeners_.clear();
  response_info_map_.clear();

  // Notify all listeners of the new connection. Do this now so that any errors
  // that occur are reported now instead of later during some unrelated call.
  // Also gives listeners a chance to send commands before other clients.
  return EnsureListenersNotifiedOfConnect();
}

Status DevToolsClientImpl::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  return SendCommandWithTimeout(method, params, nullptr);
}

Status DevToolsClientImpl::SendCommandWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout) {
  std::unique_ptr<base::DictionaryValue> result;
  return SendCommandInternal(method, params, &result, true, true, timeout);
}

Status DevToolsClientImpl::SendAsyncCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  std::unique_ptr<base::DictionaryValue> result;
  return SendCommandInternal(method, params, &result, false, false, nullptr);
}

Status DevToolsClientImpl::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    std::unique_ptr<base::DictionaryValue>* result) {
  return SendCommandAndGetResultWithTimeout(method, params, nullptr, result);
}

Status DevToolsClientImpl::SendCommandAndGetResultWithTimeout(
    const std::string& method,
    const base::DictionaryValue& params,
    const Timeout* timeout,
    std::unique_ptr<base::DictionaryValue>* result) {
  std::unique_ptr<base::DictionaryValue> intermediate_result;
  Status status = SendCommandInternal(
      method, params, &intermediate_result, true, true, timeout);
  if (status.IsError())
    return status;
  if (!intermediate_result)
    return Status(kUnknownError, "inspector response missing result");
  *result = std::move(intermediate_result);
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandAndIgnoreResponse(
    const std::string& method,
    const base::DictionaryValue& params) {
  return SendCommandInternal(method, params, nullptr, true, false, nullptr);
}

void DevToolsClientImpl::AddListener(DevToolsEventListener* listener) {
  CHECK(listener);
  listeners_.push_back(listener);
}

Status DevToolsClientImpl::HandleReceivedEvents() {
  return HandleEventsUntil(base::Bind(&ConditionIsMet),
                           Timeout(base::TimeDelta()));
}

Status DevToolsClientImpl::HandleEventsUntil(
    const ConditionalFunc& conditional_func, const Timeout& timeout) {
  if (!socket_->IsConnected())
    return Status(kDisconnected, "not connected to DevTools");

  while (true) {
    if (!socket_->HasNextMessage()) {
      bool is_condition_met = false;
      Status status = conditional_func.Run(&is_condition_met);
      if (status.IsError())
        return status;
      if (is_condition_met)
        return Status(kOk);
    }

    Status status = ProcessNextMessage(-1, timeout);
    if (status.IsError())
      return status;
  }
}

void DevToolsClientImpl::SetDetached() {
  detached_ = true;
}

void DevToolsClientImpl::SetOwner(WebViewImpl* owner) {
  owner_ = owner;
}

DevToolsClientImpl::ResponseInfo::ResponseInfo(const std::string& method)
    : state(kWaiting), method(method) {}

DevToolsClientImpl::ResponseInfo::~ResponseInfo() {}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    std::unique_ptr<base::DictionaryValue>* result,
    bool expect_response,
    bool wait_for_response,
    const Timeout* timeout) {
  if (parent_ == nullptr && !socket_->IsConnected())
    return Status(kDisconnected, "not connected to DevTools");

  int command_id = next_id_++;
  base::DictionaryValue command;
  command.SetInteger("id", command_id);
  command.SetString("method", method);
  command.SetKey("params", params.Clone());
  std::string message = SerializeValue(&command);
  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Command: " << method << " (id=" << command_id
            << ") " << id_ << " " << FormatValueForDisplay(params);
  }
  if (parent_ != nullptr) {
    base::DictionaryValue params2;
    params2.SetString("sessionId", session_id_);
    params2.SetString("message", message);
    Status status = parent_->SendCommandInternal(
        "Target.sendMessageToTarget", params2, nullptr, true, false, timeout);
    if (status.IsError())
      return status;
  } else if (!socket_->Send(message)) {
    return Status(kDisconnected, "unable to send message to renderer");
  }

  if (expect_response) {
    linked_ptr<ResponseInfo> response_info =
        make_linked_ptr(new ResponseInfo(method));
    if (timeout)
      response_info->command_timeout = *timeout;
    response_info_map_[command_id] = response_info;

    if (wait_for_response) {
      while (response_info->state == kWaiting) {
        Status status = ProcessNextMessage(
            command_id, Timeout(base::TimeDelta::FromMinutes(10), timeout));
        if (status.IsError()) {
          if (response_info->state == kReceived)
            response_info_map_.erase(command_id);
          return status;
        }
      }
      if (response_info->state == kBlocked) {
        response_info->state = kIgnored;
        return Status(kUnexpectedAlertOpen);
      }
      CHECK_EQ(response_info->state, kReceived);
      internal::InspectorCommandResponse& response = response_info->response;
      if (!response.result)
        return ParseInspectorError(response.error);
      *result = std::move(response.result);
    }
  } else {
    CHECK(!wait_for_response);
  }
  return Status(kOk);
}

Status DevToolsClientImpl::ProcessNextMessage(
    int expected_id,
    const Timeout& timeout) {
  ScopedIncrementer increment_stack_count(&stack_count_);

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
    return parent_->ProcessNextMessage(-1, timeout);

  std::string message;
  switch (socket_->ReceiveNextMessage(&message, timeout)) {
    case SyncWebSocket::kOk:
      break;
    case SyncWebSocket::kDisconnected: {
      std::string err = "Unable to receive message from renderer";
      LOG(ERROR) << err;
      return Status(kDisconnected, err);
    }
    case SyncWebSocket::kTimeout: {
      std::string err =
          "Timed out receiving message from renderer: " +
          base::StringPrintf("%.3lf", timeout.GetDuration().InSecondsF());
      LOG(ERROR) << err;
      return Status(kTimeout, err);
    }
    default:
      NOTREACHED();
      break;
  }

  return HandleMessage(expected_id, message);
}

Status DevToolsClientImpl::HandleMessage(int expected_id,
                                         const std::string& message) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  if (!parser_func_.Run(message, expected_id, &type, &event, &response)) {
    LOG(ERROR) << "Bad inspector message: " << message;
    return Status(kUnknownError, "bad inspector message: " + message);
  }

  if (type == internal::kEventMessageType)
    return ProcessEvent(event);
  CHECK_EQ(type, internal::kCommandResponseMessageType);
  return ProcessCommandResponse(response);
}

Status DevToolsClientImpl::ProcessEvent(const internal::InspectorEvent& event) {
  if (IsVLogOn(1)) {
    // Note: ChromeDriver log-replay depends on the format of this logging.
    // see chromedriver/log_replay/devtools_log_reader.cc.
    VLOG(1) << "DevTools WebSocket Event: " << event.method << " " << id_ << " "
            << FormatValueForDisplay(*event.params);
  }
  unnotified_event_listeners_ = listeners_;
  unnotified_event_ = &event;
  Status status = EnsureListenersNotifiedOfEvent();
  unnotified_event_ = NULL;
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
    for (ResponseInfoMap::const_iterator iter = response_info_map_.begin();
         iter != response_info_map_.end(); ++iter) {
      if (iter->first > max_id)
        continue;
      if (iter->second->state == kWaiting)
        iter->second->state = kBlocked;
    }
    if (enable_status.IsError())
      return status;
  }
  if (event.method == "Target.receivedMessageFromTarget") {
    std::string session_id;
    if (!event.params->GetString("sessionId", &session_id))
      return Status(
          kUnknownError,
          "missing sessionId in Target.receivedMessageFromTarget event");
    if (children_.count(session_id) == 0)
      // ChromeDriver only cares about iframe targets. If we don't know about
      // this sessionId, then it must be of a different target type and should
      // be ignored.
      return Status(kOk);
    DevToolsClientImpl* child = children_[session_id];
    std::string message;
    if (!event.params->GetString("message", &message))
      return Status(
          kUnknownError,
          "missing message in Target.receivedMessageFromTarget event");

    WebViewImplHolder childHolder(child->owner_);
    return child->HandleMessage(-1, message);
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
            << " (id=" << response.id << ") " << id_ << " " << result;
  }

  if (iter == response_info_map_.end())
    return Status(kUnknownError, "unexpected command response");

  linked_ptr<ResponseInfo> response_info = response_info_map_[response.id];
  response_info_map_.erase(response.id);

  if (response_info->state != kIgnored) {
    response_info->state = kReceived;
    response_info->response.id = response.id;
    response_info->response.error = response.error;
    if (response.result)
      response_info->response.result.reset(response.result->DeepCopy());
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
        this,
        unnotified_cmd_response_info_->method,
        *unnotified_cmd_response_info_->response.result.get(),
        unnotified_cmd_response_info_->command_timeout);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

namespace internal {

bool ParseInspectorMessage(
    const std::string& message,
    int expected_id,
    InspectorMessageType* type,
    InspectorEvent* event,
    InspectorCommandResponse* command_response) {
  // We want to allow invalid characters in case they are valid ECMAScript
  // strings. For example, webplatform tests use this to check string handling
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::Read(message, base::JSON_REPLACE_INVALID_CHARACTERS);
  base::DictionaryValue* message_dict;
  if (!message_value || !message_value->GetAsDictionary(&message_dict))
    return false;

  int id;
  if (!message_dict->HasKey("id")) {
    std::string method;
    if (!message_dict->GetString("method", &method))
      return false;
    base::DictionaryValue* params = NULL;
    message_dict->GetDictionary("params", &params);

    *type = kEventMessageType;
    event->method = method;
    if (params)
      event->params.reset(params->DeepCopy());
    else
      event->params.reset(new base::DictionaryValue());
    return true;
  } else if (message_dict->GetInteger("id", &id)) {
    base::DictionaryValue* unscoped_error = NULL;
    base::DictionaryValue* unscoped_result = NULL;
    *type = kCommandResponseMessageType;
    command_response->id = id;
    // As per Chromium issue 392577, DevTools does not necessarily return a
    // "result" dictionary for every valid response. In particular,
    // Tracing.start and Tracing.end command responses do not contain one.
    // So, if neither "error" nor "result" keys are present, just provide
    // a blank result dictionary.
    if (message_dict->GetDictionary("result", &unscoped_result))
      command_response->result.reset(unscoped_result->DeepCopy());
    else if (message_dict->GetDictionary("error", &unscoped_error))
      base::JSONWriter::Write(*unscoped_error, &command_response->error);
    else
      command_response->result.reset(new base::DictionaryValue());
    return true;
  }
  return false;
}

}  // namespace internal
