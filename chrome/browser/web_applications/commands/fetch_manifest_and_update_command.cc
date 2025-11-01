// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/model/web_app_comparison.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page_manifest_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

namespace web_app {

FetchManifestAndUpdateCommand::~FetchManifestAndUpdateCommand() = default;
FetchManifestAndUpdateCommand::FetchManifestAndUpdateCommand(
    const GURL& install_url,
    const webapps::ManifestId& expected_manifest_id,
    FetchManifestAndUpdateCallback callback)
    : WebAppCommand<SharedWebContentsLock, FetchManifestAndUpdateResult>(
          "FetchManifestAndUpdateCommand",
          SharedWebContentsLockDescription(),
          std::move(callback),
          FetchManifestAndUpdateResult::kShutdown),
      install_url_(install_url),
      expected_manifest_id_(expected_manifest_id) {
  GetMutableDebugValue().Set("install_url",
                             install_url.possibly_invalid_spec());
  GetMutableDebugValue().Set("expected_manifest_id",
                             expected_manifest_id_.possibly_invalid_spec());
}

void FetchManifestAndUpdateCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  url_loader_ = web_contents_lock_->web_contents_manager().CreateUrlLoader();
  url_loader_->LoadUrl(
      install_url_, &web_contents_lock_->shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&FetchManifestAndUpdateCommand::OnUrlLoaded,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnUrlLoaded(
    webapps::WebAppUrlLoaderResult result) {
  switch (result) {
    case webapps::WebAppUrlLoaderResult::kUrlLoaded:
    case webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded:
      break;
    case webapps::WebAppUrlLoaderResult::kFailedUnknownReason:
    case webapps::WebAppUrlLoaderResult::kFailedPageTookTooLong:
    case webapps::WebAppUrlLoaderResult::kFailedWebContentsDestroyed:
    case webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded:
      CompleteAndSelfDestruct(CommandResult::kSuccess,
                              FetchManifestAndUpdateResult::kUrlLoadingError);
      return;
  }
  data_retriever_ =
      web_contents_lock_->web_contents_manager().CreateDataRetriever();
  data_retriever_->GetPrimaryPageFirstSpecifiedManifest(
      web_contents_lock_->shared_web_contents(),
      base::BindOnce(&FetchManifestAndUpdateCommand::OnManifestRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnManifestRetrieved(
    const base::expected<blink::mojom::ManifestPtr,
                         blink::mojom::RequestManifestErrorPtr>& result) {
  if (!result.has_value()) {
    GetMutableDebugValue().Set("manifest_error",
                               base::ToString(result.error()->error));
    for (const auto& error : result.error()->details) {
      GetMutableDebugValue()
          .EnsureList("manifest_error_details")
          ->Append(error->message);
    }
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateResult::kManifestRetrievalError);
    return;
  }
  CHECK(result.value());

  const blink::mojom::Manifest& manifest = *result.value();
  if (blink::IsEmptyManifest(manifest)) {
    GetMutableDebugValue().Set("manifest_error", "empty");
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  if (manifest.id != expected_manifest_id_) {
    GetMutableDebugValue().Set("manifest_id",
                               manifest.id.possibly_invalid_spec());
    GetMutableDebugValue().Set("manifest_error", "manifest_id_mismatch");
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  if (!manifest.has_valid_specified_start_url) {
    GetMutableDebugValue().Set("manifest_error", "no_specified_start_url");
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  bool has_any_icon = false;
  for (const auto& icon : manifest.icons) {
    if (base::Contains(icon.purpose,
                       blink::mojom::ManifestImageResource_Purpose::ANY)) {
      has_any_icon = true;
      break;
    }
  }
  if (!has_any_icon) {
    GetMutableDebugValue().Set("manifest_error", "no_any_icon");
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          manifest, *data_retriever_,
          /*background_installation=*/false,
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          web_contents_lock_->shared_web_contents().GetWeakPtr(),
          [](IconUrlSizeSet&) {}, *GetMutableDebugValue().EnsureDict("job"),
          base::BindOnce(
              &FetchManifestAndUpdateCommand::OnWebAppInfoCreatedFromManifest,
              weak_factory_.GetWeakPtr()),
          {.bypass_icon_generation_if_no_url = true,
           .fail_all_if_any_fail = true,
           .defer_icon_fetching = true,
           .use_manifest_icons_as_trusted = true});
}

void FetchManifestAndUpdateCommand::OnWebAppInfoCreatedFromManifest(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateResult::kManifestToWebAppInstallInfoFailed);
    return;
  }
  install_info_ = std::move(install_info);

  app_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *app_lock_,
      {GenerateAppIdFromManifestId(expected_manifest_id_)},
      base::BindOnce(&FetchManifestAndUpdateCommand::OnAppLockAcquired,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnAppLockAcquired() {
  if (!app_lock_->registrar().AppMatches(
          GenerateAppIdFromManifestId(expected_manifest_id_),
          WebAppFilter::InstalledInChrome())) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kAppNotInstalled);
    return;
  }

  const WebApp* app = app_lock_->registrar().GetAppById(
      GenerateAppIdFromManifestId(expected_manifest_id_));
  CHECK(app);

  if (WebAppComparison::CompareWebApps(*app, *install_info_)
          .ExistingAppWithoutPendingEqualsNewUpdate()) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateResult::kSuccessNoUpdateDetected);
    return;
  }

  manifest_to_install_info_job_->FetchIcons(
      *install_info_, app_lock_->shared_web_contents(),
      base::BindOnce(&FetchManifestAndUpdateCommand::OnIconsFetched,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnIconsFetched() {
  switch (manifest_to_install_info_job_->icon_download_result()) {
    case IconsDownloadedResult::kCompleted:
      break;
    case IconsDownloadedResult::kPrimaryPageChanged:
      CompleteAndSelfDestruct(
          CommandResult::kSuccess,
          FetchManifestAndUpdateResult::kPrimaryPageChanged);
      return;
    case IconsDownloadedResult::kAbortedDueToFailure:
      CompleteAndSelfDestruct(CommandResult::kSuccess,
                              FetchManifestAndUpdateResult::kIconDownloadError);
      return;
  }

  app_lock_->install_finalizer().FinalizeUpdate(
      *install_info_,
      base::BindOnce(&FetchManifestAndUpdateCommand::OnUpdateFinalized,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnUpdateFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  if (code != webapps::InstallResultCode::kSuccessAlreadyInstalled) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            FetchManifestAndUpdateResult::kInstallationError);
    return;
  }

  const WebApp* app = app_lock_->registrar().GetAppById(app_id);
  CHECK(app);
  if (app->pending_update_info().has_value()) {
    {
      ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
      update->UpdateApp(app_id)->SetPendingUpdateInfo(std::nullopt);
    }
    app_lock_->registrar().NotifyPendingUpdateInfoChanged(
        app_id, /*pending_update_available=*/false,
        WebAppRegistrar::PendingUpdateInfoChangePassKey());
  }

  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          FetchManifestAndUpdateResult::kSuccess);
}

}  // namespace web_app
