// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/protocol_handler_manager.h"

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

ProtocolHandlerManager::ProtocolHandlerManager(Profile* profile)
    : app_registrar_(nullptr) {}

ProtocolHandlerManager::~ProtocolHandlerManager() = default;

void ProtocolHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  app_registrar_ = registrar;
}

void ProtocolHandlerManager::Start() {
  DCHECK(app_registrar_);
}

std::vector<apps::ProtocolHandlerInfo>
ProtocolHandlerManager::GetAppProtocolHandlerInfos(
    const std::string& app_id) const {
  // TODO(crbug.com/1019239): Implement OS-specific protocol handler
  // registration.
  NOTIMPLEMENTED();
  return std::vector<apps::ProtocolHandlerInfo>();
}

void ProtocolHandlerManager::RegisterOsProtocolHandlers(const AppId& app_id) {
  // TODO(crbug.com/1019239): Implement OS-specific protocol handler
  // registration.
  NOTIMPLEMENTED();
}

void ProtocolHandlerManager::RegisterOsProtocolHandlers(
    const AppId& app_id,
    const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
  // TODO(crbug.com/1019239): Implement OS-specific protocol handler
  // registration.
  NOTIMPLEMENTED();
}

void ProtocolHandlerManager::UnregisterOsProtocolHandlers(const AppId& app_id) {
  // TODO(crbug.com/1019239): Implement OS-specific protocol handler
  // registration.
  NOTIMPLEMENTED();
}

void ProtocolHandlerManager::UnregisterOsProtocolHandlers(
    const AppId& app_id,
    const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
  // TODO(crbug.com/1019239): Implement OS-specific protocol handler
  // registration.
  NOTIMPLEMENTED();
}

}  // namespace web_app
