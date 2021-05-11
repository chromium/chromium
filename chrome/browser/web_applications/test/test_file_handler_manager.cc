// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "base/containers/contains.h"

namespace web_app {

TestFileHandlerManager::TestFileHandlerManager(Profile* profile)
    : FileHandlerManager(profile) {
  FileHandlerManager::DisableOsIntegrationForTesting();
}

TestFileHandlerManager::~TestFileHandlerManager() = default;

const apps::FileHandlers* TestFileHandlerManager::GetAllFileHandlers(
    const AppId& app_id) {
  if (!base::Contains(file_handlers_, app_id))
    return nullptr;

  return &file_handlers_[app_id];
}

void TestFileHandlerManager::InstallFileHandler(const AppId& app_id,
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
