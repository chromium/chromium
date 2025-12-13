// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/app_update_data_read_command.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/concurrent_closures.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/icons/icon_masker.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

IconPurpose ConvertIconSyncPurposeToImageResourcePurpose(
    sync_pb::WebAppIconInfo_Purpose purpose) {
  switch (purpose) {
    case sync_pb::WebAppIconInfo_Purpose_ANY:
      return IconPurpose::ANY;
    case sync_pb::WebAppIconInfo_Purpose_MASKABLE:
      return IconPurpose::MASKABLE;
    // Trusted icons should never have a MONOCHROME purpose, and UNSPECIFIED is
    // a data error.
    case sync_pb::WebAppIconInfo_Purpose_MONOCHROME:
    case sync_pb::WebAppIconInfo_Purpose_UNSPECIFIED:
      NOTREACHED();
  }
}

std::string ToString(AppUpdateDataReadResult read_result) {
  switch (read_result) {
    case AppUpdateDataReadResult::kFlagNotEnabled:
      return "flag_not_enabled";
    case AppUpdateDataReadResult::kAppNotInstalled:
      return "app_not_installed";
    case AppUpdateDataReadResult::kAppDoesNotHavePendingUpdate:
      return "app_doesnt_have_pending_update";
    case AppUpdateDataReadResult::kFailedToReadExistingAppIcons:
      return "failed_to_read_existing_app_icons";
    case AppUpdateDataReadResult::kFailedToReadPendingAppIconsWhenRequested:
      return "failed_to_read_pending_app_icons_when_requested";
    case AppUpdateDataReadResult::kSystemShutdown:
      return "system_shutdown_while_commands_were_running";
    case AppUpdateDataReadResult::kSuccess:
      return "data_parse_success";
  }
}

}  // namespace

AppUpdateDataReadCommand::AppUpdateDataReadCommand(
    const webapps::AppId& app_id,
    base::OnceCallback<void(UpdateMetadata)> completed_callback)
    : WebAppCommand<AppLock, AppUpdateDataReadResult, UpdateMetadata>(
          "AppUpdateDataReadCommand",
          AppLockDescription(app_id),
          base::BindOnce([](AppUpdateDataReadResult result,
                            UpdateMetadata update_metadata) {
            base::UmaHistogramEnumeration("WebApp.PendingUpdateData.ReadResult",
                                          result);
            return update_metadata;
          }).Then(std::move(completed_callback)),
          /*args_for_shutdown=*/
          std::make_tuple(AppUpdateDataReadResult::kSystemShutdown,
                          std::nullopt)),
      app_id_(app_id) {
  GetMutableDebugValue().Set("app_id", app_id);
}

AppUpdateDataReadCommand::~AppUpdateDataReadCommand() = default;

void AppUpdateDataReadCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (!base::FeatureList::IsEnabled(features::kWebAppPredictableAppUpdating)) {
    ReportResultAndDestroy(AppUpdateDataReadResult::kFlagNotEnabled);
    return;
  }

  const WebAppRegistrar& registrar = lock_->registrar();
  if (!registrar.AppMatches(app_id_, WebAppFilter::InstalledInChrome())) {
    ReportResultAndDestroy(AppUpdateDataReadResult::kAppNotInstalled);
    return;
  }

  const WebApp* web_app = registrar.GetAppById(app_id_);
  if (!web_app->pending_update_info().has_value()) {
    GetMutableDebugValue().Set(
        "result",
        ToString(AppUpdateDataReadResult::kAppDoesNotHavePendingUpdate));
    ReportResultAndDestroy(
        AppUpdateDataReadResult::kAppDoesNotHavePendingUpdate);
    return;
  }

  // Start fetching the icons for the update dialog.
  pending_update_info_ = *web_app->pending_update_info();
  GetMutableDebugValue().Set("pending_update_info",
                             proto::ToValue(pending_update_info_));

  std::optional<IconPurpose> trusted_icon_purpose_if_any =
      pending_update_info_.trusted_icons_size() > 0
          ? std::optional(ConvertIconSyncPurposeToImageResourcePurpose(
                pending_update_info_.trusted_icons(0).purpose()))
          : std::nullopt;
  lock_->icon_manager().ReadIconsForPendingUpdate(
      app_id_, web_app::kIconSizeForUpdateDialog, trusted_icon_purpose_if_any,
      base::BindOnce(&AppUpdateDataReadCommand::OnIconFetchedMaybeMaskForUpdate,
                     weak_factory_.GetWeakPtr()));
}

void AppUpdateDataReadCommand::OnIconFetchedMaybeMaskForUpdate(
    IconMetadataForUpdate icon_metadata) {
  // The `from_icon` has to be populated.
  if (icon_metadata.from_icon.drawsNothing()) {
    ReportResultAndDestroy(
        AppUpdateDataReadResult::kFailedToReadExistingAppIcons);
    return;
  }

  base::ConcurrentClosures concurrent;
  auto from_icon_callback =
      base::BindOnce(&AppUpdateDataReadCommand::SetOldIconForIdentityUpdate,
                     weak_factory_.GetWeakPtr())
          .Then(concurrent.CreateClosure());

  if (icon_metadata.from_icon_purpose == IconPurpose::MASKABLE) {
    web_app::MaskIconOnOs(std::move(icon_metadata.from_icon),
                          std::move(from_icon_callback));
  } else {
    std::move(from_icon_callback).Run(std::move(icon_metadata.from_icon));
  }

  // Only start reading the "to" icons if they are required.
  if (icon_metadata.to_icon.has_value()) {
    if (icon_metadata.to_icon->drawsNothing()) {
      ReportResultAndDestroy(
          AppUpdateDataReadResult::kFailedToReadPendingAppIconsWhenRequested);
      return;
    }

    auto to_icon_callback =
        base::BindOnce(&AppUpdateDataReadCommand::SetNewIconForIdentityUpdate,
                       weak_factory_.GetWeakPtr())
            .Then(concurrent.CreateClosure());

    if (icon_metadata.to_icon_purpose == IconPurpose::MASKABLE) {
      web_app::MaskIconOnOs(std::move(*icon_metadata.to_icon),
                            std::move(to_icon_callback));
    } else {
      std::move(to_icon_callback).Run(std::move(*icon_metadata.to_icon));
    }
  }

  std::move(concurrent)
      .Done(base::BindOnce(
          &AppUpdateDataReadCommand::OnIconsProcessedCreateIdentity,
          weak_factory_.GetWeakPtr()));
}

void AppUpdateDataReadCommand::SetOldIconForIdentityUpdate(SkBitmap old_icon) {
  update_.old_icon = gfx::Image::CreateFrom1xBitmap(std::move(old_icon));
}

void AppUpdateDataReadCommand::SetNewIconForIdentityUpdate(SkBitmap new_icon) {
  update_.new_icon = gfx::Image::CreateFrom1xBitmap(std::move(new_icon));
  GetMutableDebugValue().Set("new_icon_set", true);
}

void AppUpdateDataReadCommand::OnIconsProcessedCreateIdentity() {
  // By this time, the old icon in `WebAppIdentityUpdate` should already have
  // been populated at least, so verify that.
  CHECK(!update_.old_icon.IsEmpty());

  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  CHECK(web_app);

  update_.old_title = base::UTF8ToUTF16(web_app->untranslated_name());
  update_.new_title =
      pending_update_info_.has_name()
          ? std::make_optional(base::UTF8ToUTF16(pending_update_info_.name()))
          : std::nullopt;
  GetMutableDebugValue().Set("new_name", update_.new_title.has_value());
  ReportResultAndDestroy(AppUpdateDataReadResult::kSuccess);
}

void AppUpdateDataReadCommand::ReportResultAndDestroy(
    AppUpdateDataReadResult data_read_result) {
  GetMutableDebugValue().Set("result", ToString(data_read_result));
  CommandResult command_result = CommandResult::kFailure;
  switch (data_read_result) {
    case AppUpdateDataReadResult::kAppNotInstalled:
    case AppUpdateDataReadResult::kAppDoesNotHavePendingUpdate:
    case AppUpdateDataReadResult::kFailedToReadExistingAppIcons:
    case AppUpdateDataReadResult::kFailedToReadPendingAppIconsWhenRequested:
    case AppUpdateDataReadResult::kSystemShutdown:
      command_result = CommandResult::kFailure;
      break;
    case AppUpdateDataReadResult::kFlagNotEnabled:
    case AppUpdateDataReadResult::kSuccess:
      command_result = CommandResult::kSuccess;
      break;
  }
  CompleteAndSelfDestruct(command_result, data_read_result,
                          command_result == CommandResult::kSuccess
                              ? std::make_optional(update_)
                              : std::nullopt);
}

}  // namespace web_app
