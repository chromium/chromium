// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/manifest_update_job.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/jobs/finalize_update_job.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/image_visual_diff.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

// static
std::unique_ptr<ManifestUpdateJob> ManifestUpdateJob::CreateAndStart(
    Profile& profile,
    Lock* lock,
    WithAppResources* lock_resources,
    content::WebContents* web_contents,
    base::DictValue* debug_value,
    blink::mojom::ManifestPtr manifest,
    WebAppDataRetriever* data_retriever,
    base::Clock* clock,
    ManifestUpdateJobCallback callback,
    Options options) {
  CHECK(lock);
  CHECK(lock_resources);
  CHECK(web_contents);
  CHECK(debug_value);
  CHECK(manifest);
  CHECK(data_retriever);
  CHECK(clock);

  std::unique_ptr<ManifestUpdateJob> job = base::WrapUnique(
      new ManifestUpdateJob(profile, lock, lock_resources, web_contents,
                            debug_value, std::move(manifest), data_retriever,
                            clock, std::move(callback), options));
  job->Start();
  return job;
}

ManifestUpdateJob::ManifestUpdateJob(Profile& profile,
                                     Lock* lock,
                                     WithAppResources* lock_resources,
                                     content::WebContents* web_contents,
                                     base::DictValue* debug_value,
                                     blink::mojom::ManifestPtr manifest,
                                     WebAppDataRetriever* data_retriever,
                                     base::Clock* clock,
                                     ManifestUpdateJobCallback callback,
                                     Options options)
    : profile_(profile),
      lock_(*lock),
      lock_resources_(*lock_resources),
      web_contents_(web_contents->GetWeakPtr()),
      debug_value_(*debug_value),
      manifest_(std::move(manifest)),
      data_retriever_(*data_retriever),
      clock_(*clock),
      app_id_(GenerateAppIdFromManifest(*manifest_)),
      callback_(std::move(callback)),
      options_(options) {
  debug_value_->Set(
      "options",
      base::DictValue()
          .Set("force_silent_update_identity",
               options_.force_silent_update_identity)
          .Set("skip_icon_download_if_no_manifest_change",
               options_.skip_icon_download_if_no_manifest_change)
          .Set("bypass_icon_generation_if_no_url",
               options_.bypass_icon_generation_if_no_url)
          .Set("fail_if_any_icon_download_fails",
               options_.fail_if_any_icon_download_fails)
          .Set("record_icon_results_on_update",
               options_.record_icon_results_on_update)
          .Set("use_manifest_icons_as_trusted",
               options_.use_manifest_icons_as_trusted)
          .Set("previous_time_for_silent_icon_update",
               options_.previous_time_for_silent_icon_update.has_value()
                   ? base::ToString(
                         options_.previous_time_for_silent_icon_update.value())
                   : "nullopt"));
}

ManifestUpdateJob::~ManifestUpdateJob() = default;

void ManifestUpdateJob::Start() {
  if (IsWebContentsDestroyed()) {
    Complete(Result::kWebContentsDestroyed);
    return;
  }

  if (!lock_resources_->registrar().AppMatches(
          app_id_, WebAppFilter::IsAppEligibleForManifestUpdate())) {
    Complete(Result::kAppNotAllowedToUpdate);
    return;
  }

  WebAppInstallInfoConstructOptions construct_options;
  construct_options.bypass_icon_generation_if_no_url =
      options_.bypass_icon_generation_if_no_url;
  construct_options.fail_all_if_any_fail =
      options_.fail_if_any_icon_download_fails;
  construct_options.record_icon_results_on_update =
      options_.record_icon_results_on_update;
  construct_options.use_manifest_icons_as_trusted =
      options_.use_manifest_icons_as_trusted;
  // Always defer icon fetching for the job to handle it explicitly.
  construct_options.defer_icon_fetching = true;

  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *manifest_, *data_retriever_,
          /*background_installation=*/false,
          webapps::WebappInstallSource::MENU_BROWSER_TAB, web_contents_,
          [](IconUrlSizeSet&) {},
          *debug_value_->EnsureDict("manifest_to_install_info_job"),
          base::BindOnce(&ManifestUpdateJob::OnWebAppInfoCreated,
                         weak_factory_.GetWeakPtr()),
          construct_options, /*fallback_info=*/std::nullopt);
}

void ManifestUpdateJob::OnWebAppInfoCreated(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (IsWebContentsDestroyed()) {
    Complete(Result::kWebContentsDestroyed);
    return;
  }

  if (!install_info) {
    Complete(Result::kManifestConversionFailed);
    return;
  }

  new_install_info_ = std::move(install_info);

  const WebApp* app = lock_resources_->registrar().GetAppById(app_id_);
  CHECK(app);

  web_app_comparison_ =
      WebAppComparison::CompareWebApps(*app, *new_install_info_);
  debug_value_->Set("web_app_diff", web_app_comparison_.ToDict());

  // Store the conditions for which a silent update of the app's identity is
  // allowed.
  bool is_trusted_install = lock_resources_->registrar().AppMatches(
                                app_id_, WebAppFilter::IsTrusted()) ||
                            options_.use_manifest_icons_as_trusted;
  bool can_fix_generated_icons =
      app->is_generated_icon() &&
      app->latest_install_source() == webapps::WebappInstallSource::SYNC &&
      generated_icon_fix_util::IsWithinFixTimeWindow(*app) &&
      web_app_comparison_.ExistingAppWithoutPendingEqualsNewUpdate();
  bool is_migration_suggestion = lock_resources_->registrar().AppMatches(
      app_id_, WebAppFilter::IsAppSuggestedForMigration());
  silently_update_app_identity_ =
      (is_trusted_install || can_fix_generated_icons ||
       options_.force_silent_update_identity || is_migration_suggestion);

  if (can_fix_generated_icons) {
    ScopedRegistryUpdate update = lock_resources_->sync_bridge().BeginUpdate();
    generated_icon_fix_util::EnsureFixTimeWindowStarted(
        *lock_resources_, update, app_id_,
        proto::GENERATED_ICON_FIX_SOURCE_MANIFEST_UPDATE);
  }
  debug_value_->Set("is_trusted_install", is_trusted_install);
  debug_value_->Set("can_fix_generated_icons", can_fix_generated_icons);
  debug_value_->Set("silently_update_identity", silently_update_app_identity_);

  if (!can_fix_generated_icons) {
    if (web_app_comparison_.ExistingAppWithoutPendingEqualsNewUpdate()) {
      WritePendingUpdateInfoThenComplete(std::nullopt, Result::kNoUpdateNeeded);
      return;
    }
    if (web_app_comparison_.ExistingAppWithPendingEqualsNewUpdate() &&
        !options_.force_silent_update_identity) {
      Complete(Result::kNoUpdateNeeded);
      return;
    }
  }

  if (web_app_comparison_.IsNameChangeOnly() &&
      !silently_update_app_identity_) {
    proto::PendingUpdateInfo update;
    update.set_name(base::UTF16ToUTF8(new_install_info_->title.value()));
    WritePendingUpdateInfoThenComplete(
        std::move(update),
        Result::kPendingUpdateRecorded_AppOnlyHasSecurityUpdate);
    return;
  }

  // Load existing icons from disk.
  base::ConcurrentClosures barrier;
  lock_resources_->icon_manager().ReadAllIcons(
      app_id_, base::BindOnce(&ManifestUpdateJob::OnExistingIconsLoaded,
                              weak_factory_.GetWeakPtr())
                   .Then(barrier.CreateClosure()));

  // If shortcut menu items didn't change (or force update), we might need
  // existing shortcut icons. The logic in ManifestSilentUpdateCommand says:
  // "Since the shortcut menu items did not change, load the existing icons from
  // **disk** for the silent update (which acts like a re-install)."
  if (!options_.force_silent_update_identity &&
      web_app_comparison_.shortcut_menu_item_infos_equality()) {
    lock_resources_->icon_manager().ReadAllShortcutsMenuIcons(
        app_id_,
        base::BindOnce(&ManifestUpdateJob::OnExistingShortcutsMenuIconsLoaded,
                       weak_factory_.GetWeakPtr())
            .Then(barrier.CreateClosure()));
  }

  std::move(barrier).Done(base::BindOnce(
      &ManifestUpdateJob::ContinueToFetchIcons, weak_factory_.GetWeakPtr()));
}

void ManifestUpdateJob::OnExistingIconsLoaded(
    WebAppIconManager::WebAppBitmaps icon_bitmaps) {
  existing_manifest_icon_bitmaps_ = std::move(icon_bitmaps.manifest_icons);
  existing_trusted_icon_bitmaps_ = std::move(icon_bitmaps.trusted_icons);
}

void ManifestUpdateJob::OnExistingShortcutsMenuIconsLoaded(
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);
}

void ManifestUpdateJob::ContinueToFetchIcons() {
  if (IsWebContentsDestroyed()) {
    Complete(Result::kWebContentsDestroyed);
    return;
  }

  // Skip downloading icons from the network if they didn't change and we are
  // configured to skip them.
  IconUrlExtractionOptions icon_fetch_options;
  if (options_.skip_icon_download_if_no_manifest_change &&
      !options_.force_silent_update_identity) {
    icon_fetch_options.product_icons =
        !web_app_comparison_.primary_icons_equality() ||
        silently_update_app_identity_;
    icon_fetch_options.shortcut_menu_item_icons =
        !web_app_comparison_.shortcut_menu_item_infos_equality();
  }

  manifest_to_install_info_job_->FetchIcons(
      *new_install_info_, *web_contents_,
      base::BindOnce(&ManifestUpdateJob::OnIconsFetched,
                     weak_factory_.GetWeakPtr()),
      /*icon_url_modifications=*/std::nullopt, icon_fetch_options);
}

void ManifestUpdateJob::OnIconsFetched() {
  if (IsWebContentsDestroyed()) {
    Complete(Result::kWebContentsDestroyed);
    return;
  }

  if (manifest_to_install_info_job_->icon_download_result() ==
      IconsDownloadedResult::kAbortedDueToFailure) {
    Complete(Result::kIconDownloadFailed);
    return;
  }

  if (manifest_to_install_info_job_->icon_download_result() ==
      IconsDownloadedResult::kPrimaryPageChanged) {
    Complete(Result::kUserNavigated);
    return;
  }

  FinalizeUpdateIfSilentChangesExist();
}

void ManifestUpdateJob::FinalizeUpdateIfSilentChangesExist() {
  // Copy over any icons that did not have manifest changes.
  const WebApp* web_app = lock_resources_->registrar().GetAppById(app_id_);
  if (web_app_comparison_.shortcut_menu_item_infos_equality()) {
    new_install_info_->shortcuts_menu_item_infos =
        web_app->shortcuts_menu_item_infos();
    new_install_info_->shortcuts_menu_icon_bitmaps =
        existing_shortcuts_menu_icon_bitmaps_;
  }

  // Exit early to finalize the update if we know that we are silently updating
  // the app identity.
  if (silently_update_app_identity_) {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile_.get());

    install_update_job_ = std::make_unique<FinalizeUpdateJob>(
        &lock_.get(), &lock_resources_.get(), *provider, *new_install_info_);
    install_update_job_->Start(
        base::BindOnce(&ManifestUpdateJob::UpdateFinalizedWritePendingInfo,
                       weak_factory_.GetWeakPtr(), std::nullopt,
                       /*silent_icon_update_happened=*/false));
    return;
  }
  CHECK(!options_.force_silent_update_identity);

  silent_update_required_ =
      !web_app_comparison_.other_fields_equality() ||
      !web_app_comparison_.shortcut_menu_item_infos_equality();
  debug_value_->Set("silent_update_required",
                    base::ToString(silent_update_required_));

  std::optional<proto::PendingUpdateInfo> pending_update_info;
  if (!web_app_comparison_.name_equality()) {
    pending_update_info = proto::PendingUpdateInfo();
    pending_update_info->set_name(
        base::UTF16ToUTF8(new_install_info_->title.value()));
    new_install_info_->title = base::UTF8ToUTF16(web_app->untranslated_name());
  }

  if (web_app_comparison_.primary_icons_equality()) {
    CHECK(silent_update_required_);
    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;
    new_install_info_->is_generated_icon = web_app->is_generated_icon();
    WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile_.get());

    install_update_job_ = std::make_unique<FinalizeUpdateJob>(
        &lock_.get(), &lock_resources_.get(), *provider, *new_install_info_);
    install_update_job_->Start(base::BindOnce(
        &ManifestUpdateJob::UpdateFinalizedWritePendingInfo,
        weak_factory_.GetWeakPtr(), std::move(pending_update_info),
        /*silent_icon_update_happened=*/false));
    return;
  }
  CHECK(!new_install_info_->trusted_icons.empty());

  if (new_install_info_->trusted_icon_bitmaps.empty()) {
    Complete(Result::kManifestConversionFailed);
    return;
  }

  static constexpr int kLogoSizeInDialog = 96;
  SkBitmap old_trusted_icon = [&]() {
    std::optional<apps::IconInfo> trusted_icon =
        lock_resources_->registrar().GetSingleTrustedAppIconForSecuritySurfaces(
            app_id_, kLogoSizeInDialog);
    if (!trusted_icon.has_value()) {
      return SkBitmap();
    }
    blink::mojom::ManifestImageResource_Purpose purpose =
        ConvertIconPurposeToManifestImagePurpose(trusted_icon->purpose);
    auto old_bitmaps_to_use =
        existing_trusted_icon_bitmaps_.GetBitmapsForPurpose(purpose);
    if (old_bitmaps_to_use.empty()) {
      // We may have fallen back to the normal bitmaps if the app was installed
      // before trusted icons were supported.
      old_bitmaps_to_use =
          existing_manifest_icon_bitmaps_.GetBitmapsForPurpose(purpose);
      if (old_bitmaps_to_use.empty()) {
        return SkBitmap();
      }
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

  base::Time current_time = clock_->Now();
  bool silent_icon_update_throttled = ThrottleForSilentIconUpdates(
      options_.previous_time_for_silent_icon_update, current_time);
  bool silent_icon_update =
      !HasMoreThanTenPercentImageDiff(&old_trusted_icon, &new_trusted_icon) &&
      !silent_icon_update_throttled;
  if (silent_icon_update) {
    time_for_icon_diff_check_ = current_time;
  }

  if (old_trusted_icon.empty() || !silent_icon_update) {
    if (!pending_update_info.has_value()) {
      pending_update_info = proto::PendingUpdateInfo();
    }
    debug_value_->Set("greater_than_ten_percent", true);
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

    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;
    new_install_info_->is_generated_icon = web_app->is_generated_icon();
  } else {
    silent_update_required_ = true;
    debug_value_->Set("silent_update_required",
                      base::ToString(silent_update_required_));
  }

  if (silent_update_required_) {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile_.get());

    install_update_job_ = std::make_unique<FinalizeUpdateJob>(
        &lock_.get(), &lock_resources_.get(), *provider, *new_install_info_);
    install_update_job_->Start(
        base::BindOnce(&ManifestUpdateJob::UpdateFinalizedWritePendingInfo,
                       weak_factory_.GetWeakPtr(),
                       std::move(pending_update_info), silent_icon_update));
  } else {
    CHECK(pending_update_info);
    Result result_for_icon_changes =
        silent_icon_update_throttled
            ? Result::kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle
            : Result::kPendingUpdateRecorded_AppOnlyHasSecurityUpdate;
    WritePendingUpdateInfoThenComplete(pending_update_info,
                                       result_for_icon_changes);
  }
}

void ManifestUpdateJob::UpdateFinalizedWritePendingInfo(
    std::optional<proto::PendingUpdateInfo> pending_update_info,
    bool silent_icon_update_happened,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  install_update_job_.reset();
  debug_value_->Set("silent_update_install_code", base::ToString(code));

  if (!webapps::IsSuccess(code)) {
    Complete(Result::kInstallFinalizeFailed);
    return;
  }

  // Always write the pending update info so we clear it if it was already
  // populated.
  Result result =
      pending_update_info.has_value()
          ? Result::kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges
          : (silent_icon_update_happened
                 ? Result::kSilentlyUpdatedDueToSmallIconComparison
                 : Result::kSilentlyUpdated);
  WritePendingUpdateInfoThenComplete(std::move(pending_update_info), result);
}

void ManifestUpdateJob::WritePendingUpdateInfoThenComplete(
    std::optional<proto::PendingUpdateInfo> pending_update,
    Result result) {
  enum class IconOperation {
    kNone,
    kWriteIcons,
    kDeleteIcons
  } icon_operation = IconOperation::kNone;

  const WebApp* web_app = lock_resources_->registrar().GetAppById(app_id_);
  CHECK(web_app);

  if (web_app->pending_update_info() == pending_update) {
    Complete(result);
    return;
  }

  bool new_pending_update_has_icons =
      pending_update.has_value() && !pending_update->trusted_icons().empty();
  bool old_pending_update_has_icons =
      web_app->pending_update_info().has_value() &&
      !web_app->pending_update_info()->trusted_icons().empty();
  if (!new_pending_update_has_icons && old_pending_update_has_icons) {
    icon_operation = IconOperation::kDeleteIcons;
  } else if (new_pending_update_has_icons) {
    icon_operation = IconOperation::kWriteIcons;
  }

  auto write_pending_update_info_to_db = base::BindOnce(
      &ManifestUpdateJob::WritePendingUpdateToWebAppUpdateObservers,
      weak_factory_.GetWeakPtr(), std::move(pending_update));

  switch (icon_operation) {
    case IconOperation::kNone:
      std::move(write_pending_update_info_to_db).Run();
      Complete(result);
      return;
    case IconOperation::kDeleteIcons:
      std::move(write_pending_update_info_to_db).Run();
      lock_resources_->icon_manager().DeletePendingIconData(
          app_id_, WebAppIconManager::DeletePendingPassKey(),
          base::BindOnce(
              [](Result original_result, bool icon_operation_success) {
                if (!icon_operation_success) {
                  return Result::kIconWriteToDiskFailed;
                }
                return original_result;
              },
              result)
              .Then(base::BindOnce(&ManifestUpdateJob::Complete,
                                   weak_factory_.GetWeakPtr())));
      return;
    case IconOperation::kWriteIcons:
      CHECK(!pending_trusted_icon_bitmaps_.empty());
      CHECK(!pending_manifest_icon_bitmaps_.empty());
      lock_resources_->icon_manager().WritePendingIconData(
          app_id_, std::move(pending_trusted_icon_bitmaps_),
          std::move(pending_manifest_icon_bitmaps_),
          base::BindOnce(
              [](Result original_result, base::OnceClosure write_callback,
                 bool icon_operation_success) {
                if (!icon_operation_success) {
                  return Result::kIconWriteToDiskFailed;
                }
                std::move(write_callback).Run();
                return original_result;
              },
              result, std::move(write_pending_update_info_to_db))
              .Then(base::BindOnce(&ManifestUpdateJob::Complete,
                                   weak_factory_.GetWeakPtr())));
      return;
  }
}

void ManifestUpdateJob::WritePendingUpdateToWebAppUpdateObservers(
    std::optional<proto::PendingUpdateInfo> pending_update) {
  if (pending_update.has_value() && !pending_update->trusted_icons().empty()) {
    CHECK(!time_for_icon_diff_check_.has_value());
  }
  bool trigger_pending_update_observers = false;
  {
    web_app::ScopedRegistryUpdate update =
        lock_resources_->sync_bridge().BeginUpdate();
    web_app::WebApp* app_to_update = update->UpdateApp(app_id_);
    CHECK(app_to_update);
    trigger_pending_update_observers =
        app_to_update->pending_update_info() != pending_update;

    if (pending_update.has_value()) {
      pending_update->set_was_ignored(false);
    }
    app_to_update->SetPendingUpdateInfo(pending_update);
  }

  if (trigger_pending_update_observers) {
    lock_resources_->registrar().NotifyPendingUpdateInfoChanged(
        app_id_, pending_update.has_value(),
        base::PassKey<ManifestUpdateJob>());
  }
}

void ManifestUpdateJob::Complete(Result result) {
  if (callback_.is_null()) {
    return;
  }
  ManifestUpdateJobResultWithTimestamp result_with_timestamp(
      result, time_for_icon_diff_check_);
  debug_value_->Set("result", base::ToString(result));
  debug_value_->Set("time_for_icon_diff_check",
                    time_for_icon_diff_check_.has_value()
                        ? base::ToString(time_for_icon_diff_check_.value())
                        : "null");
  // To avoid re-entry during the call to CreateAndStart, always post this as
  // a task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(result_with_timestamp)));
}

bool ManifestUpdateJob::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

}  // namespace web_app
