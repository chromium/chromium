// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_manager.h"

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

BookmarkAppShortcutManager::BookmarkAppShortcutManager(Profile* profile)
    : web_app::AppShortcutManager(profile) {}

BookmarkAppShortcutManager::~BookmarkAppShortcutManager() = default;

void BookmarkAppShortcutManager::GetShortcutInfoForApp(
    const web_app::AppId& app_id,
    GetShortcutInfoCallback callback) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->GetInstalledExtension(app_id);
  DCHECK(extension);
  web_app::GetShortcutInfoForApp(extension, profile(), std::move(callback));
}

}  // namespace extensions
