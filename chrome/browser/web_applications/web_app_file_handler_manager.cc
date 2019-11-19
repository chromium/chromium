// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_file_handler_manager.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

WebAppFileHandlerManager::WebAppFileHandlerManager(Profile* profile)
    : FileHandlerManager(profile) {}

WebAppFileHandlerManager::~WebAppFileHandlerManager() = default;

const std::vector<apps::FileHandlerInfo>*
WebAppFileHandlerManager::GetFileHandlers(const AppId& app_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace web_app
