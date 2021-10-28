// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/fake_install_finalizer.h"

#include "base/callback.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/crx_file/id_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

// static
AppId FakeInstallFinalizer::GetAppIdForUrl(const GURL& url) {
  return GenerateAppId(/*manifest_id=*/absl::nullopt, url);
}

FakeInstallFinalizer::FakeInstallFinalizer()
    : WebAppInstallFinalizer(nullptr, nullptr, nullptr) {}

FakeInstallFinalizer::~FakeInstallFinalizer() = default;

void FakeInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  finalize_options_list_.push_back(options);
  Finalize(web_app_info, InstallResultCode::kSuccessNewInstall,
           std::move(callback));
}

void FakeInstallFinalizer::FinalizeUpdate(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  Finalize(web_app_info, InstallResultCode::kSuccessAlreadyInstalled,
           std::move(callback));
}

void FakeInstallFinalizer::UninstallExternalWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource webapp_uninstall_source,
    UninstallWebAppCallback callback) {
  user_uninstalled_external_apps_.erase(app_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*uninstalled=*/true));
}

void FakeInstallFinalizer::UninstallExternalWebAppByUrl(
    const GURL& app_url,
    webapps::WebappUninstallSource webapp_uninstall_source,
    UninstallWebAppCallback callback) {
  DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
  uninstall_external_web_app_urls_.push_back(app_url);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [this, app_url, callback = std::move(callback)]() mutable {
                       bool result =
                           next_uninstall_external_web_app_results_[app_url];
                       next_uninstall_external_web_app_results_.erase(app_url);
                       std::move(callback).Run(result);
                     }));
}

void FakeInstallFinalizer::UninstallWithoutRegistryUpdateFromSync(
    const std::vector<AppId>& web_apps,
    RepeatingUninstallCallback callback) {
  NOTREACHED();
}

bool FakeInstallFinalizer::CanUserUninstallWebApp(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

void FakeInstallFinalizer::UninstallWebApp(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback) {
  NOTIMPLEMENTED();
}

void FakeInstallFinalizer::RetryIncompleteUninstalls(
    const std::vector<AppId>& apps_to_uninstall) {
  NOTREACHED();
}

bool FakeInstallFinalizer::WasPreinstalledWebAppUninstalled(
    const AppId& app_id) const {
  return base::Contains(user_uninstalled_external_apps_, app_id);
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
void FakeInstallFinalizer::SetRemoveSourceCallbackForTesting(
    base::RepeatingCallback<void(const AppId&)>) {
  NOTIMPLEMENTED();
}

void FakeInstallFinalizer::SetNextFinalizeInstallResult(
    const AppId& app_id,
    InstallResultCode code) {
  next_app_id_ = app_id;
  next_result_code_ = code;
}

void FakeInstallFinalizer::SetNextUninstallExternalWebAppResult(
    const GURL& app_url,
    bool uninstalled) {
  DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));
  next_uninstall_external_web_app_results_[app_url] = uninstalled;
}

void FakeInstallFinalizer::SimulateExternalAppUninstalledByUser(
    const AppId& app_id) {
  DCHECK(!base::Contains(user_uninstalled_external_apps_, app_id));
  user_uninstalled_external_apps_.insert(app_id);
}

void FakeInstallFinalizer::Finalize(const WebApplicationInfo& web_app_info,
                                    InstallResultCode code,
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

  // Store input data copies for inspecting in tests.
  web_app_info_copy_ = std::make_unique<WebApplicationInfo>(web_app_info);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), app_id, code));
}

}  // namespace web_app
