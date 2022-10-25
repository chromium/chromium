// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;

namespace simple_devtools_protocol_client {

namespace {

const char kId[] = "id";
const char kSessionId[] = "sessionId";
const char kMethod[] = "method";
const char kParams[] = "params";

int g_next_message_id = 0;

}  // namespace

SimpleDevToolsProtocolClient::SimpleDevToolsProtocolClient() = default;
SimpleDevToolsProtocolClient::~SimpleDevToolsProtocolClient() = default;

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

void SimpleDevToolsProtocolClient::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> json_message) {
  DCHECK_EQ(agent_host, agent_host_);

  base::StringPiece message_str(
      reinterpret_cast<const char*>(json_message.data()), json_message.size());
  base::Value message_value = *base::JSONReader::Read(message_str);

  // Handle response message.
  if (absl::optional<int> id = message_value.GetDict().FindInt(kId)) {
    auto it = pending_response_map_.find(*id);
    if (it == pending_response_map_.cend())
      agent_host_->GetProcessHost()->ShutdownForBadMessage(
          content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);

    // Result handler callback may add more callbacks, so make sure we use
    // the iterator to clean up dispatch map before calling the callback.
    ResponseCallback callback(std::move(it->second));
    pending_response_map_.erase(it);

    std::move(callback).Run(std::move(message_value).TakeDict());
    return;
  }

  // Handle notification message.
  const std::string* event_name = message_value.GetDict().FindString(kMethod);
  if (event_name) {
    auto it = event_map_.find(*event_name);
    if (it != event_map_.cend()) {
      // Use a snapshot of the event's handler list verifying each callback
      // before calling it since it could be removed by the previous callback.
      std::vector<EventCallback> handlers(it->second);
      bool first_callback = true;
      for (auto& callback : handlers) {
        if (first_callback || HasEventHandler(*event_name, callback)) {
          callback.Run(message_value.GetDict());
          first_callback = false;
        }
      }
    }
  }
}

void SimpleDevToolsProtocolClient::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  if (agent_host_ == agent_host) {
    agent_host_ = nullptr;
    pending_response_map_.clear();
  }
}

void SimpleDevToolsProtocolClient::SendSessionCommand(
    const std::string method,
    base::Value::Dict params,
    const std::string session_id,
    ResponseCallback response_callback) {
  DCHECK(agent_host_);

  int id = g_next_message_id++;

  base::Value::Dict command;
  command.Set(kId, id);
  command.Set(kMethod, std::move(method));
  if (params.size())
    command.Set(kParams, std::move(params));
  if (!session_id.empty())
    command.Set(kSessionId, std::move(session_id));

  pending_response_map_.insert({id, std::move(response_callback)});

  std::string json_command;
  base::JSONWriter::Write(base::Value(std::move(command)), &json_command);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_command)));
}

void SimpleDevToolsProtocolClient::SendCommand(
    const std::string method,
    base::Value::Dict params,
    ResponseCallback response_callback) {
  SendSessionCommand(method, std::move(params), std::string(),
                     std::move(response_callback));
}

void SimpleDevToolsProtocolClient::SendCommand(
    const std::string method,
    ResponseCallback response_callback) {
  SendSessionCommand(method, base::Value::Dict(), std::string(),
                     std::move(response_callback));
}

void SimpleDevToolsProtocolClient::SendCommand(const std::string method,
                                               base::Value::Dict params) {
  SendSessionCommand(method, std::move(params), std::string(),
                     base::DoNothing());
}

void SimpleDevToolsProtocolClient::SendCommand(const std::string method) {
  SendSessionCommand(method, base::Value::Dict(), std::string(),
                     base::DoNothing());
}

void SimpleDevToolsProtocolClient::AddEventHandler(
    const std::string& event_name,
    EventCallback event_callback) {
  event_map_[event_name].push_back(std::move(event_callback));
}

bool SimpleDevToolsProtocolClient::HasEventHandler(
    const std::string& event_name,
    const EventCallback& event_callback) {
  auto it = event_map_.find(event_name);
  if (it == event_map_.cend())
    return false;

  std::vector<EventCallback>& handlers = it->second;
  auto handler = std::find(handlers.cbegin(), handlers.cend(), event_callback);

  return handler != handlers.cend();
}

void SimpleDevToolsProtocolClient::RemoveEventHandler(
    const std::string& event_name,
    const EventCallback& event_callback) {
  auto it = event_map_.find(event_name);
  if (it == event_map_.cend())
    return;

  std::vector<EventCallback>& handlers = it->second;

  auto handler = std::find(handlers.cbegin(), handlers.cend(), event_callback);
  if (handler != handlers.cend()) {
    handlers.erase(handler);
    if (handlers.empty())
      event_map_.erase(it);
  }
}

}  // namespace simple_devtools_protocol_client