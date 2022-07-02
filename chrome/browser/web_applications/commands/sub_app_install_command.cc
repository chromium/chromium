// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/sub_app_install_command.h"
#include "chrome/browser/web_applications/web_app_helpers.h"

static blink::mojom::SubAppsServiceAddResultCode InstallResultCodeToMojo(
    webapps::InstallResultCode install_result_code) {
  blink::mojom::SubAppsServiceAddResultCode mojom_install_result_code;
  switch (install_result_code) {
    case webapps::InstallResultCode::kSuccessNewInstall:
      mojom_install_result_code =
          blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall;
      break;
    case webapps::InstallResultCode::kSuccessAlreadyInstalled:
      mojom_install_result_code =
          blink::mojom::SubAppsServiceAddResultCode::kSuccessAlreadyInstalled;
      break;
    case webapps::InstallResultCode::kUserInstallDeclined:
      mojom_install_result_code =
          blink::mojom::SubAppsServiceAddResultCode::kUserInstallDeclined;
      break;
    case webapps::InstallResultCode::kExpectedAppIdCheckFailed:
      mojom_install_result_code =
          blink::mojom::SubAppsServiceAddResultCode::kExpectedAppIdCheckFailed;
      break;
    default:
      mojom_install_result_code =
          blink::mojom::SubAppsServiceAddResultCode::kFailure;
      break;
  }
  return mojom_install_result_code;
}

void SubAppInstallCommand::OnSyncSourceRemoved() {}

void SubAppInstallCommand::OnShutdown() {}

base::Value SubAppInstallCommand::ToDebugValue() const {
  return base::Value("SubAppInstallCommand");
}

SubAppInstallCommand::SubAppInstallCommand(
    web_app::WebAppInstallManager* install_manager,
    web_app::WebAppRegistrar* registrar,
    web_app::AppId& parent_app_id,
    std::vector<std::pair<web_app::UnhashedAppId, GURL>> sub_apps,
    base::flat_set<web_app::AppId> app_ids_for_lock,
    base::OnceCallback<void(AppInstallResults)> callback)
    : WebAppCommand{web_app::WebAppCommandLock::CreateForAppLock(
          app_ids_for_lock)},
      install_manager_{install_manager},
      registrar_{registrar},
      requested_installs_{std::move(sub_apps)},
      parent_app_id_{parent_app_id},
      install_callback_{std::move(callback)} {}

SubAppInstallCommand::~SubAppInstallCommand() = default;

void SubAppInstallCommand::Start() {
  DCHECK(state_ == State::kNotStarted);

  // Check if parent app is installed.
  if (!registrar_->IsInstalled(parent_app_id_)) {
    // Add failure reason to each app
    base::ranges::transform(
        requested_installs_, std::inserter(results_, results_.begin()),
        [](auto const& pair) {
          return std::pair{
              pair.first,
              blink::mojom::SubAppsServiceAddResultCode::kParentAppUninstalled};
        });
    SignalCompletionAndSelfDestruct(
        web_app::CommandResult::kFailure,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  if (requested_installs_.empty()) {
    SignalCompletionAndSelfDestruct(
        web_app::CommandResult::kSuccess,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  // Populate pending_installs_ from requested_installs_
  base::ranges::transform(
      requested_installs_,
      std::inserter(pending_installs_, pending_installs_.begin()),
      [](auto const& pair) { return pair.first; });

  num_pending_dialog_callbacks_ = pending_installs_.size();

  state_ = State::kPendingDialogCallbacks;
  StartNextInstall();
}

void SubAppInstallCommand::StartNextInstall() {
  DCHECK(!requested_installs_.empty());
  std::pair<web_app::UnhashedAppId, GURL> install_info =
      std::move(requested_installs_.back());
  const web_app::UnhashedAppId& unhashed_app_id = install_info.first;
  GURL install_url = install_info.second;
  requested_installs_.pop_back();

  // TODO(https://crbug.com/1327963): Update to use WebAppCommand version of
  // WebAppInstallManager::InstallSubApp once implemented.
  install_manager_->InstallSubApp(
      parent_app_id_, install_url,
      web_app::GenerateAppIdFromUnhashed(unhashed_app_id),
      base::BindOnce(&SubAppInstallCommand::OnDialogRequested,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id),
      base::BindOnce(&SubAppInstallCommand::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr(), unhashed_app_id));
}

void SubAppInstallCommand::OnDialogRequested(
    const web_app::UnhashedAppId& unhashed_app_id,
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    web_app::WebAppInstallationAcceptanceCallback acceptance_callback) {
  acceptance_callbacks_.emplace_back(unhashed_app_id, std::move(web_app_info),
                                     std::move(acceptance_callback));

  num_pending_dialog_callbacks_--;
  DCHECK_GE(num_pending_dialog_callbacks_, 0u);
  MaybeShowDialog();
}

void SubAppInstallCommand::MaybeShowDialog() {
  if (num_pending_dialog_callbacks_ > 0) {
    DCHECK(!requested_installs_.empty());
    StartNextInstall();
    return;
  }

  if (acceptance_callbacks_.empty()) {
    SignalCompletionAndSelfDestruct(
        web_app::CommandResult::kFailure,
        base::BindOnce(std::move(install_callback_), results_));
    return;
  }

  state_ = State::kPendingInstallComplete;
  // TODO(https://crbug.com/1313109): Replace the placeholder blanket user
  // acceptance below with a permissions dialog shown to the user.
  for (auto& [unhashed_app_id, web_app_info, acceptance_callback] :
       acceptance_callbacks_) {
    std::move(acceptance_callback).Run(true, std::move(web_app_info));
  }
  acceptance_callbacks_.clear();
}

void SubAppInstallCommand::OnInstalled(
    const web_app::UnhashedAppId& unhashed_app_id,
    const web_app::AppId& app_id,
    webapps::InstallResultCode result) {
  AddResultAndRemoveFromPendingInstalls(unhashed_app_id, result);

  // In case an installation returns with a failure before running the dialog
  // callback.
  if (state_ == State::kPendingDialogCallbacks) {
    num_pending_dialog_callbacks_--;
    MaybeShowDialog();
    return;
  }

  DCHECK_GE(pending_installs_.size(), 0u);
  MaybeFinishCommand();
}

void SubAppInstallCommand::MaybeFinishCommand() {
  if (pending_installs_.size() > 0) {
    return;
  }

  SignalCompletionAndSelfDestruct(
      web_app::CommandResult::kSuccess,
      base::BindOnce(std::move(install_callback_), results_));
}

void SubAppInstallCommand::AddResultAndRemoveFromPendingInstalls(
    const web_app::UnhashedAppId& unhashed_app_id,
    webapps::InstallResultCode result) {
  std::pair result_pair(unhashed_app_id, InstallResultCodeToMojo(result));
  results_.emplace_back(result_pair);
  pending_installs_.erase(unhashed_app_id);
}
