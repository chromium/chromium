// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_COMMIT_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_COMMIT_TASK_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/origin.h"

class PrefService;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppRegistrar;
class WebAppSyncBridge;

enum class WebAppUninstallCommitTaskResult {
  kSuccess = 0,
  kError = 1,
};

// Uninstalls a given web app by:
// 1) Unregistering OS hooks.
// 2) Deleting the app from the database.
// 3) Deleting data on disk.
// Extra invariants:
// * There is never more than one uninstall task operating on the same app at
//   the same time.
// TODO(https://crbug.com/1162477): Make the database delete happen last.
class WebAppUninstallCommitTask {
 public:
  using UninstallCallback =
      base::OnceCallback<void(WebAppUninstallCommitTaskResult)>;

  WebAppUninstallCommitTask(OsIntegrationManager* os_integration_manager,
                            WebAppSyncBridge* sync_bridge,
                            WebAppIconManager* icon_manager,
                            WebAppRegistrar* registrar,
                            PrefService* profile_prefs);
  ~WebAppUninstallCommitTask();

  // The given `app_id` must correspond to an app in the `registrar`.
  void Start(const AppId& app_id,
             url::Origin app_origin,
             webapps::WebappUninstallSource source,
             UninstallCallback callback);

 private:
  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void MaybeFinishUninstall();

  enum class State {
    kNotStarted = 0,
    kPendingOsHooksDeletion = 1,
    kPendingIconDataDeletion = 2,
    kDone = 3,
  } state_ = State::kNotStarted;

  AppId app_id_;
  webapps::WebappUninstallSource source_;
  OsIntegrationManager* os_integration_manager_;
  WebAppSyncBridge* sync_bridge_;
  WebAppIconManager* icon_manager_;
  WebAppRegistrar* registrar_;
  PrefService* profile_prefs_;

  UninstallCallback callback_;
  bool errors_ = false;
  base::WeakPtrFactory<WebAppUninstallCommitTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_COMMIT_TASK_H_
