// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_file_handler_manager.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/file_handler.h"

namespace web_app {

WebAppFileHandlerManager::WebAppFileHandlerManager(Profile* profile)
    : FileHandlerManager(profile) {}

WebAppFileHandlerManager::~WebAppFileHandlerManager() = default;

const apps::FileHandlers* WebAppFileHandlerManager::GetAllFileHandlers(
    const AppId& app_id) {
  const WebApp* web_app = registrar()->GetAppById(app_id);
  return web_app ? &web_app->file_handlers() : nullptr;
}

}  // namespace web_app
