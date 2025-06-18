// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
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
    case ManifestSilentUpdateCommandStage::
        kComparingNonSecuritySensitiveManifestData:
      return os << "kComparingNonSecuritySensitiveManifestData";
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
          std::move(callback),
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
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  Observe(web_contents_.get());

  // ManifestSilentUpdateCommandStage::kFetchingNewManifestData:
  stage_ = ManifestSilentUpdateCommandStage::kFetchingNewManifestData;
  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(&ManifestSilentUpdateCommand::StashNewManifestJson,
                     GetWeakPtr()),
      params);
}

void ManifestSilentUpdateCommand::StashNewManifestJson(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);

  GetMutableDebugValue().Set(
      "manifest_url", opt_manifest ? opt_manifest->manifest_url.spec() : "");
  GetMutableDebugValue().Set("manifest_installable_result",
                             base::ToString(installable_status));

  if (installable_status != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
    return;
  }
  CHECK(opt_manifest);
  CHECK(!new_install_info_);

  new_install_info_ = std::make_unique<WebAppInstallInfo>(
      CreateWebAppInfoFromManifest(*opt_manifest));
  app_id_ = GenerateAppIdFromManifestId(new_install_info_->manifest_id());

  // Start validating scope extensions.
  ScopeExtensions new_scope_extensions = new_install_info_->scope_extensions;

  lock_->origin_association_manager().GetWebAppOriginAssociations(
      new_install_info_->manifest_id(), std::move(new_scope_extensions),
      base::BindOnce(
          &ManifestSilentUpdateCommand::StashValidatedScopeExtensions,
          GetWeakPtr()));
}

void ManifestSilentUpdateCommand::StashValidatedScopeExtensions(
    ScopeExtensions validated_scope_extensions) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kFetchingNewManifestData);

  if (IsWebContentsDestroyed()) {
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  new_install_info_->validated_scope_extensions =
      std::make_optional(std::move(validated_scope_extensions));

  stage_ = ManifestSilentUpdateCommandStage::kAcquiringAppLock;
  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(lock_), *app_lock_, {app_id_},
      base::BindOnce(&ManifestSilentUpdateCommand::OnAppLockRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void ManifestSilentUpdateCommand::OnAppLockRetrieved() {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::kAcquiringAppLock);
  // ManifestSilentUpdateCommandStage::kLoadingExistingManifestData
  stage_ = ManifestSilentUpdateCommandStage::kLoadingExistingManifestData;
  if (!app_lock_->registrar().IsInRegistrar(app_id_)) {
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kAppNotInstalled);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppNotInstalled);
    return;
  }
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
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed);
    return;
  }

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
  // kComparingNonSecuritySensitiveManifestData
  stage_ = ManifestSilentUpdateCommandStage::
      kComparingNonSecuritySensitiveManifestData;

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  CHECK(new_install_info_);

  // Start construction of pending update information here, or at the end of
  // IsInstallationComplete().
  if (!AreNonSecuritySensitiveDataChangesNeeded(
          *web_app, &existing_shortcuts_menu_icon_bitmaps_,
          *new_install_info_)) {
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kAppUpToDate);
    CompleteCommandAndSelfDestruct(
        ManifestSilentUpdateCheckResult::kAppUpToDate);
  } else {
    // Revert the security sensitive changes to match that of the web app to
    // apply the non-security sensitive updates without changing the identity.
    new_install_info_->title = base::UTF8ToUTF16(web_app->untranslated_name());
    new_install_info_->manifest_icons = web_app->manifest_icons();
    new_install_info_->icon_bitmaps = existing_app_icon_bitmaps_;
    app_lock_->install_finalizer().FinalizeUpdate(
        *new_install_info_,
        base::BindOnce(
            &ManifestSilentUpdateCommand::NonSecuritySensitiveFieldsApplied,
            GetWeakPtr()));
  }
}

// ManifestUpdateCheckStage::kFinalizingSilentManifestChanges
void ManifestSilentUpdateCommand::NonSecuritySensitiveFieldsApplied(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CHECK_EQ(stage_, ManifestSilentUpdateCommandStage::
                       kComparingNonSecuritySensitiveManifestData);
  stage_ = ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges;
  if (!IsSuccess(code)) {
    GetMutableDebugValue().Set("installation_code", base::ToString(code));
    base::UmaHistogramEnumeration(
        "Webapp.Update.ManifestSilentUpdateCheckResult",
        ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall);
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
  CHECK(!AreNonSecuritySensitiveDataChangesNeeded(
      *existing_web_app, &existing_shortcuts_menu_icon_bitmaps_,
      *new_install_info_));
  CHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);

  base::UmaHistogramEnumeration(
      "Webapp.Update.ManifestSilentUpdateCheckResult",
      ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  CompleteCommandAndSelfDestruct(
      ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
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
        return CommandResult::kSuccess;
      case ManifestSilentUpdateCheckResult::kAppNotInstalled:
      case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
      case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
      case ManifestSilentUpdateCheckResult::kWebContentsDestroyed:
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

}  // namespace web_app
