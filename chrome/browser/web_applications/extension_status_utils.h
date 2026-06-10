// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

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

// Returns whether the extension with `extension_id` is force installed by
// policy, and fills `reason` (if non-null) with expository text.
bool IsExtensionForceInstalled(content::BrowserContext* context,
                               const std::string& extension_id,
                               std::u16string* reason);

// Returns whether the extension with `extension_id` was installed as a default
// extension/app.
bool IsExtensionDefaultInstalled(content::BrowserContext* context,
                                 const std::string& extension_id);

// Returns whether the user has uninstalled an externally installed extension
// with |extension_id|.
bool IsExternalExtensionUninstalled(content::BrowserContext* context,
                                    const std::string& extension_id);

// Waits for extension system ready to run callback.
void OnExtensionSystemReady(content::BrowserContext* context,
                            base::OnceClosure callback);

// Checks if default apps perform new installation.
bool DidPreinstalledAppsPerformNewInstallation(Profile* profile);

// Returns if the app is managed by extension default apps.
bool IsPreinstalledAppId(const std::string& app_id);

// Makes WasManagedByPreinstalledApps return true for testing.
void SetPreinstalledAppIdForTesting(const char* app_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
