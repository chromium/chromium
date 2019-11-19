// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied and modified from
// https://chromium.googlesource.com/chromium/src/+/a3f9d4fac81fc86065d867ab08fa4912ddf662c7/headless/lib/browser/headless_devtools_client_impl.cc
// Modifications include namespace, path, simplifying and removing unnecessary
// codes.

#include "components/autofill_assistant/browser/devtools/devtools_client.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace autofill_assistant {

DevtoolsClient::DevtoolsClient(
    scoped_refptr<content::DevToolsAgentHost> agent_host)
    : agent_host_(agent_host),
      input_domain_(this),
      dom_domain_(this),
      runtime_domain_(this),
      network_domain_(this),
      target_domain_(this),
      renderer_crashed_(false),
      next_message_id_(0),
      frame_tracker_(this) {
  browser_main_thread_ =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
  agent_host_->AttachClient(this);
  frame_tracker_.Start();
}

DevtoolsClient::~DevtoolsClient() {
  frame_tracker_.Stop();
  agent_host_->DetachClient(this);
}

input::Domain* DevtoolsClient::GetInput() {
  return &input_domain_;
}

dom::Domain* DevtoolsClient::GetDOM() {
  return &dom_domain_;
}

runtime::Domain* DevtoolsClient::GetRuntime() {
  return &runtime_domain_;
}

network::Domain* DevtoolsClient::GetNetwork() {
  return &network_domain_;
}

target::ExperimentalDomain* DevtoolsClient::GetTarget() {
  return &target_domain_;
}

void DevtoolsClient::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    const std::string& optional_node_frame_id,
    base::OnceCallback<void(const ReplyStatus&, const base::Value&)> callback) {
  SendMessageWithParams(method, std::move(params), optional_node_frame_id,
                        std::move(callback));
}

void DevtoolsClient::SendMessage(const char* method,
                                 std::unique_ptr<base::Value> params,
                                 const std::string& optional_node_frame_id,
                                 base::OnceClosure callback) {
  SendMessageWithParams(method, std::move(params), optional_node_frame_id,
                        std::move(callback));
}

template <typename CallbackType>
void DevtoolsClient::SendMessageWithParams(
    const char* method,
    std::unique_ptr<base::Value> params,
    const std::string& optional_node_frame_id,
    CallbackType callback) {
  base::DictionaryValue message;
  message.SetString("method", method);
  message.Set("params", std::move(params));

  std::string optional_session_id =
      GetSessionIdForFrame(optional_node_frame_id);
  if (!optional_session_id.empty()) {
    message.SetString("sessionId", optional_session_id);
  }

  if (renderer_crashed_)
    return;
  int id = next_message_id_;
  next_message_id_ += 2;  // We only send even numbered messages.
  message.SetInteger("id", id);
  pending_messages_[id] = Callback(std::move(callback));

  std::string json_message;
  base::JSONWriter::Write(message, &json_message);

  bool success = agent_host_->DispatchProtocolMessage(this, json_message);
  DCHECK(success);
}

void DevtoolsClient::RegisterEventHandler(
    const char* method,
    base::RepeatingCallback<void(const base::Value&)> callback) {
  DCHECK(event_handlers_.find(method) == event_handlers_.end());
  event_handlers_[method] = std::move(callback);
}

void DevtoolsClient::UnregisterEventHandler(const char* method) {
  DCHECK(event_handlers_.find(method) != event_handlers_.end());
  event_handlers_.erase(method);
}

void DevtoolsClient::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& json_message) {
  DCHECK_EQ(agent_host, agent_host_.get());

  std::unique_ptr<base::Value> message =
      base::JSONReader::ReadDeprecated(json_message, base::JSON_PARSE_RFC);
  DCHECK(message && message->is_dict());

  const base::DictionaryValue* message_dict;
  bool success = message->GetAsDictionary(&message_dict);
  DCHECK(success);

  success = message_dict->HasKey("id")
                ? DispatchMessageReply(std::move(message), *message_dict)
                : DispatchEvent(std::move(message), *message_dict);
  if (!success)
    DVLOG(2) << "Unhandled protocol message: " << json_message;
}

bool DevtoolsClient::DispatchMessageReply(
    std::unique_ptr<base::Value> owning_message,
    const base::DictionaryValue& message_dict) {
  const base::Value* id_value = message_dict.FindKey("id");
  if (!id_value) {
    NOTREACHED() << "ID must be specified.";
    return false;
  }
  auto it = pending_messages_.find(id_value->GetInt());
  if (it == pending_messages_.end()) {
    NOTREACHED() << "Unexpected reply";
    return false;
  }
  Callback callback = std::move(it->second);
  pending_messages_.erase(it);
  if (!callback.callback_with_result.is_null()) {
    const base::DictionaryValue* result_dict;
    ReplyStatus status;
    if (message_dict.GetDictionary("result", &result_dict)) {
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &DevtoolsClient::DispatchMessageReplyWithResultTask,
                weak_ptr_factory_.GetWeakPtr(), std::move(owning_message),
                std::move(callback.callback_with_result), status, result_dict));
      } else {
        std::move(callback.callback_with_result).Run(status, *result_dict);
      }
    } else if (message_dict.GetDictionary("error", &result_dict)) {
      auto null_value = std::make_unique<base::Value>();
      DLOG(ERROR) << "Error in method call result: " << *result_dict;
      FillReplyStatusFromErrorDict(&status, *result_dict);
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(&DevtoolsClient::DispatchMessageReplyWithResultTask,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(null_value),
                           std::move(callback.callback_with_result), status,
                           null_value.get()));
      } else {
        std::move(callback.callback_with_result).Run(status, *null_value);
      }
    } else {
      NOTREACHED() << "Reply has neither result nor error";
      return false;
    }
  } else if (!callback.callback.is_null()) {
    if (browser_main_thread_) {
      browser_main_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<DevtoolsClient> self,
                 base::OnceClosure callback) {
                if (self)
                  std::move(callback).Run();
              },
              weak_ptr_factory_.GetWeakPtr(), std::move(callback.callback)));
    } else {
      std::move(callback.callback).Run();
    }
  }
  return true;
}

void DevtoolsClient::DispatchMessageReplyWithResultTask(
    std::unique_ptr<base::Value> owning_message,
    base::OnceCallback<void(const ReplyStatus&, const base::Value&)> callback,
    const ReplyStatus& reply_status,
    const base::Value* result_dict) {
  std::move(callback).Run(reply_status, *result_dict);
}

bool DevtoolsClient::DispatchEvent(std::unique_ptr<base::Value> owning_message,
                                   const base::DictionaryValue& message_dict) {
  const base::Value* method_value = message_dict.FindKey("method");
  if (!method_value)
    return false;
  const std::string& method = method_value->GetString();
  if (method == "Inspector.targetCrashed")
    renderer_crashed_ = true;
  EventHandlerMap::const_iterator it = event_handlers_.find(method);
  if (it == event_handlers_.end()) {
    if (method != "Inspector.targetCrashed")
      DVLOG(2) << "Unknown event: " << method;
    return false;
  }
  if (!it->second.is_null()) {
    const base::DictionaryValue* result_dict;
    if (!message_dict.GetDictionary("params", &result_dict)) {
      NOTREACHED() << "Badly formed event parameters";
      return false;
    }
    if (browser_main_thread_) {
      // DevTools assumes event handling is async so we must post a task here or
      // we risk breaking things.
      browser_main_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(&DevtoolsClient::DispatchEventTask,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(owning_message), &it->second, result_dict));
    } else {
      DispatchEventTask(std::move(owning_message), &it->second, result_dict);
    }
  }
  return true;
}

void DevtoolsClient::DispatchEventTask(
    std::unique_ptr<base::Value> owning_message,
    const EventHandler* event_handler,
    const base::DictionaryValue* result_dict) {
  event_handler->Run(*result_dict);
}

void DevtoolsClient::FillReplyStatusFromErrorDict(
    ReplyStatus* status,
    const base::DictionaryValue& error_dict) {
  const base::Value* code;
  if (error_dict.Get("code", &code) && code->is_int()) {
    status->error_code = code->GetInt();
  } else {
    status->error_code = -1;  // unknown error code
  }

  const base::Value* message;
  if (error_dict.Get("message", &message) && message->is_string()) {
    status->error_message = message->GetString();
  } else {
    status->error_message = "unknown";
  }
}

void DevtoolsClient::AgentHostClosed(content::DevToolsAgentHost* agent_host) {
  // Agent host is not expected to be closed when this object is alive.
  renderer_crashed_ = true;
}

std::string DevtoolsClient::GetSessionIdForFrame(
    const std::string& frame_id) const {
  return frame_tracker_.GetSessionIdForFrame(frame_id);
}

DevtoolsClient::Callback::Callback() = default;

DevtoolsClient::Callback::Callback(Callback&& other) = default;

DevtoolsClient::Callback::Callback(base::OnceClosure callback)
    : callback(std::move(callback)) {}

DevtoolsClient::Callback::Callback(
    base::OnceCallback<void(const ReplyStatus&, const base::Value&)> callback)
    : callback_with_result(std::move(callback)) {}

DevtoolsClient::Callback::~Callback() = default;

DevtoolsClient::Callback& DevtoolsClient::Callback::operator=(
    Callback&& other) = default;

DevtoolsClient::FrameTracker::FrameTracker(DevtoolsClient* client)
    : client_(client) {}

DevtoolsClient::FrameTracker::~FrameTracker() = default;

void DevtoolsClient::FrameTracker::Start() {
  client_->RegisterEventHandler(
      "Target.attachedToTarget",
      base::BindRepeating(&DevtoolsClient::FrameTracker::OnAttachedToTarget,
                          weak_ptr_factory_.GetWeakPtr()));
  client_->RegisterEventHandler(
      "Target.detachedFromTarget",
      base::BindRepeating(&DevtoolsClient::FrameTracker::OnDetachedFromTarget,
                          weak_ptr_factory_.GetWeakPtr()));

  started_ = true;

  // Start auto attaching so that we can keep track of what session got started
  // for what target. We use flatten = true to cover the entire frame tree.
  client_->GetTarget()->SetAutoAttach(
      target::SetAutoAttachParams::Builder()
          .SetAutoAttach(true)
          .SetWaitForDebuggerOnStart(false)
          .SetFlatten(true)
          .Build(),
      /* node_frame_id= */ "",
      base::BindOnce(&DevtoolsClient::FrameTracker::OnSetAutoAttach,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevtoolsClient::FrameTracker::Stop() {
  if (!started_) {
    return;
  }

  client_->UnregisterEventHandler("Target.attachedToTarget");
  client_->UnregisterEventHandler("Target.detachedFromTarget");

  started_ = false;
}

void DevtoolsClient::FrameTracker::OnSetAutoAttach(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<target::SetAutoAttachResult> result) {
  // This is not used since result doesn't contain anything useful. The real
  // action is happening in the On(Attached|Detached) functions.
  DCHECK(result);
}

std::string DevtoolsClient::FrameTracker::GetSessionIdForFrame(
    std::string frame_id) const {
  if (frame_id.empty()) {
    return std::string();
  }

  auto it = sessions_map_.find(frame_id);
  if (it != sessions_map_.end()) {
    return it->second;
  }
  DVLOG(3) << "No session id for frame_id: " << frame_id;
  return std::string();
}

std::string DevtoolsClient::FrameTracker::FindTargetId(
    const base::Value& value) {
  const base::Value* target_info = value.FindKey("targetInfo");
  if (!target_info) {
    DVLOG(3) << "No target_info found in " << value;
    return std::string();
  }
  const std::string* target_id = target_info->FindStringKey("targetId");
  if (!target_id) {
    DVLOG(3) << "No target_id found in " << *target_info;
    return std::string();
  }

  return *target_id;
}

std::string DevtoolsClient::FrameTracker::FindSessionId(
    const base::Value& value) {
  const std::string* session_id = value.FindStringKey("sessionId");
  if (!session_id) {
    DVLOG(3) << "No session_id found in " << value;
    return std::string();
  }

  return *session_id;
}

void DevtoolsClient::FrameTracker::OnAttachedToTarget(
    const base::Value& value) {
  std::string session_id = FindSessionId(value);
  std::string target_id = FindTargetId(value);

  if (!session_id.empty() && !target_id.empty()) {
    sessions_map_[target_id] = session_id;
  }
}

void DevtoolsClient::FrameTracker::OnDetachedFromTarget(
    const base::Value& value) {
  std::string target_id = FindTargetId(value);

  auto it = sessions_map_.find(target_id);
  if (it != sessions_map_.end()) {
    sessions_map_.erase(it);
  }
}

}  // namespace autofill_assistant.
