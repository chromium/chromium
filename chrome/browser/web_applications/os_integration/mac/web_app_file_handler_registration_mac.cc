// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

bool FileHandlingIconsSupportedByOs() {
  // TODO(crbug.com/40185574): implement and flip this to true.
  return false;
}

void RegisterFileHandlersWithOs(const webapps::AppId& app_id,
                                const std::string& app_name,
                                const base::FilePath& profile_path,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  // On MacOS, file associations are managed through app shims in the
  // Applications directory. File handler registration is handled via shortcuts
  // creation. However app shim creation does need to know file associations for
  // all profiles an app is installed in. As such, persist the file handler
  // information in AppShimRegistry.
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      app_id, profile_path, GetFileExtensionsFromFileHandlers(file_handlers),
      GetMimeTypesFromFileHandlers(file_handlers));
  std::move(callback).Run(Result::kOk);
}

void UnregisterFileHandlersWithOs(const webapps::AppId& app_id,
                                  const base::FilePath& profile_path,
                                  ResultCallback callback) {
  // File handler information is embedded in the app shims. When those are
  // updated, file handlers are also unregistered.
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(app_id, profile_path,
                                                           {}, {});
  // When updating file handlers, |callback| here triggers the registering of
  // the new file handlers. It is therefore important that |callback| not be
  // dropped on the floor.
  // https://crbug.com/1201993
  std::move(callback).Run(Result::kOk);
}

}  // namespace web_app
