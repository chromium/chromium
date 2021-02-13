// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_

#include <string>

#include "base/callback_forward.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

bool IsExtensionBlockedByPolicy(content::BrowserContext* context,
                                const std::string& extension_id);

// Returns whether the extension with |extension_id| is installed regardless of
// disabled/blocked/terminated status.
bool IsExtensionInstalled(content::BrowserContext* context,
                          const std::string& extension_id);

// Returns whether the user has uninstalled an externally installed extension
// with |extension_id|.
bool IsExternalExtensionUninstalled(content::BrowserContext* context,
                                    const std::string& extension_id);

// Waits for extension system ready to run callback.
void OnExtensionSystemReady(content::BrowserContext* context,
                            base::OnceClosure callback);

// Checks if default apps perform new installation.
bool DidDefaultAppsPerformNewInstallation(Profile* profile);

// Returns if the app is managed by extension default apps. This is a hardcoded
// list of default apps for Windows/Linux/MacOS platforms that should be
// migrated from extension to web app. Need to be removed after migration is
// done.
bool IsDefaultAppId(const std::string& app_id);

// Makes WasManagedByDefaultApps return true for testing.
void SetDefaultAppIdForTesting(const char* app_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
