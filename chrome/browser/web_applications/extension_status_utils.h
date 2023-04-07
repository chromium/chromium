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
namespace testing {
// Because the allow-list needs to stick around for a while, this flag makes it
// easy for us to continue testing chrome apps on Windows/Mac/Linux without
// having to jump through hurdles to add ids to the allow-list.
// TODO(http://b/268221237): Remove this & tests on WML once allow-list is
// removed.
extern bool g_enable_chrome_apps_for_testing;
}  // namespace testing

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

// Clears any recording of |extension_id| as being an externally installed
// extension uninstalled by the user. Returns whether any change was made.
bool ClearExternalExtensionUninstalled(content::BrowserContext* context,
                                       const std::string& extension_id);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
// Returns whether |extension_id| is a Chrome App and should be blocked by the
// Chrome Apps Deprecation. Policy installed Chrome Apps are still allowed, and
// all apps are allowed if the deprecation feature flag is not enabled.
bool IsExtensionUnsupportedDeprecatedApp(content::BrowserContext* context,
                                         const std::string& extension_id);
#endif

// Waits for extension system ready to run callback.
void OnExtensionSystemReady(content::BrowserContext* context,
                            base::OnceClosure callback);

// Checks if default apps perform new installation.
bool DidPreinstalledAppsPerformNewInstallation(Profile* profile);

// Returns if the app is managed by extension default apps. This is a hardcoded
// list of default apps for Windows/Linux/MacOS platforms that should be
// migrated from extension to web app. Need to be removed after migration is
// done.
bool IsPreinstalledAppId(const std::string& app_id);

// Makes WasManagedByPreinstalledApps return true for testing.
void SetPreinstalledAppIdForTesting(const char* app_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
