// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"

namespace web_app {

base::CommandLine GetAppLauncherCommand(const AppId& app_id,
                                        const base::FilePath& app_launcher_path,
                                        const base::FilePath& profile_path);

// Returns the extension required for new installations of |app_id| based on
// current state of duplicate installations of |app_id| in other profiles.
std::wstring GetAppNameExtensionForNextInstall(
    const AppId& app_id,
    const base::FilePath& profile_path);

base::FilePath GetAppSpecificLauncherFilename(const std::wstring& app_name);

// See https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for
// the allowed characters in a prog_id. Since the prog_id is stored in the
// Windows registry, the mapping between a given profile+app_id and a prog_id
// can not be changed.
std::wstring GetProgIdForApp(const base::FilePath& profile_path,
                             const AppId& app_id);

// Makes an app-specific copy of chrome_pwa_launcher.exe that lives in the web
// application directory |web_app_path|. Returns path of the launcher file if
// successful, base::nullopt otherwise.
base::Optional<base::FilePath> CreateAppLauncherFile(
    const std::wstring& app_name,
    const std::wstring& app_name_extension,
    const base::FilePath& web_app_path);

// Checks if there is an installation of this app in another profile that needs
// to be updated with a profile specific name and executes required update.
void CheckAndUpdateExternalInstallations(const base::FilePath& cur_profile_path,
                                         const AppId& app_id,
                                         base::OnceCallback<void()> callback);

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

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HANDLER_REGISTRATION_UTILS_WIN_H_
