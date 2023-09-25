// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class WebAppProvider;
class OsIntegrationManager;

class WebAppFileHandlerManager {
 public:
  explicit WebAppFileHandlerManager(Profile* profile);
  WebAppFileHandlerManager(const WebAppFileHandlerManager&) = delete;
  WebAppFileHandlerManager& operator=(const WebAppFileHandlerManager&) = delete;
  virtual ~WebAppFileHandlerManager();

  void SetProvider(base::PassKey<OsIntegrationManager>,
                   WebAppProvider& provider);
  void Start();

  // Called by tests to enable file handling icon infrastructure on a platform
  // independently of whether it's needed or used in production. Note that the
  // feature flag must also separately be enabled.
  static void SetIconsSupportedByOsForTesting(bool value);

  using LaunchInfos =
      std::vector<std::tuple<GURL, std::vector<base::FilePath>>>;

  // Given an app and a list of files, calculates the total set of launches that
  // should result, returning one action URL and a non-empty list of files for
  // each client to be created. Some or all of `launch_files` may not result in
  // launches, but no file will be represented in more than one launch.
  LaunchInfos GetMatchingFileHandlerUrls(
      const webapps::AppId& app_id,
      const std::vector<base::FilePath>& launch_files);

  // Enables and registers OS specific file handlers for OSs that need them.
  // Currently on Chrome OS, file handlers are enabled and registered as long as
  // the app is installed.
  void EnableAndRegisterOsFileHandlers(const webapps::AppId& app_id,
                                       ResultCallback callback);

  // Disables file handlers for all OSs and unregisters OS specific file
  // handlers for OSs that need them. On Chrome OS file handlers are registered
  // separately but they are still enabled and disabled here.
  void DisableAndUnregisterOsFileHandlers(const webapps::AppId& app_id,
                                          ResultCallback callback);

  // Gets all enabled file handlers for |app_id|. |nullptr| if the app has no
  // enabled file handlers. Note: The lifetime of the file handlers are tied to
  // the app they belong to.
  const apps::FileHandlers* GetEnabledFileHandlers(
      const webapps::AppId& app_id) const;

  // Returns true when the system supports file type association icons.
  static bool IconsEnabled();

 protected:
  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers or if app_id was uninstalled.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  // `virtual` for testing.
  virtual const apps::FileHandlers* GetAllFileHandlers(
      const webapps::AppId& app_id) const;

  virtual bool IsDisabledForTesting();

 private:
  // Sets whether `app_id` should have its File Handling abilities surfaces in
  // the OS. In theory, this should match the actual OS integration state (e.g.
  // the contents of the .desktop file on Linux), however, that's only enforced
  // on a best-effort basis.
  void SetOsIntegrationState(const webapps::AppId& app_id,
                             OsIntegrationState os_state);

  // Indicates whether file handlers should be OS-registered for an app. As with
  // `SetOsIntegrationState()`, there may be a mismatch with the actual OS
  // registry.
  bool ShouldOsIntegrationBeEnabled(const webapps::AppId& app_id) const;

  static bool disable_automatic_file_handler_cleanup_for_testing_;

  [[maybe_unused]] const raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::WeakPtrFactory<WebAppFileHandlerManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_
