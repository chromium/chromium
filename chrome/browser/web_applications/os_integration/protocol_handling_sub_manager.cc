// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/protocol_handling_sub_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "url/gurl.h"

namespace web_app {

namespace {

std::vector<apps::ProtocolHandlerInfo> GetApprovedProtocolHandlers(
    const proto::WebAppOsIntegrationState& state) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers_info;
  for (const auto& proto_data : state.protocols_handled().protocols()) {
    apps::ProtocolHandlerInfo info;
    info.protocol = proto_data.protocol();
    info.url = GURL(proto_data.url());
    protocol_handlers_info.push_back(info);
  }
  return protocol_handlers_info;
}

void RecordProtocolHandlingResult(const std::string histogram_name,
                                  Result result) {
  base::UmaHistogramBoolean(histogram_name, (result == Result::kOk));
}

}  // namespace

ProtocolHandlingSubManager::ProtocolHandlingSubManager(
    const base::FilePath& profile_path,
    WebAppProvider& provider)
    : profile_path_(profile_path), provider_(provider) {}

ProtocolHandlingSubManager::~ProtocolHandlingSubManager() = default;

void ProtocolHandlingSubManager::Configure(
    const webapps::AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_protocols_handled());
  if (provider_->registrar_unsafe().GetInstallState(app_id) !=
      proto::INSTALLED_WITH_OS_INTEGRATION) {
    std::move(configure_done).Run();
    return;
  }

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  DCHECK(web_app);

  for (const auto& protocol_handler : web_app->protocol_handlers()) {
    if (base::Contains(web_app->disallowed_launch_protocols(),
                       protocol_handler.protocol)) {
      continue;
    }

    proto::ProtocolsHandled::Protocol* protocol =
        desired_state.mutable_protocols_handled()->add_protocols();
    protocol->set_protocol(protocol_handler.protocol);
    protocol->set_url(protocol_handler.url.spec());
  }
  std::move(configure_done).Run();
}

void ProtocolHandlingSubManager::Execute(
    const webapps::AppId& app_id,
    const std::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  // No-op if both the desired and current states are empty.
  if (!desired_state.has_protocols_handled() &&
      !current_state.has_protocols_handled()) {
    std::move(callback).Run();
    return;
  }

  CHECK_OS_INTEGRATION_ALLOWED();

  // Handle unregistration case.
  if (current_state.has_protocols_handled() &&
      !desired_state.has_protocols_handled()) {
    UnregisterProtocolHandlersWithOs(
        app_id, profile_path_,
        base::BindOnce(&RecordProtocolHandlingResult,
                       "WebApp.ProtocolHandlers.Unregistration.Result")
            .Then(std::move(callback)));
    return;
  }

  // Handle registration case.
  if (!current_state.has_protocols_handled() &&
      desired_state.has_protocols_handled()) {
    RegisterProtocolHandlersWithOs(
        app_id, provider_->registrar_unsafe().GetAppShortName(app_id),
        profile_path_, GetApprovedProtocolHandlers(desired_state),
        base::BindOnce(&RecordProtocolHandlingResult,
                       "WebApp.ProtocolHandlers.Registration.Result")
            .Then(std::move(callback)));
    return;
  }

  // If an update is required, then both states should have protocol handling
  // information.
  DCHECK(desired_state.has_protocols_handled());
  DCHECK(current_state.has_protocols_handled());

  // Protocol Handling Diff detection.
  std::string desired_protocols_handled =
      desired_state.protocols_handled().SerializeAsString();
  std::string current_protocols_handled =
      current_state.protocols_handled().SerializeAsString();
  if (desired_protocols_handled == current_protocols_handled) {
    std::move(callback).Run();
    return;
  }

  // If either a registration or unregistration is not done, then an update
  // needs to happen.
  auto register_and_complete =
      base::BindOnce(&RegisterProtocolHandlersWithOs, app_id,
                     provider_->registrar_unsafe().GetAppShortName(app_id),
                     profile_path_, GetApprovedProtocolHandlers(desired_state),
                     base::BindOnce(&RecordProtocolHandlingResult,
                                    "WebApp.ProtocolHandlers.Update.Result")
                         .Then(std::move(callback)));
  UnregisterProtocolHandlersWithOs(
      app_id, profile_path_,
      base::IgnoreArgs<Result>(std::move(register_and_complete)));
}

void ProtocolHandlingSubManager::ForceUnregister(const webapps::AppId& app_id,
                                                 base::OnceClosure callback) {
  UnregisterProtocolHandlersWithOs(
      app_id, profile_path_,
      base::BindOnce(&RecordProtocolHandlingResult,
                     "WebApp.ProtocolHandlers.Unregistration.Result")
          .Then(std::move(callback)));
}

}  // namespace web_app
