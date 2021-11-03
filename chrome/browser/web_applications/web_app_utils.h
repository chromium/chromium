// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace web_app {

// These functions return true if the WebApp System or its subset is allowed
// for a given profile.
// |profile| can be original profile or its secondary off-the-record profile.
// Returns false if |profile| is nullptr.
//
// Is main WebApp System allowed (WebAppProvider exists):
bool AreWebAppsEnabled(const Profile* profile);
// Is user allowed to install web apps from UI:
bool AreWebAppsUserInstallable(Profile* profile);
// Can system web apps be installed:
bool AreSystemWebAppsSupported();

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

// Returns true if the WebApp should have `web_app::WebAppChromeOsData()`.
bool IsChromeOsDataMandatory();

// Returns true if sync should install web apps locally by default.
bool AreAppsLocallyInstalledBySync();

// Returns whether `old_handlers` contains all handlers in `new_handlers`.
// Useful for determining whether the user's approval of the API needs to be
// reset during app update.
bool AreNewFileHandlersASubsetOfOld(const apps::FileHandlers& old_handlers,
                                    const apps::FileHandlers& new_handlers);

// Returns true if `new_handlers` are effectively the same or less broad than
// the file handlers for PWAs installed under the same origin as `url` in
// `profile`. In other words, if `new_handlers` would not change the text
// returned by `GetFileHandlersForAllWebAppsWithOrigin()`, then this will return
// true, otherwise false.
bool AreFileHandlersAlreadyRegistered(Profile* profile,
                                      const GURL& url,
                                      const apps::FileHandlers& new_handlers);

// Returns all file handlers associated with any apps at the origin of `url`, in
// the `profile`. This is not limited to a particular app's scope because it's
// used for display in permissions contexts, and permissions are origin-bound.
apps::FileHandlers GetFileHandlersForAllWebAppsWithOrigin(Profile* profile,
                                                          const GURL& url);

// Returns a display-ready string that holds all file type associations handled
// by all installed apps that are scoped under the origin of `url`. This means
// that if the provided URL is example.com/app/, the returned value will also
// include file types for example.com/alternate_app/. On Linux, where files are
// associated via MIME types, this will return MIME types like "text/plain,
// image/png". On all other platforms, where files are associated via file
// extensions, this will return capitalized file extensions with the period
// truncated, like "TXT, PNG". `found_multiple`, when non-null, will be set to
// indicate whether the returned string is a list (false indicates it's a single
// object).
// TODO(estade): remove this when kDesktopPWAsFileHandlingSettingsGated is
// default.
std::u16string GetFileTypeAssociationsHandledByWebAppsForDisplay(
    Profile* profile,
    const GURL& url,
    bool* found_multiple = nullptr);

// Returns a display-ready string that holds all file type associations handled
// by the app referenced by `app_id`. On Linux, where files are associated via
// MIME types, this will return MIME types like "text/plain, image/png". On all
// other platforms, where files are associated via file extensions, this will
// return capitalized file extensions with the period truncated, like "TXT,
// PNG". `found_multiple`, when non-null, will be set to indicate whether the
// returned string is a list (false indicates it's a single object).
std::u16string GetFileTypeAssociationsHandledByWebAppForDisplay(
    Profile* profile,
    const AppId& app_id,
    bool* found_multiple = nullptr);

// Updates the approved or disallowed protocol list for the given app. If
// necessary, it also updates the protocol registration with the OS.
void PersistProtocolHandlersUserChoice(
    Profile* profile,
    const AppId& app_id,
    const GURL& protocol_url,
    bool allowed,
    base::OnceClosure update_finished_callback);

// Updates the File Handling API approval state for the given app. If
// necessary, it also updates the registration with the OS.
void PersistFileHandlersUserChoice(Profile* profile,
                                   const AppId& app_id,
                                   bool allowed,
                                   base::OnceClosure update_finished_callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The kLacrosPrimary and kWebAppsCrosapi features are each independently
// sufficient to enable the web apps Crosapi (used for Lacros web app
// management).
bool IsWebAppsCrosapiEnabled();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Enables System Web Apps so we can test SWA features in Lacros, even we don't
// have actual SWAs in Lacros.
void EnableSystemWebAppsInLacrosForTesting();
#endif

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
