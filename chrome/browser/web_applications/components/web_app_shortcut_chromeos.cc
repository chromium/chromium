// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  return true;
}

bool DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info) {
  return true;
}

void UpdatePlatformShortcuts(const base::FilePath& web_app_path,
                             const std::u16string& old_app_title,
                             const ShortcutInfo& shortcut_info) {}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  return ShortcutLocations();
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {}

}  // namespace internals

}  // namespace web_app
