// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/commands/web_app_install_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "fetch_manifest_and_install_command.h"

namespace web_app {

FetchManifestAndInstallCommand::FetchManifestAndInstallCommand(
    WebAppInstallFinalizer* install_finalizer,
    WebAppRegistrar* registrar,
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback)
    : WebAppCommand(WebAppCommandLock::CreateForNoOpLock()),
      install_finalizer_(install_finalizer),
      registrar_(registrar),
      install_surface_(install_surface),
      web_contents_(contents),
      bypass_service_worker_check_(bypass_service_worker_check),
      dialog_callback_(std::move(dialog_callback)),
      install_callback_(std::move(callback)),
      data_retriever_(std::make_unique<WebAppDataRetriever>()) {}

FetchManifestAndInstallCommand::FetchManifestAndInstallCommand(
    WebAppInstallFinalizer* install_finalizer,
    WebAppRegistrar* registrar,
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback,
    WebAppInstallFlow flow)
    : WebAppCommand(WebAppCommandLock::CreateForNoOpLock()),
      install_finalizer_(install_finalizer),
      registrar_(registrar),
      install_surface_(install_surface),
      web_contents_(contents),
      bypass_service_worker_check_(bypass_service_worker_check),
      dialog_callback_(std::move(dialog_callback)),
      install_callback_(std::move(callback)),
      data_retriever_(std::make_unique<WebAppDataRetriever>()),
      use_fallback_(use_fallback),
      flow_(flow) {}

FetchManifestAndInstallCommand::~FetchManifestAndInstallCommand() = default;

void FetchManifestAndInstallCommand::Start() {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (use_fallback_) {
    data_retriever_->GetWebAppInstallInfo(
        web_contents_.get(),
        base::BindOnce(&FetchManifestAndInstallCommand::OnGetWebAppInstallInfo,
                       weak_factory_.GetWeakPtr()));
  } else {
    install_info_ = std::make_unique<WebAppInstallInfo>();
    FetchManifest();
  }
}

void FetchManifestAndInstallCommand::OnSyncSourceRemoved() {
  // TODO(crbug.com/1320086): remove after uninstall from sync is async.
  Abort(webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
}

void FetchManifestAndInstallCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

content::WebContents*
FetchManifestAndInstallCommand::GetInstallingWebContents() {
  return web_contents_.get();
}

base::Value FetchManifestAndInstallCommand::ToDebugValue() const {
  auto debug_value = debug_log_.Clone();
  debug_value.Set("command_name", "FetchManifestAndInstallCommand");
  debug_value.Set("command_id", id());
  debug_value.Set("app_id", app_id_);
  return base::Value(std::move(debug_value));
}

void FetchManifestAndInstallCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), AppId(), code));
}

bool FetchManifestAndInstallCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void FetchManifestAndInstallCommand::OnGetWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> fallback_web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!fallback_web_app_info) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  install_info_ = std::move(fallback_web_app_info);
  LogInstallInfo();

  return FetchManifest();
}

void FetchManifestAndInstallCommand::FetchManifest() {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnDidPerformInstallableCheck,
          weak_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!use_fallback_ && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  auto manifest_id = install_info_->manifest_id;
  auto start_url = install_info_->start_url;

  if (opt_manifest) {
    if (opt_manifest->start_url.is_valid())
      install_info_->start_url = opt_manifest->start_url;

    if (opt_manifest->id.has_value()) {
      install_info_->manifest_id = absl::optional<std::string>(
          base::UTF16ToUTF8(opt_manifest->id.value()));
    }
    LogInstallInfo();
  }

  app_id_ = GenerateAppId(install_info_->manifest_id, install_info_->start_url);

  command_manager()->ScheduleCommand(std::make_unique<WebAppInstallCommand>(
      app_id_, Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      install_finalizer_, std::move(data_retriever_), registrar_,
      install_surface_, web_contents_, std::move(dialog_callback_),
      std::move(install_callback_), std::move(install_info_),
      std::move(opt_manifest), manifest_url, flow_));
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
}

void FetchManifestAndInstallCommand::LogInstallInfo() {
  debug_log_.Set("manifest_id",
                 install_info_->manifest_id.has_value()
                     ? base::Value(install_info_->manifest_id.value())
                     : base::Value());
  debug_log_.Set("start_url", install_info_->start_url.spec());
  debug_log_.Set("name", install_info_->title);
}
}  // namespace web_app
