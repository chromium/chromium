// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;

namespace simple_devtools_protocol_client {

namespace {
// Use --vmodule=simple_devtools_protocol_client=2 switch to log protocol
// messages.
constexpr int kVLogLevel = 2;

const char kId[] = "id";
const char kSessionId[] = "sessionId";
const char kMethod[] = "method";
const char kParams[] = "params";

int g_next_message_id = 0;

}  // namespace

SimpleDevToolsProtocolClient::SimpleDevToolsProtocolClient() = default;
SimpleDevToolsProtocolClient::SimpleDevToolsProtocolClient(
    const std::string& session_id)
    : session_id_(session_id) {}

SimpleDevToolsProtocolClient::~SimpleDevToolsProtocolClient() {
  if (parent_client_)
    parent_client_->sessions_.erase(session_id_);
}

void SimpleDevToolsProtocolClient::AttachClient(
    scoped_refptr<content::DevToolsAgentHost> agent_host) {
  DCHECK(!agent_host_);
  agent_host_ = agent_host;
  agent_host_->AttachClient(this);
}

void SimpleDevToolsProtocolClient::DetachClient() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void SimpleDevToolsProtocolClient::AttachToBrowser() {
  AttachClient(DevToolsAgentHost::CreateForBrowser(
      /*tethering_task_runner=*/nullptr,
      DevToolsAgentHost::CreateServerSocketCallback()));
}

void SimpleDevToolsProtocolClient::AttachToWebContents(
    content::WebContents* web_contents) {
  AttachClient(DevToolsAgentHost::GetOrCreateFor(web_contents));
}

std::string SimpleDevToolsProtocolClient::GetTargetId() {
  DCHECK(agent_host_);
  return agent_host_->GetId();
}

std::unique_ptr<SimpleDevToolsProtocolClient>
SimpleDevToolsProtocolClient::CreateSession(const std::string& session_id) {
  auto client = std::make_unique<SimpleDevToolsProtocolClient>(session_id);
  client->parent_client_ = this;
  sessions_[session_id] = client.get();
  return client;
}

void SimpleDevToolsProtocolClient::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> json_message) {
  DCHECK_EQ(agent_host, agent_host_);

  std::string_view str_message(
      reinterpret_cast<const char*>(json_message.data()), json_message.size());
  base::Value message_value = *base::JSONReader::Read(str_message);
  base::Value::Dict& message = message_value.GetDict();

  if (const std::string* session_id = message.FindString("sessionId")) {
    auto it = sessions_.find(*session_id);
    if (it != sessions_.cend()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &SimpleDevToolsProtocolClient::DispatchProtocolMessageTask,
              it->second->GetWeakPtr(), std::move(message)));
      return;
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleDevToolsProtocolClient::DispatchProtocolMessageTask,
                     GetWeakPtr(), std::move(message)));
}

void SimpleDevToolsProtocolClient::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  if (agent_host_ == agent_host) {
    agent_host_ = nullptr;
    pending_response_map_.clear();
  }
}

void SimpleDevToolsProtocolClient::DispatchProtocolMessageTask(
    base::Value::Dict message) {
  VLOG(kVLogLevel) << "\n[CDP RECV] " << message.DebugString();

  // Handle response message shutting down the host if it's unexpected.
  if (std::optional<int> id = message.FindInt(kId)) {
    auto it = pending_response_map_.find(*id);
    if (it == pending_response_map_.cend()) {
      LOG(ERROR) << "Unexpected message id=" << *id;
      agent_host_->GetProcessHost()->ShutdownForBadMessage(
          content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    }

    // Result handler callback may add more callbacks, so make sure we use
    // the iterator to clean up pending response map before calling the
    // callback.
    ResponseCallback callback(std::move(it->second));
    pending_response_map_.erase(it);

    std::move(callback).Run(std::move(message));
    return;
  }

  // Handle notification message ignoring the stuff we're not expecting.
  const std::string* event_name = message.FindString(kMethod);
  if (!event_name)
    return;

  // Try to locate the event specific handler first and if there is none,
  // check the domain events handler specified as 'Domain.*'.
  auto it = event_handler_map_.find(*event_name);
  if (it == event_handler_map_.cend())
    return;

  // Use a snapshot of the event's handler list verifying each callback
  // before calling it since it could be removed by the previous callback.
  std::vector<EventCallback> handlers(it->second);
  bool first_callback = true;
  for (auto& callback : handlers) {
    if (first_callback || HasEventHandler(*event_name, callback)) {
      first_callback = false;
      callback.Run(message);
    }
  }
}

void SimpleDevToolsProtocolClient::SendProtocolMessage(
    base::Value::Dict message) {
  if (parent_client_ && !agent_host_) {
    parent_client_->SendProtocolMessage(std::move(message));
    return;
  }

  VLOG(kVLogLevel) << "\n[CDP SEND] " << message.DebugString();

  std::string json_message;
  base::JSONWriter::Write(base::Value(std::move(message)), &json_message);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_message)));
}

void SimpleDevToolsProtocolClient::SendCommand(
    const std::string& method,
    base::Value::Dict params,
    ResponseCallback response_callback) {
  int id = g_next_message_id++;

  base::Value::Dict message;
  message.Set(kId, id);
  message.Set(kMethod, method);
  if (params.size())
    message.Set(kParams, std::move(params));
  if (!session_id_.empty())
    message.Set(kSessionId, session_id_);

  pending_response_map_.insert({id, std::move(response_callback)});

  SendProtocolMessage(std::move(message));
}

void SimpleDevToolsProtocolClient::SendCommand(
    const std::string& method,
    ResponseCallback response_callback) {
  SendCommand(method, base::Value::Dict(), std::move(response_callback));
}

void SimpleDevToolsProtocolClient::SendCommand(const std::string& method,
                                               base::Value::Dict params) {
  SendCommand(method, std::move(params), base::DoNothing());
}

void SimpleDevToolsProtocolClient::SendCommand(const std::string& method) {
  SendCommand(method, base::Value::Dict(), base::DoNothing());
}

void SimpleDevToolsProtocolClient::AddEventHandler(
    const std::string& event_name,
    EventCallback event_callback) {
  event_handler_map_[event_name].push_back(std::move(event_callback));
}

void SimpleDevToolsProtocolClient::RemoveEventHandler(
    const std::string& event_name,
    const EventCallback& event_callback) {
  auto it = event_handler_map_.find(event_name);
  if (it == event_handler_map_.cend())
    return;

  std::vector<EventCallback>& handlers = it->second;

  auto handler = std::find(handlers.cbegin(), handlers.cend(), event_callback);
  if (handler != handlers.cend()) {
    handlers.erase(handler);
    if (handlers.empty())
      event_handler_map_.erase(it);
  }
}

bool SimpleDevToolsProtocolClient::HasEventHandler(
    const std::string& event_name,
    const EventCallback& event_callback) {
  auto it = event_handler_map_.find(event_name);
  if (it == event_handler_map_.cend())
    return false;

  std::vector<EventCallback>& handlers = it->second;
  auto handler = std::find(handlers.cbegin(), handlers.cend(), event_callback);

  return handler != handlers.cend();
}

base::WeakPtr<SimpleDevToolsProtocolClient>
SimpleDevToolsProtocolClient::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace simple_devtools_protocol_client
