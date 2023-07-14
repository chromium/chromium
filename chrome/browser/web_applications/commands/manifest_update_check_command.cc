// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

ManifestUpdateCheckCommand::ManifestUpdateCheckCommand(
    const GURL& url,
    const AppId& app_id,
    base::Time check_time,
    base::WeakPtr<content::WebContents> web_contents,
    CompletedCallback callback,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    std::unique_ptr<WebAppIconDownloader> icon_downloader)
    : WebAppCommandTemplate<AppLock>("ManifestUpdateCheckCommand"),
      url_(url),
      app_id_(app_id),
      check_time_(check_time),
      completed_callback_(std::move(callback)),
      lock_description_(app_id),
      web_contents_(web_contents),
      data_retriever_(std::move(data_retriever)),
      icon_downloader_(std::move(icon_downloader)) {}

ManifestUpdateCheckCommand::~ManifestUpdateCheckCommand() = default;

const LockDescription& ManifestUpdateCheckCommand::lock_description() const {
  return lock_description_;
}

void ManifestUpdateCheckCommand::OnShutdown() {
  CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult::kSystemShutdown);
}

base::Value ManifestUpdateCheckCommand::ToDebugValue() const {
  base::Value::Dict data = debug_log_.Clone();
  data.Set("app_id", app_id_);
  data.Set("url", url_.spec());
  data.Set("stage", base::ToString(stage_));
  return base::Value(std::move(data));
}

void ManifestUpdateCheckCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }
  Observe(web_contents_.get());

  // Runs a linear sequence of asynchronous and synchronous steps.
  // This sequence can be early exited at any point by a call to
  // CompleteCommandAndSelfDestruct().
  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommand::DownloadNewManifestData,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::LoadExistingManifestData,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::CompareManifestData,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::ResolveIdentityChanges,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::CheckComplete, GetWeakPtr()));
}

void ManifestUpdateCheckCommand::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (url::IsSameOriginWith(navigation_handle->GetPreviousPrimaryMainFrameURL(),
                            navigation_handle->GetURL())) {
    return;
  }

  CompleteCommandAndSelfDestruct(
      ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation);
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kDownloadingNewManifestData:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommand::DownloadNewManifestData(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kPendingAppLock);
  stage_ = ManifestUpdateCheckStage::kDownloadingNewManifestData;

  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommand::DownloadNewManifestJson,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::StashNewManifestJson,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::DownloadNewIconBitmaps,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::StashNewIconBitmaps,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::ValidateNewScopeExtensions,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::StashValidatedScopeExtensions,
                     GetWeakPtr()),

      std::move(next_step_callback));
}

void ManifestUpdateCheckCommand::DownloadNewManifestJson(
    WebAppDataRetriever::CheckInstallabilityCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = false;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      /*bypass_service_worker_check=*/true, std::move(next_step_callback),
      params);
}

void ManifestUpdateCheckCommand::StashNewManifestJson(
    base::OnceClosure next_step_callback,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  debug_log_.Set("manifest_url", manifest_url.spec());
  debug_log_.Set("manifest_installable_result", base::ToString(installable_status));

  if (installable_status != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult::kAppNotEligible);
    return;
  }
  DCHECK(opt_manifest);
  CHECK(!new_install_info_);

  new_install_info_ = std::make_unique<WebAppInstallInfo>(
      CreateWebAppInfoFromManifest(*opt_manifest, manifest_url));
  CHECK(new_install_info_->manifest_id.is_valid());

  if (app_id_ != GenerateAppIdFromManifestId(new_install_info_->manifest_id)) {
    CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult::kAppIdMismatch);
    return;
  }

  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommand::DownloadNewIconBitmaps(
    WebAppIconDownloader::WebAppIconDownloaderCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  CHECK(new_install_info_);
  base::flat_set<GURL> icon_urls =
      GetValidIconUrlsToDownload(*new_install_info_);

  IconDownloaderOptions options = {.skip_page_favicons = true,
                                   .fail_all_if_any_fail = true};
  icon_downloader_->Start(web_contents_.get(), icon_urls,
                          std::move(next_step_callback), options);
}

void ManifestUpdateCheckCommand::StashNewIconBitmaps(
    base::OnceClosure next_step_callback,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  debug_log_.Set("icon_download_result", base::ToString(result));

  RecordIconDownloadMetrics(result, icons_http_results);

  if (result != IconsDownloadedResult::kCompleted) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kIconDownloadFailed);
    return;
  }

  PopulateOtherIcons(new_install_info_.get(), icons_map);
  PopulateProductIcons(new_install_info_.get(), &icons_map);

  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommand::ValidateNewScopeExtensions(
    OnDidGetWebAppOriginAssociations next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  CHECK(new_install_info_);
  CHECK(new_install_info_->manifest_id.is_valid());
  ScopeExtensions new_scope_extensions = new_install_info_->scope_extensions;

  lock_->origin_association_manager().GetWebAppOriginAssociations(
      new_install_info_->manifest_id, std::move(new_scope_extensions),
      std::move(next_step_callback));
}

void ManifestUpdateCheckCommand::StashValidatedScopeExtensions(
    base::OnceClosure next_step_callback,
    ScopeExtensions validated_scope_extensions) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  new_install_info_->validated_scope_extensions =
      absl::make_optional(std::move(validated_scope_extensions));
  std::move(next_step_callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kLoadingExistingManifestData:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommand::LoadExistingManifestData(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);
  stage_ = ManifestUpdateCheckStage::kLoadingExistingManifestData;

  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommand::LoadExistingAppIcons,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommand::StashExistingAppIcons,
                     GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommand::LoadExistingShortcutsMenuIcons,
          GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommand::StashExistingShortcutsMenuIcons,
          GetWeakPtr()),

      std::move(next_step_callback));
}

void ManifestUpdateCheckCommand::LoadExistingAppIcons(
    WebAppIconManager::ReadIconBitmapsCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  lock_->icon_manager().ReadAllIcons(app_id_, std::move(next_step_callback));
}

void ManifestUpdateCheckCommand::StashExistingAppIcons(
    base::OnceClosure next_step_callback,
    IconBitmaps icon_bitmaps) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  if (icon_bitmaps.empty()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kIconReadFromDiskFailed);
    return;
  }

  existing_app_icon_bitmaps_ = std::move(icon_bitmaps);
  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommand::LoadExistingShortcutsMenuIcons(
    WebAppIconManager::ReadShortcutsMenuIconsCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_, std::move(next_step_callback));
}

void ManifestUpdateCheckCommand::StashExistingShortcutsMenuIcons(
    base::OnceClosure next_step_callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);
  std::move(next_step_callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kComparingManifestData:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommand::CompareManifestData(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);
  stage_ = ManifestUpdateCheckStage::kComparingManifestData;

  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  DCHECK(web_app);

  CHECK(new_install_info_);
  manifest_data_changes_ = GetManifestDataChanges(
      GetWebApp(), &existing_app_icon_bitmaps_,
      &existing_shortcuts_menu_icon_bitmaps_, *new_install_info_);

  std::move(next_step_callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kResolvingIdentityChanges:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommand::ResolveIdentityChanges(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kComparingManifestData);
  stage_ = ManifestUpdateCheckStage::kResolvingIdentityChanges;

  if (manifest_data_changes_.app_name_changed) {
    manifest_data_changes_.app_name_identity_update_decision =
        MakeAppNameIdentityUpdateDecision();
  }
  if (manifest_data_changes_.app_icon_identity_change) {
    manifest_data_changes_.app_icon_identity_update_decision =
        MakeAppIconIdentityUpdateDecision();
  }

  // Record metrics prior to reverts to capture attempts to change name/icons.
  RecordIdentityConfirmationMetrics(manifest_data_changes_, GetWebApp());

  // Apply reverts before showing the confirmation dialog to avoid showing
  // rejected changes in the preview.
  RevertIdentityChangesIfNeeded();

  if (manifest_data_changes_.RequiresConfirmation()) {
    ConfirmAppIdentityUpdate(std::move(next_step_callback));
    return;
  }

  std::move(next_step_callback).Run();
}

IdentityUpdateDecision
ManifestUpdateCheckCommand::MakeAppNameIdentityUpdateDecision() const {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kResolvingIdentityChanges);
  DCHECK(manifest_data_changes_.app_name_changed);

  const WebApp& web_app = GetWebApp();
  if (CanWebAppSilentlyUpdateIdentity(web_app)) {
    return IdentityUpdateDecision::kSilentlyAllow;
  }

  if (CanShowIdentityUpdateConfirmationDialog(lock_->registrar(), web_app)) {
    return IdentityUpdateDecision::kGetUserConfirmation;
  }

  return IdentityUpdateDecision::kRevert;
}

IdentityUpdateDecision
ManifestUpdateCheckCommand::MakeAppIconIdentityUpdateDecision() const {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kResolvingIdentityChanges);
  DCHECK(manifest_data_changes_.app_icon_identity_change);

  const WebApp& web_app = GetWebApp();
  if (CanWebAppSilentlyUpdateIdentity(web_app) ||
      base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating)) {
    return IdentityUpdateDecision::kSilentlyAllow;
  }

  // Web apps that were installed by sync but have generated icons get a window
  // of time where they can "fix" themselves silently to use the site provided
  // icons.
  constexpr base::TimeDelta kSyncGeneratedIconFixWindowDuration = base::Days(7);
  if (base::FeatureList::IsEnabled(
          features::kWebAppSyncGeneratedIconUpdateFix) &&
      web_app.is_generated_icon() &&
      web_app.latest_install_source() == webapps::WebappInstallSource::SYNC &&
      check_time_ <
          (web_app.install_time() + kSyncGeneratedIconFixWindowDuration)) {
    return IdentityUpdateDecision::kSilentlyAllow;
  }

  if (CanShowIdentityUpdateConfirmationDialog(lock_->registrar(), web_app) &&
      base::FeatureList::IsEnabled(features::kPwaUpdateDialogForIcon)) {
    return IdentityUpdateDecision::kGetUserConfirmation;
  }

  return IdentityUpdateDecision::kRevert;
}

void ManifestUpdateCheckCommand::ConfirmAppIdentityUpdate(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kResolvingIdentityChanges);

  DCHECK(
      CanShowIdentityUpdateConfirmationDialog(lock_->registrar(), GetWebApp()));
  DCHECK(manifest_data_changes_.RequiresConfirmation());

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  const SkBitmap* before_icon = nullptr;
  const SkBitmap* after_icon = nullptr;
  if (manifest_data_changes_.app_icon_identity_change) {
    before_icon = &manifest_data_changes_.app_icon_identity_change->before;
    after_icon = &manifest_data_changes_.app_icon_identity_change->after;
  } else {
    // Even though this is just a name update an icon still needs to be shown
    // to the user, find an existing icon to show in the before/after.
    for (SquareSizePx size : kIdentitySizes) {
      auto it = existing_app_icon_bitmaps_.any.find(size);
      if (it != existing_app_icon_bitmaps_.any.end()) {
        before_icon = &it->second;
        after_icon = before_icon;
        break;
      }
    }
    // TODO(crbug.com/1409710): Try other sizes if the above sizes fail.
  }

  if (before_icon == nullptr || after_icon == nullptr ||
      before_icon->drawsNothing() || after_icon->drawsNothing()) {
    OnIdentityUpdateConfirmationComplete(std::move(next_step_callback),
                                         AppIdentityUpdate::kSkipped);
    return;
  }

  lock_->ui_manager().ShowWebAppIdentityUpdateDialog(
      app_id_,
      /*title_change=*/manifest_data_changes_.app_name_changed,
      /*icon_change=*/
      manifest_data_changes_.app_icon_identity_change.has_value(),
      /*old_title=*/base::UTF8ToUTF16(GetWebApp().untranslated_name()),
      /*new_title=*/new_install_info_->title,
      /*old_icon=*/*before_icon,
      /*new_icon=*/*after_icon, web_contents_.get(),
      base::BindOnce(
          &ManifestUpdateCheckCommand::OnIdentityUpdateConfirmationComplete,
          GetWeakPtr(), std::move(next_step_callback)));
}

void ManifestUpdateCheckCommand::OnIdentityUpdateConfirmationComplete(
    base::OnceClosure next_step_callback,
    AppIdentityUpdate app_identity_update) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kResolvingIdentityChanges);

  switch (app_identity_update) {
    case AppIdentityUpdate::kAllowed:
      break;

    case AppIdentityUpdate::kUninstall:
      CompleteCommandAndSelfDestruct(
          ManifestUpdateCheckResult::kAppIdentityUpdateRejectedAndUninstalled);
      return;

    case AppIdentityUpdate::kSkipped: {
      manifest_data_changes_.app_name_identity_update_decision =
          IdentityUpdateDecision::kRevert;
      manifest_data_changes_.app_icon_identity_update_decision =
          IdentityUpdateDecision::kRevert;
      RevertIdentityChangesIfNeeded();
      break;
    }
  }

  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommand::RevertIdentityChangesIfNeeded() {
  if (manifest_data_changes_.app_name_identity_update_decision ==
          IdentityUpdateDecision::kRevert &&
      manifest_data_changes_.app_name_changed) {
    // Revert to WebApp::untranslated_name() instead of
    // WebAppRegistrar::GetAppShortName() because that's the field
    // WebAppInstallInfo::title gets written to (see SetWebAppManifestFields()).
    new_install_info_->title =
        base::UTF8ToUTF16(GetWebApp().untranslated_name());
    manifest_data_changes_.app_name_changed = false;
  }

  if (manifest_data_changes_.app_icon_identity_update_decision ==
          IdentityUpdateDecision::kRevert &&
      manifest_data_changes_.app_icon_identity_change) {
    const WebApp& web_app = GetWebApp();
    new_install_info_->manifest_icons = web_app.manifest_icons();
    new_install_info_->icon_bitmaps = existing_app_icon_bitmaps_;
    new_install_info_->is_generated_icon = web_app.is_generated_icon();
    manifest_data_changes_.app_icon_identity_change.reset();
    manifest_data_changes_.any_app_icon_changed = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kComplete:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommand::CheckComplete() {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kResolvingIdentityChanges);
  stage_ = ManifestUpdateCheckStage::kComplete;

  ManifestUpdateCheckResult check_result =
      manifest_data_changes_ ? ManifestUpdateCheckResult::kAppUpdateNeeded
                             : ManifestUpdateCheckResult::kAppUpToDate;
  CompleteCommandAndSelfDestruct(check_result);
}

const WebApp& ManifestUpdateCheckCommand::GetWebApp() const {
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  DCHECK(web_app);
  return *web_app;
}

bool ManifestUpdateCheckCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestUpdateCheckCommand::CompleteCommandAndSelfDestruct(
    ManifestUpdateCheckResult check_result) {
  debug_log_.Set("result", base::ToString(check_result));

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
        return CommandResult::kShutdown;
    }
  }();

  Observe(nullptr);
  SignalCompletionAndSelfDestruct(
      command_result,
      base::BindOnce(std::move(completed_callback_), check_result,
                     check_result == ManifestUpdateCheckResult::kAppUpdateNeeded
                         ? absl::make_optional<WebAppInstallInfo>(
                               std::move(*new_install_info_))
                         : absl::nullopt));
}

}  // namespace web_app
