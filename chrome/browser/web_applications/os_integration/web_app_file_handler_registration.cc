// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace web_app {

// This block defines stub implementations of OS specific methods for
// FileHandling. Currently, Windows, MacOSX and Desktop Linux (but not Chrome
// OS) have their own implementations.
#if BUILDFLAG(IS_CHROMEOS)
bool ShouldRegisterFileHandlersWithOs() {
  return false;
}

bool FileHandlingIconsSupportedByOs() {
  return false;
}

void RegisterFileHandlersWithOs(const webapps::AppId& app_id,
                                const std::string& app_name,
                                const base::FilePath& profile_path,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  DCHECK(ShouldRegisterFileHandlersWithOs());
  // Stub function for OS's which don't register file handlers with the OS.
  std::move(callback).Run(Result::kOk);
}

void UnregisterFileHandlersWithOs(const webapps::AppId& app_id,
                                  const base::FilePath& profile_path,
                                  ResultCallback callback) {
  DCHECK(ShouldRegisterFileHandlersWithOs());
  // Stub function for OS's which don't register file handlers with the OS.
  std::move(callback).Run(Result::kOk);
}
#endif

}  // namespace web_app
