// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_WEB_APP_JOB_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// Removes a web app from the database and cleans up all assets and OS
// integrations. Disconnects it from any of its sub apps and uninstalls them too
// if they have no other install sources.
// Adds it to `UserUninstalledPreinstalledWebAppPrefs` if it was default
// installed and the removal was user initiated.
class RemoveWebAppJob : public UninstallJob {
 public:
  // `is_initial_request` indicates that this operation is not a byproduct of
  // removing the last install source from a web app via external management and
  // will be treated as a user uninstall.
  RemoveWebAppJob(webapps::WebappUninstallSource uninstall_source,
                  Profile& profile,
                  AppId app_id,
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
  void MaybeFinishPrimaryRemoval();
  void ProcessSubAppsPendingRemovalOrComplete();
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;
  AppId app_id_;
  bool is_initial_request_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_;
  Callback callback_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool pending_app_profile_deletion_ = false;
  bool errors_ = false;
  absl::optional<webapps::UninstallResultCode> primary_removal_result_;

  std::vector<AppId> sub_apps_pending_removal_;
  std::unique_ptr<RemoveInstallSourceJob> sub_job_;
  base::Value::Dict completed_sub_job_debug_dict_;

  base::WeakPtrFactory<RemoveWebAppJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
