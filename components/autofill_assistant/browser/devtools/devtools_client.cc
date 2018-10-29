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
      renderer_crashed_(false),
      next_message_id_(0),
      weak_ptr_factory_(this) {
  browser_main_thread_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {content::BrowserThread::UI});
  agent_host_->AttachClient(this);
}

DevtoolsClient::~DevtoolsClient() {
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

void DevtoolsClient::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    base::OnceCallback<void(const base::Value&)> callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void DevtoolsClient::SendMessage(const char* method,
                                 std::unique_ptr<base::Value> params,
                                 base::OnceClosure callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

template <typename CallbackType>
void DevtoolsClient::SendMessageWithParams(const char* method,
                                           std::unique_ptr<base::Value> params,
                                           CallbackType callback) {
  base::DictionaryValue message;
  message.SetString("method", method);
  message.Set("params", std::move(params));

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

void DevtoolsClient::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& json_message) {
  DCHECK_EQ(agent_host, agent_host_.get());

  std::unique_ptr<base::Value> message =
      base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
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
    if (message_dict.GetDictionary("result", &result_dict)) {
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &DevtoolsClient::DispatchMessageReplyWithResultTask,
                weak_ptr_factory_.GetWeakPtr(), std::move(owning_message),
                std::move(callback.callback_with_result), result_dict));
      } else {
        std::move(callback.callback_with_result).Run(*result_dict);
      }
    } else if (message_dict.GetDictionary("error", &result_dict)) {
      auto null_value = std::make_unique<base::Value>();
      DLOG(ERROR) << "Error in method call result: " << *result_dict;
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &DevtoolsClient::DispatchMessageReplyWithResultTask,
                weak_ptr_factory_.GetWeakPtr(), std::move(null_value),
                std::move(callback.callback_with_result), null_value.get()));
      } else {
        std::move(callback.callback_with_result).Run(*null_value);
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
    base::OnceCallback<void(const base::Value&)> callback,
    const base::Value* result_dict) {
  std::move(callback).Run(*result_dict);
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

void DevtoolsClient::AgentHostClosed(content::DevToolsAgentHost* agent_host) {
  // Agent host is not expected to be closed when this object is alive.
  renderer_crashed_ = true;
}

DevtoolsClient::Callback::Callback() = default;

DevtoolsClient::Callback::Callback(Callback&& other) = default;

DevtoolsClient::Callback::Callback(base::OnceClosure callback)
    : callback(std::move(callback)) {}

DevtoolsClient::Callback::Callback(
    base::OnceCallback<void(const base::Value&)> callback)
    : callback_with_result(std::move(callback)) {}

DevtoolsClient::Callback::~Callback() = default;

DevtoolsClient::Callback& DevtoolsClient::Callback::operator=(
    Callback&& other) = default;

}  // namespace autofill_assistant.
