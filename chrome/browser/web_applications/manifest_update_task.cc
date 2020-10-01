// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_task.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

namespace {

bool HaveIconContentsChanged(
    const std::map<SquareSizePx, SkBitmap>& disk_icon_bitmaps,
    const std::map<SquareSizePx, SkBitmap>& downloaded_icon_bitmaps) {
  if (downloaded_icon_bitmaps.size() != disk_icon_bitmaps.size())
    return true;

  for (const std::pair<const SquareSizePx, SkBitmap>& entry :
       downloaded_icon_bitmaps) {
    SquareSizePx size = entry.first;
    const SkBitmap& downloaded_bitmap = entry.second;

    auto it = disk_icon_bitmaps.find(size);
    if (it == disk_icon_bitmaps.end())
      return true;

    const SkBitmap& disk_bitmap = it->second;
    if (!gfx::BitmapsAreEqual(downloaded_bitmap, disk_bitmap))
      return true;
  }

  return false;
}

}  // namespace

ManifestUpdateTask::ManifestUpdateTask(const GURL& url,
                                       const AppId& app_id,
                                       content::WebContents* web_contents,
                                       StoppedCallback stopped_callback,
                                       bool hang_for_testing,
                                       const AppRegistrar& registrar,
                                       const AppIconManager& icon_manager,
                                       WebAppUiManager* ui_manager,
                                       InstallManager* install_manager)
    : content::WebContentsObserver(web_contents),
      registrar_(registrar),
      icon_manager_(icon_manager),
      ui_manager_(*ui_manager),
      install_manager_(*install_manager),
      url_(url),
      app_id_(app_id),
      stopped_callback_(std::move(stopped_callback)),
      hang_for_testing_(hang_for_testing) {
  // Task starts by waiting for DidFinishLoad() to be called.
  stage_ = Stage::kPendingPageLoad;
}

ManifestUpdateTask::~ManifestUpdateTask() {
#if DCHECK_IS_ON()
  if (destructor_called_ptr_) {
    DCHECK(!(*destructor_called_ptr_));
    *destructor_called_ptr_ = true;
  }
#endif
}

// content::WebContentsObserver:
void ManifestUpdateTask::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (stage_ != Stage::kPendingPageLoad || hang_for_testing_)
    return;

  if (render_frame_host->GetParent() != nullptr)
    return;

  stage_ = Stage::kPendingInstallableData;
  InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = false;
  InstallableManager::FromWebContents(web_contents())
      ->GetData(params,
                base::BindOnce(&ManifestUpdateTask::OnDidGetInstallableData,
                               AsWeakPtr()));
}

// content::WebContentsObserver:
void ManifestUpdateTask::WebContentsDestroyed() {
  switch (stage_) {
    case Stage::kPendingPageLoad:
    case Stage::kPendingInstallableData:
    case Stage::kPendingIconDownload:
      DestroySelf(ManifestUpdateResult::kWebContentsDestroyed);
      return;
    case Stage::kPendingIconReadFromDisk:
    case Stage::kPendingWindowsClosed:
    case Stage::kPendingInstallation:
      // These stages should have stopped listening to the web contents.
      NOTREACHED();
      Observe(nullptr);
      break;
  }
}

void ManifestUpdateTask::OnDidGetInstallableData(const InstallableData& data) {
  DCHECK_EQ(stage_, Stage::kPendingInstallableData);

  if (!data.errors.empty()) {
    DestroySelf(ManifestUpdateResult::kAppNotEligible);
    return;
  }

  DCHECK(data.manifest);
  web_application_info_.emplace();
  UpdateWebAppInfoFromManifest(*data.manifest, &web_application_info_.value());

  if (IsUpdateNeededForManifest()) {
    UpdateAfterWindowsClose();
    return;
  }

  LoadAndCheckIconContents();
}

bool ManifestUpdateTask::IsUpdateNeededForManifest() const {
  DCHECK(web_application_info_.has_value());

  if (app_id_ != GenerateAppIdFromURL(web_application_info_->start_url))
    return false;

  if (web_application_info_->theme_color !=
      registrar_.GetAppThemeColor(app_id_))
    return true;

  if (web_application_info_->scope != registrar_.GetAppScopeInternal(app_id_))
    return true;

  if (web_application_info_->display_mode !=
      registrar_.GetAppDisplayMode(app_id_)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(features::kWebAppManifestDisplayOverride) &&
      web_application_info_->display_override !=
          registrar_.GetAppDisplayModeOverride(app_id_)) {
    return true;
  }

  if (web_application_info_->icon_infos != registrar_.GetAppIconInfos(app_id_))
    return true;

  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      web_application_info_->shortcuts_menu_item_infos !=
          registrar_.GetAppShortcutsMenuItemInfos(app_id_)) {
    return true;
  }

  const apps::ShareTarget* app_share_target =
      registrar_.GetAppShareTarget(app_id_);
  if (app_share_target) {
    if (!web_application_info_->share_target ||
        *web_application_info_->share_target != *app_share_target) {
      return true;
    }
  } else if (web_application_info_->share_target) {
    return true;
  }

  // TODO(crbug.com/926083): Check more manifest fields.
  return false;
}

void ManifestUpdateTask::UpdateAfterWindowsClose() {
  DCHECK(stage_ == Stage::kPendingInstallableData ||
         stage_ == Stage::kPendingIconReadFromDisk);
  stage_ = Stage::kPendingWindowsClosed;
  Observe(nullptr);

  ui_manager_.NotifyOnAllAppWindowsClosed(
      app_id_,
      base::BindOnce(&ManifestUpdateTask::OnAllAppWindowsClosed, AsWeakPtr()));
}

void ManifestUpdateTask::LoadAndCheckIconContents() {
  DCHECK(stage_ == Stage::kPendingInstallableData);
  stage_ = Stage::kPendingIconDownload;

  DCHECK(web_application_info_.has_value());
  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_application_info_);
  icon_downloader_.emplace(
      web_contents(), std::move(icon_urls),
      WebAppIconDownloader::Histogram::kForUpdate,
      base::BindOnce(&ManifestUpdateTask::OnIconsDownloaded, AsWeakPtr()));
  icon_downloader_->SkipPageFavicons();
  icon_downloader_->FailAllIfAnyFail();
  icon_downloader_->Start();
}

void ManifestUpdateTask::OnIconsDownloaded(bool success, IconsMap icons_map) {
  DCHECK(stage_ == Stage::kPendingIconDownload);

  if (!success) {
    DestroySelf(ManifestUpdateResult::kIconDownloadFailed);
    return;
  }

  stage_ = Stage::kPendingIconReadFromDisk;
  Observe(nullptr);
  icon_manager_.ReadAllIcons(
      app_id_, base::BindOnce(&ManifestUpdateTask::OnAllIconsRead, AsWeakPtr(),
                              std::move(icons_map)));
}

void ManifestUpdateTask::OnAllIconsRead(IconsMap downloaded_icons_map,
                                        IconBitmaps disk_icon_bitmaps) {
  DCHECK(stage_ == Stage::kPendingIconReadFromDisk);

  if (disk_icon_bitmaps.empty()) {
    DestroySelf(ManifestUpdateResult::kIconReadFromDiskFailed);
    return;
  }

  DCHECK(web_application_info_.has_value());
  FilterAndResizeIconsGenerateMissing(&web_application_info_.value(),
                                      &downloaded_icons_map);

  // TODO: compare in a BEST_EFFORT blocking PostTaskAndReply.
  if (IsUpdateNeededForIconContents(disk_icon_bitmaps)) {
    UpdateAfterWindowsClose();
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    icon_manager_.ReadAllShortcutsMenuIcons(
        app_id_,
        base::BindOnce(&ManifestUpdateTask::OnAllShortcutsMenuIconsRead,
                       AsWeakPtr()));
  } else {
    DestroySelf(ManifestUpdateResult::kAppUpToDate);
  }
}

bool ManifestUpdateTask::IsUpdateNeededForIconContents(
    const IconBitmaps& disk_icon_bitmaps) const {
  DCHECK(web_application_info_.has_value());
  const std::map<SquareSizePx, SkBitmap>& downloaded_icon_bitmaps_any =
      web_application_info_->icon_bitmaps_any;
  if (HaveIconContentsChanged(disk_icon_bitmaps.any,
                              downloaded_icon_bitmaps_any)) {
    return true;
  }
  const std::map<SquareSizePx, SkBitmap>& downloaded_icon_bitmaps_maskable =
      web_application_info_->icon_bitmaps_maskable;
  if (HaveIconContentsChanged(disk_icon_bitmaps.maskable,
                              downloaded_icon_bitmaps_maskable)) {
    return true;
  }

  return false;
}

void ManifestUpdateTask::OnAllShortcutsMenuIconsRead(
    ShortcutsMenuIconsBitmaps disk_shortcuts_menu_icons_bitmaps) {
  DCHECK(stage_ == Stage::kPendingIconReadFromDisk);

  DCHECK(web_application_info_.has_value());

  if (IsUpdateNeededForShortcutsMenuIconsContents(
          disk_shortcuts_menu_icons_bitmaps)) {
    UpdateAfterWindowsClose();
    return;
  }

  DestroySelf(ManifestUpdateResult::kAppUpToDate);
}

bool ManifestUpdateTask::IsUpdateNeededForShortcutsMenuIconsContents(
    const ShortcutsMenuIconsBitmaps& disk_shortcuts_menu_icons_bitmaps) const {
  DCHECK(web_application_info_.has_value());
  const ShortcutsMenuIconsBitmaps& downloaded_shortcuts_menu_icons_bitmaps =
      web_application_info_->shortcuts_menu_icons_bitmaps;
  if (downloaded_shortcuts_menu_icons_bitmaps.size() !=
      disk_shortcuts_menu_icons_bitmaps.size()) {
    return true;
  }

  for (size_t i = 0; i < downloaded_shortcuts_menu_icons_bitmaps.size(); ++i) {
    const std::map<SquareSizePx, SkBitmap>& downloaded_icon_bitmaps =
        downloaded_shortcuts_menu_icons_bitmaps[i];
    const std::map<SquareSizePx, SkBitmap>& disk_icon_bitmaps =
        disk_shortcuts_menu_icons_bitmaps[i];
    if (HaveIconContentsChanged(disk_icon_bitmaps, downloaded_icon_bitmaps))
      return true;
  }

  return false;
}

void ManifestUpdateTask::OnAllAppWindowsClosed() {
  DCHECK_EQ(stage_, Stage::kPendingWindowsClosed);

  DCHECK(web_application_info_.has_value());

  // The app's name must not change due to an automatic update.
  // TODO(crbug.com/1088338): Provide a safe way for apps to update their name.
  web_application_info_->title =
      base::UTF8ToUTF16(registrar_.GetAppShortName(app_id_));

  // Preserve the user's choice of opening in browser tab or standalone window.
  switch (registrar_.GetAppUserDisplayMode(app_id_)) {
    case DisplayMode::kBrowser:
      web_application_info_->open_as_window = false;
      break;
    case DisplayMode::kStandalone:
      web_application_info_->open_as_window = true;
      break;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      break;
  }

  stage_ = Stage::kPendingInstallation;
  install_manager_.UpdateWebAppFromInfo(
      app_id_, std::make_unique<WebApplicationInfo>(*web_application_info_),
      base::BindOnce(&ManifestUpdateTask::OnInstallationComplete, AsWeakPtr()));
}

void ManifestUpdateTask::OnInstallationComplete(
    const AppId& app_id,
    InstallResultCode code) {
  DCHECK_EQ(stage_, Stage::kPendingInstallation);

  if (!IsSuccess(code)) {
    DestroySelf(ManifestUpdateResult::kAppUpdateFailed);
    return;
  }

  DCHECK_EQ(app_id_, app_id);
  DCHECK(!IsUpdateNeededForManifest());
  DCHECK_EQ(code, InstallResultCode::kSuccessAlreadyInstalled);

  DestroySelf(ManifestUpdateResult::kAppUpdated);
}

void ManifestUpdateTask::DestroySelf(ManifestUpdateResult result) {
  // Asserts that calling the callback results in |this| getting deleted.
#if DCHECK_IS_ON()
  bool destructor_called = false;
  destructor_called_ptr_ = &destructor_called;
#endif
  std::move(stopped_callback_).Run(*this, result);
#if DCHECK_IS_ON()
  DCHECK(destructor_called);
#endif
}

}  // namespace web_app
