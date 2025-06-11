// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include "base/i18n/time_formatting.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCommandStage stage) {
  switch (stage) {
    case ManifestSilentUpdateCommandStage::kStartManifestDataFetching:
      return os << "kStartManifestDataFetching";
    case ManifestSilentUpdateCommandStage::kLoadingExistingManifestData:
      return os << "kLoadingExistingManifestData";
    case ManifestSilentUpdateCommandStage::kCompleteCommand:
      return os << "kCompleteCommand";
  }
}

ManifestSilentUpdateCommand::ManifestSilentUpdateCommand(
    const GURL& url,
    const webapps::AppId& app_id,
    base::Time check_time,
    base::WeakPtr<content::WebContents> web_contents,
    CompletedCallback callback,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    std::unique_ptr<WebAppIconDownloader> icon_downloader)
    : WebAppCommand<AppLock,
                    ManifestUpdateCheckResult,
                    std::unique_ptr<WebAppInstallInfo>>(
          "ManifestSilentUpdateCommand",
          AppLockDescription(app_id),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(ManifestUpdateCheckResult::kSystemShutdown,
                          /*new_install_info=*/nullptr)),
      url_(url),
      app_id_(app_id),
      check_time_(check_time),
      web_contents_(web_contents),
      data_retriever_(std::move(data_retriever)),
      icon_downloader_(std::move(icon_downloader)) {
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("url", url_.spec());
  GetMutableDebugValue().Set("stage", base::ToString(stage_));
  GetMutableDebugValue().Set("check_time",
                             base::TimeFormatFriendlyDateAndTime(check_time_));
}

ManifestSilentUpdateCommand::~ManifestSilentUpdateCommand() = default;

void ManifestSilentUpdateCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  Observe(web_contents_.get());

  // ManifestSilentUpdateCommandStage::kStartManifestDataFetching:
  stage_ = ManifestSilentUpdateCommandStage::kStartManifestDataFetching;
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
  DCHECK_EQ(stage_,
            ManifestSilentUpdateCommandStage::kStartManifestDataFetching);

  GetMutableDebugValue().Set(
      "manifest_url", opt_manifest ? opt_manifest->manifest_url.spec() : "");
  GetMutableDebugValue().Set("manifest_installable_result",
                             base::ToString(installable_status));

  if (installable_status != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult::kAppNotEligible);
    return;
  }
  DCHECK(opt_manifest);
  CHECK(!new_install_info_);

  new_install_info_ = std::make_unique<WebAppInstallInfo>(
      CreateWebAppInfoFromManifest(*opt_manifest));

  if (app_id_ !=
      GenerateAppIdFromManifestId(new_install_info_->manifest_id())) {
    CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult::kAppIdMismatch);
    return;
  }

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
  DCHECK_EQ(stage_,
            ManifestSilentUpdateCommandStage::kStartManifestDataFetching);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  new_install_info_->validated_scope_extensions =
      std::make_optional(std::move(validated_scope_extensions));

  // ManifestSilentUpdateCommandStage::kLoadingExistingManifestData
  stage_ = ManifestSilentUpdateCommandStage::kLoadingExistingManifestData;
  lock_->icon_manager().ReadAllIcons(
      app_id_,
      base::BindOnce(&ManifestSilentUpdateCommand::StashExistingAppIcons,
                     GetWeakPtr()));
}

void ManifestSilentUpdateCommand::StashExistingAppIcons(
    IconBitmaps icon_bitmaps) {
  DCHECK_EQ(stage_,
            ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);

  if (icon_bitmaps.empty()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kIconReadFromDiskFailed);
    return;
  }

  lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_,
      base::BindOnce(
          &ManifestSilentUpdateCommand::StashExistingShortcutsMenuIcons,
          GetWeakPtr()));
}

void ManifestSilentUpdateCommand::StashExistingShortcutsMenuIcons(
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  DCHECK_EQ(stage_,
            ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);

  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);
  CheckComplete();
}

// ManifestSilentUpdateCommandStage::kCompleteCommand:
void ManifestSilentUpdateCommand::CheckComplete() {
  DCHECK_EQ(stage_,
            ManifestSilentUpdateCommandStage::kLoadingExistingManifestData);
  stage_ = ManifestSilentUpdateCommandStage::kCompleteCommand;

  ManifestUpdateCheckResult check_result =
      manifest_data_changes_ ? ManifestUpdateCheckResult::kAppUpdateNeeded
                             : ManifestUpdateCheckResult::kAppUpToDate;
  CompleteCommandAndSelfDestruct(check_result);
}

const WebApp& ManifestSilentUpdateCommand::GetWebApp() const {
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  DCHECK(web_app);
  return *web_app;
}

bool ManifestSilentUpdateCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestSilentUpdateCommand::CompleteCommandAndSelfDestruct(
    ManifestUpdateCheckResult check_result) {
  GetMutableDebugValue().Set("result", base::ToString(check_result));

  CommandResult command_result = [&] {
    switch (check_result) {
      case ManifestUpdateCheckResult::kAppUpdateNeeded:
      case ManifestUpdateCheckResult::kAppIdentityUpdateRejectedAndUninstalled:
      case ManifestUpdateCheckResult::kAppUpToDate:
        return CommandResult::kSuccess;
      case ManifestUpdateCheckResult::kAppIdMismatch:
      case ManifestUpdateCheckResult::kAppNotEligible:
      case ManifestUpdateCheckResult::kIconDownloadFailed:
      case ManifestUpdateCheckResult::kIconReadFromDiskFailed:
      case ManifestUpdateCheckResult::kWebContentsDestroyed:
      case ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation:
        return CommandResult::kFailure;
      case ManifestUpdateCheckResult::kSystemShutdown:
        NOTREACHED() << "This should be handled by OnShutdown()";
    }
  }();

  Observe(nullptr);
  CompleteAndSelfDestruct(
      command_result, check_result,
      check_result == ManifestUpdateCheckResult::kAppUpdateNeeded
          ? std::move(new_install_info_)
          : nullptr);
}

}  // namespace web_app
