// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_protocol_handler_manager.h"

#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppProtocolHandlerManager::WebAppProtocolHandlerManager(Profile* profile)
    : ProtocolHandlerManager(profile) {}

WebAppProtocolHandlerManager::~WebAppProtocolHandlerManager() = default;

std::vector<apps::ProtocolHandlerInfo>
WebAppProtocolHandlerManager::GetAppProtocolHandlerInfos(
    const std::string& app_id) const {
  const WebApp* web_app =
      app_registrar_->AsWebAppRegistrar()->GetAppById(app_id);

  if (!web_app)
    return {};

  return web_app->protocol_handlers();
}

}  // namespace web_app
