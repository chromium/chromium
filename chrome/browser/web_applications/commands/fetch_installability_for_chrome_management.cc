// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

FetchInstallabilityForChromeManagement::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<WebAppUrlLoader> url_loader,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    FetchInstallabilityForChromeManagementCallback callback)
    : WebAppCommandTemplate<NoopLock>("FetchInstallabilityForChromeManagement"),
      noop_lock_description_(std::make_unique<NoopLockDescription>()),
      url_(url),
      web_contents_(web_contents),
      url_loader_(std::move(url_loader)),
      data_retriever_(std::move(data_retriever)),
      callback_(std::move(callback)) {
  DCHECK(web_contents_);
  DCHECK(url_loader_);
  DCHECK(data_retriever_);
  DCHECK(callback_);
}

FetchInstallabilityForChromeManagement::
    ~FetchInstallabilityForChromeManagement() = default;

LockDescription& FetchInstallabilityForChromeManagement::lock_description()
    const {
  DCHECK(noop_lock_description_ || app_lock_description_);
  if (app_lock_description_)
    return *app_lock_description_;
  return *noop_lock_description_;
}

void FetchInstallabilityForChromeManagement::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }

  url_loader_->LoadUrl(url_, web_contents_.get(),
                       WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
                       base::BindOnce(&FetchInstallabilityForChromeManagement::
                                          OnUrlLoadedCheckInstallability,
                                      weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnSyncSourceRemoved() {
  // No action needed. Any changes to installation status will correctly be read
  // & reflected in the command result.
}

void FetchInstallabilityForChromeManagement::OnShutdown() {
  Abort(InstallableCheckResult::kNotInstallable);
}

base::Value FetchInstallabilityForChromeManagement::ToDebugValue() const {
  base::Value::Dict debug_value;
  debug_value.Set("url", url_.spec());
  debug_value.Set("app_id", app_id_);
  debug_value.Set("error_log", base::Value(error_log_.Clone()));
  return base::Value(std::move(debug_value));
}

void FetchInstallabilityForChromeManagement::OnUrlLoadedCheckInstallability(
    WebAppUrlLoader::Result result) {
  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    base::Value::Dict url_loader_error;
    url_loader_error.Set("WebAppUrlLoader::Result",
                         ConvertUrlLoaderResultToString(result));
    error_log_.Append(std::move(url_loader_error));
  }

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), /*bypass_service_worker_check=*/true,
      base::BindOnce(&FetchInstallabilityForChromeManagement::
                         OnWebAppInstallabilityChecked,
                     weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnWebAppInstallabilityChecked(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }
  if (!is_installable) {
    DCHECK(callback_);
    SignalCompletionAndSelfDestruct(
        CommandResult::kSuccess,
        base::BindOnce(std::move(callback_),
                       InstallableCheckResult::kNotInstallable, absl::nullopt));
    return;
  }
  DCHECK(opt_manifest);
  app_id_ = GenerateAppIdFromManifest(*opt_manifest);

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(noop_lock_), {app_id_},
          base::BindOnce(
              &FetchInstallabilityForChromeManagement::OnAppLockGranted,
              weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnAppLockGranted(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (IsWebContentsDestroyed()) {
    error_log_.Append(base::Value("Web contents destroyed"));
    Abort(InstallableCheckResult::kNotInstallable);
    return;
  }
  DCHECK(!app_id_.empty());
  InstallableCheckResult result;
  if (app_lock_->registrar().IsInstalled(app_id_)) {
    result = InstallableCheckResult::kAlreadyInstalled;
  } else {
    result = InstallableCheckResult::kInstallable;
  }
  DCHECK(callback_);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), result, app_id_));
}

void FetchInstallabilityForChromeManagement::Abort(
    InstallableCheckResult result) {
  DCHECK(callback_);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), result, absl::nullopt));
}

bool FetchInstallabilityForChromeManagement::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

}  // namespace web_app
