// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/app_migration_data_read_command.h"

#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/icons/icon_masker.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

AppMigrationDataReadCommand::AppMigrationDataReadCommand(
    const webapps::AppId& old_app_id,
    const webapps::AppId& new_app_id,
    bool is_forced_migration_on_startup,
    base::OnceCallback<void(UpdateMetadata)> completed_callback)
    : WebAppCommand<AppLock, UpdateMetadata>(
          "AppMigrationDataReadCommand",
          AppLockDescription({old_app_id, new_app_id}),
          std::move(completed_callback),
          /*args_for_shutdown=*/
          std::make_tuple(std::nullopt)),
      old_app_id_(old_app_id),
      new_app_id_(new_app_id) {
  update_.is_forced_migration = is_forced_migration_on_startup;

  GetMutableDebugValue().Set("old_app_id", old_app_id);
  GetMutableDebugValue().Set("new_app_id", new_app_id);
}

AppMigrationDataReadCommand::~AppMigrationDataReadCommand() = default;

void AppMigrationDataReadCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  const WebAppRegistrar& registrar = lock_->registrar();

  CHECK(base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi));
  if (!registrar.AppMatches(old_app_id_,
                            WebAppFilter::IsAppValidMigrationSource()) ||
      !registrar.GetInstallState(new_app_id_).has_value()) {
    return;
  }

  auto* old_web_app = registrar.GetAppById(old_app_id_);
  CHECK(old_web_app->pending_migration_info().has_value());

  GetMutableDebugValue().Set(
      "pending_migration_info",
      old_web_app->pending_migration_info()->AsDebugValue());

  const auto barrier_closure = base::BarrierClosure(
      /*num_closures=*/update_.is_forced_migration ? 1 : 2,
      base::BindOnce(
          &AppMigrationDataReadCommand::OnIconsProcessedCreateIdentity,
          weak_factory_.GetWeakPtr()));

  ReadSingleIcon(
      old_app_id_,
      base::BindOnce(&AppMigrationDataReadCommand::SetOldIconForIdentityUpdate,
                     weak_factory_.GetWeakPtr())
          .Then(barrier_closure));

  // If this is a forced migration, icon changes are ignored
  if (!update_.is_forced_migration) {
    ReadSingleIcon(
        new_app_id_,
        base::BindOnce(
            &AppMigrationDataReadCommand::SetNewIconForIdentityUpdate,
            weak_factory_.GetWeakPtr())
            .Then(barrier_closure));
  }
}

void AppMigrationDataReadCommand::ReadSingleIcon(
    const webapps::AppId& app_id,
    base::OnceCallback<void(SkBitmap)> callback) {
  IconPurpose purpose = IconPurpose::ANY;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  auto* web_app = lock_->registrar().GetAppById(app_id);
  if (!web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE).empty()) {
    purpose = IconPurpose::MASKABLE;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

  lock_->icon_manager().ReadTrustedIconsWithFallbackToManifestIcons(
      app_id, {web_app::kIconSizeForUpdateDialog}, purpose,
      base::BindOnce(
          [](base::OnceCallback<void(SkBitmap)> callback,
             IconMetadataFromDisk icon_bitmap_metadata) {
            auto icon = icon_bitmap_metadata
                            .icons_map[web_app::kIconSizeForUpdateDialog];
            if (icon_bitmap_metadata.purpose == IconPurpose::MASKABLE) {
              web_app::MaskIconOnOs(std::move(icon), std::move(callback));
            } else {
              std::move(callback).Run(std::move(icon));
            }
          },
          std::move(callback)));
}

void AppMigrationDataReadCommand::SetOldIconForIdentityUpdate(
    SkBitmap old_icon) {
  update_.old_icon = gfx::Image::CreateFrom1xBitmap(std::move(old_icon));
}

void AppMigrationDataReadCommand::SetNewIconForIdentityUpdate(
    SkBitmap new_icon) {
  update_.new_icon = gfx::Image::CreateFrom1xBitmap(std::move(new_icon));
  GetMutableDebugValue().Set("new_icon_set", true);
}

void AppMigrationDataReadCommand::OnIconsProcessedCreateIdentity() {
  // By this time, the old icon in `WebAppIdentityUpdate` should already have
  // been populated at least, so verify that.
  CHECK(!update_.old_icon.IsEmpty());

  const WebApp* old_web_app = lock_->registrar().GetAppById(old_app_id_);
  const WebApp* new_web_app = lock_->registrar().GetAppById(new_app_id_);
  CHECK(old_web_app);
  CHECK(new_web_app);

  // Name changes are ignored for forced migration.
  update_.old_title = base::UTF8ToUTF16(old_web_app->untranslated_name());
  if (old_web_app->untranslated_name() != new_web_app->untranslated_name() &&
      !update_.is_forced_migration) {
    update_.new_title = base::UTF8ToUTF16(new_web_app->untranslated_name());
    GetMutableDebugValue().Set("new_name", update_.new_title.has_value());
  }
  update_.old_start_url = old_web_app->start_url();
  update_.new_start_url = new_web_app->start_url();

  // TODO(japhet): Figure out how to handle the case where the icon hasn't
  // changed.
  CompleteAndSelfDestruct(CommandResult::kSuccess, std::make_optional(update_));
}

}  // namespace web_app
