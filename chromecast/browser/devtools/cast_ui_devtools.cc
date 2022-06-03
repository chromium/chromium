// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_ui_devtools.h"

#include "chromecast/base/chromecast_switches.h"
#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/page_agent.h"
#include "components/ui_devtools/switches.h"
#include "components/ui_devtools/views/dom_agent_views.h"
#include "components/ui_devtools/views/overlay_agent_views.h"
#include "components/ui_devtools/views/page_agent_views.h"

namespace chromecast {
namespace shell {

CastUIDevTools::CastUIDevTools(network::mojom::NetworkContext* network_context)
    : devtools_server_(CreateServer(network_context)) {}

CastUIDevTools::~CastUIDevTools() = default;

std::unique_ptr<ui_devtools::UiDevToolsServer> CastUIDevTools::CreateServer(
    network::mojom::NetworkContext* network_context) const {
  constexpr int kUiDevToolsDefaultPort = 9223;
  int port = GetSwitchValueInt(ui_devtools::switches::kEnableUiDevTools,
                               kUiDevToolsDefaultPort);
  auto server =
      ui_devtools::UiDevToolsServer::CreateForViews(network_context, port);
  DCHECK(server);
  auto client = std::make_unique<ui_devtools::UiDevToolsClient>(
      "UiDevToolsClient", server.get());
  auto dom_agent_views = ui_devtools::DOMAgentViews::Create();
  DCHECK(dom_agent_views);
  auto* dom_agent_views_ptr = dom_agent_views.get();
  client->AddAgent(
      std::make_unique<ui_devtools::PageAgentViews>(dom_agent_views_ptr));
  client->AddAgent(std::move(dom_agent_views));
  client->AddAgent(
      std::make_unique<ui_devtools::CSSAgent>(dom_agent_views_ptr));
  client->AddAgent(ui_devtools::OverlayAgentViews::Create(dom_agent_views_ptr));
  server->AttachClient(std::move(client));
  return server;
}

}  // namespace shell
}  // namespace chromecast
