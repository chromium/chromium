// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <set>
#include <vector>

#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppRegistrar;

class WebAppFileHandlerManager {
 public:
  explicit WebAppFileHandlerManager(Profile* profile);
  WebAppFileHandlerManager(const WebAppFileHandlerManager&) = delete;
  WebAppFileHandlerManager& operator=(const WebAppFileHandlerManager&) = delete;
  virtual ~WebAppFileHandlerManager();

  // |registrar| is used to observe OnWebAppInstalled/Uninstalled events.
  void SetSubsystems(WebAppRegistrar* registrar);
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

  // Set a callback which is fired when the file handling expiry time is
  // updated.
  void SetOnFileHandlingExpiryUpdatedForTesting(
      base::RepeatingCallback<void()> on_file_handling_expiry_updated);

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
  void DisableAndUnregisterOsFileHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool)> callback);

  // Updates the file handling origin trial expiry timer based on a currently
  // open instance of the site. This will not update the expiry timer if
  // |app_id| has force enabled file handling origin trial.
  void MaybeUpdateFileHandlingOriginTrialExpiry(
      content::WebContents* web_contents,
      const AppId& app_id);

  // Force enables File Handling origin trial. This will register the App's file
  // handlers even if the App does not have a valid origin trial token.
  void ForceEnableFileHandlingOriginTrial(const AppId& app_id);

  // Disable a force enabled File Handling origin trial. This will unregister
  // App's file handlers.
  void DisableForceEnabledFileHandlingOriginTrial(const AppId& app_id);

  // Returns whether App's file handling is force enabled.
  bool IsFileHandlingForceEnabled(const AppId& app_id);

  // Gets all enabled file handlers for |app_id|. |nullptr| if the app has no
  // enabled file handlers. Note: The lifetime of the file handlers are tied to
  // the app they belong to.
  const apps::FileHandlers* GetEnabledFileHandlers(const AppId& app_id);

  // Determines whether file handling is allowed for |app_id|. This is true if
  // the app has a valid origin trial token for the file handling API or if the
  // FileHandlingAPI flag is enabled.
  bool IsFileHandlingAPIAvailable(const AppId& app_id);

  // Indicates whether file handlers have been registered for an app.
  bool AreFileHandlersEnabled(const AppId& app_id) const;

  // Returns true when the system supports file type association icons.
  static bool IconsEnabled();

 protected:
  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers or if app_id was uninstalled.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  // `virtual` for testing.
  virtual const apps::FileHandlers* GetAllFileHandlers(const AppId& app_id);

 private:
  void OnOriginTrialExpiryTimeReceived(
      mojo::AssociatedRemote<blink::mojom::FileHandlingExpiry> /*interface*/,
      const AppId& app_id,
      base::Time expiry_time);

  // Removes file handlers whose origin trials have expired (assuming
  // kFileHandlingAPI isn't enabled). Returns the number of apps that had file
  // handlers unregistered, for use in tests.
  int CleanupAfterOriginTrials();

  void UpdateFileHandlersForOriginTrialExpiryTime(
      const AppId& app_id,
      const base::Time& expiry_time);

  static bool disable_automatic_file_handler_cleanup_for_testing_;
  bool disable_os_integration_for_testing_ = false;
  base::RepeatingCallback<void()> on_file_handling_expiry_updated_for_testing_;

  Profile* const profile_;
  WebAppRegistrar* registrar_ = nullptr;

  base::WeakPtrFactory<WebAppFileHandlerManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
