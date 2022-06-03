// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"
#include "base/containers/contains.h"

namespace web_app {

FakeWebAppFileHandlerManager::FakeWebAppFileHandlerManager(Profile* profile)
    : WebAppFileHandlerManager(profile) {
  WebAppFileHandlerManager::DisableOsIntegrationForTesting();
}

FakeWebAppFileHandlerManager::~FakeWebAppFileHandlerManager() = default;

const apps::FileHandlers* FakeWebAppFileHandlerManager::GetAllFileHandlers(
    const AppId& app_id) {
  if (!base::Contains(file_handlers_, app_id))
    return nullptr;

  return &file_handlers_[app_id];
}

void FakeWebAppFileHandlerManager::InstallFileHandler(const AppId& app_id,
                                                      const GURL& action,
                                                      const AcceptMap& accept,
                                                      bool enable) {
  if (!base::Contains(file_handlers_, app_id))
    file_handlers_[app_id] = apps::FileHandlers();

  apps::FileHandler file_handler;
  file_handler.action = action;

  for (const auto& it : accept) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = it.first;
    accept_entry.file_extensions = it.second;
    file_handler.accept.push_back(accept_entry);
  }

  file_handlers_[app_id].push_back(file_handler);

  if (enable)
    EnableAndRegisterOsFileHandlers(app_id);
}

}  // namespace web_app
