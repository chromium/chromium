// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"

class GURL;
class Profile;
#if BUILDFLAG(IS_CHROMEOS)
enum class SystemWebAppType;
#endif  // BUILDFLAG(IS_CHROMEOS)

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

// These functions return true if the WebAppProvider is allowed
// for a given profile. This does not consider 'original' profiles. Returns
// false if |profile| is off-the-record or nullptr.
//
// Note: For ChromeOS guest profiles, this instead returns 'true' if the profile
// is off-the-record, and 'false' if it is not (as the user guest profile is
// hard-coded as OTR).
bool AreWebAppsEnabled(Profile* profile);

// Is user allowed to install web apps from UI:
bool AreWebAppsUserInstallable(Profile* profile);

// Get BrowserContext to use for a WebApp KeyedService creation. This will
// return a `nullptr` if `AreWebAppsEnabled` returns false for the given
// profile of `context`.
// Note: On ChromeOS only, if web apps are disabled for the profile of the
// `context`, then this will consider the profile's original profile to support
// the system web app implementation.
// TODO(https://crbug.com/384063076): Stop returning for profiles on ChromeOS
// where `AreWebAppsEnabled` returns `false`.
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

// Checks whether |policy_id| specifies a Chrome App.
bool IsChromeAppPolicyId(std::string_view policy_id);

#if BUILDFLAG(IS_CHROMEOS)
// Checks whether |policy_id| specifies an Arc App.
bool IsArcAppPolicyId(std::string_view policy_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Checks whether |policy_id| specifies a Web App.
bool IsWebAppPolicyId(std::string_view policy_id);

// TODO(https://crbug.com/411013748) Move WebApp utils to WebAppPolicyManager
#if BUILDFLAG(IS_CHROMEOS)
// Checks whether |policy_id| specifies a System Web App.
bool IsSystemWebAppPolicyId(std::string_view policy_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Checks whether |policy_id| specifies a Preinstalled Web App.
bool IsPreinstalledWebAppPolicyId(std::string_view policy_id);

// Checks whether |policy_id| specifies an Isolated Web App.
bool IsIsolatedWebAppPolicyId(std::string_view policy_id);

std::vector<std::string> GetPolicyIds(Profile* profile, const WebApp& web_app);

#if BUILDFLAG(IS_CHROMEOS)
// Maps `SystemWebAppType` to a policy id. Returns the associated policy id.
// Returns std::nullopt for apps not included in official builds.
std::optional<std::string_view> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType swa_type);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns the policy ID for a given preinstalled web app ID. Note that not all
// preinstalled web apps are supposed to have a policy ID (currently we only
// support EDU apps) - in all other cases this will return std::nullopt.
std::optional<std::string_view> GetPolicyIdForPreinstalledWebApp(
    std::string_view preinstalled_web_app_id);

void SetPreinstalledWebAppsMappingForTesting(
    std::optional<base::flat_map<std::string_view, std::string_view>>
        preinstalled_web_apps_mapping_for_testing);

// Returns whether `url` is in scope `scope`. False if scope is invalid.
bool IsInScope(const GURL& url, const GURL& scope);

// Returns whether the `login_mode` should force a start at OS login.
bool IsRunOnOsLoginModeEnabledForAutostart(RunOnOsLoginMode login_mode);

inline constexpr char kAppSettingsPageEntryPointsHistogramName[] =
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

// Resets all content settings for the given `app_scope` to their default
// values.
void ResetAllContentSettingsForWebApp(Profile* profile, const GURL& app_scope);

// TODO(http://b/331208955): Remove after migration.
// Returns whether |app_id| will soon refer to a system web app given |sources|.
bool WillBeSystemWebApp(const webapps::AppId& app_id,
                        WebAppManagementTypes sources);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
