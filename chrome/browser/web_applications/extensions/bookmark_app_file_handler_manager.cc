// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_file_handler_manager.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"

namespace extensions {

BookmarkAppFileHandlerManager::BookmarkAppFileHandlerManager(Profile* profile)
    : web_app::FileHandlerManager(profile) {}

BookmarkAppFileHandlerManager::~BookmarkAppFileHandlerManager() = default;

const std::vector<apps::FileHandlerInfo>*
BookmarkAppFileHandlerManager::GetFileHandlers(const web_app::AppId& app_id) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(app_id);
  return FileHandlers::GetFileHandlers(extension);
}

}  // namespace extensions
