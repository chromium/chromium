// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
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
    std::optional<base::Time> previous_time_for_silent_icon_update,
    bool force_trusted_silent_update,
    FetchManifestAndUpdateCallback callback)
    : WebAppCommand<SharedWebContentsLock,
                    FetchManifestAndUpdateCompletionInfo>(
          "FetchManifestAndUpdateCommand",
          SharedWebContentsLockDescription(),
          base::BindOnce([](FetchManifestAndUpdateCompletionInfo info) {
            base::UmaHistogramEnumeration(
                "WebApp.FetchManifestAndUpdate.Result", info.result);
            return info;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          FetchManifestAndUpdateCompletionInfo(
              FetchManifestAndUpdateResult::kShutdown,
              std::nullopt)),
      install_url_(install_url),
      expected_manifest_id_(expected_manifest_id),
      previous_time_for_silent_icon_update_(
          previous_time_for_silent_icon_update),
      force_trusted_silent_update_(force_trusted_silent_update) {
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
      CompleteAndSelfDestruct(
          CommandResult::kSuccess,
          FetchManifestAndUpdateCompletionInfo(
              FetchManifestAndUpdateResult::kUrlLoadingError));
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
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kManifestRetrievalError));
    return;
  }
  CHECK(result.value());

  blink::mojom::ManifestPtr manifest = result.value()->Clone();
  if (blink::IsEmptyManifest(*manifest)) {
    GetMutableDebugValue().Set("manifest_error", "empty");
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kInvalidManifest));
    return;
  }

  if (manifest->id != expected_manifest_id_) {
    GetMutableDebugValue().Set("foundmanifest_id",
                               manifest->id.possibly_invalid_spec());
    GetMutableDebugValue().Set("manifest_error", "manifest_id_mismatch");
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kInvalidManifest));
    return;
  }

  if (!manifest->has_valid_specified_start_url) {
    GetMutableDebugValue().Set("manifest_error", "no_specified_start_url");
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kInvalidManifest));
    return;
  }

  bool has_any_icon = false;
  for (const auto& icon : manifest->icons) {
    if (std::ranges::contains(
            icon.purpose, blink::mojom::ManifestImageResource_Purpose::ANY)) {
      has_any_icon = true;
      break;
    }
  }
  if (!has_any_icon) {
    GetMutableDebugValue().Set("manifest_error", "no_any_icon");
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kInvalidManifest));
    return;
  }

  app_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *app_lock_,
      {GenerateAppIdFromManifestId(expected_manifest_id_)},
      base::BindOnce(&FetchManifestAndUpdateCommand::OnAppLockAcquired,
                     weak_factory_.GetWeakPtr(), std::move(manifest)));
}

void FetchManifestAndUpdateCommand::OnAppLockAcquired(
    blink::mojom::ManifestPtr manifest) {
  if (!app_lock_->registrar().AppMatches(
          GenerateAppIdFromManifestId(expected_manifest_id_),
          WebAppFilter::InstalledInChrome())) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        FetchManifestAndUpdateCompletionInfo(
            FetchManifestAndUpdateResult::kAppNotInstalled));
    return;
  }

  ManifestUpdateJob::Options options;
  options.previous_time_for_silent_icon_update =
      previous_time_for_silent_icon_update_;
  options.force_silent_update_identity = force_trusted_silent_update_;
  options.bypass_icon_generation_if_no_url = true;
  options.fail_if_any_icon_download_fails = true;
  options.use_manifest_icons_as_trusted = force_trusted_silent_update_;

  manifest_update_job_ = ManifestUpdateJob::CreateAndStart(
      *Profile::FromBrowserContext(
          app_lock_->shared_web_contents().GetBrowserContext()),
      app_lock_.get(), app_lock_.get(), &app_lock_->shared_web_contents(),
      GetMutableDebugValue().EnsureDict("manifest_update_job"),
      std::move(manifest), data_retriever_.get(), &app_lock_->clock(),
      base::BindOnce(&FetchManifestAndUpdateCommand::OnUpdateJobCompleted,
                     weak_factory_.GetWeakPtr()),
      std::move(options));
}

void FetchManifestAndUpdateCommand::OnUpdateJobCompleted(
    ManifestUpdateJobResultWithTimestamp result_info) {
  FetchManifestAndUpdateResult result;
  CommandResult command_result = CommandResult::kSuccess;
  switch (result_info.result()) {
    case ManifestUpdateJobResult::kNoUpdateNeeded:
      result = FetchManifestAndUpdateResult::kSuccessNoUpdateDetected;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kSilentlyUpdated:
    case ManifestUpdateJobResult::kSilentlyUpdatedDueToSmallIconComparison:
      result = FetchManifestAndUpdateResult::kSuccess;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppOnlyHasSecurityUpdate:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges:
      result = FetchManifestAndUpdateResult::kSuccess;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kIconDownloadFailed:
      result = FetchManifestAndUpdateResult::kIconDownloadError;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kIconReadFromDiskFailed:
      result = FetchManifestAndUpdateResult::kIconDownloadError;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kManifestConversionFailed:
      result = FetchManifestAndUpdateResult::kManifestToWebAppInstallInfoFailed;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kInstallFinalizeFailed:
      result = FetchManifestAndUpdateResult::kInstallationError;
      command_result = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kAppNotAllowedToUpdate:
      result = FetchManifestAndUpdateResult::kAppNotInstalled;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kUserNavigated:
      result = FetchManifestAndUpdateResult::kPrimaryPageChanged;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestUpdateJobResult::kWebContentsDestroyed:
      result = FetchManifestAndUpdateResult::kManifestRetrievalError;
      command_result = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kIconWriteToDiskFailed:
      result = FetchManifestAndUpdateResult::kIconDownloadError;
      command_result = CommandResult::kFailure;
      break;
  }

  CompleteAndSelfDestruct(command_result,
                          FetchManifestAndUpdateCompletionInfo(
                              result, result_info.time_for_icon_diff_check()));
}

}  // namespace web_app
