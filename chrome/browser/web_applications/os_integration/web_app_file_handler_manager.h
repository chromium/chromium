// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class WebAppRegistrar;
class WebAppSyncBridge;

class WebAppFileHandlerManager {
 public:
  explicit WebAppFileHandlerManager(Profile* profile);
  WebAppFileHandlerManager(const WebAppFileHandlerManager&) = delete;
  WebAppFileHandlerManager& operator=(const WebAppFileHandlerManager&) = delete;
  virtual ~WebAppFileHandlerManager();

  void SetSubsystems(WebAppSyncBridge* sync_bridge);
  void Start();

  // Disables OS integrations, such as shortcut creation on Linux or modifying
  // the registry on Windows, to prevent side effects while testing.
  // `set_os_integration` will be invoked every time OS integration would have
  // been toggled with a boolean that is true for enabled. Note: When disabled,
  // file handling integration will not work on most operating systems.
  static void DisableOsIntegrationForTesting(
      const base::RepeatingCallback<void(bool)>& set_os_integration);

  // Called by tests to enable file handling icon infrastructure on a platform
  // independently of whether it's needed or used in production. Note that the
  // feature flag must also separately be enabled.
  static void SetIconsSupportedByOsForTesting(bool value);

  // Returns |app_id|'s URL registered to handle |launch_files|'s extensions, or
  // nullopt otherwise.
  absl::optional<GURL> GetMatchingFileHandlerURL(
      const AppId& app_id,
      const std::vector<base::FilePath>& launch_files);

  // Enables and registers OS specific file handlers for OSs that need them.
  // Currently on Chrome OS, file handlers are enabled and registered as long as
  // the app is installed.
  void EnableAndRegisterOsFileHandlers(const AppId& app_id);

  // Disables file handlers for all OSs and unregisters OS specific file
  // handlers for OSs that need them. On Chrome OS file handlers are registered
  // separately but they are still enabled and disabled here.
  void DisableAndUnregisterOsFileHandlers(const AppId& app_id,
                                          ResultCallback callback);

  // Gets all enabled file handlers for |app_id|. |nullptr| if the app has no
  // enabled file handlers. Note: The lifetime of the file handlers are tied to
  // the app they belong to.
  const apps::FileHandlers* GetEnabledFileHandlers(const AppId& app_id) const;

  // Determines whether file handling is allowed for |app_id|.
  bool IsFileHandlingAPIAvailable(const AppId& app_id) const;

  // Returns true when the system supports file type association icons.
  static bool IconsEnabled();

  void SyncOsIntegrationStateForTesting();

 protected:
  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers or if app_id was uninstalled.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  // `virtual` for testing.
  virtual const apps::FileHandlers* GetAllFileHandlers(
      const AppId& app_id) const;

 private:
  // Sets whether `app_id` should have its File Handling abilities surfaces in
  // the OS. In theory, this should match the actual OS integration state (e.g.
  // the contents of the .desktop file on Linux), however, that's only enforced
  // on a best-effort basis.
  void SetOsIntegrationState(const AppId& app_id, OsIntegrationState os_state);

  // Indicates whether file handlers should be OS-registered for an app. As with
  // `SetOsIntegrationState()`, there may be a mismatch with the actual OS
  // registry.
  bool ShouldOsIntegrationBeEnabled(const AppId& app_id) const;

  // Refreshes the OS integration state for all apps. This is useful to handle
  // the case where the File Handling feature became enabled or disabled since
  // the last time Chromium ran.
  void SyncOsIntegrationState();

  const WebAppRegistrar* GetRegistrar() const;

  static bool disable_automatic_file_handler_cleanup_for_testing_;

  [[maybe_unused]] const raw_ptr<Profile> profile_;
  raw_ptr<WebAppSyncBridge> sync_bridge_;

  base::WeakPtrFactory<WebAppFileHandlerManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_FILE_HANDLER_MANAGER_H_
