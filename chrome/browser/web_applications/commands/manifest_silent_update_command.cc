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
#include "base/containers/contains.h"
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
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/model/web_app_comparison.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
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
#include "chrome/common/chrome_features.h"
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
namespace {

blink::mojom::ManifestImageResource_Purpose
ConvertIconPurposeToManifestImagePurpose(apps::IconInfo::Purpose app_purpose) {
  switch (app_purpose) {
    case apps::IconInfo::Purpose::kAny:
      return blink::mojom::ManifestImageResource_Purpose::ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return blink::mojom::ManifestImageResource_Purpose::MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return blink::mojom::ManifestImageResource_Purpose::MASKABLE;
  }
}

void CopyIconsToPendingUpdateInfo(
    const std::vector<apps::IconInfo>& icon_infos,
    google::protobuf::RepeatedPtrField<sync_pb::WebAppIconInfo>*
        destination_icons) {
  for (const auto& icon_info : icon_infos) {
    sync_pb::WebAppIconInfo* pending_icon = destination_icons->Add();

    pending_icon->set_url(icon_info.url.spec());
    sync_pb::WebAppIconInfo_Purpose icon_purpose =
        IconInfoPurposeToSyncPurpose(icon_info.purpose);
    pending_icon->set_purpose(icon_purpose);
    if (icon_info.square_size_px.has_value()) {
      pending_icon->set_size_in_px(icon_info.square_size_px.value());
    }
  }
}

constexpr base::TimeDelta kDelayForTenPercentIconDiffSilentUpdate =
    base::Days(1);
constexpr const char kBypassSmallIconDiffThrottle[] =
    "bypass-small-icon-diff-throttle";

// Returns whether the throttle for less than 10% icon diffs will be applied.
// This returns false if:
// 1. This is the first silent icon update that might be triggered.
// 2. The command line flag to skip the throttle has been applied.
// 3. If less than (or equal to) 24 hours has passed since the last update was
// applied for an icon that was less than 10% different.
bool ThrottleForSilentIconUpdates(
    std::optional<base::Time> previous_time_for_silent_icon_update,
    base::Time new_icon_check_time) {
  if (!previous_time_for_silent_icon_update.has_value()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kBypassSmallIconDiffThrottle)) {
    return false;
  }

  return (new_icon_check_time <= (*previous_time_for_silent_icon_update +
                                  kDelayForTenPercentIconDiffSilentUpdate));
}

google::protobuf::RepeatedPtrField<proto::DownloadedIconSizeInfo>
GetIconSizesPerPurposeForBitmaps(const IconBitmaps& icon_bitmaps) {
  google::protobuf::RepeatedPtrField<proto::DownloadedIconSizeInfo>
      purpose_size_maps;

  proto::DownloadedIconSizeInfo* downloaded_icon_info_any =
      purpose_size_maps.Add();
  downloaded_icon_info_any->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  for (const auto& [size, _] : icon_bitmaps.any) {
    downloaded_icon_info_any->add_icon_sizes(size);
  }

  proto::DownloadedIconSizeInfo* downloaded_icon_info_maskable =
      purpose_size_maps.Add();
  downloaded_icon_info_maskable->set_purpose(
      sync_pb::WebAppIconInfo_Purpose_MASKABLE);
  for (const auto& [size, _] : icon_bitmaps.maskable) {
    downloaded_icon_info_maskable->add_icon_sizes(size);
  }

  proto::DownloadedIconSizeInfo* downloaded_icon_info_monochrome =
      purpose_size_maps.Add();
  downloaded_icon_info_monochrome->set_purpose(
      sync_pb::WebAppIconInfo_Purpose_MONOCHROME);
  for (const auto& [size, _] : icon_bitmaps.monochrome) {
    downloaded_icon_info_monochrome->add_icon_sizes(size);
  }

  CHECK_EQ(static_cast<size_t>(purpose_size_maps.size()), kIconPurposes.size());

  return purpose_size_maps;
}

}  // namespace

bool IsAppUpdated(ManifestSilentUpdateCheckResult result) {
  switch (result) {
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
    case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
    case ManifestSilentUpdateCheckResult::kUserNavigated:
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      return false;
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
      return true;
  }
}

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCommandStage stage) {
  switch (stage) {
    case ManifestSilentUpdateCommandStage::kNotStarted:
      return os << "kNotStarted";
    case ManifestSilentUpdateCommandStage::kFetchingNewManifestData:
      return os << "kFetchingNewManifestData";
    case ManifestSilentUpdateCommandStage::kLoadingExistingManifestData:
      return os << "kLoadingExistingManifestData";
    case ManifestSilentUpdateCommandStage::kAcquiringAppLock:
      return os << "kAcquiringAppLock";
    case ManifestSilentUpdateCommandStage::kConstructingWebAppInfo:
      return os << "kConstructingWebAppInfo";
    case ManifestSilentUpdateCommandStage::kLoadingExistingAndNewManifestIcons:
      return os << "kLoadingExistingAndNewManifestIcons";
    case ManifestSilentUpdateCommandStage::kComparingManifestData:
      return os << "kComparingManifestData";
    case ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges:
      return os << "kFinalizingSilentManifestChanges";
    case ManifestSilentUpdateCommandStage::
        kWritingPendingUpdateIconBitmapsToDisk:
      return os << "kWritingPendingUpdateIconBitmapsToDisk";
    case web_app::ManifestSilentUpdateCommandStage::
        kDeletingPendingUpdateIconsFromDisk:
      return os << "kDeletingPendingUpdateIconsFromDisk";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult result) {
  switch (result) {
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
      return os << "kAppUpdateFailedDuringInstall";
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
      return os << "kAppSilentlyUpdated";
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
      return os << "kAppUpToDate";
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
      return os << "kIconReadFromDiskFailed";
    case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
      return os << "kWebContentsDestroyed";
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
      return os << "kAppOnlyHasSecurityUpdate";
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
      return os << "kAppHasNonSecurityAndSecurityChanges";
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
      return os << "kPendingIconWriteToDiskFailed";
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
      return os << "kInvalidManifest";
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
      return os << "kInvalidPendingUpdateInfo";
    case ManifestSilentUpdateCheckResult::kUserNavigated:
      return os << "kUserNavigated";
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
      return os << "kManifestToWebAppInstallInfoError";
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
      return os << "kAppHasSecurityUpdateDueToThrottle";
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      return os << "kAppNotAllowedToUpdate";
  }
}

ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo() =
    default;
ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo(
    ManifestSilentUpdateCheckResult result)
    : result(result) {}
ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo(
    ManifestSilentUpdateCompletionInfo&&) = default;
ManifestSilentUpdateCompletionInfo&
ManifestSilentUpdateCompletionInfo::operator=(
    ManifestSilentUpdateCompletionInfo&&) = default;

base::Value::Dict ManifestSilentUpdateCompletionInfo::ToDebugValue() {
  return base::Value::Dict()
      .Set("result", base::ToString(result))
      .Set("time_for_icon_diff_check",
           time_for_icon_diff_check.has_value()
               ? base::TimeFormatShortDateAndTime(
                     time_for_icon_diff_check.value())
               : base::EmptyString16());
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
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  SetStage(ManifestSilentUpdateCommandStage::kFetchingNewManifestData);
  webapps::InstallableParams params;
  params.valid_primary_icon = true;
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
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
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
      base::BindOnce(
          &ManifestSilentUpdateCommand::StartManifestToInstallInfoJob,
          weak_factory_.GetWeakPtr(), std::move(opt_manifest)));
}

void ManifestSilentUpdateCommand::StartManifestToInstallInfoJob(
    blink::mojom::ManifestPtr opt_manifest) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kAcquiringAppLock);
  CHECK(app_lock_->IsGranted());

  bool is_trusted_app_for_manifest_installs =
      app_lock_->registrar().AppMatches(app_id_, WebAppFilter::IsTrusted());

  // Only allow apps that are either trusted, or they open in a dedicated
  // window.
  if (!is_trusted_app_for_manifest_installs &&
      !app_lock_->registrar().AppMatches(
          app_id_, WebAppFilter::OpensInDedicatedWindow())) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate);
    return;
  }

  WebAppInstallInfoConstructOptions construct_options;
  construct_options.fail_all_if_any_fail = true;
  construct_options.defer_icon_fetching = true;
  construct_options.record_icon_results_on_update = true;
  construct_options.use_manifest_icons_as_trusted =
      is_trusted_app_for_manifest_installs;

  // The `background_installation` and `install_source` fields here don't matter
  // because this is not logged anywhere.
  SetStage(ManifestSilentUpdateCommandStage::kConstructingWebAppInfo);
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *opt_manifest, *data_retriever_.get(),
          /*background_installation=*/false,
          webapps::WebappInstallSource::MENU_BROWSER_TAB, web_contents_,
          [](IconUrlSizeSet&) {},
          *GetMutableDebugValue().EnsureDict("manifest_to_install_info_job"),
          base::BindOnce(
              &ManifestSilentUpdateCommand::OnWebAppInfoCreatedFromManifest,
              GetWeakPtr()),
          construct_options);
}

void ManifestSilentUpdateCommand::OnWebAppInfoCreatedFromManifest(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kConstructingWebAppInfo);
  CHECK(!new_install_info_);
  CHECK(app_lock_);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE, ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  if (!install_info) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE,
        ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError);
    return;
  }

  new_install_info_ = std::move(install_info);

  // If there are no changes to the manifest metadata (ignoring icon bitmaps),
  // exit early.
  const WebApp* app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(app);

  web_app_comparison_ =
      WebAppComparison::CompareWebApps(*app, *new_install_info_);
  GetMutableDebugValue().Set("web_app_diff", web_app_comparison_.ToDict());

  // Store the conditions for which a silent update of the app's identity is
  // allowed. This is usually possible if:
  // 1. The app is a trusted one, installed via policy or default installed.
  // 2. The app has generated icons, but was sync installed and is within the
  // time frame in which it can be fixed, and there are no other changes in the
  // app.
  bool is_trusted_install =
      base::FeatureList::IsEnabled(
          features::kSilentPolicyAndDefaultAppUpdating) &&
      app_lock_->registrar().AppMatches(app_id_, WebAppFilter::IsTrusted());
  bool can_fix_generated_icons =
      app->is_generated_icon() &&
      app->latest_install_source() == webapps::WebappInstallSource::SYNC &&
      generated_icon_fix_util::IsWithinFixTimeWindow(*app) &&
      web_app_comparison_.ExistingAppWithoutPendingEqualsNewUpdate();
  silently_update_app_identity_ =
      (is_trusted_install || can_fix_generated_icons);

  // Store the time window for generated icons being fixed in the web app.
  if (can_fix_generated_icons) {
    ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
    generated_icon_fix_util::EnsureFixTimeWindowStarted(
        *app_lock_, update, app_id_,
        proto::GENERATED_ICON_FIX_SOURCE_MANIFEST_UPDATE);
  }
  GetMutableDebugValue().Set("silently_update_identity",
                             silently_update_app_identity_);

  // Silent app updates based on the generated icon fix requires the updating
  // algorithm to proceed, so perform all functions that exit early here in case
  // that doesn't need to happen.
  if (!can_fix_generated_icons) {
    // First, handle the case where the existing app (without the pending
    // update) matches the new install, so we can clear the pending info (if
    // there was any) and return early. The only case where this doesn't happen
    // is if the app needs to be updated silently (like for fixing generated
    // icons), in which case, allow the update to proceed silently.
    if (web_app_comparison_.ExistingAppWithoutPendingEqualsNewUpdate()) {
      WritePendingUpdateInfoThenComplete(
          /*pending_update=*/std::nullopt,
          ManifestSilentUpdateCheckResult::kAppUpToDate);
      return;
    }

    // Exit early if the existing pending update info matches the seen data.
    // Instead of writing pending update info, we simply exit directly.
    if (web_app_comparison_.ExistingAppWithPendingEqualsNewUpdate()) {
      CompleteCommandAndSelfDestruct(
          FROM_HERE, ManifestSilentUpdateCheckResult::kAppUpToDate);
      return;
    }
  }

  // After this line, we know that something in the system needs to update.

  // If it's only a name change, simply skip to the end to write the pending
  // update info.
  // Skip the case where the new name is empty - we will pretend it is the
  // same and update the rest of the information.
  if (web_app_comparison_.IsNameChangeOnly() &&
      !silently_update_app_identity_) {
    proto::PendingUpdateInfo update;
    update.set_name(base::UTF16ToUTF8(new_install_info_->title.value()));
    WritePendingUpdateInfoThenComplete(
        std::move(update),
        ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
    return;
  }

  // Next, we are loading icons from disk and the network.
  base::ConcurrentClosures barrier;
  // The existing icons always need to be read from disk, as we need to do the
  // 10% comparison even if the urls change.
  app_lock_->icon_manager().ReadAllIcons(
      app_id_, base::BindOnce(&ManifestSilentUpdateCommand::OnAppIconsLoaded,
                              GetWeakPtr())
                   .Then(barrier.CreateClosure()));
  if (web_app_comparison_.shortcut_menu_item_infos_equality()) {
    // Since the shortcut menu items did not change, load the existing icons
    // from **disk** for the silent update (which acts like a re-install).
    app_lock_->icon_manager().ReadAllShortcutsMenuIcons(
        app_id_,
        base::BindOnce(&ManifestSilentUpdateCommand::OnShortcutIconsLoaded,
                       GetWeakPtr())
            .Then(barrier.CreateClosure()));
  }
  // Meanwhile, skip downloading icons from the network that we know didn't
  // change, and thus we'll just use what we have on disk.
  // If `silently_update_app_identity_` is true, we need all of the new product
  // icons for the update, so fetch them.
  IconUrlExtractionOptions icon_fetch_options{
      .product_icons = !web_app_comparison_.primary_icons_equality() ||
                       silently_update_app_identity_,
      .shortcut_menu_item_icons =
          !web_app_comparison_.shortcut_menu_item_infos_equality()};
  manifest_to_install_info_job_->FetchIcons(
      *new_install_info_, *web_contents_, barrier.CreateClosure(),
      /*icon_url_modifications=*/std::nullopt, icon_fetch_options);

  std::move(barrier).Done(base::BindOnce(
      &ManifestSilentUpdateCommand::FinalizeUpdateIfSilentChangesExist,
      weak_factory_.GetWeakPtr()));

  SetStage(
      ManifestSilentUpdateCommandStage::kLoadingExistingAndNewManifestIcons);
}

void ManifestSilentUpdateCommand::FinalizeUpdateIfSilentChangesExist() {
  CHECK_EQ(
      stage_,
      ManifestSilentUpdateCommandStage::kLoadingExistingAndNewManifestIcons);
  SetStage(ManifestSilentUpdateCommandStage::kComparingManifestData);

  // Copy over any icons that did not have manifest changes, and thus we loaded
  // from disk to avoid hitting the network
  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(new_install_info_);
  if (web_app_comparison_.shortcut_menu_item_infos_equality()) {
    new_install_info_->shortcuts_menu_item_infos =
        web_app->shortcuts_menu_item_infos();
    new_install_info_->shortcuts_menu_icon_bitmaps =
        existing_shortcuts_menu_icon_bitmaps_;
  }

  // Update app silently and exit early if allowed.
  if (silently_update_app_identity_) {
    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(
            [](const webapps::AppId& expected_app_id,
               const webapps::AppId& app_id, webapps::InstallResultCode code) {
              CHECK_EQ(expected_app_id, app_id);
              // Transform the install result code to the command result.
              if (!IsSuccess(code)) {
                return ManifestSilentUpdateCheckResult::
                    kAppUpdateFailedDuringInstall;
              }
              return ManifestSilentUpdateCheckResult::kAppSilentlyUpdated;
            },
            app_id_)
            .Then(base::BindOnce(
                &ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct,
                GetWeakPtr(), FROM_HERE)));
    return;
  }

  silent_update_required_ =
      !web_app_comparison_.other_fields_equality() ||
      !web_app_comparison_.shortcut_menu_item_infos_equality();
  GetMutableDebugValue().Set("silent_update_required",
                             base::ToString(silent_update_required_));

  // If the app's icons can be silently updated, they should not be reverted.
  if (web_app_comparison_.primary_icons_equality()) {
    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;
    new_install_info_->is_generated_icon = web_app->is_generated_icon();
  }

  // Both of these cases should have already been handled & exited early.
  CHECK(!web_app_comparison_.ExistingAppWithoutPendingEqualsNewUpdate());
  CHECK(!web_app_comparison_.IsNameChangeOnly());

  std::optional<proto::PendingUpdateInfo> pending_update_info;
  if (!web_app_comparison_.name_equality()) {
    pending_update_info = proto::PendingUpdateInfo();
    pending_update_info->set_name(
        base::UTF16ToUTF8(new_install_info_->title.value()));
    new_install_info_->title = base::UTF8ToUTF16(web_app->untranslated_name());
  }

  // Exit early if there are no icon url changes (and only silent update changes
  // with possible name changes).
  if (web_app_comparison_.primary_icons_equality()) {
    // The case where only the name changes and nothing else is handled before
    // fetching icons.
    CHECK(silent_update_required_);

    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(
            &ManifestSilentUpdateCommand::UpdateFinalizedWritePendingInfo,
            GetWeakPtr(), std::move(pending_update_info)));
    return;
  }
  // After this line, the icon urls have changed. Those icons are either stored
  // in PendingUpdateInfo if there is a more than 10% diff, or silently updated
  // otherwise.

  CHECK(!new_install_info_->trusted_icons.empty());

  // Fail early if the icons didn't download correctly
  if (manifest_to_install_info_job_->icon_download_result() !=
          IconsDownloadedResult::kCompleted ||
      new_install_info_->trusted_icon_bitmaps.empty()) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE,
        ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError);
    return;
  }

  static constexpr int kLogoSizeInDialog = 96;
  // Now, fetch the first icon at or larger than `kLogoSizeInDialog` for both
  // the old and new icon.
  // Our icon generation logic should always generate an icon at this size or
  // larger.
  SkBitmap old_trusted_icon = [&]() {
    std::optional<apps::IconInfo> trusted_icon =
        app_lock_->registrar().GetSingleTrustedAppIconForSecuritySurfaces(
            app_id_, kLogoSizeInDialog);
    // Some apps don't have any icons, and are all generated.
    if (!trusted_icon.has_value()) {
      return SkBitmap();
    }
    blink::mojom::ManifestImageResource_Purpose purpose =
        ConvertIconPurposeToManifestImagePurpose(trusted_icon->purpose);
    auto old_bitmaps_to_use =
        existing_trusted_icon_bitmaps_.GetBitmapsForPurpose(purpose);
    if (old_bitmaps_to_use.empty()) {
      return SkBitmap();
    }
    auto old_icon_it = old_bitmaps_to_use.lower_bound(kLogoSizeInDialog);
    CHECK(old_icon_it != old_bitmaps_to_use.end());
    return old_icon_it->second;
  }();

  apps::IconInfo::Purpose purpose = new_install_info_->trusted_icons[0].purpose;
  SkBitmap new_trusted_icon = [&]() {
    const std::map<SquareSizePx, SkBitmap>& icons =
        new_install_info_->trusted_icon_bitmaps.GetBitmapsForPurpose(
            ConvertIconPurposeToManifestImagePurpose(purpose));
    auto icon_it = icons.lower_bound(kLogoSizeInDialog);
    CHECK(icon_it != icons.end());
    return icon_it->second;
  }();

  base::Time current_time = app_lock_->clock().Now();

  // TODO(crbug.com/437379182): HasMoreThanTenPercentImageDiff() should happen
  // in a different thread.
  // Only update icons silently if the icons are less than ten percent in
  // difference in a pixel by pixel comparison, and if icon updates shouldn't be
  // throttled.
  bool silent_icon_update_throttled = ThrottleForSilentIconUpdates(
      previous_time_for_silent_icon_update_, current_time);
  bool silent_icon_update =
      !HasMoreThanTenPercentImageDiff(&old_trusted_icon, &new_trusted_icon) &&
      !silent_icon_update_throttled;
  if (silent_icon_update) {
    completion_info_.time_for_icon_diff_check = current_time;
  }

  // Case: The icons are being set in the PendingUpdateInfo to be updated later.
  if (old_trusted_icon.empty() || !silent_icon_update) {
    if (!pending_update_info.has_value()) {
      pending_update_info = proto::PendingUpdateInfo();
    }
    GetMutableDebugValue().Set("greater_than_ten_percent", true);
    CopyIconsToPendingUpdateInfo(new_install_info_->trusted_icons,
                                 pending_update_info->mutable_trusted_icons());
    CopyIconsToPendingUpdateInfo(new_install_info_->manifest_icons,
                                 pending_update_info->mutable_manifest_icons());

    *pending_update_info->mutable_downloaded_trusted_icons() =
        GetIconSizesPerPurposeForBitmaps(
            new_install_info_->trusted_icon_bitmaps);
    *pending_update_info->mutable_downloaded_manifest_icons() =
        GetIconSizesPerPurposeForBitmaps(new_install_info_->icon_bitmaps);
    pending_trusted_icon_bitmaps_ = new_install_info_->trusted_icon_bitmaps;
    pending_manifest_icon_bitmaps_ = new_install_info_->icon_bitmaps;

    // Reset the security sensitive icons from the ones loaded from disk.
    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;

    // We can not update generated icons if we are outside of the time window to
    // silently update them - so we must persist this state.
    new_install_info_->is_generated_icon = web_app->is_generated_icon();
  } else {
    // Silent updates are allowed if the icons are less than 10% diff.
    silent_update_required_ = true;
    GetMutableDebugValue().Set("silent_update_required",
                               base::ToString(silent_update_required_));
  }

  if (silent_update_required_) {
    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(
            &ManifestSilentUpdateCommand::UpdateFinalizedWritePendingInfo,
            GetWeakPtr(), std::move(pending_update_info)));
  } else {
    // If there is no silent update, that means it MUST be pending update. Also
    // measure if the pending update is because the icon updates were throttled.
    CHECK(pending_update_info);
    ManifestSilentUpdateCheckResult result_for_icon_changes =
        silent_icon_update_throttled
            ? ManifestSilentUpdateCheckResult::
                  kAppHasSecurityUpdateDueToThrottle
            : ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate;
    WritePendingUpdateInfoThenComplete(pending_update_info,
                                       result_for_icon_changes);
  }
}

void ManifestSilentUpdateCommand::UpdateFinalizedWritePendingInfo(
    std::optional<proto::PendingUpdateInfo> pending_update_info,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kComparingManifestData);
  CHECK(silent_update_required_);
  SetStage(ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges);
  GetMutableDebugValue().Set("silent_update_install_code",
                             base::ToString(code));
  if (!IsSuccess(code)) {
    CompleteCommandAndSelfDestruct(
        FROM_HERE,
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
    return;
  }
  CHECK_EQ(app_id_, app_id);
  CHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);

  // Always write the pending update info so we clear it if it was already
  // populated.
  ManifestSilentUpdateCheckResult result =
      pending_update_info.has_value()
          ? ManifestSilentUpdateCheckResult::
                kAppHasNonSecurityAndSecurityChanges
          : ManifestSilentUpdateCheckResult::kAppSilentlyUpdated;
  WritePendingUpdateInfoThenComplete(std::move(pending_update_info), result);
}

void ManifestSilentUpdateCommand::WritePendingUpdateInfoThenComplete(
    std::optional<proto::PendingUpdateInfo> pending_update,
    ManifestSilentUpdateCheckResult result) {
  // Evaluate before `pending_update` is std::move'd.
  enum class IconOperation {
    kNone,
    kWriteIcons,
    kDeleteIcons
  } icon_operation = IconOperation::kNone;

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(web_app);

  // Exit early if there is no change to the pending update info.
  if (web_app->pending_update_info() == pending_update) {
    CompleteCommandAndSelfDestruct(FROM_HERE, result);
    return;
  }

  // Determine the icon operation if the pending update info is changing.
  bool new_pending_update_has_icons =
      pending_update.has_value() && !pending_update->trusted_icons().empty();
  bool old_pending_update_has_icons =
      web_app->pending_update_info().has_value() &&
      !web_app->pending_update_info()->trusted_icons().empty();
  if (!new_pending_update_has_icons && old_pending_update_has_icons) {
    icon_operation = IconOperation::kDeleteIcons;
  } else if (new_pending_update_has_icons) {
    // This is guaranteed to be called if there is a difference in between the
    // pending update info stored in the app vs an incoming pending update info,
    // as without that, the command exits early above.
    icon_operation = IconOperation::kWriteIcons;
  }

  auto write_pending_update_info_to_db = base::BindOnce(
      &ManifestSilentUpdateCommand::WritePendingUpdateToWebAppUpdateObservers,
      GetWeakPtr(), std::move(pending_update));

  // Handle any writing or deleting the pending update icons.
  switch (icon_operation) {
    case IconOperation::kNone:
      std::move(write_pending_update_info_to_db).Run();
      CompleteCommandAndSelfDestruct(FROM_HERE, result);
      return;
    case IconOperation::kDeleteIcons:
      SetStage(ManifestSilentUpdateCommandStage::
                   kDeletingPendingUpdateIconsFromDisk);
      // To mitigate the impact of failure conditions for deletion (system is
      // shut down mid-command, crash mid-command, failure of the operation,
      // etc), first update the web app protobuf to ensure that it doesn't
      // expect images that aren't actually on disk.
      //
      // The failure case would be that we don't clean up the images on disk,
      // which is acceptable.
      std::move(write_pending_update_info_to_db).Run();
      app_lock_->icon_manager().DeletePendingIconData(
          app_id_, WebAppIconManager::DeletePendingPassKey(),
          base::BindOnce(
              [](ManifestSilentUpdateCheckResult originaL_result,
                 bool icon_operation_success) {
                if (!icon_operation_success) {
                  return ManifestSilentUpdateCheckResult::
                      kPendingIconWriteToDiskFailed;
                }
                return originaL_result;
              },
              result)
              .Then(base::BindOnce(
                  &ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct,
                  GetWeakPtr(), FROM_HERE)));
      return;
    case IconOperation::kWriteIcons:
      SetStage(ManifestSilentUpdateCommandStage::
                   kWritingPendingUpdateIconBitmapsToDisk);
      CHECK(!pending_trusted_icon_bitmaps_.empty());
      CHECK(!pending_manifest_icon_bitmaps_.empty());
      // To mitigate the impact of failure conditions for writing icons (system
      // is shut down mid-command, crash mid-command, failure of the operation,
      // etc), first write the images before updating the web app.  If the icons
      // fail to write, then do NOT write the pending update to the database.
      //
      // The failure case would be that some icons on disk end up being updated,
      // but all expected images sizes are there. This is acceptable, and is
      // corrected the next time the new manifest is seen (as the new urls are
      // not saved).
      app_lock_->icon_manager().WritePendingIconData(
          app_id_, std::move(pending_trusted_icon_bitmaps_),
          std::move(pending_manifest_icon_bitmaps_),
          base::BindOnce(
              [](ManifestSilentUpdateCheckResult original_result,
                 base::OnceClosure write_callback,
                 bool icon_operation_success) {
                if (!icon_operation_success) {
                  return ManifestSilentUpdateCheckResult::
                      kPendingIconWriteToDiskFailed;
                }
                std::move(write_callback).Run();
                return original_result;
              },
              result, std::move(write_pending_update_info_to_db))
              .Then(base::BindOnce(
                  &ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct,
                  GetWeakPtr(), FROM_HERE)));
      return;
  }
}

void ManifestSilentUpdateCommand::WritePendingUpdateToWebAppUpdateObservers(
    std::optional<proto::PendingUpdateInfo> pending_update) {
  // The tracking of time for the icon diff check should not happen if there are
  // icons populated in the `PendingUpdateInfo`.
  if (pending_update.has_value() && !pending_update->trusted_icons().empty()) {
    CHECK(!completion_info_.time_for_icon_diff_check.has_value());
  }
  bool trigger_pending_update_observers = false;
  // First write the pending update into the app, and store whether observers
  // need to be updated.
  {
    web_app::ScopedRegistryUpdate update =
        app_lock_->sync_bridge().BeginUpdate();
    web_app::WebApp* app_to_update = update->UpdateApp(app_id_);
    CHECK(app_to_update);
    trigger_pending_update_observers =
        app_to_update->pending_update_info() != pending_update;

    // This is used to ensure that the update is shown to the user as an
    // expanded chip. At this point, it is guaranteed to be a pending update
    // that the user has not seen before, and thus hasn't ignored it.
    if (pending_update.has_value()) {
      pending_update->set_was_ignored(false);
    }
    app_to_update->SetPendingUpdateInfo(pending_update);
  }

  // Only trigger observers of a pending update info change if the value
  // previously stored in the web app has changed from that of an incoming one.
  if (trigger_pending_update_observers) {
    app_lock_->registrar().NotifyPendingUpdateInfoChanged(
        app_id_, pending_update.has_value(),
        WebAppRegistrar::PendingUpdateInfoChangePassKey());
  }
}

void ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct(
    base::Location location,
    ManifestSilentUpdateCheckResult check_result) {
  Observe(nullptr);

  bool record_update;
  CommandResult command_result;
  switch (check_result) {
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
      record_update = true;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
    case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
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

void ManifestSilentUpdateCommand::OnAppIconsLoaded(
    WebAppIconManager::WebAppBitmaps icon_bitmaps) {
  existing_manifest_icon_bitmaps_ = std::move(icon_bitmaps.manifest_icons);
  existing_trusted_icon_bitmaps_ = std::move(icon_bitmaps.trusted_icons);
}

void ManifestSilentUpdateCommand::OnShortcutIconsLoaded(
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);
}

}  // namespace web_app
