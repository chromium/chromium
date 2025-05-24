// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_H_
#define COMPONENTS_DEVTOOLS_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {
class WebContents;
}

namespace simple_devtools_protocol_client {

class SimpleDevToolsProtocolClient : public content::DevToolsAgentHostClient {
 public:
  typedef base::OnceCallback<void(base::Value::Dict)> ResponseCallback;
  typedef base::RepeatingCallback<void(const base::Value::Dict&)> EventCallback;

  SimpleDevToolsProtocolClient();
  explicit SimpleDevToolsProtocolClient(const std::string& session_id);

  ~SimpleDevToolsProtocolClient() override;

  void AttachClient(scoped_refptr<content::DevToolsAgentHost> agent_host);
  void DetachClient();

  void AttachToBrowser();
  void AttachToWebContents(content::WebContents* web_contents);

  std::unique_ptr<SimpleDevToolsProtocolClient> CreateSession(
      const std::string& session_id);

  void AddEventHandler(const std::string& event_name,
                       EventCallback event_callback);
  void RemoveEventHandler(const std::string& event_name,
                          const EventCallback& event_callback);

  void SendCommand(const std::string& method,
                   base::Value::Dict params,
                   ResponseCallback response_callback);

  void SendCommand(const std::string& method,
                   ResponseCallback response_callback);

  void SendCommand(const std::string& method, base::Value::Dict params);

  void SendCommand(const std::string& method);

  std::string GetTargetId();

 protected:
  // content::DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> json_message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;

  // Virtual for tests.
  virtual void DispatchProtocolMessageTask(base::Value::Dict message);

  void SendProtocolMessage(base::Value::Dict message);

  bool HasEventHandler(const std::string& event_name,
                       const EventCallback& event_callback);

  base::WeakPtr<SimpleDevToolsProtocolClient> GetWeakPtr();

  const std::string session_id_;
  raw_ptr<SimpleDevToolsProtocolClient> parent_client_ = nullptr;
  base::flat_map<std::string,
                 raw_ptr<SimpleDevToolsProtocolClient, CtnExperimental>>
      sessions_;

  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  base::flat_map<int, ResponseCallback> pending_response_map_;
  base::flat_map<std::string, std::vector<EventCallback>> event_handler_map_;

  base::WeakPtrFactory<SimpleDevToolsProtocolClient> weak_ptr_factory_{this};
};

}  // namespace simple_devtools_protocol_client

#endif  // COMPONENTS_DEVTOOLS_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_SIMPLE_DEVTOOLS_PROTOCOL_CLIENT_H_