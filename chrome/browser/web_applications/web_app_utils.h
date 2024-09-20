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

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"

class GURL;
class Profile;

namespace apps {
enum class LaunchContainer;
enum class RunOnOsLoginMode;
}  // namespace apps

namespace base {
class FilePath;
}

namespace content {
class RenderFrameHost;
class BrowserContext;
}

namespace web_app {

namespace error_page {
// |alternative_error_page_params| dictionary key values in the
// |AlternativeErrorPageOverrideInfo| mojom struct.
const char kMessage[] = "web_app_error_page_message";
const char kAppShortName[] = "app_short_name";
const char kIconUrl[] = "icon_url";
const char kSupplementaryIcon[] = "supplementary_icon";

// This must match the HTML element id of the svg to show as a supplementary
// icon on the default offline error page.
const char16_t kOfflineIconId[] = u"offlineIcon";
}  // namespace error_page

// These functions return true if the WebApp System or its subset is allowed
// for a given profile.
// |profile| can be original profile or its secondary off-the-record profile.
// Returns false if |profile| is nullptr.
//
// Is main WebApp System allowed (WebAppProvider exists):
bool AreWebAppsEnabled(Profile* profile);
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
    const webapps::AppId& app_id);

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
                                                 const webapps::AppId& app_id);

// As above, but returns the extensions handled by the app as a vector of
// strings.
std::vector<std::u16string> TransformFileExtensionsForDisplay(
    const std::set<std::string>& extensions);

// Check if only |specified_sources| exist in the |sources|
bool HasAnySpecifiedSourcesAndNoOtherSources(
    WebAppManagementTypes sources,
    WebAppManagementTypes specified_sources);

// Check if all types of |sources| for |app_id| are uninstallable by the user.
bool CanUserUninstallWebApp(const webapps::AppId& app_id,
                            WebAppManagementTypes sources);

// Extracts app_id from chrome://app-settings/<app-id> URL path.
webapps::AppId GetAppIdFromAppSettingsUrl(const GURL& url);

// Returns whether `url` is in scope `scope`. False if scope is invalid.
bool IsInScope(const GURL& url, const GURL& scope);

// Returns whether the `login_mode` should force a start at OS login.
bool IsRunOnOsLoginModeEnabledForAutostart(RunOnOsLoginMode login_mode);

#if BUILDFLAG(IS_CHROMEOS)
// Web apps crosapi (used for Lacros web app management) will be enabled if
// Lacros is the primary browser.
bool IsWebAppsCrosapiEnabled();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Allow user web apps on profiles other than the main profile.
void SetSkipMainProfileCheckForTesting(bool skip_check);

bool IsMainProfileCheckSkippedForTesting();

// The storage partitions' domain name for the experimental web app isolation.
// TODO(crbug.com/40260833): use a better domain name, or maybe use a unique
// domain for each app.
constexpr char kExperimentalWebAppStorageParitionDomain[] = "goldfish";
#endif

constexpr char kAppSettingsPageEntryPointsHistogramName[] =
    "WebApp.AppSettingsPage.EntryPoints";

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppSettingsPageEntryPoint enum listing in
// tools/metrics/histograms/metadata/webapps/enums.xml.
enum class AppSettingsPageEntryPoint {
  kPageInfoView = 0,
  kChromeAppsPage = 1,
  kBrowserCommand = 2,
  kSubAppsInstallPrompt = 3,
  kNotificationSettingsButton = 4,
  kSiteDataDialog = 5,
  kMaxValue = kSiteDataDialog,
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

// Converts RunOnOsLoginMode from RunOnOsLoginMode to
// apps::RunOnOsLoginMode.
apps::RunOnOsLoginMode ConvertOsLoginMode(RunOnOsLoginMode login_mode);

const char* IconsDownloadedResultToString(IconsDownloadedResult result);

content::mojom::AlternativeErrorPageOverrideInfoPtr ConstructWebAppErrorPage(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context,
    std::u16string message,
    std::u16string supplementary_icon);

bool IsValidScopeForLinkCapturing(const GURL& scope);

// TODO(http://b/331208955): Remove after migration.
// Returns whether |app_id| will soon refer to a system web app given |sources|.
bool WillBeSystemWebApp(const webapps::AppId& app_id,
                        WebAppManagementTypes sources);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
