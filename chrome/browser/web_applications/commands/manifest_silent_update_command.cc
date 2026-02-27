// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ostream>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_closures.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/model/web_app_comparison.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/image_visual_diff.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCommandStage stage) {
  switch (stage) {
    case ManifestSilentUpdateCommandStage::kNotStarted:
      return os << "kNotStarted";
    case ManifestSilentUpdateCommandStage::kFetchingNewManifestData:
      return os << "kFetchingNewManifestData";
    case ManifestSilentUpdateCommandStage::kAcquiringAppLock:
      return os << "kAcquiringAppLock";
    case ManifestSilentUpdateCommandStage::kManifestUpdateJob:
      return os << "kManifestUpdateJob";
  }
}

ManifestSilentUpdateCommand::ManifestSilentUpdateCommand(
    content::WebContents& web_contents,
    std::optional<base::Time> previous_time_for_silent_icon_update,
    CompletedCallback callback)
    : WebAppCommand<NoopLock, ManifestSilentUpdateCompletionInfo>(
          "ManifestSilentUpdateCommand",
          NoopLockDescription(),
          base::BindOnce([](ManifestSilentUpdateCompletionInfo
                                completion_info) {
            base::UmaHistogramEnumeration(
                "Webapp.Update.ManifestSilentUpdateCheckResult",
                completion_info.result);
            return completion_info;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          ManifestSilentUpdateCompletionInfo(
              ManifestSilentUpdateCheckResult::kSystemShutdown)),
      web_contents_(web_contents.GetWeakPtr()),
      previous_time_for_silent_icon_update_(
          previous_time_for_silent_icon_update) {
  Observe(web_contents_.get());
  SetStage(ManifestSilentUpdateCommandStage::kNotStarted);
}

ManifestSilentUpdateCommand::~ManifestSilentUpdateCommand() = default;

void ManifestSilentUpdateCommand::PrimaryPageChanged(content::Page& page) {
  auto error = ManifestSilentUpdateCheckResult::kUserNavigated;
  GetMutableDebugValue().Set(
      "primary_page_changed",
      page.GetMainDocument().GetLastCommittedURL().possibly_invalid_spec());
  if (IsStarted()) {
    CompleteCommandAndSelfDestruct(FROM_HERE, error);
    return;
  }
  GetMutableDebugValue().Set("failed_before_start", true);
  failed_before_start_ = error;
}

void ManifestSilentUpdateCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  lock_ = std::move(lock);
  if (failed_before_start_.has_value()) {
    CompleteCommandAndSelfDestruct(FROM_HERE, *failed_before_start_);
    return;
  }

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed);
    return;
  }
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  SetStage(ManifestSilentUpdateCommandStage::kFetchingNewManifestData);
  // This explicitly does NOT ask to download the primary icon, to prevent
  // network usage and because we check for the icon downloading later.
  // However, kValidManifestIgnoreDisplay does still check for the existence of
  // a primary icon url.
  // TODO(https://crbug.com/468037835): Make this criteria logic not need the
  // whole InstallableManager layer here if possible.
  webapps::InstallableParams params;
  params.check_eligibility = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &ManifestSilentUpdateCommand::OnManifestFetchedAcquireAppLock,
          GetWeakPtr()),
      params);
}

void ManifestSilentUpdateCommand::SetStage(
    ManifestSilentUpdateCommandStage stage) {
  stage_ = stage;
  GetMutableDebugValue().Set("stage", base::ToString(stage));
}

void ManifestSilentUpdateCommand::OnManifestFetchedAcquireAppLock(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed);
    return;
  }

  GetMutableDebugValue().Set("installable_status",
                             base::ToString(installable_status));

  if (!opt_manifest) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kInvalidManifest);
    return;
  }

  // Note: These are filtered below as we require a specified start_url and
  // name.
  bool manifest_is_default = blink::IsDefaultManifest(
      *opt_manifest, web_contents_->GetLastCommittedURL());
  GetMutableDebugValue().Set("manifest_is_default", manifest_is_default);
  GetMutableDebugValue().Set(
      "manifest_url", opt_manifest->manifest_url.possibly_invalid_spec());
  GetMutableDebugValue().Set("manifest_id",
                             opt_manifest->id.possibly_invalid_spec());
  GetMutableDebugValue().Set("manifest_start_url",
                             opt_manifest->start_url.possibly_invalid_spec());

  if (installable_status != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kInvalidManifest);
    return;
  }
  if (opt_manifest->icons.empty()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kInvalidManifest);
    return;
  }

  CHECK(opt_manifest->id.is_valid());
  app_id_ = GenerateAppIdFromManifestId(opt_manifest->id);

  SetStage(ManifestSilentUpdateCommandStage::kAcquiringAppLock);
  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(lock_), *app_lock_, {app_id_},
      base::BindOnce(&ManifestSilentUpdateCommand::OnAppLockAcquired,
                     GetWeakPtr(), std::move(opt_manifest)));
}

void ManifestSilentUpdateCommand::OnAppLockAcquired(
    blink::mojom::ManifestPtr manifest) {
  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed);
    return;
  }
  SetStage(ManifestSilentUpdateCommandStage::kManifestUpdateJob);
  ManifestUpdateJob::Options options;
  options.fail_if_any_icon_download_fails = true;
  options.record_icon_results_on_update = true;
  options.use_manifest_icons_as_trusted =
      app_lock_->registrar().AppMatches(app_id_, WebAppFilter::IsTrusted());
  options.previous_time_for_silent_icon_update =
      previous_time_for_silent_icon_update_;

  manifest_update_job_ = ManifestUpdateJob::CreateAndStart(
      *Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      app_lock_.get(), app_lock_.get(), web_contents_.get(),
      GetMutableDebugValue().EnsureDict("manifest_update_job"),
      std::move(manifest), data_retriever_.get(), &app_lock_->clock(),
      base::BindOnce(&ManifestSilentUpdateCommand::OnUpdateJobCompleted,
                     GetWeakPtr()),
      std::move(options));
}

void ManifestSilentUpdateCommand::OnUpdateJobCompleted(
    ManifestUpdateJobResultWithTimestamp result_info) {
  completion_info_.time_for_icon_diff_check =
      result_info.time_for_icon_diff_check();

  ManifestSilentUpdateCheckResult check_result;
  switch (result_info.result()) {
    case ManifestUpdateJobResult::kNoUpdateNeeded:
      check_result = ManifestSilentUpdateCheckResult::kAppUpToDate;
      break;
    case ManifestUpdateJobResult::kSilentlyUpdated:
      check_result = ManifestSilentUpdateCheckResult::kAppSilentlyUpdated;
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppOnlyHasSecurityUpdate:
      check_result = ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate;
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle:
      check_result =
          ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle;
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges:
      check_result =
          ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges;
      break;
    case ManifestUpdateJobResult::kInstallFinalizeFailed:
      check_result =
          ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall;
      break;
    case ManifestUpdateJobResult::kIconDownloadFailed:
      check_result =
          ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError;
      break;
    case ManifestUpdateJobResult::kIconWriteToDiskFailed:
      check_result =
          ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed;
      break;
    case ManifestUpdateJobResult::kIconReadFromDiskFailed:
      check_result = ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed;
      break;
    case ManifestUpdateJobResult::kManifestConversionFailed:
      check_result =
          ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError;
      break;
    case ManifestUpdateJobResult::kAppNotAllowedToUpdate:
      check_result = ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate;
      break;
    case ManifestUpdateJobResult::kWebContentsDestroyed:
      check_result = ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed;
      break;
    case ManifestUpdateJobResult::kUserNavigated:
      check_result = ManifestSilentUpdateCheckResult::kUserNavigated;
      break;
    case ManifestUpdateJobResult::kSilentlyUpdatedDueToSmallIconComparison:
      check_result = ManifestSilentUpdateCheckResult::
          kAppSilentlyUpdatedDueToSmallIconComparison;
      break;
  }

  CompleteCommandAndSelfDestruct(FROM_HERE, check_result);
}

void ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct(
    base::Location location,
    ManifestSilentUpdateCheckResult check_result) {
  Observe(nullptr);

  bool record_update;
  CommandResult command_result;
  switch (check_result) {
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
    case ManifestSilentUpdateCheckResult::
        kAppSilentlyUpdatedDueToSmallIconComparison:
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
      record_update = true;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
    case ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed:
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
    case ManifestSilentUpdateCheckResult::kUserNavigated:
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      record_update = false;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
      record_update = false;
      command_result = CommandResult::kFailure;
      break;
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
      NOTREACHED() << "The value should only be specified in the constructor "
                      "and never given to this method.";
  }
  if (record_update && app_lock_) {
    app_lock_->sync_bridge().SetAppManifestUpdateTime(app_id_,
                                                      app_lock_->clock().Now());
  }

  completion_info_.result = check_result;
  GetMutableDebugValue().Set("completion_info",
                             completion_info_.ToDebugValue());
  CompleteAndSelfDestruct(command_result, std::move(completion_info_),
                          location);
}

bool ManifestSilentUpdateCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

}  // namespace web_app
