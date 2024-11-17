// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace web_app {

FetchInstallabilityForChromeManagement::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<webapps::WebAppUrlLoader> url_loader,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    FetchInstallabilityForChromeManagementCallback callback)
    : WebAppCommand<NoopLock,
                    InstallableCheckResult,
                    std::optional<webapps::AppId>>(
          "FetchInstallabilityForChromeManagement",
          NoopLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(InstallableCheckResult::kNotInstallable,
                          std::nullopt)),
      url_(url),
      web_contents_(web_contents),
      url_loader_(std::move(url_loader)),
      data_retriever_(std::move(data_retriever)) {
  CHECK(url.is_valid());
  CHECK(web_contents_);
  CHECK(url_loader_);
  CHECK(data_retriever_);
  GetMutableDebugValue().Set("url", url.spec());
}

FetchInstallabilityForChromeManagement::
    ~FetchInstallabilityForChromeManagement() = default;

void FetchInstallabilityForChromeManagement::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }

  url_loader_->LoadUrl(
      url_, web_contents_.get(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&FetchInstallabilityForChromeManagement::
                         OnUrlLoadedCheckInstallability,
                     weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnUrlLoadedCheckInstallability(
    webapps::WebAppUrlLoaderResult result) {
  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }
  GetMutableDebugValue().Set("WebAppUrlLoader::Result",
                             ConvertUrlLoaderResultToString(result));

  if (result == webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }

  if (result == webapps::WebAppUrlLoaderResult::kFailedPageTookTooLong) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }

  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(&FetchInstallabilityForChromeManagement::
                         OnWebAppInstallabilityChecked,
                     weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnWebAppInstallabilityChecked(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }
  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }
  DCHECK(opt_manifest);
  app_id_ = GenerateAppIdFromManifest(*opt_manifest);
  GetMutableDebugValue().Set("app_id", app_id_);
  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(noop_lock_), *app_lock_, {app_id_},
      base::BindOnce(&FetchInstallabilityForChromeManagement::OnAppLockGranted,
                     weak_factory_.GetWeakPtr()));
}

void FetchInstallabilityForChromeManagement::OnAppLockGranted() {
  CHECK(app_lock_);
  CHECK(app_lock_->IsGranted());

  if (IsWebContentsDestroyed()) {
    GetMutableDebugValue().Set("web_contents_destroyed", true);
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            InstallableCheckResult::kNotInstallable,
                            std::nullopt);
    return;
  }
  DCHECK(!app_id_.empty());
  InstallableCheckResult result;
  if (app_lock_->registrar().IsInstalled(app_id_)) {
    result = InstallableCheckResult::kAlreadyInstalled;
  } else {
    result = InstallableCheckResult::kInstallable;
  }
  CompleteAndSelfDestruct(CommandResult::kSuccess, result, app_id_);
}

bool FetchInstallabilityForChromeManagement::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

}  // namespace web_app
