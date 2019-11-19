// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_shortcut_manager.h"

#include "base/callback.h"

namespace web_app {

WebAppShortcutManager::WebAppShortcutManager(Profile* profile)
    : AppShortcutManager(profile) {}

WebAppShortcutManager::~WebAppShortcutManager() = default;

bool WebAppShortcutManager::CanCreateShortcuts() const {
  NOTIMPLEMENTED();
  return false;
}

void WebAppShortcutManager::GetShortcutInfoForApp(
    const AppId& app_id,
    GetShortcutInfoCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace web_app
