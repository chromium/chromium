// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// See public API WebAppCommandScheduler::RemoveInstallUrl() for docs.
class RemoveInstallUrlJob : public UninstallJob {
 public:
  RemoveInstallUrlJob(webapps::WebappUninstallSource uninstall_source,
                      Profile& profile,
                      absl::optional<webapps::AppId> app_id,
                      WebAppManagement::Type install_source,
                      GURL install_url);
  ~RemoveInstallUrlJob() override;

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  base::Value ToDebugValue() const override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;
  absl::optional<webapps::AppId> app_id_;
  WebAppManagement::Type install_source_;
  GURL install_url_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_ = nullptr;
  Callback callback_;

  std::unique_ptr<RemoveInstallSourceJob> sub_job_;
  base::Value completed_sub_job_debug_value_;

  base::WeakPtrFactory<RemoveInstallUrlJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
