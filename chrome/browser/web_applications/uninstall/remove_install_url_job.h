// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class RemoveInstallSourceJob;

// Removes `install_source`'s `install_url` from `app_id`, if `app_id` is unset
// then the first matching web app that has `install_url` for `install_source`
// will be used.
// This will remove the install source if there are no remaining install URLs
// for that install source which in turn will remove the web app if there are no
// remaining install sources for the web app.
// TODO(crbug.com/1434692): There could potentially be multiple app matches for
// `install_source` and `install_url`, handle this case explicitly.
class RemoveInstallUrlJob : public UninstallJob {
 public:
  RemoveInstallUrlJob(webapps::WebappUninstallSource uninstall_source,
                      Profile& profile,
                      absl::optional<AppId> app_id,
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
  absl::optional<AppId> app_id_;
  WebAppManagement::Type install_source_;
  GURL install_url_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_;
  Callback callback_;

  std::unique_ptr<RemoveInstallSourceJob> sub_job_;
  base::Value completed_sub_job_debug_value_;

  base::WeakPtrFactory<RemoveInstallUrlJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REMOVE_INSTALL_URL_JOB_H_
