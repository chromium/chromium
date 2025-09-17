// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_closures.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/image_visual_diff.h"
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

bool AreNonSecuritySensitiveDataChangesNeeded(
    const WebApp& existing_web_app,
    const ShortcutsMenuIconBitmaps* existing_shortcuts_menu_icon_bitmaps,
    const WebAppInstallInfo& new_install_info) {
  if (existing_web_app.manifest_id() != new_install_info.manifest_id()) {
    return true;
  }
  if (existing_web_app.start_url() != new_install_info.start_url()) {
    return true;
  }
  if (existing_web_app.theme_color() != new_install_info.theme_color) {
    return true;
  }
  if (existing_web_app.scope() != new_install_info.scope) {
    return true;
  }
  if (existing_web_app.display_mode() != new_install_info.display_mode) {
    return true;
  }
  if (existing_web_app.display_mode_override() !=
      new_install_info.display_override) {
    return true;
  }
  if (existing_web_app.shortcuts_menu_item_infos() !=
      new_install_info.shortcuts_menu_item_infos) {
    return true;
  }
  if (existing_web_app.share_target() != new_install_info.share_target) {
    return true;
  }
  if (existing_web_app.protocol_handlers() !=
      new_install_info.protocol_handlers) {
    return true;
  }
  if (existing_web_app.note_taking_new_note_url() !=
      new_install_info.note_taking_new_note_url) {
    return true;
  }
  if (existing_web_app.file_handlers() != new_install_info.file_handlers) {
    return true;
  }
  if (existing_web_app.background_color() !=
      new_install_info.background_color) {
    return true;
  }
  if (existing_web_app.dark_mode_theme_color() !=
      new_install_info.dark_mode_theme_color) {
    return true;
  }
  if (existing_web_app.dark_mode_background_color() !=
      new_install_info.dark_mode_background_color) {
    return true;
  }
  if (existing_web_app.launch_handler() != new_install_info.launch_handler) {
    return true;
  }
  if (existing_web_app.permissions_policy() !=
      new_install_info.permissions_policy) {
    return true;
  }
  if (existing_shortcuts_menu_icon_bitmaps &&
      *existing_shortcuts_menu_icon_bitmaps !=
          new_install_info.shortcuts_menu_icon_bitmaps) {
    return true;
  }
  if (existing_web_app.scope_extensions() !=
      new_install_info.scope_extensions) {
    return true;
  }
  if (existing_web_app.tab_strip() != new_install_info.tab_strip) {
    return true;
  }
  if (existing_web_app.related_applications() !=
      new_install_info.related_applications) {
    return true;
  }
  // TODO(crbug.com/424246884): Check more manifest fields.

  return false;
}

sync_pb::WebAppIconInfo_Purpose ConvertIconPurposeToSyncPurpose(
    apps::IconInfo::Purpose purpose) {
  switch (purpose) {
    case apps::IconInfo::Purpose::kAny:
      return sync_pb::WebAppIconInfo_Purpose::WebAppIconInfo_Purpose_ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return sync_pb::WebAppIconInfo_Purpose::WebAppIconInfo_Purpose_MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return sync_pb::WebAppIconInfo_Purpose::WebAppIconInfo_Purpose_MASKABLE;
  }
}

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

bool HasSecuritySensitiveChangesForPendingUpdate(
    const proto::PendingUpdateInfo& pending_update_info) {
  return pending_update_info.has_name() ||
         (!pending_update_info.trusted_icons().empty() &&
          !pending_update_info.manifest_icons().empty());
}

void CopyIconsToPendingUpdateInfo(
    const std::vector<apps::IconInfo>& icon_infos,
    google::protobuf::RepeatedPtrField<sync_pb::WebAppIconInfo>*
        destination_icons) {
  for (const auto& icon_info : icon_infos) {
    sync_pb::WebAppIconInfo* pending_icon = destination_icons->Add();

    pending_icon->set_url(icon_info.url.spec());
    sync_pb::WebAppIconInfo_Purpose icon_purpose =
        ConvertIconPurposeToSyncPurpose(icon_info.purpose);
    pending_icon->set_purpose(icon_purpose);
    if (icon_info.square_size_px.has_value()) {
      pending_icon->set_size_in_px(icon_info.square_size_px.value());
    }
  }
}

}  // namespace

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
    case ManifestSilentUpdateCommandStage::kComparingManifestData:
      return os << "kComparingManifestData";
    case ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges:
      return os << "kFinalizingSilentManifestChanges";
    case ManifestSilentUpdateCommandStage::
        kWritingPendingUpdateIconBitmapsToDisk:
      return os << "kWritingPendingUpdateIconBitmapsToDisk";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult stage) {
  switch (stage) {
    case ManifestSilentUpdateCheckResult::kAppNotInstalled:
      return os << "kAppNotInstalled";
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
  }
}

ManifestSilentUpdateCommand::ManifestSilentUpdateCommand(
    content::WebContents& web_contents,
    CompletedCallback callback)
    : WebAppCommand<NoopLock, ManifestSilentUpdateCheckResult>(
          "ManifestSilentUpdateCommand",
          NoopLockDescription(),
          base::BindOnce([](ManifestSilentUpdateCheckResult result) {
            base::UmaHistogramEnumeration(
                "Webapp.Update.ManifestSilentUpdateCheckResult", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          std::make_tuple(ManifestSilentUpdateCheckResult::kSystemShutdown)),
      web_contents_(web_contents.GetWeakPtr()) {
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
    CompleteCommandAndSelfDestruct(error);
    return;
  }
  GetMutableDebugValue().Set("failed_before_start", true);
  failed_before_start_ = error;
}

void ManifestSilentUpdateCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  lock_ = std::move(lock);
  if (failed_before_start_.has_value()) {
    CompleteCommandAndSelfDestruct(*failed_before_start_);
    return;
  }

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  SetStage(ManifestSilentUpdateCommandStage::kAcquiringAppLock);
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &ManifestSilentUpdateCommand::OnManifestFetchedAcquireAppLock,
          GetWeakPtr()),
      webapps::InstallableParams());
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
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kAcquiringAppLock);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  GetMutableDebugValue().Set("installable_status",
                             webapps::GetErrorMessage(installable_status));

  if (!opt_manifest) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kInvalidManifest);
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
        ManifestSilentUpdateCheckResult::kInvalidManifest);
    return;
  }

  // TODO(crbug.com/438266139): Ignore name field in the manifest and still
  // allow silent updates to happen.
  if (!opt_manifest->has_valid_specified_start_url ||
      !opt_manifest->name.has_value() || opt_manifest->name->empty()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kInvalidManifest);
    return;
  }

  CHECK(opt_manifest->id.is_valid());
  app_id_ = GenerateAppIdFromManifestId(opt_manifest->id);

  SetStage(ManifestSilentUpdateCommandStage::kFetchingNewManifestData);
  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(lock_), *app_lock_, {app_id_},
      base::BindOnce(
          &ManifestSilentUpdateCommand::StartManifestToInstallInfoJob,
          weak_factory_.GetWeakPtr(), std::move(opt_manifest)));
}

void ManifestSilentUpdateCommand::StartManifestToInstallInfoJob(
    blink::mojom::ManifestPtr opt_manifest) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);
  CHECK(app_lock_->IsGranted());
  if (!app_lock_->registrar().IsInRegistrar(app_id_)) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppNotInstalled);
    return;
  }

  // Compare trusted icons from the new incoming manifest with the one seen for
  // the existing web app. The latter is guaranteed, but the former is not, in
  // which case, prefer to update silently without updating icons, mimicking the
  // `Cache-Control:Immutable` behavior.
  new_manifest_trusted_icon_metadata_ =
      GetTrustedIconsFromManifest(opt_manifest->icons);
  if (new_manifest_trusted_icon_metadata_.has_value()) {
    CHECK(new_manifest_trusted_icon_metadata_->square_size_px.has_value());
    existing_manifest_trusted_icon_metadata_ =
        app_lock_->registrar().GetSingleTrustedAppIconForSecuritySurfaces(
            app_id_,
            new_manifest_trusted_icon_metadata_->square_size_px.value());

    has_icon_url_changed_ =
        new_manifest_trusted_icon_metadata_.has_value() &&
        existing_manifest_trusted_icon_metadata_.has_value() &&
        new_manifest_trusted_icon_metadata_->url !=
            existing_manifest_trusted_icon_metadata_->url;
  }
  GetMutableDebugValue().Set("has_icon_url_changed", has_icon_url_changed_);

  WebAppInstallInfoConstructOptions construct_options;
  construct_options.fail_all_if_any_fail = true;
  if (!has_icon_url_changed_) {
    construct_options.skip_primary_icon_download = true;
  }

  // The `background_installation` and `install_source` fields here don't matter
  // because this is not logged anywhere.
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *opt_manifest, *data_retriever_.get(),
          /*background_installation=*/false,
          webapps::WebappInstallSource::MENU_BROWSER_TAB, web_contents_,
          [](IconUrlSizeSet&) {}, GetMutableDebugValue(),
          base::BindOnce(
              &ManifestSilentUpdateCommand::OnWebAppInfoCreatedFromManifest,
              GetWeakPtr()),
          construct_options);
}

void ManifestSilentUpdateCommand::OnWebAppInfoCreatedFromManifest(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);
  CHECK(!new_install_info_);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  new_install_info_ = std::move(install_info);

  SetStage(ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);
  LoadExistingAppAndShortcutIcons(base::BindOnce(
      &ManifestSilentUpdateCommand::FinalizeUpdateIfSilentChangesExist,
      weak_factory_.GetWeakPtr()));
}

void ManifestSilentUpdateCommand::FinalizeUpdateIfSilentChangesExist() {
  CHECK_EQ(stage_,
           ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);
  SetStage(ManifestSilentUpdateCommandStage::kComparingManifestData);

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(new_install_info_);

  silent_update_required_ = AreNonSecuritySensitiveDataChangesNeeded(
      *web_app, &existing_shortcuts_menu_icon_bitmaps_, *new_install_info_);
  GetMutableDebugValue().Set("silent_update_required",
                             base::ToString(silent_update_required_));
  proto::PendingUpdateInfo pending_update_info;
  std::u16string new_title;
  base::TrimWhitespace(new_install_info_->title, base::TRIM_ALL, &new_title);
  bool has_name_changed =
      !new_title.empty() && new_install_info_->title !=
                                base::UTF8ToUTF16(web_app->untranslated_name());
  GetMutableDebugValue().Set("has_name_changed", has_name_changed);

  // Changes to preinstalled or admin installed web apps are always silently
  // applied since they are installed by trusted sources. There should be no
  // pending update info saved for these web apps.
  if (base::FeatureList::IsEnabled(
          features::kSilentPolicyAndDefaultAppUpdating) &&
      (web_app->IsPolicyInstalledApp() || web_app->IsPreinstalledApp())) {
    if (!has_icon_url_changed_ && !has_name_changed &&
        !silent_update_required_) {
      CompleteCommandAndSelfDestruct(
          ManifestSilentUpdateCheckResult::kAppUpToDate);
      return;
    }

    new_install_info_->trusted_icons = new_install_info_->manifest_icons;
    new_install_info_->trusted_icon_bitmaps = new_install_info_->icon_bitmaps;

    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(&ManifestSilentUpdateCommand::
                           UpdateFinalizedWritePendingInfoIfNeeded,
                       GetWeakPtr(),
                       std::optional<proto::PendingUpdateInfo>()));
    return;
  }

  if (!has_icon_url_changed_ && !has_name_changed && !silent_update_required_) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpToDate);
    return;
  }
  // After this line, we know that something in the system needs to update.

  if (has_name_changed) {
    pending_update_info.set_name(base::UTF16ToUTF8(new_install_info_->title));
    new_install_info_->title = base::UTF8ToUTF16(web_app->untranslated_name());
  }

  if (!has_icon_url_changed_) {
    if (!silent_update_required_) {
      // App name has changed.
      UpdateFinalizedWritePendingInfoIfNeeded(
          std::move(pending_update_info), app_id_,
          webapps::InstallResultCode::kSuccessAlreadyInstalled);
      return;
    }

    // Trusted icons are not downloaded because the url has not changed. Thus,
    // for the update, populate trusted icons from database.
    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;

    std::optional<proto::PendingUpdateInfo> opt_pending_update =
        HasSecuritySensitiveChangesForPendingUpdate(pending_update_info)
            ? pending_update_info
            : std::optional<proto::PendingUpdateInfo>();
    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(&ManifestSilentUpdateCommand::
                           UpdateFinalizedWritePendingInfoIfNeeded,
                       GetWeakPtr(), std::move(opt_pending_update)));
    return;
  }
  // After this line, the icon urls have changed. Those icons are either stored
  // in PendingUpdateInfo if there is amore than 10% diff or silently updated
  // otherwise.

  CHECK(new_manifest_trusted_icon_metadata_.has_value());
  CHECK(new_manifest_trusted_icon_metadata_->square_size_px.has_value());
  int icon_size_to_use = *new_manifest_trusted_icon_metadata_->square_size_px;

  CHECK(!new_install_info_->trusted_icons.empty());
  CHECK(!new_install_info_->trusted_icon_bitmaps.empty());

  auto existing_trusted_icon_bitmaps_to_use =
      existing_trusted_icon_bitmaps_.GetBitmapsForPurpose(
          ConvertIconPurposeToManifestImagePurpose(
              existing_manifest_trusted_icon_metadata_->purpose));
  auto new_trusted_icon_bitmaps_to_use =
      new_install_info_->trusted_icon_bitmaps.GetBitmapsForPurpose(
          ConvertIconPurposeToManifestImagePurpose(
              new_manifest_trusted_icon_metadata_->purpose));

  auto existing_trusted_icon_it =
      existing_trusted_icon_bitmaps_to_use.find(icon_size_to_use);
  auto new_trusted_icon_it =
      new_trusted_icon_bitmaps_to_use.find(icon_size_to_use);
  CHECK(new_trusted_icon_it != new_trusted_icon_bitmaps_to_use.end());

  bool has_existing_trusted_icon =
      existing_trusted_icon_it != existing_trusted_icon_bitmaps_to_use.end();

  // TODO(crbug.com/437379182): HasMoreThanTenPercentImageDiff() should happen
  // in a different thread.
  // Case: The icons are being set in the PendingUpdateInfo to be updated later.
  if (!has_existing_trusted_icon ||
      HasMoreThanTenPercentImageDiff(&(existing_trusted_icon_it->second),
                                     &(new_trusted_icon_it->second))) {
    // PendingUpdateInfo is used in the optional user update UX.
    CopyIconsToPendingUpdateInfo(new_install_info_->trusted_icons,
                                 pending_update_info.mutable_trusted_icons());
    CopyIconsToPendingUpdateInfo(new_install_info_->manifest_icons,
                                 pending_update_info.mutable_manifest_icons());

    pending_trusted_icon_bitmaps_ = new_install_info_->trusted_icon_bitmaps;
    pending_manifest_icon_bitmaps_ = new_install_info_->icon_bitmaps;

    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->trusted_icons = web_app->trusted_icons();
    new_install_info_->icon_bitmaps = existing_manifest_icon_bitmaps_;
    new_install_info_->trusted_icon_bitmaps = existing_trusted_icon_bitmaps_;
  } else {
    // Silent updates are allowed if the icons are less than 10% diff.
    silent_update_required_ = true;
    GetMutableDebugValue().Set("silent_update_required",
                               base::ToString(silent_update_required_));
  }

  std::optional<proto::PendingUpdateInfo> opt_pending_update =
      HasSecuritySensitiveChangesForPendingUpdate(pending_update_info)
          ? pending_update_info
          : std::optional<proto::PendingUpdateInfo>();
  if (silent_update_required_) {
    app_lock_->install_finalizer().FinalizeUpdate(
        new_install_info_->Clone(),
        base::BindOnce(&ManifestSilentUpdateCommand::
                           UpdateFinalizedWritePendingInfoIfNeeded,
                       GetWeakPtr(), std::move(opt_pending_update)));
  } else {
    // If there is no silent update, that means it MUST be pending update.
    CHECK(opt_pending_update);
    UpdateFinalizedWritePendingInfoIfNeeded(
        std::move(opt_pending_update), app_id_,
        webapps::InstallResultCode::kSuccessAlreadyInstalled);
  }
}

void ManifestSilentUpdateCommand::UpdateFinalizedWritePendingInfoIfNeeded(
    std::optional<proto::PendingUpdateInfo> pending_update_info,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kComparingManifestData);
  SetStage(ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges);
  GetMutableDebugValue().Set("silent_update_install_code",
                             base::ToString(code));
  if (!IsSuccess(code)) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
    return;
  }

  CHECK_EQ(app_id_, app_id);
  CHECK(new_install_info_);
  const WebApp* existing_web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(existing_web_app);
  // Ensure that non security sensitive data changes are no longer needed post
  // application.
  // `existing_shortcuts_menu_icon_bitmaps` has to be nullptr, otherwise this
  // CHECK will fail. This is because `existing_shortcuts_menu_icon_bitmaps` is
  // cached from before the manifest changes are applied, and once they are
  // applied, the value of `existing_shortcuts_menu_icon_bitmaps` will need to
  // be updated. It is expensive to read the icons by calling the
  // `WebAppIconManager` again, so the simpler solution is to pass in `nullptr`
  // to bypass this CHECK.
  CHECK(!AreNonSecuritySensitiveDataChangesNeeded(
      *existing_web_app, /*existing_shortcuts_menu_icon_bitmaps=*/nullptr,
      *new_install_info_));
  CHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);
  CHECK(!pending_update_info.has_value() ||
        HasSecuritySensitiveChangesForPendingUpdate(*pending_update_info));

  if (!pending_update_info.has_value()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
    return;
  }

  // Update the web app with non-security sensitive changes and store security
  // sensitive changes to pending update info.
  {
    web_app::ScopedRegistryUpdate update =
        app_lock_->sync_bridge().BeginUpdate();
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    // Record if we are adding a pending update if there wasn't one before, so
    // can correctly notify observers only if there was a change.
    pending_updated_added_ = !app_to_update->pending_update_info().has_value();
    CHECK(app_to_update);
    app_to_update->SetPendingUpdateInfo(std::move(pending_update_info));
  }

  // Write the pending trusted and pending manifest icon bitmaps to disk.
  SetStage(
      ManifestSilentUpdateCommandStage::kWritingPendingUpdateIconBitmapsToDisk);
  app_lock_->icon_manager().WritePendingIconData(
      app_id_, pending_trusted_icon_bitmaps_, pending_manifest_icon_bitmaps_,
      base::BindOnce(
          [](bool silent_update_required, bool bitmaps_write_success) {
            if (!bitmaps_write_success) {
              return ManifestSilentUpdateCheckResult::
                  kPendingIconWriteToDiskFailed;
            }
            if (silent_update_required) {
              return ManifestSilentUpdateCheckResult::
                  kAppHasNonSecurityAndSecurityChanges;
            }
            return ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate;
          },
          silent_update_required_)
          .Then(base::BindOnce(
              &ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct,
              GetWeakPtr())));
}

void ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct(
    ManifestSilentUpdateCheckResult check_result) {
  GetMutableDebugValue().Set("result", base::ToString(check_result));
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
    case ManifestSilentUpdateCheckResult::kAppNotInstalled:
    case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
    case ManifestSilentUpdateCheckResult::kUserNavigated:
      record_update = false;
      command_result = CommandResult::kSuccess;
      break;
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
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
  if (pending_updated_added_) {
    app_lock_->registrar().NotifyPendingUpdateInfoChanged(
        app_id_, /*pending_update_available=*/true,
        base::PassKey<ManifestSilentUpdateCommand>());
  }
  CompleteAndSelfDestruct(command_result, check_result);
}

bool ManifestSilentUpdateCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestSilentUpdateCommand::LoadExistingAppAndShortcutIcons(
    base::OnceClosure on_complete) {
  base::ConcurrentClosures barrier;
  app_lock_->icon_manager().ReadAllIcons(
      app_id_, base::BindOnce(&ManifestSilentUpdateCommand::OnAppIconsLoaded,
                              GetWeakPtr())
                   .Then(barrier.CreateClosure()));

  app_lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_,
      base::BindOnce(&ManifestSilentUpdateCommand::OnShortcutIconsLoaded,
                     GetWeakPtr())
          .Then(barrier.CreateClosure()));
  std::move(barrier).Done(std::move(on_complete));
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
