// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// See public API WebAppCommandScheduler::UninstallWebApp() for docs.
class RemoveWebAppJob : public UninstallJob {
 public:
  // `is_initial_request` indicates that this operation is not a byproduct of
  // removing the last install source from a web app via external management and
  // will be treated as a user uninstall.
  RemoveWebAppJob(webapps::WebappUninstallSource uninstall_source,
                  Profile& profile,
                  webapps::AppId app_id,
                  bool is_initial_request = true);
  ~RemoveWebAppJob() override;

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  base::Value ToDebugValue() const override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void OnTranslationDataDeleted(bool success);
  void OnWebAppProfileDeleted(Profile* profile);
  void OnIsolatedWebAppBrowsingDataCleared();
  void MaybeFinishPrimaryRemoval();
  void ProcessSubAppsPendingRemovalOrComplete();
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;
  webapps::AppId app_id_;
  bool is_initial_request_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_ = nullptr;
  Callback callback_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool isolated_web_app_browsing_data_cleared_ = false;
  bool hooks_uninstalled_ = false;
  bool errors_ = false;
  bool has_isolated_storage_ = false;
  absl::optional<webapps::UninstallResultCode> primary_removal_result_;

  std::vector<webapps::AppId> sub_apps_pending_removal_;
  std::unique_ptr<RemoveInstallSourceJob> sub_job_;
  base::Value::Dict completed_sub_job_debug_dict_;

  base::WeakPtrFactory<RemoveWebAppJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
