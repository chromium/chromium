// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/generated_icon_fix_command.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {

GeneratedIconFixCommand::GeneratedIconFixCommand(
    webapps::AppId app_id,
    GeneratedIconFixSource source,
    base::OnceCallback<void(GeneratedIconFixResult)> callback)
    : WebAppCommand<SharedWebContentsWithAppLock, GeneratedIconFixResult>(
          "GeneratedIconFixCommand",
          SharedWebContentsWithAppLockDescription({app_id}),
          std::move(callback),
          GeneratedIconFixResult::kShutdown),
      app_id_(std::move(app_id)),
      source_(source) {
  CHECK(base::FeatureList::IsEnabled(
      features::kWebAppSyncGeneratedIconBackgroundFix));
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("source", base::ToString(source_));
  GetMutableDebugValue().Set("stop_location", stop_location_.ToString());
}

GeneratedIconFixCommand::~GeneratedIconFixCommand() = default;

void GeneratedIconFixCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);

  const WebApp* app = lock_->registrar().GetAppById(app_id_);
  if (!app) {
    Stop(GeneratedIconFixResult::kAppUninstalled, FROM_HERE);
    return;
  }

  // DCHECK instead of CHECK to avoid crashing at start up.
  DCHECK(generated_icon_fix_util::HasRemainingAttempts(*app));

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    generated_icon_fix_util::RecordFixAttempt(*lock_, update, app_id_, source_);
  }

  // Refresh WebApp pointer as the above mutation destroys the previous one.
  app = lock_->registrar().GetAppById(app_id_);

  install_info_ =
      std::make_unique<WebAppInstallInfo>(app->manifest_id(), app->start_url());
  icon_downloader_ = lock_->web_contents_manager().CreateIconDownloader();
  IconUrlSizeSet icon_urls;
  install_info_->manifest_icons = app->manifest_icons();
  // Set title and start_url for PopulateProductIcons() in case it tries to
  // generate icons again.
  install_info_->title = base::UTF8ToUTF16(app->untranslated_name());
  for (const apps::IconInfo& icon_info : install_info_->manifest_icons) {
    icon_urls.emplace(IconUrlWithSize::CreateForUnspecifiedSize(icon_info.url));
  }
  icon_downloader_->Start(
      &lock_->shared_web_contents(), icon_urls,
      base::BindOnce(&GeneratedIconFixCommand::OnIconsDownloaded,
                     weak_factory_.GetWeakPtr()));
}

void GeneratedIconFixCommand::OnIconsDownloaded(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (result != IconsDownloadedResult::kCompleted) {
    Stop(GeneratedIconFixResult::kDownloadFailure, FROM_HERE);
    return;
  }

  PopulateProductIcons(install_info_.get(), &icons_map);
  if (install_info_->is_generated_icon) {
    Stop(GeneratedIconFixResult::kStillGenerated, FROM_HERE);
    return;
  }

  // Note: Empty params are noops, WriteData() never deletes icons.
  lock_->icon_manager().WriteData(
      app_id_, install_info_->icon_bitmaps, /*shortcuts_menu_icons=*/{},
      /*other_icons_map=*/{},
      base::BindOnce(&GeneratedIconFixCommand::OnIconsWritten,
                     weak_factory_.GetWeakPtr()));
}

void GeneratedIconFixCommand::OnIconsWritten(bool success) {
  if (!success) {
    Stop(GeneratedIconFixResult::kWriteFailure, FROM_HERE);
    return;
  }

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    SetWebAppProductIconFields(*install_info_, *update->UpdateApp(app_id_));
  }
  lock_->install_manager().NotifyWebAppManifestUpdated(app_id_);
  Stop(GeneratedIconFixResult::kSuccess, FROM_HERE);
}

void GeneratedIconFixCommand::Stop(GeneratedIconFixResult result,
                                   base::Location location) {
  CHECK(result != GeneratedIconFixResult::kShutdown);
  stop_location_ = std::move(location);

  // TODO(crbug.com/40185008): Record the attempt and call
  // GeneratedIconFixManager::MaybeScheduleNextFix() if !success.

  CompleteAndSelfDestruct(
      [&] {
        switch (result) {
          case GeneratedIconFixResult::kDownloadFailure:
          case GeneratedIconFixResult::kStillGenerated:
          case GeneratedIconFixResult::kWriteFailure:
            return CommandResult::kFailure;
          case GeneratedIconFixResult::kAppUninstalled:
          case GeneratedIconFixResult::kShutdown:
          case GeneratedIconFixResult::kSuccess:
            return CommandResult::kSuccess;
        }
      }(),
      result);
}

}  // namespace web_app
