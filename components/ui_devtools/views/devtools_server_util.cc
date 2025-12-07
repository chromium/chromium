// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/devtools_server_util.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/page_agent.h"
#include "components/ui_devtools/switches.h"
#include "components/ui_devtools/tracing_agent.h"
#include "components/ui_devtools/views/dom_agent_views.h"
#include "components/ui_devtools/views/overlay_agent_views.h"
#include "components/ui_devtools/views/page_agent_views.h"

namespace ui_devtools {

std::unique_ptr<UiDevToolsServer> CreateUiDevToolsServerForViews(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    std::unique_ptr<ConnectorDelegate> connector,
    const base::FilePath& active_port_output_directory) {
  constexpr int kUiDevToolsDefaultPort = 9223;
  int port = UiDevToolsServer::GetUiDevToolsPort(switches::kEnableUiDevTools,
                                                 kUiDevToolsDefaultPort);
  auto server = UiDevToolsServer::CreateForViews(
      std::move(io_thread_task_runner), port, active_port_output_directory);
  DCHECK(server);
  auto client =
      std::make_unique<UiDevToolsClient>("UiDevToolsClient", server.get());
  auto dom_agent_views = DOMAgentViews::Create();
  auto* dom_agent_views_ptr = dom_agent_views.get();
  client->AddAgent(std::make_unique<PageAgentViews>(dom_agent_views_ptr));
  client->AddAgent(std::move(dom_agent_views));
  client->AddAgent(std::make_unique<CSSAgent>(dom_agent_views_ptr));
  client->AddAgent(OverlayAgentViews::Create(dom_agent_views_ptr));
  auto tracing_agent = std::make_unique<TracingAgent>(std::move(connector));
  server->set_tracing_agent(tracing_agent.get());
  client->AddAgent(std::move(tracing_agent));
  server->AttachClient(std::move(client));
  return server;
}

}  // namespace ui_devtools
