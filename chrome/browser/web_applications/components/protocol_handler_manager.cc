// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/protocol_handler_manager.h"

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_protocol_handler_registration.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

namespace web_app {

ProtocolHandlerManager::ProtocolHandlerManager(Profile* profile)
    : app_registrar_(nullptr), profile_(profile) {}

ProtocolHandlerManager::~ProtocolHandlerManager() = default;

void ProtocolHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  app_registrar_ = registrar;
}

void ProtocolHandlerManager::Start() {
  DCHECK(app_registrar_);
}

base::Optional<GURL> ProtocolHandlerManager::TranslateProtocolUrl(
    const AppId& app_id,
    const GURL& protocol_url) const {
  std::vector<ProtocolHandler> handlers = GetAppProtocolHandlers(app_id);

  for (const auto& handler : handlers) {
    if (handler.protocol() == protocol_url.scheme()) {
      return handler.TranslateUrl(protocol_url);
    }
  }

  return base::nullopt;
}

std::vector<ProtocolHandler> ProtocolHandlerManager::GetHandlersFor(
    const std::string& protocol) const {
  std::vector<ProtocolHandler> protocol_handlers;

  for (const auto& app_id : app_registrar_->GetAppIds()) {
    const std::vector<ProtocolHandler> handlers =
        GetAppProtocolHandlers(app_id);
    for (const auto& handler : handlers) {
      if (handler.protocol() == protocol)
        protocol_handlers.push_back(handler);
    }
  }

  return protocol_handlers;
}

std::vector<ProtocolHandler> ProtocolHandlerManager::GetAppProtocolHandlers(
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

void ProtocolHandlerManager::RegisterOsProtocolHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool)> callback) {
  const std::vector<apps::ProtocolHandlerInfo> handlers =
      GetAppProtocolHandlerInfos(app_id);
  RegisterOsProtocolHandlers(app_id, handlers, std::move(callback));
}

void ProtocolHandlerManager::RegisterOsProtocolHandlers(
    const AppId& app_id,
    const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers,
    base::OnceCallback<void(bool)> callback) {
  if (!app_registrar_->IsLocallyInstalled(app_id))
    return;

  if (!protocol_handlers.empty()) {
    RegisterProtocolHandlersWithOs(
        app_id, app_registrar_->GetAppShortName(app_id), profile_,
        protocol_handlers, std::move(callback));
  }
}

void ProtocolHandlerManager::UnregisterOsProtocolHandlers(const AppId& app_id) {
  const std::vector<apps::ProtocolHandlerInfo> handlers =
      GetAppProtocolHandlerInfos(app_id);
  UnregisterOsProtocolHandlers(app_id, handlers);
}

void ProtocolHandlerManager::UnregisterOsProtocolHandlers(
    const AppId& app_id,
    const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
  if (!protocol_handlers.empty())
    UnregisterProtocolHandlersWithOs(app_id, profile_, protocol_handlers);
}

}  // namespace web_app
