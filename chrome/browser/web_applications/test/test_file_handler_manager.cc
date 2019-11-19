// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_file_handler_manager.h"

namespace web_app {

TestFileHandlerManager::TestFileHandlerManager()
    : FileHandlerManager(nullptr) {}

TestFileHandlerManager::~TestFileHandlerManager() = default;

const std::vector<apps::FileHandlerInfo>*
TestFileHandlerManager::GetFileHandlers(const AppId& app_id) {
  if (!base::Contains(file_handlers_, app_id))
    return nullptr;

  return &file_handlers_[app_id];
}

void TestFileHandlerManager::InstallFileHandler(
    const AppId& app_id,
    const GURL& action,
    std::vector<std::string> accepts) {
  if (!base::Contains(file_handlers_, app_id))
    file_handlers_[app_id] = std::vector<apps::FileHandlerInfo>();

  apps::FileHandlerInfo info;
  info.id = action.spec();
  info.verb = apps::file_handler_verbs::kOpenWith;

  for (const auto& accept : accepts) {
    if (accept[0] == '.')
      info.extensions.insert(accept.substr(1));
    else
      info.types.insert(accept);
  }

  file_handlers_[app_id].push_back(info);
}

}  // namespace web_app
