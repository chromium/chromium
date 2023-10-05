// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_SOURCE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_SOURCE_JOB_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class RemoveWebAppJob;

// See public API WebAppCommandScheduler::RemoveInstallSource() for docs.
class RemoveInstallSourceJob : public UninstallJob {
 public:
  RemoveInstallSourceJob(webapps::WebappUninstallSource uninstall_source,
                         Profile& profile,
                         webapps::AppId app_id,
                         WebAppManagement::Type install_source);
  ~RemoveInstallSourceJob() override;

  const webapps::AppId& app_id() const { return app_id_; }

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  base::Value ToDebugValue() const override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void RemoveInstallSourceFromDatabase(OsHooksErrors os_hooks_errors);
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;
  webapps::AppId app_id_;
  WebAppManagement::Type install_source_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_;
  Callback callback_;

  std::unique_ptr<RemoveWebAppJob> sub_job_;
  base::Value completed_sub_job_debug_value_;

  base::WeakPtrFactory<RemoveInstallSourceJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_SOURCE_JOB_H_
