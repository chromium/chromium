// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"

#include "base/containers/contains.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

namespace {

struct MatchingWebAppResult {
  raw_ptr<const WebApp> app = nullptr;
  bool is_only_install_url = false;
};

MatchingWebAppResult FindMatchingWebApp(
    const WebAppRegistrar& registrar,
    const std::optional<webapps::AppId>& app_id,
    const WebAppManagement::Type& install_source,
    const GURL& install_url) {
  if (app_id.has_value()) {
    const WebApp* candidate_app = registrar.GetAppById(app_id.value());
    if (!candidate_app) {
      return {};
    }
    const WebApp::ExternalConfigMap& config_map =
        candidate_app->management_to_external_config_map();
    auto map_it = config_map.find(install_source);
    if (map_it == config_map.end()) {
      return {};
    }

    const base::flat_set<GURL>& install_urls = map_it->second.install_urls;
    if (!base::Contains(install_urls, install_url)) {
      return {};
    }

    return {
        .app = candidate_app,
        .is_only_install_url = install_urls.size() == 1u,
    };
  }

  for (const WebApp& candidate_app : registrar.GetApps()) {
    const WebApp::ExternalConfigMap& config_map =
        candidate_app.management_to_external_config_map();
    auto it = config_map.find(install_source);
    if (it != config_map.end()) {
      const base::flat_set<GURL>& install_urls = it->second.install_urls;
      if (base::Contains(install_urls, install_url)) {
        return {
            .app = &candidate_app,
            .is_only_install_url = install_urls.size() == 1u,
        };
      }
    }
  }

  return {};
}

}  // namespace

RemoveInstallUrlJob::RemoveInstallUrlJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    base::Value::Dict& debug_value,
    std::optional<webapps::AppId> app_id,
    WebAppManagement::Type install_source,
    GURL install_url)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      debug_value_(debug_value),
      app_id_(std::move(app_id)),
      install_source_(install_source),
      install_url_(std::move(install_url)) {
  debug_value_->Set("!job", "RemoveInstallUrlJob");
  debug_value_->Set("uninstall_source", base::ToString(uninstall_source_));
  debug_value_->Set("install_source", base::ToString(install_source_));
  debug_value_->Set("install_url", install_url_.spec());
}

RemoveInstallUrlJob::~RemoveInstallUrlJob() = default;

void RemoveInstallUrlJob::Start(AllAppsLock& lock, Callback callback) {
  lock_ = &lock;
  callback_ = std::move(callback);
  debug_value_->Set("has_callback", !callback_.is_null());

  auto [app, is_only_install_url] = FindMatchingWebApp(
      lock_->registrar(), app_id_, install_source_, install_url_);

  if (!app) {
    CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  if (is_only_install_url) {
    sub_job_ = std::make_unique<RemoveInstallSourceJob>(
        uninstall_source_, profile_.get(), *debug_value_->EnsureDict("sub_job"),
        app->app_id(), WebAppManagementTypes({install_source_}));
    sub_job_->Start(
        *lock_, base::BindOnce(&RemoveInstallUrlJob::CompleteAndSelfDestruct,
                               weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    CHECK(update->UpdateApp(app->app_id())
              ->RemoveInstallUrlForSource(install_source_, install_url_));

    // This is no longer a valid pointer after the registry update, clear it for
    // safety.
    app = nullptr;
  }
  CompleteAndSelfDestruct(webapps::UninstallResultCode::kInstallUrlRemoved);
}

webapps::WebappUninstallSource RemoveInstallUrlJob::uninstall_source() const {
  return uninstall_source_;
}

void RemoveInstallUrlJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  debug_value_->Set("result", base::ToString(code));
  std::move(callback_).Run(code);
}

}  // namespace web_app
