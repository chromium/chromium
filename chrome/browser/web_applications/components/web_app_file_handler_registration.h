// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "components/services/app_service/public/cpp/file_handler.h"

class Profile;

namespace web_app {

// True if file handlers are managed externally by the operating system, and
// Chrome supports file handling on this operating system.
// In practice, this is false on Chrome OS (as Chrome OS uses Chrome's installed
// apps to find file handlers), and on operating systems where Chrome doesn't
// know how to register file handlers.
bool ShouldRegisterFileHandlersWithOs();

// Do OS-specific registration to handle opening files with the specified
// |file_extensions| and |mime_types| with the PWA with the specified |app_id|.
// This may also involve creating a shim app to launch Chrome from.
// Note: Some operating systems (such as Chrome OS) may not need to do any work
// here.
void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers);

// Undo the file extensions registration for the PWA with specified |app_id|.
// If a shim app was required, also removes the shim app.
void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  std::unique_ptr<ShortcutInfo> info,
                                  base::OnceCallback<void()> callback);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
using RegisterMimeTypesOnLinuxCallback =
    base::OnceCallback<bool(base::FilePath profile_path,
                            std::string file_contents)>;

// Exposed for testing purposes. Register the set of
// MIME-type-to-file-extensions mappings corresponding to |file_handlers|. File
// I/O and a a callout to the Linux shell are performed asynchronously in a
// |callback|, which is set automatically on the usual install code path.
void RegisterMimeTypesOnLinux(const AppId& app_id,
                              Profile* profile,
                              const apps::FileHandlers& file_handlers,
                              RegisterMimeTypesOnLinuxCallback callback);

// Override the default |callback| passed to RegisterMimeTypesOnLinux. Used in
// automated browser tests.
void SetRegisterMimeTypesOnLinuxCallbackForTesting(
    RegisterMimeTypesOnLinuxCallback callback);
#endif

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_FILE_HANDLER_REGISTRATION_H_
