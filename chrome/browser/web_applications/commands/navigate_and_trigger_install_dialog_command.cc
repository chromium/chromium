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
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/installable/installable_logging.h"
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
    std::unique_ptr<WebAppUrlLoader> url_loader,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    Profile* profile)
    : WebAppCommandTemplate<NoopLock>("NavigateAndTriggerInstallDialogCommand"),
      noop_lock_description_(std::make_unique<NoopLockDescription>()),
      install_url_(install_url),
      origin_url_(origin_url),
      is_renderer_initiated_(is_renderer_initiated),
      callback_(std::move(callback)),
      ui_manager_(ui_manager),
      url_loader_(std::move(url_loader)),
      data_retriever_(std::move(data_retriever)),
      profile_(profile) {
  CHECK(url_loader_);
  CHECK(data_retriever_);
  CHECK(profile_);
}

NavigateAndTriggerInstallDialogCommand::
    ~NavigateAndTriggerInstallDialogCommand() = default;

const LockDescription&
NavigateAndTriggerInstallDialogCommand::lock_description() const {
  CHECK(noop_lock_description_ || app_lock_description_);
  if (app_lock_description_) {
    return *app_lock_description_;
  }
  return *noop_lock_description_;
}

void NavigateAndTriggerInstallDialogCommand::OnShutdown() {
  Abort(NavigateAndTriggerInstallDialogCommandResult::kFailure);
}

base::Value NavigateAndTriggerInstallDialogCommand::ToDebugValue() const {
  base::Value::Dict debug_value;
  debug_value.Set("install_url", install_url_.spec());
  debug_value.Set("origin_url", origin_url_.spec());
  debug_value.Set("is_renderer_initiated", is_renderer_initiated_);
  debug_value.Set("error_log", base::Value(error_log_.Clone()));
  return base::Value(std::move(debug_value));
}

bool NavigateAndTriggerInstallDialogCommand::IsWebContentsDestroyed() {
  return web_contents_ == nullptr || web_contents_->IsBeingDestroyed();
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

  web_contents_ = ui_manager_->CreateNewTab();
  url_loader_->LoadUrl(
      std::move(load_url_params), web_contents_,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&NavigateAndTriggerInstallDialogCommand::OnUrlLoaded,
                     weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnUrlLoaded(
    WebAppUrlLoader::Result result) {
  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    base::Value::Dict url_loader_error;
    url_loader_error.Set("WebAppUrlLoader::Result",
                         ConvertUrlLoaderResultToString(result));
    error_log_.Append(std::move(url_loader_error));
    Abort(NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_, /*bypass_service_worker_check=*/true,
      base::BindOnce(
          &NavigateAndTriggerInstallDialogCommand::OnInstallabilityChecked,
          weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnInstallabilityChecked(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    base::Value::Dict data_retriever_error;
    data_retriever_error.Set("webapps::InstallableStatusCode",
                             GetErrorMessage(error_code));
    error_log_.Append(std::move(data_retriever_error));
    Abort(NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  CHECK(opt_manifest);
  app_id_ = GenerateAppIdFromManifest(*opt_manifest);

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(noop_lock_), {app_id_},
          base::BindOnce(
              &NavigateAndTriggerInstallDialogCommand::OnAppLockGranted,
              weak_factory_.GetWeakPtr()));
}

void NavigateAndTriggerInstallDialogCommand::OnAppLockGranted(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(NavigateAndTriggerInstallDialogCommandResult::kFailure);
    return;
  }
  CHECK(!app_id_.empty());
  if (app_lock_->registrar().IsInstalled(app_id_)) {
    // If the app is already installed, we don't show the dialog. Since nothing
    // went wrong, this is still considered a success.
    SignalCompletionAndSelfDestruct(
        CommandResult::kSuccess,
        base::BindOnce(
            std::move(callback_),
            NavigateAndTriggerInstallDialogCommandResult::kAlreadyInstalled));
    return;
  }
  ui_manager_->TriggerInstallDialog(web_contents_);
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(
          std::move(callback_),
          NavigateAndTriggerInstallDialogCommandResult::kDialogShown));
}

void NavigateAndTriggerInstallDialogCommand::Abort(
    NavigateAndTriggerInstallDialogCommandResult result) {
  SignalCompletionAndSelfDestruct(CommandResult::kFailure,
                                  base::BindOnce(std::move(callback_), result));
}

}  // namespace web_app
