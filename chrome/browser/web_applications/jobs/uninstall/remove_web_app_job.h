// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// This should VERY rarely be used directly, and instead just used from other
// jobs once all install managements are removed.
class RemoveWebAppJob : public UninstallJob {
 public:
  // `webapps::IsUserUninstall(uninstall_source)` indicates that this operation
  // is not a byproduct of removing the last install source from a web app via
  // external management and will be treated as a user uninstall.
  RemoveWebAppJob(webapps::WebappUninstallSource uninstall_source,
                  Profile& profile,
                  base::Value::Dict& debug_value,
                  webapps::AppId app_id);
  ~RemoveWebAppJob() override;

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void SynchronizeAndUninstallOsHooks();
  void OnIconDataDeleted(bool success);
  void OnTranslationDataDeleted(bool success);
  void OnWebAppProfileDeleted(Profile* profile);
  void OnIsolatedWebAppBrowsingDataCleared();
  void MaybeFinishPrimaryRemoval();
  void ProcessSubAppsPendingRemovalOrComplete();
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);
  void OnIsolatedWebAppOwnedLocationDeleted();

  const webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  const raw_ref<Profile> profile_;
  const raw_ref<base::Value::Dict> debug_value_;
  const webapps::AppId app_id_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_ = nullptr;
  Callback callback_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool isolated_web_app_browsing_data_cleared_ = false;
  bool isolated_web_app_owned_location_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool errors_ = false;
  bool has_isolated_storage_ = false;
  std::optional<webapps::UninstallResultCode> primary_removal_result_;
  std::optional<IsolatedWebAppStorageLocation> location_;

  std::vector<webapps::AppId> sub_apps_pending_removal_;
  std::unique_ptr<RemoveInstallSourceJob> sub_job_;

  base::WeakPtrFactory<RemoveWebAppJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
