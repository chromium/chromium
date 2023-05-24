// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/crx_file/id_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

// static
AppId FakeInstallFinalizer::GetAppIdForUrl(const GURL& url) {
  return GenerateAppId(/*manifest_id=*/absl::nullopt, url);
}

FakeInstallFinalizer::FakeInstallFinalizer()
    : WebAppInstallFinalizer(nullptr) {}

FakeInstallFinalizer::~FakeInstallFinalizer() = default;

void FakeInstallFinalizer::FinalizeInstall(
    const WebAppInstallInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  finalize_options_list_.push_back(options);
  Finalize(web_app_info, webapps::InstallResultCode::kSuccessNewInstall,
           std::move(callback));
}

void FakeInstallFinalizer::FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                                          InstallFinalizedCallback callback) {
  Finalize(web_app_info, webapps::InstallResultCode::kSuccessAlreadyInstalled,
           std::move(callback));
}

void FakeInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    WebAppManagement::Type source,
    webapps::WebappUninstallSource uninstall_surface,
    UninstallWebAppCallback callback) {
  user_uninstalled_external_apps_.erase(app_id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                webapps::UninstallResultCode::kSuccess));
}

void FakeInstallFinalizer::UninstallExternalWebAppByUrl(
    const GURL& app_url,
    WebAppManagement::Type source,
    webapps::WebappUninstallSource uninstall_surface,
    UninstallWebAppCallback callback) {
  DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
  uninstall_external_web_app_urls_.push_back(app_url);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [this, app_url, callback = std::move(callback)]() mutable {
                       webapps::UninstallResultCode result =
                           next_uninstall_external_web_app_results_[app_url];
                       next_uninstall_external_web_app_results_.erase(app_url);
                       std::move(callback).Run(result);
                     }));
}

void FakeInstallFinalizer::UninstallWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback) {
  NOTIMPLEMENTED();
}

bool FakeInstallFinalizer::CanReparentTab(const AppId& app_id,
                                          bool shortcut_created) const {
  return true;
}

void FakeInstallFinalizer::ReparentTab(const AppId& app_id,
                                       bool shortcut_created,
                                       content::WebContents* web_contents) {
  ++num_reparent_tab_calls_;
}

void FakeInstallFinalizer::SetNextFinalizeInstallResult(
    const AppId& app_id,
    webapps::InstallResultCode code) {
  next_app_id_ = app_id;
  next_result_code_ = code;
}

void FakeInstallFinalizer::SetNextUninstallExternalWebAppResult(
    const GURL& app_url,
    webapps::UninstallResultCode code) {
  DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));
  next_uninstall_external_web_app_results_[app_url] = code;
}

void FakeInstallFinalizer::SimulateExternalAppUninstalledByUser(
    const AppId& app_id) {
  DCHECK(!base::Contains(user_uninstalled_external_apps_, app_id));
  user_uninstalled_external_apps_.insert(app_id);
}

bool FakeInstallFinalizer::WasPreinstalledWebAppUninstalled(
    const AppId& app_id) {
  return base::Contains(user_uninstalled_external_apps_, app_id);
}

void FakeInstallFinalizer::Finalize(const WebAppInstallInfo& web_app_info,
                                    webapps::InstallResultCode code,
                                    InstallFinalizedCallback callback) {
  AppId app_id = GetAppIdForUrl(web_app_info.start_url);
  if (next_app_id_.has_value()) {
    app_id = next_app_id_.value();
    next_app_id_.reset();
  }

  if (next_result_code_.has_value()) {
    code = next_result_code_.value();
    next_result_code_.reset();
  }

  OsHooksErrors os_hooks_errors;

  // Store input data copies for inspecting in tests.
  web_app_info_copy_ =
      std::make_unique<WebAppInstallInfo>(web_app_info.Clone());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), app_id, code, os_hooks_errors));
}

}  // namespace web_app
