// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_REGISTRATION_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"

class Profile;

namespace web_app {

// True if file handlers are managed externally by the operating system, and
// Chrome supports file handling on this operating system.
// In practice, this is false on Chrome OS (as Chrome OS uses Chrome's installed
// apps to find file handlers), and on operating systems where Chrome doesn't
// know how to register file handlers.
bool ShouldRegisterFileHandlersWithOs();

// Returns true if file type association icons are supported by the OS.
bool FileHandlingIconsSupportedByOs();

// Do OS-specific registration to handle opening files with the specified
// |file_extensions| and |mime_types| with the PWA with the specified |app_id|.
// This may also involve creating a shim app to launch Chrome from.
// Note: Some operating systems (such as Chrome OS) may not need to do any work
// here.
void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback);

// Undo the file extensions registration for the PWA with specified |app_id|.
// If a shim app was required, also removes the shim app.
void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  ResultCallback callback);

#if BUILDFLAG(IS_LINUX)
// Exposed for testing purposes. Register the set of
// MIME-type-to-file-extensions mappings corresponding to |file_handlers|. File
// I/O and a a callout to the Linux shell are performed asynchronously.
void InstallMimeInfoOnLinux(const AppId& app_id,
                            Profile* profile,
                            const apps::FileHandlers& file_handlers);

using UpdateMimeInfoDatabaseOnLinuxCallback =
    base::RepeatingCallback<bool(base::FilePath profile_path,
                                 std::string xdg_command,
                                 std::string file_contents)>;

// Override the |callback| used to handle updating the Linux MIME-info database
// (the default is to use xdg-mime).
void SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(
    UpdateMimeInfoDatabaseOnLinuxCallback callback);
#endif

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_REGISTRATION_H_
