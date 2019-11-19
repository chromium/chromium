// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_

#include <memory>

#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/devtools_server.h"

namespace ui_devtools {

// A factory helper to construct a UiDevToolsServer for Views.
// The connector is used in TracingAgent to hook up with the tracing service.
std::unique_ptr<UiDevToolsServer> CreateUiDevToolsServerForViews(
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<ConnectorDelegate> connector);

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_
