// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// See public API WebAppCommandScheduler::RemoveInstallUrlMaybeUninstall() for
// docs.
// Note: This can remove an installation management if no install urls are left,
// and the whole app if no managements are left.
class RemoveInstallUrlJob : public UninstallJob {
 public:
  RemoveInstallUrlJob(webapps::WebappUninstallSource uninstall_source,
                      Profile& profile,
                      base::Value::Dict& debug_value,
                      std::optional<webapps::AppId> app_id,
                      WebAppManagement::Type install_source,
                      GURL install_url);
  ~RemoveInstallUrlJob() override;

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  const webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  const raw_ref<Profile> profile_;
  const raw_ref<base::Value::Dict> debug_value_;
  const std::optional<webapps::AppId> app_id_;
  const WebAppManagement::Type install_source_;
  const GURL install_url_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_ = nullptr;
  Callback callback_;

  std::unique_ptr<RemoveInstallSourceJob> sub_job_;

  base::WeakPtrFactory<RemoveInstallUrlJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
