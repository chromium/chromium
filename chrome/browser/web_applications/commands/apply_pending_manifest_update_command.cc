// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"

#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace web_app {
namespace {

std::vector<apps::IconInfo> ConvertSyncProtoIconsToAppIconInfos(
    const google::protobuf::RepeatedPtrField<sync_pb::WebAppIconInfo>&
        icon_infos_proto) {
  std::vector<apps::IconInfo> icon_infos;
  icon_infos.reserve(icon_infos_proto.size());
  for (const auto& icon_info_proto : icon_infos_proto) {
    apps::IconInfo icon_info;
    if (icon_info_proto.has_size_in_px()) {
      icon_info.square_size_px = icon_info_proto.size_in_px();
    }
    DCHECK(icon_info_proto.has_url());
    icon_info.url = GURL(icon_info_proto.url());
    std::optional<apps::IconInfo::Purpose> icon_purpose =
        SyncPurposeToIconInfoPurpose(icon_info_proto.purpose());
    CHECK(icon_purpose.has_value());
    icon_info.purpose = icon_purpose.value();
    icon_infos.push_back(std::move(icon_info));
  }
  return icon_infos;
}

using IconPurposeAndSizes = std::pair<IconPurpose, SortedSizesPx>;
IconPurposeAndSizes ConvertSyncDownloadedIconInfoToAppIcon(
    const proto::DownloadedIconSizeInfo& downloaded_info) {
  std::optional<apps::IconInfo::Purpose> icon_purpose =
      SyncPurposeToIconInfoPurpose(downloaded_info.purpose());

  CHECK(icon_purpose.has_value());
  const IconPurpose purpose = static_cast<IconPurpose>(icon_purpose.value());
  std::vector<SquareSizePx> sizes_vector;
  for (const auto& size : downloaded_info.icon_sizes()) {
    sizes_vector.push_back(static_cast<SquareSizePx>(size));
  }
  SortedSizesPx sizes_set(std::move(sizes_vector));
  return IconPurposeAndSizes(purpose, std::move(sizes_set));
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateCommandStage stage) {
  switch (stage) {
    case ApplyPendingManifestUpdateCommandStage::kNotStarted:
      return os << "kNotStarted";
    case ApplyPendingManifestUpdateCommandStage::kAquiringAppLock:
      return os << "kAquiringAppLock";
    case ApplyPendingManifestUpdateCommandStage::kSynchronizingOS:
      return os << "kSynchronizingOS";
    case ApplyPendingManifestUpdateCommandStage::
        kDeletingPendingIconDirectories:
      return os << "kDeletingPendingIconDirectories";
    case ApplyPendingManifestUpdateCommandStage::kDeletingPendingUpdateInfo:
      return os << "kDeletingPendingUpdateInfo";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateResult stage) {
  switch (stage) {
    case ApplyPendingManifestUpdateResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ApplyPendingManifestUpdateResult::kAppNotInstalled:
      return os << "kAppNotInstalled";
    case ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully:
      return os << "kIconChangeAppliedSuccessfully";
    case ApplyPendingManifestUpdateResult::
        kFailedToOverwriteIconsFromPendingIcons:
      return os << "kFailedToOverwriteIconsFromPendingIcons";
    case ApplyPendingManifestUpdateResult::kNoPendingUpdate:
      return os << "kNoPendingUpdate";
    case ApplyPendingManifestUpdateResult::kFailedToRemovePendingIconsFromDisk:
      return os << "kFailedToRemovePendingIconsFromDisk";
    case ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully:
      return os << "kAppNameUpdatedSuccessfully";
    case ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully:
      return os << "kAppNameAndIconsUpdatedSuccessfully";
  }
}

ApplyPendingManifestUpdateCommand::ApplyPendingManifestUpdateCommand(
    const webapps::AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    CompletedCallback callback)
    : WebAppCommand<AppLock, ApplyPendingManifestUpdateResult>(
          "ApplyPendingManifestUpdateCommand",
          AppLockDescription(app_id),
          base::BindOnce([](ApplyPendingManifestUpdateResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.Update.ApplyPendingManifestUpdateResult", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          std::make_tuple(ApplyPendingManifestUpdateResult::kSystemShutdown)),
      app_id_(app_id),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {
  GetMutableDebugValue().Set("app_id", app_id_);
}

ApplyPendingManifestUpdateCommand::~ApplyPendingManifestUpdateCommand() =
    default;

void ApplyPendingManifestUpdateCommand::SetStage(
    ApplyPendingManifestUpdateCommandStage stage) {
  stage_ = stage;
  GetMutableDebugValue().Set("stage", base::ToString(stage));
}

void ApplyPendingManifestUpdateCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  CHECK_EQ(stage_, ApplyPendingManifestUpdateCommandStage::kNotStarted);
  SetStage(ApplyPendingManifestUpdateCommandStage::kAquiringAppLock);
  lock_ = std::move(lock);
  if (!lock_->registrar().IsInRegistrar(app_id_)) {
    CompleteCommandAndSelfDestruct(
        ApplyPendingManifestUpdateResult::kAppNotInstalled);
    return;
  }
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);

  if (!web_app->pending_update_info().has_value()) {
    CompleteCommandAndSelfDestruct(
        ApplyPendingManifestUpdateResult::kNoPendingUpdate);
    return;
  }

  // Either the app name or icons have changed.
  const proto::PendingUpdateInfo& pending_update =
      web_app->pending_update_info().value();
  has_icon_changes_ = !pending_update.trusted_icons().empty() &&
                      !pending_update.manifest_icons().empty();
  has_name_change_ = pending_update.has_name();

  // Only the name has changed
  if (has_name_change_ && !has_icon_changes_) {
    ApplyPendingUpdateInfoToWebApp(/*success=*/true);
    return;
  }

  lock_->icon_manager().OverwriteAppIconsFromPendingIcons(
      app_id_, PassKey(),
      base::BindOnce(
          &ApplyPendingManifestUpdateCommand::ApplyPendingUpdateInfoToWebApp,
          AsWeakPtr()));
}

void ApplyPendingManifestUpdateCommand::ApplyPendingUpdateInfoToWebApp(
    bool success) {
  CHECK_EQ(stage_, ApplyPendingManifestUpdateCommandStage::kAquiringAppLock);
  if (!success) {
    CompleteCommandAndSelfDestruct(ApplyPendingManifestUpdateResult::
                                       kFailedToOverwriteIconsFromPendingIcons);
    return;
  }
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  CHECK(web_app);
  const proto::PendingUpdateInfo& pending_update =
      web_app->pending_update_info().value();
  base::OnceClosure completion_callback;

  // Update the web app with the pending update info fields.
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id_);
    if (has_name_change_) {
      app_to_update->SetName(pending_update.name());

      // If only the app name has changed, do not need to delete pending icons
      // from disk. However, if the icons also have changed, this will be
      // overwritten by the DeletePendingIconsFromDisk callback below.
      completion_callback = base::BindOnce(
          &ApplyPendingManifestUpdateCommand::
              DeletePendingUpdateInfoThenComplete,
          AsWeakPtr(),
          ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully);
    }

    if (has_icon_changes_) {
      app_to_update->SetManifestIcons(
          ConvertSyncProtoIconsToAppIconInfos(pending_update.manifest_icons()));
      app_to_update->SetTrustedIcons(
          ConvertSyncProtoIconsToAppIconInfos(pending_update.trusted_icons()));

      // The icons are generated only if icon downloads fail, in which case
      // there would be no pending update info stored in the app and this
      // command wouldn't have existed.
      app_to_update->SetIsGeneratedIcon(/*is_generated_icon=*/false);

      for (const auto& downloaded_info :
           pending_update.downloaded_trusted_icons()) {
        // MONOCHROME trusted icons are not supported and will crash if called
        // with SetStoredTrustedIconSizes.
        if (downloaded_info.purpose() ==
            sync_pb::WebAppIconInfo_Purpose::
                WebAppIconInfo_Purpose_MONOCHROME) {
          continue;
        }
        IconPurposeAndSizes purpose_and_sizes =
            ConvertSyncDownloadedIconInfoToAppIcon(downloaded_info);
        app_to_update->SetStoredTrustedIconSizes(
            purpose_and_sizes.first, std::move(purpose_and_sizes.second));
      }

      for (const auto& downloaded_info :
           pending_update.downloaded_manifest_icons()) {
        IconPurposeAndSizes purpose_and_sizes =
            ConvertSyncDownloadedIconInfoToAppIcon(downloaded_info);
        app_to_update->SetDownloadedIconSizes(
            purpose_and_sizes.first, std::move(purpose_and_sizes.second));
      }

      // DeletePendingIconsFromDisk calls DeletePendingUpdateInfo() after it has
      // removed the pending icon directories from the disk.
      completion_callback = base::BindOnce(
          &ApplyPendingManifestUpdateCommand::DeletePendingIconsFromDisk,
          AsWeakPtr());
    }

    // Measure the time when the app was updated.
    app_to_update->SetManifestUpdateTime(lock_->clock().Now());
  }
  SetStage(ApplyPendingManifestUpdateCommandStage::kSynchronizingOS);
  lock_->os_integration_manager().Synchronize(app_id_,
                                              std::move(completion_callback));
}

void ApplyPendingManifestUpdateCommand::DeletePendingIconsFromDisk() {
  CHECK_EQ(stage_, ApplyPendingManifestUpdateCommandStage::kSynchronizingOS);

  // There has to be icon changes.
  ApplyPendingManifestUpdateResult final_result =
      ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully;
  if (has_name_change_) {
    final_result =
        ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully;
  }

  // Deletion of pending manifest and pending trusted icon directories should
  // only happen after the web app has been updated and OS integration is
  // completed.
  SetStage(
      ApplyPendingManifestUpdateCommandStage::kDeletingPendingIconDirectories);
  lock_->icon_manager().DeletePendingIconData(
      app_id_, WebAppIconManager::DeletePendingPassKey(),
      base::BindOnce(
          [](ApplyPendingManifestUpdateResult expected_success_result,
             bool success) {
            return success ? expected_success_result
                           : ApplyPendingManifestUpdateResult::
                                 kFailedToRemovePendingIconsFromDisk;
          },
          final_result)
          .Then(base::BindOnce(&ApplyPendingManifestUpdateCommand::
                                   DeletePendingUpdateInfoThenComplete,
                               AsWeakPtr())));
}

void ApplyPendingManifestUpdateCommand::DeletePendingUpdateInfoThenComplete(
    ApplyPendingManifestUpdateResult expected_result) {
  CHECK(stage_ == ApplyPendingManifestUpdateCommandStage::kSynchronizingOS ||
        stage_ == ApplyPendingManifestUpdateCommandStage::
                      kDeletingPendingIconDirectories);
  SetStage(ApplyPendingManifestUpdateCommandStage::kDeletingPendingUpdateInfo);
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id_);
    app_to_update->SetPendingUpdateInfo(std::nullopt);
  }
  lock_->install_manager().NotifyWebAppManifestUpdated(app_id_);
  lock_->registrar().NotifyPendingUpdateInfoChanged(
      app_id_, /*pending_update_available=*/false,
      WebAppRegistrar::PendingUpdateInfoChangePassKey());
  CompleteCommandAndSelfDestruct(expected_result);
}

void ApplyPendingManifestUpdateCommand::CompleteCommandAndSelfDestruct(
    ApplyPendingManifestUpdateResult check_result) {
  GetMutableDebugValue().Set("result", base::ToString(check_result));

  CommandResult command_result;
  switch (check_result) {
    case ApplyPendingManifestUpdateResult::kAppNotInstalled:
    case ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully:
    case ApplyPendingManifestUpdateResult::
        kFailedToOverwriteIconsFromPendingIcons:
    case ApplyPendingManifestUpdateResult::kNoPendingUpdate:
    case ApplyPendingManifestUpdateResult::kFailedToRemovePendingIconsFromDisk:
    case ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully:
    case ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully:
      command_result = CommandResult::kSuccess;
      break;
    case ApplyPendingManifestUpdateResult::kSystemShutdown:
      NOTREACHED() << "The value should only be specified in the constructor "
                      "and never given to this method.";
  }

  CompleteAndSelfDestruct(command_result, check_result);
}

}  // namespace web_app
