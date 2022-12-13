// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/protocol_handling_sub_manager.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace web_app {

ProtocolHandlingSubManager::ProtocolHandlingSubManager(
    WebAppRegistrar& registrar)
    : registrar_(registrar) {}

ProtocolHandlingSubManager::~ProtocolHandlingSubManager() = default;

void ProtocolHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  // Disable protocol handler unregistration on Win7 due to bad interactions
  // between preinstalled app scenarios and the need for elevation to unregister
  // protocol handlers on that platform. See crbug.com/1224327 for context.
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    std::move(configure_done).Run();
    return;
  }
#endif

  DCHECK_EQ(desired_state.manifest_protocol_handlers_states().size(), 0);

  if (!registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  desired_state.clear_manifest_protocol_handlers_states();

  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app) {
    std::move(configure_done).Run();
    return;
  }

  for (const auto& protocol_handler : web_app->protocol_handlers()) {
    if (base::Contains(web_app->disallowed_launch_protocols(),
                       protocol_handler.protocol)) {
      continue;
    }

    proto::WebAppProtocolHandler* protocol_handler_state =
        desired_state.add_manifest_protocol_handlers_states();
    protocol_handler_state->set_protocol(protocol_handler.protocol);
    protocol_handler_state->set_url(protocol_handler.url.spec());
  }
  std::move(configure_done).Run();
}

void ProtocolHandlingSubManager::Start() {}

void ProtocolHandlingSubManager::Shutdown() {}

void ProtocolHandlingSubManager::Execute(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    const absl::optional<proto::WebAppOsIntegrationState>& current_state,
    base::OnceClosure callback) {
  NOTREACHED() << "Not yet implemented";
}

}  // namespace web_app
