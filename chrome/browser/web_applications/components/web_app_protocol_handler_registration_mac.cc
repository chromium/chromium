// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_protocol_handler_registration.h"

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    base::OnceCallback<void(bool)> callback) {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
  registry->RegisterAppProtocolHandlers(app_id, protocol_handlers);
  std::move(callback).Run(true);
}

void UnregisterProtocolHandlersWithOs(
    const AppId& app_id,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers) {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
  registry->DeregisterAppProtocolHandlers(app_id, protocol_handlers);
}

}  // namespace web_app
