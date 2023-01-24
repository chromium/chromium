// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/url_handling_sub_manager.h"

#include <utility>

#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

UrlHandlingSubManager::UrlHandlingSubManager(WebAppRegistrar& registrar)
    : registrar_(registrar) {}

UrlHandlingSubManager::~UrlHandlingSubManager() = default;

void UrlHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_url_handling());

  if (!registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  const WebApp* web_app = registrar_->GetAppById(app_id);

  proto::UrlHandling* url_handling_proto = desired_state.mutable_url_handling();
  for (const auto& url_handler : web_app->url_handlers()) {
    proto::UrlHandling::UrlHandler* url_handler_proto =
        url_handling_proto->add_url_handlers();
    url_handler_proto->set_origin(url_handler.origin.Serialize());
    url_handler_proto->set_has_origin_wildcard(url_handler.has_origin_wildcard);

    for (const std::string& path : url_handler.paths) {
      url_handler_proto->add_paths(path);
    }
    for (const std::string& exclude_path : url_handler.exclude_paths) {
      url_handler_proto->add_exclude_paths(exclude_path);
    }
  }

  std::move(configure_done).Run();
}

void UrlHandlingSubManager::Start() {}

void UrlHandlingSubManager::Shutdown() {}

void UrlHandlingSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  // Not implemented yet.
  std::move(callback).Run();
}

}  // namespace web_app
