// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_

#include <stddef.h>

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace web_app {

class WebAppProvider;

namespace default_offline {
// |alternative_error_page_params| dictionary key values in the
// |AlternativeErrorPageOverrideInfo| mojom struct.
const char kMessage[] = "web_app_default_offline_message";
const char kAppShortName[] = "app_short_name";
const char kIconUrl[] = "icon_url";
}  // namespace default_offline

// These functions return true if the WebApp System or its subset is allowed
// for a given profile.
// |profile| can be original profile or its secondary off-the-record profile.
// Returns false if |profile| is nullptr.
//
// Is main WebApp System allowed (WebAppProvider exists):
bool AreWebAppsEnabled(const Profile* profile);
// Is user allowed to install web apps from UI:
bool AreWebAppsUserInstallable(Profile* profile);

// Get BrowserContext to use for a WebApp KeyedService creation.
content::BrowserContext* GetBrowserContextForWebApps(
    content::BrowserContext* context);
content::BrowserContext* GetBrowserContextForWebAppMetrics(
    content::BrowserContext* context);

// Returns a root directory for all Web Apps themed data.
//
// All the related directory getters always require |web_apps_root_directory| as
// a first argument to avoid directory confusions.
base::FilePath GetWebAppsRootDirectory(Profile* profile);

// Returns a directory to store local cached manifest resources in
// OS-independent manner. Use GetManifestResourcesDirectoryForApp function to
// get per-app manifest resources directory.
//
// To store OS-specific integration data, use
// GetOsIntegrationResourcesDirectoryForApp declared in web_app_shortcut.h.
base::FilePath GetManifestResourcesDirectory(
    const base::FilePath& web_apps_root_directory);
base::FilePath GetManifestResourcesDirectory(Profile* profile);

// Returns per-app directory name to store manifest resources.
base::FilePath GetManifestResourcesDirectoryForApp(
    const base::FilePath& web_apps_root_directory,
    const AppId& app_id);

base::FilePath GetWebAppsTempDirectory(
    const base::FilePath& web_apps_root_directory);

// The return value (profile categories) are used to report metrics. They are
// persisted to logs and should not be renamed. If new names are added, update
// tool/metrics/histograms/histograms.xml: "SystemWebAppProfileCategory".
std::string GetProfileCategoryForLogging(Profile* profile);

// Returns true if the WebApp should have `WebAppChromeOsData()`.
bool IsChromeOsDataMandatory();

// Returns true if sync should install web apps locally by default.
bool AreAppsLocallyInstalledBySync();

// Returns whether `old_handlers` contains all handlers in `new_handlers`.
// Useful for determining whether the user's approval of the API needs to be
// reset during app update.
bool AreNewFileHandlersASubsetOfOld(const apps::FileHandlers& old_handlers,
                                    const apps::FileHandlers& new_handlers);

// Returns a display-ready string that holds all file type associations handled
// by the app referenced by `app_id`, as well as the number if items in the
// list. This will return capitalized file extensions with the period truncated,
// like "TXT, PNG". Note that on Linux, the files must actually match both the
// specified MIME types as well as the specified file extensions, so this list
// of extensions is an incomplete picture (subset) of which file types will be
// accepted.
std::tuple<std::u16string, size_t /*count*/>
GetFileTypeAssociationsHandledByWebAppForDisplay(Profile* profile,
                                                 const AppId& app_id);

// As above, but returns the extensions handled by the app as a vector of
// strings.
std::vector<std::u16string> TransformFileExtensionsForDisplay(
    const std::set<std::string>& extensions);

// Check if only |specified_sources| exist in the |sources|
bool HasAnySpecifiedSourcesAndNoOtherSources(WebAppSources sources,
                                             WebAppSources specified_sources);

// Check if all types of |sources| are uninstallable by the user.
bool CanUserUninstallWebApp(WebAppSources sources);

// Extracts app_id from chrome://app-settings/<app-id> URL path.
AppId GetAppIdFromAppSettingsUrl(const GURL& url);

// Check if |url|'s path is an installed web app.
bool HasAppSettingsPage(Profile* profile, const GURL& url);

// Returns whether `url` is in scope `scope`. False if scope is invalid.
bool IsInScope(const GURL& url, const GURL& scope);

#if BUILDFLAG(IS_CHROMEOS)
// The kLacrosPrimary and kWebAppsCrosapi features are each independently
// sufficient to enable the web apps Crosapi (used for Lacros web app
// management).
bool IsWebAppsCrosapiEnabled();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Allow user web apps on profiles other than the main profile.
void SetSkipMainProfileCheckForTesting(bool skip_check);

bool IsMainProfileCheckSkippedForTesting();
#endif

constexpr char kAppSettingsPageEntryPointsHistogramName[] =
    "WebApp.AppSettingsPage.EntryPoints";

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppSettingsPageEntryPoint enum listing in
// tools/metrics/histograms/enums.xml.
enum class AppSettingsPageEntryPoint {
  kPageInfoView = 0,
  kChromeAppsPage = 1,
  kMaxValue = kChromeAppsPage,
};

// When user_display_mode indicates a user preference for opening in
// a browser tab, we open in a browser tab. If the developer has specified
// the app should utilize more advanced display modes and/or fallback chain,
// attempt honor those preferences. Otherwise, we open in a standalone
// window (for app_display_mode 'standalone' or 'fullscreen'), or a minimal-ui
// window (for app_display_mode 'browser' or 'minimal-ui').
//
// |is_isolated| overrides browser display mode for Isolated Web Apps because
// they can't be open as a tab.
DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    mojom::UserDisplayMode user_display_mode,
    bool is_isolated);

apps::LaunchContainer ConvertDisplayModeToAppLaunchContainer(
    DisplayMode display_mode);

std::string RunOnOsLoginModeToString(RunOnOsLoginMode mode);

// Converts RunOnOsLoginMode from RunOnOsLoginMode to
// apps::RunOnOsLoginMode.
apps::RunOnOsLoginMode ConvertOsLoginMode(RunOnOsLoginMode login_mode);

const char* IconsDownloadedResultToString(IconsDownloadedResult result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
