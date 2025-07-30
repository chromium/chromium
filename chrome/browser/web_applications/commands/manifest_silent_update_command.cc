// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
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
#include "components/webapps/browser/image_visual_diff.h"
#include "content/public/browser/web_contents.h"

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
  if (new_install_info.validated_scope_extensions.has_value() &&
      existing_web_app.validated_scope_extensions() !=
          new_install_info.validated_scope_extensions.value()) {
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

std::vector<blink::Manifest::ImageResource>
ConvertIconInfoVectorToManifestImageResourceVector(
    const std::vector<apps::IconInfo>& app_icon_infos) {
  // Keep track of `image_resources` per icon URL for to maintain 1:1
  // relationship between them.
  std::map<GURL, blink::Manifest::ImageResource> image_resources;

  for (const auto& icon_info : app_icon_infos) {
    blink::Manifest::ImageResource& resource_in_map =
        image_resources[icon_info.url];

    resource_in_map.src = icon_info.url;

    if (icon_info.square_size_px.has_value()) {
      int size_val = icon_info.square_size_px.value();
      gfx::Size new_size(size_val, size_val);
      if (!base::Contains(resource_in_map.sizes, new_size)) {
        resource_in_map.sizes.push_back(new_size);
      }
    }

    auto new_purpose =
        ConvertIconPurposeToManifestImagePurpose(icon_info.purpose);
    if (!base::Contains(resource_in_map.purpose, new_purpose)) {
      resource_in_map.purpose.push_back(new_purpose);
    }
  }

  std::vector<blink::Manifest::ImageResource> result;
  for (auto const& [url, image_resource] : image_resources) {
    result.push_back(image_resource);
  }

  return result;
}

bool HasSecuritySensitiveChangesForPendingUpdate(
    const std::optional<proto::PendingUpdateInfo>& pending_update_info) {
  if (!pending_update_info) {
    return false;
  }
  return pending_update_info->has_name() ||
         (!pending_update_info->trusted_icons().empty() &&
          !pending_update_info->manifest_icons().empty());
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCommandStage stage) {
  switch (stage) {
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
    case ManifestSilentUpdateCommandStage::kCompleteCommand:
      return os << "kCompleteCommand";
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
  }
}

ManifestSilentUpdateCommand::ManifestSilentUpdateCommand(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    CompletedCallback callback,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    std::unique_ptr<WebAppIconDownloader> icon_downloader)
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
      url_(url),
      web_contents_(web_contents),
      data_retriever_(std::move(data_retriever)),
      icon_downloader_(std::move(icon_downloader)) {
  GetMutableDebugValue().Set("url", url_.spec());
  GetMutableDebugValue().Set("stage", base::ToString(stage_));
}

ManifestSilentUpdateCommand::~ManifestSilentUpdateCommand() = default;

void ManifestSilentUpdateCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    AbortCommandOnWebContentsDestruction();
    return;
  }
  Observe(web_contents_.get());

  // ManifestSilentUpdateCommandStage::kAcquiringAppLock:
  stage_ = ManifestSilentUpdateCommandStage::kAcquiringAppLock;
  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &ManifestSilentUpdateCommand::OnManifestFetchedAcquireAppLock,
          GetWeakPtr()),
      params);
}

void ManifestSilentUpdateCommand::OnManifestFetchedAcquireAppLock(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kAcquiringAppLock);

  if (IsWebContentsDestroyed()) {
    AbortCommandOnWebContentsDestruction();
    return;
  }

  GetMutableDebugValue().Set(
      "manifest_url", opt_manifest ? opt_manifest->manifest_url.spec() : "");
  GetMutableDebugValue().Set("manifest_installable_result",
                             base::ToString(installable_status));

  if (installable_status != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
    return;
  }

  CHECK(opt_manifest);
  CHECK(opt_manifest->id.is_valid());
  app_id_ = GenerateAppIdFromManifestId(opt_manifest->id);

  // ManifestSilentUpdateCommandStage::kFetchingNewManifestData
  stage_ = ManifestSilentUpdateCommandStage::kFetchingNewManifestData;
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
  const WebApp* existing_web_app = app_lock_->registrar().GetAppById(app_id_);

  WebAppInstallInfoConstructOptions construct_options;
  construct_options.fail_all_if_any_fail = true;
  new_manifest_trusted_icon_ = GetTrustedIconsFromManifest(opt_manifest->icons);
  std::vector<blink::Manifest::ImageResource> existing_manifest_icons =
      ConvertIconInfoVectorToManifestImageResourceVector(
          existing_web_app->manifest_icons());

  // TODO(crbug.com/427566193): Use WebAppRegistrar to read trusted icons for
  // existing web app.
  existing_manifest_trusted_icon_ =
      GetTrustedIconsFromManifest(existing_manifest_icons);

  bool has_trusted_icon_url_changed =
      new_manifest_trusted_icon_.has_value() &&
      existing_manifest_trusted_icon_.has_value() &&
      new_manifest_trusted_icon_->url != existing_manifest_trusted_icon_->url;

  if (!has_trusted_icon_url_changed) {
    construct_options.skip_primary_icon_download = true;
    has_icon_url_changed_ = false;
  } else {
    has_icon_url_changed_ = true;
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
    AbortCommandOnWebContentsDestruction();
    return;
  }

  new_install_info_ = std::move(install_info);

  // Start validating scope extensions.
  ScopeExtensions new_scope_extensions = new_install_info_->scope_extensions;

  app_lock_->origin_association_manager().GetWebAppOriginAssociations(
      new_install_info_->manifest_id(), std::move(new_scope_extensions),
      base::BindOnce(&ManifestSilentUpdateCommand::
                         StashValidatedScopeExtensionsAndLoadExistingManifest,
                     GetWeakPtr()));
}

void ManifestSilentUpdateCommand::
    StashValidatedScopeExtensionsAndLoadExistingManifest(
        ScopeExtensions validated_scope_extensions) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);

  if (IsWebContentsDestroyed()) {
    AbortCommandOnWebContentsDestruction();
    return;
  }

  new_install_info_->validated_scope_extensions =
      std::make_optional(std::move(validated_scope_extensions));

  // ManifestSilentUpdateCommandStage::kLoadingExistingManifestData
  stage_ = ManifestSilentUpdateCommandStage::kLoadingExistingManifestData;
  // TODO(crbug.com/427566193): Use WebAppRegistrar to read trusted icons for
  // existing web app.
  app_lock_->icon_manager().ReadAllIcons(
      app_id_,
      base::BindOnce(&ManifestSilentUpdateCommand::StashExistingAppIcons,
                     GetWeakPtr()));
}

void ManifestSilentUpdateCommand::StashExistingAppIcons(
    IconBitmaps icon_bitmaps) {
  CHECK_EQ(stage_,
           ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);

  if (icon_bitmaps.empty()) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed);
    return;
  }
  // TODO(msiem): Use the primary icon's bitmaps when retrievable.
  existing_app_icon_bitmaps_ = std::move(icon_bitmaps);
  app_lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_,
      base::BindOnce(&ManifestSilentUpdateCommand::
                         StashExistingShortcutsMenuIconsFinalizeUpdateIfNeeded,
                     GetWeakPtr()));
}

void ManifestSilentUpdateCommand::
    StashExistingShortcutsMenuIconsFinalizeUpdateIfNeeded(
        ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  CHECK_EQ(stage_,
           ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);

  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);

  // ManifestSilentUpdateCommandStage::
  // kComparingManifestData
  stage_ = ManifestSilentUpdateCommandStage::kComparingManifestData;

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(new_install_info_);

  bool silent_update_required = AreNonSecuritySensitiveDataChangesNeeded(
      *web_app, &existing_shortcuts_menu_icon_bitmaps_, *new_install_info_);
  GetMutableDebugValue().Set("silent_update_required",
                             base::ToString(silent_update_required));
  proto::PendingUpdateInfo pending_update_info;

  // TODO(crbug.com/428976598): Silently update preinstalled or admin installed
  // apps here.
  if (new_install_info_->title !=
      base::UTF8ToUTF16(web_app->untranslated_name())) {
    pending_update_info.set_name(base::UTF16ToUTF8(new_install_info_->title));
  }

  if (has_icon_url_changed_) {
    CHECK(new_manifest_trusted_icon_.has_value());
    int new_icon_size = new_manifest_trusted_icon_->square_size_px.value_or(0);

    auto existing_icon_it = existing_app_icon_bitmaps_.any.find(new_icon_size);
    auto new_icon_it = new_install_info_->icon_bitmaps.any.find(new_icon_size);

    // TODO(crbug.com/427566601): Handle proper reading of icons from
    // WebAppIconManager and update algorithm here
    if (new_icon_it != new_install_info_->icon_bitmaps.any.end() &&
        existing_icon_it != existing_app_icon_bitmaps_.any.end() &&
        HasMoreThanTenPercentImageDiff(&(existing_icon_it->second),
                                       &(new_icon_it->second))) {
      // TODO(crbug.com/434743501): Handle policy installs for storing
      // multiple trusted icons.
      sync_pb::WebAppIconInfo* pending_trusted_icon =
          pending_update_info.add_trusted_icons();
      // TODO(crbug.com/427566601): Use trusted icon metadata here to set
      // pending update info.
      pending_trusted_icon->set_url(
          new_install_info_->manifest_icons[0].url.spec());
      sync_pb::WebAppIconInfo_Purpose icon_purpose =
          ConvertIconPurposeToSyncPurpose(
              new_install_info_->manifest_icons[0].purpose);
      pending_trusted_icon->set_purpose(icon_purpose);
      pending_trusted_icon->set_size_in_px(new_icon_size);

      // Stores manifest icon metadata in pending_update_info since it will be
      // overwritten when finalizing a silent update for non-security
      // sensitive fields.
      for (const auto& icon_info : new_install_info_->manifest_icons) {
        sync_pb::WebAppIconInfo* pending_manifest_icon =
            pending_update_info.add_manifest_icons();
        pending_manifest_icon->set_url(icon_info.url.spec());
        pending_manifest_icon->set_purpose(
            ConvertIconPurposeToSyncPurpose(icon_info.purpose));
        if (icon_info.square_size_px.has_value()) {
          pending_manifest_icon->set_size_in_px(
              icon_info.square_size_px.value());
        }
      }

      new_install_info_->icon_bitmaps = existing_app_icon_bitmaps_;
      new_install_info_->manifest_icons = web_app->manifest_icons();
      new_install_info_->trusted_icons = web_app->trusted_icons();

    } else {
      // Silent update is set to true if the icon has less than 10% image
      // diff.
      silent_update_required = true;
    }
  }

  if (!silent_update_required &&
      !HasSecuritySensitiveChangesForPendingUpdate(pending_update_info)) {
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpToDate);
    return;
  }

  // Revert the security sensitive changes to match that of the web app to
  // apply the non-security sensitive updates without changing the identity.
  new_install_info_->title = base::UTF8ToUTF16(web_app->untranslated_name());

  app_lock_->install_finalizer().FinalizeUpdate(
      *new_install_info_,
      base::BindOnce(
          &ManifestSilentUpdateCommand::NonSecuritySensitiveFieldsApplied,
          GetWeakPtr(), silent_update_required,
          std::move(pending_update_info)));
}

// ManifestUpdateCheckStage::kFinalizingSilentManifestChanges
void ManifestSilentUpdateCommand::NonSecuritySensitiveFieldsApplied(
    bool silent_update_applied,
    std::optional<proto::PendingUpdateInfo> pending_update_info,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kComparingManifestData);
  stage_ = ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges;
  if (!IsSuccess(code)) {
    GetMutableDebugValue().Set("installation_code", base::ToString(code));
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

  ManifestSilentUpdateCheckResult final_command_result;
  if (HasSecuritySensitiveChangesForPendingUpdate(pending_update_info)) {
    web_app::ScopedRegistryUpdate update =
        app_lock_->sync_bridge().BeginUpdate();
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    CHECK(app_to_update);
    app_to_update->SetPendingUpdateInfo(std::move(pending_update_info));

    if (!silent_update_applied) {
      final_command_result =
          ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate;
    } else {
      final_command_result =
          ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges;
    }
  } else {
    final_command_result = ManifestSilentUpdateCheckResult::kAppSilentlyUpdated;
  }

  CompleteCommandAndSelfDestruct(final_command_result);
}

// ManifestSilentUpdateCommandStage::kCompleteCommand
void ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct(
    ManifestSilentUpdateCheckResult check_result) {
  stage_ = ManifestSilentUpdateCommandStage::kCompleteCommand;
  GetMutableDebugValue().Set("result", base::ToString(check_result));

  CommandResult command_result = [&] {
    switch (check_result) {
      case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
      case ManifestSilentUpdateCheckResult::kAppUpToDate:
      case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
      case ManifestSilentUpdateCheckResult::
          kAppHasNonSecurityAndSecurityChanges:
      case ManifestSilentUpdateCheckResult::kAppNotInstalled:
      case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
        return CommandResult::kSuccess;
      case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
      case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
        return CommandResult::kFailure;
      case ManifestSilentUpdateCheckResult::kSystemShutdown:
        NOTREACHED() << "This should be handled by OnShutdown()";
    }
  }();

  Observe(nullptr);
  CompleteAndSelfDestruct(command_result, check_result);
}

bool ManifestSilentUpdateCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestSilentUpdateCommand::AbortCommandOnWebContentsDestruction() {
  CompleteCommandAndSelfDestruct(
      ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
}

}  // namespace web_app
