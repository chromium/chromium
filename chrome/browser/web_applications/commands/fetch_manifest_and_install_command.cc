// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/commands/web_app_install_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "content/public/browser/web_contents.h"

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

FetchManifestAndInstallCommand::~FetchManifestAndInstallCommand() = default;

void FetchManifestAndInstallCommand::Start() {
  if (!web_contents_ || web_contents_->IsBeingDestroyed())
    return Abort(webapps::InstallResultCode::kWebContentsDestroyed);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnDidPerformInstallableCheck,
          weak_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  webapps::InstallableMetrics::TrackInstallResult(false);
  std::move(install_callback_).Run(AppId(), code);
  SignalCompletionAndSelfDestruct(CommandResult::kFailure, base::DoNothing());
}

void FetchManifestAndInstallCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed())
    return Abort(webapps::InstallResultCode::kWebContentsDestroyed);

  if (!valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  } else {
    DCHECK(opt_manifest);
    AppId app_id = GenerateAppIdFromManifest(*opt_manifest);

    command_manager()->EnqueueCommand(std::make_unique<WebAppInstallCommand>(
        app_id, Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        install_finalizer_, std::move(data_retriever_), registrar_,
        install_surface_, web_contents_, std::move(dialog_callback_),
        std::move(install_callback_), std::make_unique<WebAppInstallInfo>(),
        std::move(opt_manifest), manifest_url));
    SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
  }
}

void FetchManifestAndInstallCommand::OnBeforeForcedUninstallFromSync() {
  // TODO(crbug.com/1320086): remove after uninstall from sync is async.
  return Abort(webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
}

void FetchManifestAndInstallCommand::OnShutdown() {
  return Abort(
      webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

base::Value FetchManifestAndInstallCommand::ToDebugValue() const {
  return base::Value(
      base::StringPrintf("FetchManifestAndInstallCommand %d", id()));
}

}  // namespace web_app
