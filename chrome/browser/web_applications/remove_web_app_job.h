// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_REMOVE_WEB_APP_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_REMOVE_WEB_APP_JOB_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

class Profile;

namespace web_app {

class WithAppResources;

// Uninstalls a given web app by:
// 1) Unregistering OS hooks.
// 2) Deleting the app from the database.
// 3) Deleting data on disk.
// Extra invariants:
// * There is never more than one uninstall task operating on the same app at
//   the same time.
class RemoveWebAppJob {
 public:
  using UninstallCallback = base::OnceCallback<void(bool success)>;

  static std::unique_ptr<RemoveWebAppJob> Start(
      webapps::WebappUninstallSource uninstall_source,
      AppId app_id,
      WithAppResources& resources,
      Profile& profile,
      UninstallCallback callback);

  ~RemoveWebAppJob();

 private:
  RemoveWebAppJob(webapps::WebappUninstallSource uninstall_source,
                  AppId app_id,
                  WithAppResources& resources,
                  Profile& profile,
                  UninstallCallback callback);

  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void OnTranslationDataDeleted(bool success);
  void OnWebAppProfileDeleted(Profile* profile);
  void MaybeFinishUninstall();

  webapps::WebappUninstallSource uninstall_source_;
  AppId app_id_;
  // The RemoveWebAppJob is kicked off by the WebAppUninstallCommand
  // and is constructed and destructed well within the lifetime of the
  // Uninstall command. This ensures that this class is guaranteed to be
  // destructed before any of the WebAppProvider systems shut down.
  raw_ref<WithAppResources> resources_;
  // `this` is owned by `profile_`.
  raw_ref<Profile> profile_;
  UninstallCallback callback_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool pending_app_profile_deletion_ = false;
  bool errors_ = false;
  bool done_ = false;

  base::WeakPtrFactory<RemoveWebAppJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_REMOVE_WEB_APP_JOB_H_
