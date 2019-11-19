// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_finalizer_utils.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"

namespace extensions {

namespace {

#if !defined(OS_CHROMEOS)
bool CanOsAddDesktopShortcuts() {
#if defined(OS_LINUX) || defined(OS_WIN)
  return true;
#else
  return false;
#endif
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace

bool CanBookmarkAppCreateOsShortcuts() {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void BookmarkAppCreateOsShortcuts(
    Profile* profile,
    const Extension* extension,
    bool add_to_desktop,
    base::OnceCallback<void(bool created_shortcuts)> callback) {
  DCHECK(CanBookmarkAppCreateOsShortcuts());
#if !defined(OS_CHROMEOS)
  web_app::ShortcutLocations creation_locations;
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  creation_locations.in_quick_launch_bar = false;

  if (CanOsAddDesktopShortcuts())
    creation_locations.on_desktop = add_to_desktop;

  Profile* current_profile = profile->GetOriginalProfile();
  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                           creation_locations, current_profile, extension,
                           std::move(callback));
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace extensions
