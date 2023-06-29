// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"

#include "base/containers/contains.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

using custom_handlers::ProtocolHandler;

namespace web_app {

WebAppProtocolHandlerManager::WebAppProtocolHandlerManager(Profile* profile)
    : profile_(profile) {}

WebAppProtocolHandlerManager::~WebAppProtocolHandlerManager() = default;

void WebAppProtocolHandlerManager::SetProvider(
    base::PassKey<OsIntegrationManager>,
    WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppProtocolHandlerManager::Start() {
  DCHECK(provider_);
}

absl::optional<GURL> WebAppProtocolHandlerManager::TranslateProtocolUrl(
    const AppId& app_id,
    const GURL& protocol_url) const {
  std::vector<ProtocolHandler> handlers = GetAppProtocolHandlers(app_id);

  for (const auto& handler : handlers) {
    if (handler.protocol() == protocol_url.scheme()) {
      return handler.TranslateUrl(protocol_url);
    }
  }

  return absl::nullopt;
}

std::vector<apps::ProtocolHandlerInfo>
WebAppProtocolHandlerManager::GetAppProtocolHandlerInfos(
    const std::string& app_id) const {
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);

  if (!web_app)
    return {};

  std::vector<apps::ProtocolHandlerInfo> protocol_handlers_infos;
  for (const auto& handler_info : web_app->protocol_handlers()) {
    if (!base::Contains(web_app->disallowed_launch_protocols(),
                        handler_info.protocol)) {
      protocol_handlers_infos.push_back(handler_info);
    }
  }

  return protocol_handlers_infos;
}

std::vector<ProtocolHandler>
WebAppProtocolHandlerManager::GetAppProtocolHandlers(
    const AppId& app_id) const {
  std::vector<apps::ProtocolHandlerInfo> infos =
      GetAppProtocolHandlerInfos(app_id);

  std::vector<ProtocolHandler> protocol_handlers;
  for (const auto& info : infos) {
    ProtocolHandler handler = ProtocolHandler::CreateWebAppProtocolHandler(
        info.protocol, GURL(info.url), app_id);
    protocol_handlers.push_back(handler);
  }

  return protocol_handlers;
}

std::vector<ProtocolHandler>
WebAppProtocolHandlerManager::GetAllowedHandlersForProtocol(
    const std::string& protocol) const {
  std::vector<ProtocolHandler> protocol_handlers;

  for (const WebApp& web_app : provider_->registrar_unsafe().GetApps()) {
    AppId app_id = web_app.app_id();

    if (!provider_->registrar_unsafe().IsAllowedLaunchProtocol(app_id,
                                                               protocol)) {
      continue;
    }

    for (const auto& info : web_app.protocol_handlers()) {
      if (info.protocol != protocol)
        continue;

      ProtocolHandler handler = ProtocolHandler::CreateWebAppProtocolHandler(
          info.protocol, GURL(info.url), app_id);
      protocol_handlers.push_back(handler);
    }
  }

  return protocol_handlers;
}

std::vector<ProtocolHandler>
WebAppProtocolHandlerManager::GetDisallowedHandlersForProtocol(
    const std::string& protocol) const {
  std::vector<ProtocolHandler> protocol_handlers;

  for (const WebApp& web_app : provider_->registrar_unsafe().GetApps()) {
    AppId app_id = web_app.app_id();

    if (!provider_->registrar_unsafe().IsDisallowedLaunchProtocol(app_id,
                                                                  protocol)) {
      continue;
    }

    for (const auto& info : web_app.protocol_handlers()) {
      if (info.protocol != protocol)
        continue;

      ProtocolHandler handler = ProtocolHandler::CreateWebAppProtocolHandler(
          info.protocol, GURL(info.url), app_id);
      protocol_handlers.push_back(handler);
    }
  }

  return protocol_handlers;
}

void WebAppProtocolHandlerManager::RegisterOsProtocolHandlers(
    const AppId& app_id,
    ResultCallback callback) {
  if (!provider_->registrar_unsafe().IsLocallyInstalled(app_id)) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  const std::vector<apps::ProtocolHandlerInfo> handlers =
      GetAppProtocolHandlerInfos(app_id);
  RegisterProtocolHandlersWithOs(
      app_id, provider_->registrar_unsafe().GetAppShortName(app_id),
      profile_->GetPath(), handlers, std::move(callback));
}

void WebAppProtocolHandlerManager::UnregisterOsProtocolHandlers(
    const AppId& app_id,
    base::OnceCallback<void(Result)> callback) {
  UnregisterProtocolHandlersWithOs(app_id, profile_->GetPath(),
                                   std::move(callback));
}

}  // namespace web_app
