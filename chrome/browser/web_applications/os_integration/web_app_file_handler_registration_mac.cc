// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

bool FileHandlingIconsSupportedByOs() {
  // TODO(crbug/1218237): implement and flip this to true.
  return false;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  // On MacOS, file associations are managed through app shims in the
  // Applications directory. File handler registration is handled via shortcuts
  // creation.
  NOTREACHED();
  std::move(callback).Run(Result::kOk);
}

void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  ResultCallback callback) {
  // On MacOS, file associations are managed through app shims in the
  // Applications directory. File handler unregistration is handled via
  // shortcuts deletion on MacOS.
  NOTREACHED();
  std::move(callback).Run(Result::kOk);
}

}  // namespace web_app
