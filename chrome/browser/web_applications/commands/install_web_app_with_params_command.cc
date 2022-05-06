// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_web_app_with_params_command.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/commands/web_app_install_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

InstallWebAppWithParamsCommand::InstallWebAppWithParamsCommand(
    base::WeakPtr<content::WebContents> contents,
    const WebAppInstallParams& install_params,
    webapps::WebappInstallSource install_surface,
    WebAppInstallFinalizer* install_finalizer,
    WebAppRegistrar* registrar,
    OnceInstallCallback callback,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommand(WebAppCommandLock::CreateForNoOpLock()),
      web_contents_(contents),
      install_params_(install_params),
      install_surface_(install_surface),
      install_finalizer_(install_finalizer),
      registrar_(registrar),
      install_callback_(std::move(callback)),
      data_retriever_(std::move(data_retriever)) {}

InstallWebAppWithParamsCommand::~InstallWebAppWithParamsCommand() = default;

void InstallWebAppWithParamsCommand::Start() {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      web_contents_.get(),
      base::BindOnce(
          &InstallWebAppWithParamsCommand::OnGetWebAppInstallInfoInCommand,
          weak_factory_.GetWeakPtr()));
}

void InstallWebAppWithParamsCommand::OnBeforeForcedUninstallFromSync() {
  // TODO(crbug.com/1320086): remove after uninstall from sync is async.
  Abort(webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
  return;
}

void InstallWebAppWithParamsCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
  return;
}

base::Value InstallWebAppWithParamsCommand::ToDebugValue() const {
  base::Value::Dict params_info;
  params_info.Set("InstallWebAppWithParamsCommand ID:", id());
  params_info.Set("Title",
                  install_params_.fallback_app_name.has_value()
                      ? base::Value(install_params_.fallback_app_name.value())
                      : base::Value());
  params_info.Set("Start URL",
                  install_params_.fallback_start_url.is_valid()
                      ? base::Value(install_params_.fallback_start_url.spec())
                      : base::Value());
  return base::Value(std::move(params_info));
}

void InstallWebAppWithParamsCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), AppId(), code));
}

void InstallWebAppWithParamsCommand::OnGetWebAppInstallInfoInCommand(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!web_app_info_) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  // Write values from install_params_ to web_app_info.
  bypass_service_worker_check_ = install_params_.bypass_service_worker_check;
  // Set start_url to fallback_start_url as web_contents may have been
  // redirected. Will be overridden by manifest values if present.
  DCHECK(install_params_.fallback_start_url.is_valid());
  web_app_info_->start_url = install_params_.fallback_start_url;

  if (install_params_.fallback_app_name.has_value())
    web_app_info_->title = install_params_.fallback_app_name.value();

  ApplyParamsToWebAppInstallInfo(install_params_, *web_app_info_);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &InstallWebAppWithParamsCommand::OnDidPerformInstallableCheck,
          weak_factory_.GetWeakPtr()));
}

void InstallWebAppWithParamsCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (install_params_.require_manifest && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  // A system app should always have a manifest icon.
  if (install_surface_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    DCHECK(opt_manifest);
    DCHECK(!opt_manifest->icons.empty());
  }

  if (opt_manifest) {
    if (opt_manifest->start_url.is_valid())
      web_app_info_->start_url = opt_manifest->start_url;

    if (opt_manifest->id.has_value()) {
      web_app_info_->manifest_id = absl::optional<std::string>(
          base::UTF16ToUTF8(opt_manifest->id.value()));
    }
  }

  command_manager()->ScheduleCommand(std::make_unique<WebAppInstallCommand>(
      GenerateAppId(web_app_info_->manifest_id, web_app_info_->start_url),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      install_finalizer_, std::move(data_retriever_), registrar_,
      install_surface_, web_contents_, WebAppInstallDialogCallback(),
      std::move(install_callback_), std::move(web_app_info_),
      std::move(opt_manifest), manifest_url, WebAppInstallFlow::kUnknown,
      install_params_));
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
}

}  // namespace web_app
