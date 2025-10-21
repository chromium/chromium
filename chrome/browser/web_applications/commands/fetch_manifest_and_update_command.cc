// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/model/web_app_comparison.h"
#include "chrome/browser/web_applications/web_app.h"
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
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "url/gurl.h"

namespace web_app {

FetchManifestAndUpdateCommand::~FetchManifestAndUpdateCommand() = default;
FetchManifestAndUpdateCommand::FetchManifestAndUpdateCommand(
    const GURL& install_url,
    const webapps::ManifestId& expected_manifest_id,
    FetchManifestAndUpdateCallback callback)
    : WebAppCommand<SharedWebContentsWithAppLock, FetchManifestAndUpdateResult>(
          "FetchManifestAndUpdateCommand",
          SharedWebContentsWithAppLockDescription(
              {GenerateAppIdFromManifestId(expected_manifest_id)}),
          // TODO(http://crbug.com/452416687): Add metrics callback here on
          // result.
          std::move(callback),
          FetchManifestAndUpdateResult::kShutdown),
      install_url_(install_url),
      expected_manifest_id_(expected_manifest_id) {}

void FetchManifestAndUpdateCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);
  Observe(&lock_->shared_web_contents());
  if (!lock_->registrar().AppMatches(
          GenerateAppIdFromManifestId(expected_manifest_id_),
          WebAppFilter::InstalledInChrome())) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kAppNotInstalled);
    return;
  }

  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();
  url_loader_->LoadUrl(
      install_url_, &lock_->shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&FetchManifestAndUpdateCommand::OnUrlLoaded,
                     base::Unretained(this)));
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
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();
  manifest_fetch_subscription_ =
      data_retriever_->GetPrimaryPageFirstSpecifiedManifest(
          lock_->shared_web_contents(),
          base::BindOnce(&FetchManifestAndUpdateCommand::OnManifestRetrieved,
                         weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::PrimaryPageChanged(content::Page& page) {
  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          FetchManifestAndUpdateResult::kPrimaryPageChanged);
}

void FetchManifestAndUpdateCommand::OnManifestRetrieved(
    const base::expected<blink::mojom::ManifestPtr,
                         blink::mojom::RequestManifestErrorPtr>& result) {
  if (!result.has_value()) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateResult::kManifestRetrievalError);
    return;
  }
  CHECK(result.value());

  const blink::mojom::Manifest& manifest = *result.value();
  if (blink::IsEmptyManifest(manifest)) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  if (manifest.id != expected_manifest_id_) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  if (!manifest.has_valid_specified_start_url) {
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
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            FetchManifestAndUpdateResult::kInvalidManifest);
    return;
  }

  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          manifest, *data_retriever_,
          /*background_installation=*/false,
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          lock_->shared_web_contents().GetWeakPtr(), [](IconUrlSizeSet&) {},
          *GetMutableDebugValue().EnsureDict("job"),
          base::BindOnce(
              &FetchManifestAndUpdateCommand::OnWebAppInfoCreatedFromManifest,
              weak_factory_.GetWeakPtr()),
          {
              .bypass_icon_generation_if_no_url = true,
              .fail_all_if_any_fail = true,
              .defer_icon_fetching = true,
          });
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

  const WebApp* app = lock_->registrar().GetAppById(
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
      *install_info_, lock_->shared_web_contents(),
      base::BindOnce(&FetchManifestAndUpdateCommand::OnIconsFetched,
                     weak_factory_.GetWeakPtr()));
}

void FetchManifestAndUpdateCommand::OnIconsFetched() {
  if (manifest_to_install_info_job_->icon_download_result() ==
      IconsDownloadedResult::kAbortedDueToFailure) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            FetchManifestAndUpdateResult::kIconDownloadError);
    return;
  }

  install_info_->trusted_icons = install_info_->manifest_icons;
  install_info_->trusted_icon_bitmaps = install_info_->icon_bitmaps;

  lock_->install_finalizer().FinalizeUpdate(
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

  const WebApp* app = lock_->registrar().GetAppById(app_id);
  CHECK(app);
  if (app->pending_update_info().has_value()) {
    {
      ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
      update->UpdateApp(app_id)->SetPendingUpdateInfo(std::nullopt);
    }
    lock_->registrar().NotifyPendingUpdateInfoChanged(
        app_id, /*pending_update_available=*/false,
        WebAppRegistrar::PendingUpdateInfoChangePassKey());
  }

  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          FetchManifestAndUpdateResult::kSuccess);
}

}  // namespace web_app
