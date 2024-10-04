// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/navigate_and_trigger_install_dialog_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

NavigateAndTriggerInstallDialogCommand::NavigateAndTriggerInstallDialogCommand(
    const GURL& install_url,
    const GURL& origin_url,
    bool is_renderer_initiated,
    NavigateAndTriggerInstallDialogCommandCallback callback,
    base::WeakPtr<WebAppUiManager> ui_manager,
    std::unique_ptr<webapps::WebAppUrlLoader> url_loader,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    Profile* profile)
    : WebAppCommand<NoopLock, NavigateAndTriggerInstallDialogCommandResult>(
          "NavigateAndTriggerInstallDialogCommand",
          NoopLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          NavigateAndTriggerInstallDialogCommandResult::kShutdown),
      install_url_(install_url),
      origin_url_(origin_url),
      is_renderer_initiated_(is_renderer_initiated),
      ui_manager_(ui_manager),
      url_loader_(std::move(url_loader)),
      data_retriever_(std::move(data_retriever)),
      profile_(profile) {
  CHECK(url_loader_);
  CHECK(data_retriever_);
  CHECK(profile_);
  GetMutableDebugValue().Set("install_url", install_url_.spec());
  GetMutableDebugValue().Set("origin_url", origin_url_.spec());
  GetMutableDebugValue().Set("is_renderer_initiated", is_renderer_initiated_);
}

NavigateAndTriggerInstallDialogCommand::
    ~NavigateAndTriggerInstallDialogCommand() = default;

bool NavigateAndTriggerInstallDialogCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void NavigateAndTriggerInstallDialogCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);

  content::NavigationController::LoadURLParams load_url_params(install_url_);

  // We specify a different page transition here because the common
  // `ui::PAGE_TRANSITION_LINK` can be intercepted by URL capturing logic.
  load_url_params.transition_type = ui::PAGE_TRANSITION_FROM_API;
  load_url_params.is_renderer_initiated = is_renderer_initiated_;
  // The `Navigate` implementation requires renderer-initiated navigations to
  // specify an initiator origin.
  load_url_params.initiator_origin = url::Origin::Create(origin_url_);

  content::WebContents* new_tab = ui_manager_->CreateNewTab();
  if (!new_tab) {
    // Browser may be shutting down.
    // TODO(b/331691742): Avoid starting commands when the browser is shutting
    // down.
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  web_contents_ = new_tab->GetWeakPtr();
  url_loader_->LoadUrl(
      std::move(load_url_params), web_contents_.get(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&NavigateAndTriggerInstallDialogCommand::OnUrlLoaded,
                     weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnUrlLoaded(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("WebAppUrlLoader::Result",
                             ConvertUrlLoaderResultToString(result));
  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &NavigateAndTriggerInstallDialogCommand::OnInstallabilityChecked,
          weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnInstallabilityChecked(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  GetMutableDebugValue().Set("webapps::InstallableStatusCode",
                             GetErrorMessage(error_code));
  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  CHECK(opt_manifest);
  app_id_ = GenerateAppIdFromManifest(*opt_manifest);
  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(noop_lock_), *app_lock_, {app_id_},
      base::BindOnce(&NavigateAndTriggerInstallDialogCommand::OnAppLockGranted,
                     weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnAppLockGranted() {
  CHECK(app_lock_);
  CHECK(app_lock_->IsGranted());

  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  CHECK(!app_id_.empty());
  if (app_lock_->registrar().IsInstalled(app_id_)) {
    // If the app is already installed, we don't show the dialog. Since nothing
    // went wrong, this is still considered a success.
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        NavigateAndTriggerInstallDialogCommandResult::kAlreadyInstalled);
    return;
  }
  ui_manager_->TriggerInstallDialog(web_contents_.get());
  CompleteAndSelfDestruct(
      CommandResult::kSuccess,
      NavigateAndTriggerInstallDialogCommandResult::kDialogShown);
}

}  // namespace web_app
