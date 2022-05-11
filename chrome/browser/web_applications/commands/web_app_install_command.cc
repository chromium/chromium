// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_install_command.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

WebAppInstallCommand::WebAppInstallCommand(
    const AppId& app_id,
    Profile* profile,
    WebAppInstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    WebAppRegistrar* registrar,
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    WebAppInstallFlow flow,
    absl::optional<WebAppInstallParams> install_params)
    : WebAppCommand(WebAppCommandLock::CreateForAppLock({app_id})),
      install_task_(profile,
                    install_finalizer,
                    std::move(data_retriever),
                    registrar,
                    install_surface),
      web_contents_(contents),
      dialog_callback_(std::move(dialog_callback)),
      install_callback_(std::move(callback)),
      web_app_info_(std::move(web_app_info)),
      opt_manifest_(std::move(opt_manifest)),
      manifest_url_(manifest_url),
      flow_(flow),
      app_id_(app_id),
      install_params_(install_params) {}

WebAppInstallCommand::~WebAppInstallCommand() = default;

void WebAppInstallCommand::Start() {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  install_task_.InstallWebAppOnManifestValidated(
      web_contents_.get(), std::move(dialog_callback_),
      base::BindOnce(&WebAppInstallCommand::OnInstallCompleted,
                     weak_factory_.GetWeakPtr()),
      std::move(web_app_info_), std::move(opt_manifest_), manifest_url_, flow_,
      install_params_);
}

void WebAppInstallCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id_, code));
}

void WebAppInstallCommand::OnInstallCompleted(const AppId& app_id,
                                              webapps::InstallResultCode code) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    base::Value task_error_dict = install_task_.TakeErrorDict();
    if (!task_error_dict.DictEmpty())
      command_manager()->LogToInstallManager(std::move(task_error_dict));
  }

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

void WebAppInstallCommand::OnBeforeForcedUninstallFromSync() {
  // TODO(crbug.com/1320086): remove after uninstall from sync is async.
  Abort(webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
  return;
}

void WebAppInstallCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
  return;
}

content::WebContents* WebAppInstallCommand::GetInstallingWebContents() {
  return web_contents_.get();
}

base::Value WebAppInstallCommand::ToDebugValue() const {
  return base::Value(base::StringPrintf("WebAppInstallCommand %d, app_id: %s",
                                        id(), app_id_.c_str()));
}
}  // namespace web_app
