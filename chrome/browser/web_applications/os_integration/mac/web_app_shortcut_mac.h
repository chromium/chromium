// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_MAC_H_

#include <string>

#include "base/files/file_path.h"

namespace web_app {

// Disable app shims in tests if the shortcut folder is not set.
// Because shims created in ~/Applications will not be cleaned up.
bool AppShimCreationAndLaunchDisabledForTest();

// Remove the specified app from the OS login item list.
void RemoveAppShimFromLoginItems(const std::string& app_id);

// Returns the bundle identifier for an app. If |profile_path| is unset, then
// the returned bundle id will be profile-agnostic.
std::string GetBundleIdentifierForShim(
    const std::string& app_id,
    const base::FilePath& profile_path = base::FilePath());

// Returns true when running on version of macOS that can perform code signing
// at runtime and the UseAdHocSigningForWebAppShims feature is enabled.
bool UseAdHocSigningForWebAppShims();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_MAC_H_
