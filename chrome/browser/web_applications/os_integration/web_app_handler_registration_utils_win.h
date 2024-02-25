// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_

#include <optional>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

base::CommandLine GetAppLauncherCommand(const webapps::AppId& app_id,
                                        const base::FilePath& app_launcher_path,
                                        const base::FilePath& profile_path);

// Returns the extension required for new installations of |app_id| based on
// current state of duplicate installations of |app_id| in other profiles.
std::wstring GetAppNameExtensionForNextInstall(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path);

base::FilePath GetAppSpecificLauncherFilename(const std::wstring& app_name);

// See https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for
// the allowed characters in a prog_id. Since the prog_id is stored in the
// Windows registry, the mapping between a given profile+app_id and a prog_id
// can not be changed.
std::wstring GetProgIdForApp(const base::FilePath& profile_path,
                             const webapps::AppId& app_id);

// Returns the prog_id for a file handler for |app_id| that handles the given
// |file_extensions|. An app's handled file extensions are encoded into its
// distinct prog_id so that apps can customize the displayed type name and icon
// for file types (shown when the app is the type's default handler). File
// extensions should include a leading period, e.g., ".txt". See
// https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for the
// allowed characters in a prog_id. Since the prog_id is stored in the Windows
// registry, the prog_id corresponding to the given profile, app_id, and
// file_extensions combination can't be changed.
std::wstring GetProgIdForAppFileHandler(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    const std::set<std::string>& file_extensions);

// Makes an app-specific copy of chrome_pwa_launcher.exe that lives in the web
// application directory |web_app_path|. Returns path of the launcher file if
// successful, std::nullopt otherwise.
std::optional<base::FilePath> CreateAppLauncherFile(
    const std::wstring& app_name,
    const std::wstring& app_name_extension,
    const base::FilePath& web_app_path);

// Checks if there is an installation of this app in another profile that needs
// to be updated with a profile specific name and executes required update.
void CheckAndUpdateExternalInstallations(const base::FilePath& cur_profile_path,
                                         const webapps::AppId& app_id,
                                         ResultCallback callback);

// Result of file handler registration process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegistrationResult {
  kSuccess = 0,
  kFailToCopyFromGenericLauncher = 1,
  kFailToAddFileAssociation = 2,
  kFailToDeleteExistingRegistration = 3,
  kFailToDeleteFileAssociationsForExistingRegistration = 4,
  kMaxValue = kFailToDeleteFileAssociationsForExistingRegistration
};

// Record UMA metric for the result of file handler registration.
void RecordRegistration(RegistrationResult result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_
