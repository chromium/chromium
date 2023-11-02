// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/origin.h"

class PrefService;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppInstallManager;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppTranslationManager;

// Uninstalls a given web app by:
// 1) Unregistering OS hooks.
// 2) Deleting the app from the database.
// 3) Deleting data on disk.
// Extra invariants:
// * There is never more than one uninstall task operating on the same app at
//   the same time.
class WebAppUninstallJob {
 public:
  using UninstallCallback =
      base::OnceCallback<void(webapps::UninstallResultCode)>;

  // static
  static std::unique_ptr<WebAppUninstallJob> CreateAndStart(
      const AppId& app_id,
      const url::Origin& app_origin,
      UninstallCallback callback,
      OsIntegrationManager& os_integration_manager,
      WebAppSyncBridge& sync_bridge,
      WebAppIconManager& icon_manager,
      WebAppRegistrar& registrar,
      WebAppInstallManager& install_manager,
      WebAppTranslationManager& translation_manager,
      PrefService& profile_prefs);

  ~WebAppUninstallJob();

 private:
  WebAppUninstallJob(const AppId& app_id,
                     const url::Origin& app_origin,
                     UninstallCallback callback,
                     OsIntegrationManager& os_integration_manager,
                     WebAppSyncBridge& sync_bridge,
                     WebAppIconManager& icon_manager,
                     WebAppRegistrar& registrar,
                     WebAppInstallManager& install_manager,
                     WebAppTranslationManager& translation_manager,
                     PrefService& profile_prefs);

  // The given `app_id` must correspond to an app in the `registrar`.
  // This modifies the app to set `is_uninstalling()` to true, and delete the
  // app from the registry after uninstallation is complete.
  void Start(const url::Origin& app_origin,
             OsIntegrationManager& os_integration_manager,
             WebAppIconManager& icon_manager,
             WebAppTranslationManager& translation_manager,
             PrefService& profile_prefs);
  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void OnTranslationDataDeleted(bool success);
  void MaybeFinishUninstall();

  enum class State {
    kNotStarted = 0,
    kPendingDataDeletion = 1,
    kDone = 2,
  } state_ = State::kNotStarted;

  AppId app_id_;
  UninstallCallback callback_;

  // The WebAppUninstallJob is kicked off by the WebAppUninstallCommand
  // and is constructed and destructed well within the lifetime of the
  // Uninstall command. This ensures that this class is guaranteed to be
  // destructed before any of the WebAppProvider systems shut down.
  raw_ptr<WebAppRegistrar> registrar_;
  raw_ptr<WebAppSyncBridge> sync_bridge_;
  raw_ptr<WebAppInstallManager> install_manager_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool errors_ = false;

  base::WeakPtrFactory<WebAppUninstallJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_
