// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_check_command_v2.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

bool SizeAndPurpose::operator<(const SizeAndPurpose& other) const {
  return size.height() < other.size.height() &&
         size.width() < other.size.width() && purpose < other.purpose;
}

bool SizeAndPurpose::operator==(const SizeAndPurpose& other) const {
  return size == other.size && purpose == other.purpose;
}

size_t SizeAndPurpose::absl_container_hash::operator()(
    const SizeAndPurpose& key) const {
  return absl::HashOf(key.size.width(), key.size.height(), key.purpose);
}

ManifestUpdateCheckCommandV2::ManifestUpdateCheckCommandV2(
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
          "ManifestUpdateCheckCommandV2",
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

ManifestUpdateCheckCommandV2::~ManifestUpdateCheckCommandV2() = default;

void ManifestUpdateCheckCommandV2::StartWithLock(
    std::unique_ptr<AppLock> lock) {
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
      base::BindOnce(&ManifestUpdateCheckCommandV2::DownloadNewManifestData,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::LoadExistingManifestData,
                     GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommandV2::DownloadChangedIconUrlBitmaps,
          GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::CheckComplete,
                     GetWeakPtr()));
}

IconPurpose GetIconPurpose(apps::IconInfo::Purpose purpose) {
  switch (purpose) {
    case apps::IconInfo::Purpose::kAny:
      return IconPurpose::ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return IconPurpose::MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return IconPurpose::MASKABLE;
  }
}

absl::flat_hash_map<SizeAndPurpose, GURL>
ManifestUpdateCheckCommandV2::CreateIconSizeAndPurposeMap(
    const std::vector<apps::IconInfo>& icon_infos) {
  absl::flat_hash_map<SizeAndPurpose, GURL> icons_size_and_purpose_map;
  for (const apps::IconInfo& info : icon_infos) {
    IconPurpose info_purpose = GetIconPurpose(info.purpose);
    SizeAndPurpose key = {gfx::Size(info.square_size_px.value_or(0),
                                    info.square_size_px.value_or(0)),
                          info_purpose};
    icons_size_and_purpose_map[key] = info.url;
  }
  return icons_size_and_purpose_map;
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kDownloadingNewManifestData:
////////////////////////////////////////////////////////////////////////////////
void ManifestUpdateCheckCommandV2::DownloadNewManifestData(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kPendingAppLock);
  stage_ = ManifestUpdateCheckStage::kDownloadingNewManifestData;

  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommandV2::DownloadNewManifestJson,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::StashNewManifestJson,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::ValidateNewScopeExtensions,
                     GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommandV2::StashValidatedScopeExtensions,
          GetWeakPtr()),

      std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::DownloadNewManifestJson(
    WebAppDataRetriever::CheckInstallabilityCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), std::move(next_step_callback), params);
}

void ManifestUpdateCheckCommandV2::StashNewManifestJson(
    base::OnceClosure next_step_callback,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

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

  new_icon_size_and_purpose_map_ =
      CreateIconSizeAndPurposeMap(new_install_info_->manifest_icons);

  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommandV2::ValidateNewScopeExtensions(
    OnDidGetWebAppOriginAssociations next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  CHECK(new_install_info_);
  ScopeExtensions new_scope_extensions = new_install_info_->scope_extensions;

  lock_->origin_association_manager().GetWebAppOriginAssociations(
      new_install_info_->manifest_id(), std::move(new_scope_extensions),
      std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::StashValidatedScopeExtensions(
    base::OnceClosure next_step_callback,
    ScopeExtensions validated_scope_extensions) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kWebContentsDestroyed);
    return;
  }

  new_install_info_->validated_scope_extensions =
      std::make_optional(std::move(validated_scope_extensions));
  std::move(next_step_callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kLoadingExistingManifestData:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommandV2::LoadExistingManifestData(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kDownloadingNewManifestData);
  stage_ = ManifestUpdateCheckStage::kLoadingExistingManifestData;

  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommandV2::LoadExistingAppIcons,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::StashExistingAppIcons,
                     GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommandV2::LoadExistingShortcutsMenuIcons,
          GetWeakPtr()),

      base::BindOnce(
          &ManifestUpdateCheckCommandV2::StashExistingShortcutsMenuIcons,
          GetWeakPtr()),

      std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::LoadExistingAppIcons(
    WebAppIconManager::ReadIconBitmapsCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  lock_->icon_manager().ReadAllIcons(app_id_, std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::StashExistingAppIcons(
    base::OnceClosure next_step_callback,
    IconBitmaps icon_bitmaps) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  if (icon_bitmaps.empty()) {
    CompleteCommandAndSelfDestruct(
        ManifestUpdateCheckResult::kIconReadFromDiskFailed);
    return;
  }

  existing_app_icon_bitmaps_ = std::move(icon_bitmaps);
  existing_icon_size_and_purpose_map_ =
      CreateIconSizeAndPurposeMap(GetWebApp().manifest_icons());

  std::move(next_step_callback).Run();
}

void ManifestUpdateCheckCommandV2::LoadExistingShortcutsMenuIcons(
    WebAppIconManager::ReadShortcutsMenuIconsCallback next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_, std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::StashExistingShortcutsMenuIcons(
    base::OnceClosure next_step_callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);

  existing_shortcuts_menu_icon_bitmaps_ =
      std::move(shortcuts_menu_icon_bitmaps);
  std::move(next_step_callback).Run();
}

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kCheckUpdateNeededAndDownloadIcons:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommandV2::DownloadChangedIconUrlBitmaps(
    base::OnceClosure next_step_callback) {
  DCHECK_EQ(stage_, ManifestUpdateCheckStage::kLoadingExistingManifestData);
  stage_ = ManifestUpdateCheckStage::kDownloadingChangedIconUrlBitmaps;
  RunChainedCallbacks(
      base::BindOnce(&ManifestUpdateCheckCommandV2::DownloadNewIconBitmaps,
                     GetWeakPtr()),

      base::BindOnce(&ManifestUpdateCheckCommandV2::StashNewIconBitmaps,
                     GetWeakPtr()),

      std::move(next_step_callback));
}

void ManifestUpdateCheckCommandV2::DownloadNewIconBitmaps(
    WebAppIconDownloader::WebAppIconDownloaderCallback next_step_callback) {
  DCHECK_EQ(stage_,
            ManifestUpdateCheckStage::kDownloadingChangedIconUrlBitmaps);
  CHECK(new_install_info_);
  IconUrlSizeSet icon_urls_to_download;

  for (const auto& [new_key, new_url] : new_icon_size_and_purpose_map_) {
    auto existing_map_it = existing_icon_size_and_purpose_map_.find(new_key);
    auto new_map_it = new_icon_size_and_purpose_map_.find(new_key);
    if (existing_map_it != existing_icon_size_and_purpose_map_.end() &&
        new_map_it != new_icon_size_and_purpose_map_.end() &&
        existing_map_it->second == new_map_it->second) {
      const auto& existing_icon_bitmap =
          existing_app_icon_bitmaps_.GetBitmapsForPurpose(new_key.purpose);
      new_install_info_->icon_bitmaps.SetBitmapsForPurpose(
          new_key.purpose, existing_icon_bitmap);
    } else {
      icon_urls_to_download.insert(
          IconUrlWithSize::Create(new_url, new_key.size));
    }
  }

  // Getting the URLs for other icons including shortcut, file handler, and home
  // tab icons.
  for (const auto& other_icon_url_size :
       GetValidIconUrlsNotFromManifestIconField(*new_install_info_)) {
    icon_urls_to_download.insert(other_icon_url_size);
  }

  IconDownloaderOptions options = {.skip_page_favicons = true,
                                   .fail_all_if_any_fail = true};
  icon_downloader_->Start(web_contents_.get(), icon_urls_to_download,
                          std::move(next_step_callback), options);
}

void ManifestUpdateCheckCommandV2::StashNewIconBitmaps(
    base::OnceClosure next_step_callback,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK_EQ(stage_,
            ManifestUpdateCheckStage::kDownloadingChangedIconUrlBitmaps);

  GetMutableDebugValue().Set("icon_download_result", base::ToString(result));

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

////////////////////////////////////////////////////////////////////////////////
// ManifestUpdateCheckStage::kComplete:
////////////////////////////////////////////////////////////////////////////////

void ManifestUpdateCheckCommandV2::CheckComplete() {
  DCHECK_EQ(stage_,
            ManifestUpdateCheckStage::kDownloadingChangedIconUrlBitmaps);
  stage_ = ManifestUpdateCheckStage::kComplete;

  ManifestUpdateCheckResult check_result =
      manifest_data_changes_ ? ManifestUpdateCheckResult::kAppUpdateNeeded
                             : ManifestUpdateCheckResult::kAppUpToDate;
  CompleteCommandAndSelfDestruct(check_result);
}

const WebApp& ManifestUpdateCheckCommandV2::GetWebApp() const {
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
  DCHECK(web_app);
  return *web_app;
}

bool ManifestUpdateCheckCommandV2::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestUpdateCheckCommandV2::CompleteCommandAndSelfDestruct(
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
