// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
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
  // the registry on Windows, to prevent side effects while testing. Note: When
  // disabled, file handling integration will not work on most operating
  // systems.
  void DisableOsIntegrationForTesting();

  // This is needed because cleanup can run before the tests have finished
  // setting up.
  static void DisableAutomaticFileHandlerCleanupForTesting();

  // Manually trigger file handler cleanup for tests. Returns the number of file
  // handlers which were cleaned up. After the origin trial for file handling is
  // completed this can be removed.
  int TriggerFileHandlerCleanupForTesting();

  // Called by tests to enable file handling icon infrastructure on a platform
  // independently of whether it's needed or used in production. Note that the
  // feature flag must also separately be enabled.
  static void SetIconsSupportedByOsForTesting(bool value);

  // Returns |app_id|'s URL registered to handle |launch_files|'s extensions, or
  // nullopt otherwise.
  const absl::optional<GURL> GetMatchingFileHandlerURL(
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
  const apps::FileHandlers* GetEnabledFileHandlers(const AppId& app_id);

  // Determines whether file handling is allowed for |app_id|. This is true if
  // the app has a valid origin trial token for the file handling API or if the
  // FileHandlingAPI flag is enabled.
  bool IsFileHandlingAPIAvailable(const AppId& app_id);

  // Returns true when the system supports file type association icons.
  static bool IconsEnabled();

 protected:
  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers or if app_id was uninstalled.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  // `virtual` for testing.
  virtual const apps::FileHandlers* GetAllFileHandlers(const AppId& app_id);

 private:
  // Removes file handlers whose origin trials have expired (assuming
  // kFileHandlingAPI isn't enabled). Returns the number of apps that had file
  // handlers unregistered, for use in tests.
  int CleanupAfterOriginTrials();

  // Sets whether `app_id` should have its File Handling abilities surfaces in
  // the OS. In theory, this should match the actual OS integration state (e.g.
  // the contents of the .desktop file on Linux), however, that's only enforced
  // on a best-effort basis.
  void SetOsIntegrationState(const AppId& app_id, OsIntegrationState os_state);

  // Indicates whether file handlers should be OS-registered for an app. As with
  // `SetOsIntegrationState()`, there may be a mismatch with the actual OS
  // registry.
  bool ShouldOsIntegrationBeEnabled(const AppId& app_id) const;

  const WebAppRegistrar* GetRegistrar() const;

  static bool disable_automatic_file_handler_cleanup_for_testing_;
  bool disable_os_integration_for_testing_ = false;

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;

  base::WeakPtrFactory<WebAppFileHandlerManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
