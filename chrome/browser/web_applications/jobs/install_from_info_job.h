// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_FROM_INFO_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_FROM_INFO_JOB_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

// Starts a web app installation process using prefilled
// |install_info| which holds all the data needed for installation.
// This doesn't fetch a manifest and doesn't perform all required steps for
// External installed apps: use |ExternallyManagedAppManager::Install|
// instead.
//
// The web app can be simultaneously installed from multiple sources.
// If the web app already exists and `overwrite_existing_manifest_fields` is
// false then manifest fields in `install_info` are treated only as
// fallback manifest values. If `overwrite_existing_manifest_fields` is true
// then the existing web app manifest fields will be overwritten.
// If `install_info` contains data freshly fetched from the web app's
// site then `overwrite_existing_manifest_fields` should be true. This is an
// invariant that is not verified by code.
class InstallFromInfoJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(webapps::AppId app_id,
                              webapps::InstallResultCode code)>;

  // The `install_params` controls whether and how OS hooks get installed.
  InstallFromInfoJob(Profile* profile,
                     base::Value::Dict& debug_value,
                     std::unique_ptr<WebAppInstallInfo> install_info,
                     bool overwrite_existing_manifest_fields,
                     webapps::WebappInstallSource install_surface,
                     std::optional<WebAppInstallParams> install_params,
                     ResultCallback install_callback);

  ~InstallFromInfoJob();

  void Start(WithAppResources* lock_with_app_resources);

  webapps::WebappInstallSource install_surface() const {
    return install_surface_;
  }

 private:
  void OnInstallCompleted(const webapps::AppId& app_id,
                          webapps::InstallResultCode code);

  const raw_ref<Profile> profile_;
  const raw_ref<base::Value::Dict> debug_value_;
  const webapps::ManifestId manifest_id_;
  const webapps::AppId app_id_;
  const bool overwrite_existing_manifest_fields_;
  const webapps::WebappInstallSource install_surface_;
  const std::optional<WebAppInstallParams> install_params_;

  raw_ptr<WithAppResources> lock_with_app_resources_ = nullptr;

  std::unique_ptr<WebAppInstallInfo> install_info_;
  ResultCallback callback_;

  base::WeakPtrFactory<InstallFromInfoJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_FROM_INFO_JOB_H_
