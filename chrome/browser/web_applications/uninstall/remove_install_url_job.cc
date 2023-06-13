// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/uninstall/remove_install_url_job.h"

#include "base/containers/contains.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

RemoveInstallUrlJob::RemoveInstallUrlJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    WebAppManagement::Type install_source,
    GURL install_url)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      install_source_(install_source),
      install_url_(std::move(install_url)) {}

RemoveInstallUrlJob::~RemoveInstallUrlJob() = default;

void RemoveInstallUrlJob::Start(AllAppsLock& lock, Callback callback) {
  lock_ = &lock;
  callback_ = std::move(callback);

  const WebApp* app = nullptr;
  bool is_only_install_url = false;
  for (const WebApp& candidate_app : lock_->registrar().GetApps()) {
    const WebApp::ExternalConfigMap& config_map =
        candidate_app.management_to_external_config_map();
    auto it = config_map.find(install_source_);
    if (it != config_map.end()) {
      const WebApp::ExternalManagementConfig& config = it->second;
      if (base::Contains(config.install_urls, install_url_)) {
        app = &candidate_app;
        is_only_install_url = config.install_urls.size() == 1u;
        break;
      }
    }
  }

  if (!app) {
    CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  if (is_only_install_url) {
    sub_job_ = std::make_unique<RemoveInstallSourceJob>(
        uninstall_source_, profile_.get(), app->app_id(), install_source_);
    sub_job_->Start(
        *lock_, base::BindOnce(&RemoveInstallUrlJob::CompleteAndSelfDestruct,
                               weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  {
    ScopedRegistryUpdate update(&lock_->sync_bridge());
    CHECK(update->UpdateApp(app->app_id())
              ->RemoveInstallUrlForSource(install_source_, install_url_));

    // This is no longer a valid pointer after the registry update, clear it for
    // safety.
    app = nullptr;
  }
  CompleteAndSelfDestruct(webapps::UninstallResultCode::kSuccess);
}

base::Value RemoveInstallUrlJob::ToDebugValue() const {
  base::Value::Dict dict;
  dict.Set("job", "RemoveInstallUrlJob");
  dict.Set("install_source", base::ToString(install_source_));
  dict.Set("install_url", install_url_.spec());
  dict.Set("callback", callback_.is_null());
  dict.Set("active_sub_job",
           sub_job_ ? sub_job_->ToDebugValue() : base::Value());
  dict.Set("completed_sub_job", completed_sub_job_debug_value_.Clone());
  return base::Value(std::move(dict));
}

webapps::WebappUninstallSource RemoveInstallUrlJob::uninstall_source() const {
  return uninstall_source_;
}

void RemoveInstallUrlJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);

  if (sub_job_) {
    completed_sub_job_debug_value_ = sub_job_->ToDebugValue();
    sub_job_.reset();
  }

  std::move(callback_).Run(code);
}

}  // namespace web_app
