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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// If this method returns true, then |extension_id| will not be launchable.
//
// The eventual goal is that this method should return true for all hosted apps,
// legacy packaged v1 apps, and chrome apps, for all platforms. These are the
// current exceptions:
// (1) Webstore is a hosted app. This is currently used with the kAppsGalleryURL
// switch, and will be replaced by another mechanism.
// (2) There is a feature called kChromeAppsDeprecation that is used by
// developers who want to test chrome apps on non-ChromeOS desktop
// platforms, even though they are targeting deployment only to ChromeOS. This
// requires manually setting command line arguments for chrome, and is not used
// by most/any users in the wild.
// (3) directprint.io and mobilityprint are currently allow-listed. They are in
// the process of migrating.
// (4) This method and callsites are currently not compiled onto ChromeOS.
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
