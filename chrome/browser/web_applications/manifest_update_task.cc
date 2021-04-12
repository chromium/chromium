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
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "third_party/blink/public/common/features.h"
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

bool HaveIconBitmapsChanged(const IconBitmaps& disk_icon_bitmaps,
                            const IconBitmaps& downloaded_icon_bitmaps) {
  // TODO (crbug.com/1114638): Check Monochrome icons if supported.
  return HaveIconContentsChanged(disk_icon_bitmaps.any,
                                 downloaded_icon_bitmaps.any) ||
         HaveIconContentsChanged(disk_icon_bitmaps.maskable,
                                 downloaded_icon_bitmaps.maskable);
}

}  // namespace

bool HaveFileHandlersChanged(
    const apps::FileHandlers* old_handlers,
    const std::vector<blink::Manifest::FileHandler>& new_handlers) {
  if (!old_handlers)
    return true;

  if (old_handlers->size() != new_handlers.size())
    return true;

  for (size_t i = 0; i < old_handlers->size(); ++i) {
    // Compare apps::FileHandler and blink::Manifest::FileHandler.
    const apps::FileHandler& old_handler = (*old_handlers)[i];
    const blink::Manifest::FileHandler& new_handler = new_handlers[i];

    if (old_handler.action != new_handler.action)
      return true;

    // Note: While blink::Manifest::FileHandler contains a `name` field, the
    // corresponding apps::FileHandler doesn't store this field.  As a result,
    // we don't compare the incoming blink::Manifest::FileHandler `name` field
    // anywhere and so it has no effect on the comparison result.

    // Check `accept` maps for equality.
    if (old_handler.accept.size() != new_handler.accept.size())
      return true;

    for (const auto& old_accept_entry : old_handler.accept) {
      auto new_accept_it = new_handler.accept.find(
          base::UTF8ToUTF16(old_accept_entry.mime_type));
      if (new_accept_it == new_handler.accept.end())
        return true;

      // Check `file_extensions` for equality.
      const base::flat_set<std::string>& old_extensions_set =
          old_accept_entry.file_extensions;
      const std::vector<std::u16string>& new_extensions_list =
          new_accept_it->second;

      if (old_extensions_set.size() != new_extensions_list.size())
        return true;
      for (const std::u16string& new_extension : new_extensions_list) {
        if (!base::Contains(old_extensions_set,
                            base::UTF16ToUTF8(new_extension))) {
          return true;
        }
      }
    }
  }
  return false;
}

ManifestUpdateTask::ManifestUpdateTask(
    const GURL& url,
    const AppId& app_id,
    content::WebContents* web_contents,
    StoppedCallback stopped_callback,
    bool hang_for_testing,
    const AppRegistrar& registrar,
    const AppIconManager& icon_manager,
    WebAppUiManager* ui_manager,
    InstallManager* install_manager,
    OsIntegrationManager& os_integration_manager)
    : content::WebContentsObserver(web_contents),
      registrar_(registrar),
      icon_manager_(icon_manager),
      ui_manager_(*ui_manager),
      install_manager_(*install_manager),
      os_integration_manager_(os_integration_manager),
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
  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = false;
  webapps::InstallableManager::FromWebContents(web_contents())
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
    case Stage::kPendingMaybeReadExistingIcons:
    case Stage::kPendingInstallation:
    case Stage::kPendingAssociationsUpdate:
      // These stages should have stopped listening to the web contents.
      NOTREACHED();
      Observe(nullptr);
      break;
  }
}

void ManifestUpdateTask::OnDidGetInstallableData(
    const webapps::InstallableData& data) {
  DCHECK_EQ(stage_, Stage::kPendingInstallableData);

  if (!data.NoBlockingErrors()) {
    DestroySelf(ManifestUpdateResult::kAppNotEligible);
    return;
  }

  web_application_info_.emplace();
  UpdateWebAppInfoFromManifest(data.manifest, data.manifest_url,
                               &web_application_info_.value());

  // We cannot allow the app ID to change via the manifest changing. We rely on
  // fixed app IDs to determine whether web apps installed in the user sync
  // profile has been sync installed across devices. If we allowed the app ID to
  // change then the sync system would try to redeploy the old app indefinitely,
  // additionally the new app ID would get added to the sync profile. This has
  // the potential to flood the user sync profile with an infinite number of
  // apps should the site be serving a random start_url on every navigation.
  if (app_id_ != GenerateAppIdFromURL(web_application_info_->start_url)) {
    DestroySelf(ManifestUpdateResult::kAppIdMismatch);
    return;
  }

  if (IsUpdateNeededForManifest()) {
    UpdateAfterWindowsClose();
    return;
  }

  LoadAndCheckIconContents();
}

bool ManifestUpdateTask::IsUpdateNeededForManifest() const {
  DCHECK(web_application_info_.has_value());

  if (web_application_info_->theme_color !=
      registrar_.GetAppThemeColor(app_id_))
    return true;

  if (web_application_info_->scope != registrar_.GetAppScopeInternal(app_id_))
    return true;

  if (web_application_info_->display_mode !=
      registrar_.GetAppDisplayMode(app_id_)) {
    return true;
  }

  if (web_application_info_->display_override !=
      registrar_.GetAppDisplayModeOverride(app_id_)) {
    return true;
  }

  // Allow app icon updating if the existing icons are empty - this means the
  // app icon download during install failed.
  if (base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating) &&
      web_application_info_->icon_infos !=
          registrar_.GetAppIconInfos(app_id_)) {
    return true;
  }

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

  if (web_application_info_->capture_links !=
      registrar_.GetAppCaptureLinks(app_id_)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers) &&
      web_application_info_->url_handlers !=
          registrar_.GetAppUrlHandlers(app_id_)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI) &&
      HaveFileHandlersChanged(
          /*old_handlers=*/registrar_.GetAppFileHandlers(app_id_),
          /*new_handlers=*/web_application_info_->file_handlers)) {
    return true;
  }

  // TODO(crbug.com/1072058): Check the manifest URL.
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
  DCHECK_EQ(stage_, Stage::kPendingInstallableData);
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
  DCHECK_EQ(stage_, Stage::kPendingIconDownload);

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
  DCHECK_EQ(stage_, Stage::kPendingIconReadFromDisk);

  if (disk_icon_bitmaps.empty()) {
    DestroySelf(ManifestUpdateResult::kIconReadFromDiskFailed);
    return;
  }
  DCHECK(web_application_info_.has_value());

  // Allow app icon updating if the existing icons are empty - this means the
  // app icon download during install failed.
  if (base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating)) {
    // This call populates the |web_application_info_| with all icon bitmap
    // data.
    // If this data does not match what we already have on disk, then an update
    // is necessary.
    // TODO(https://crbug.com/1184911): Reuse this data in the web app install
    // task.
    FilterAndResizeIconsGenerateMissing(&web_application_info_.value(),
                                        &downloaded_icons_map);
    // TODO: compare in a BEST_EFFORT blocking PostTaskAndReply.
    if (IsUpdateNeededForIconContents(disk_icon_bitmaps)) {
      UpdateAfterWindowsClose();
      return;
    }
  } else if (base::FeatureList::IsEnabled(
                 features::kDesktopPWAsAppIconShortcutsMenu)) {
    // FilterAndResizeIconsGenerateMissing calls PopulateShortcutItemIcons. We
    // need that call to happen still if redownloading app icons is disabled, so
    // manually call that here.
    // This call allows us to compare the shortcut icons on disk with the ones
    // that would be generated after an update.
    // TODO(https://crbug.com/1184911): Reuse this data in the web app install
    // task.
    PopulateShortcutItemIcons(&web_application_info_.value(),
                              &downloaded_icons_map);
  }

  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    icon_manager_.ReadAllShortcutsMenuIcons(
        app_id_,
        base::BindOnce(&ManifestUpdateTask::OnAllShortcutsMenuIconsRead,
                       AsWeakPtr()));
  } else {
    NoManifestUpdateRequired();
  }
}

bool ManifestUpdateTask::IsUpdateNeededForIconContents(
    const IconBitmaps& disk_icon_bitmaps) const {
  DCHECK(web_application_info_.has_value());
  return HaveIconBitmapsChanged(disk_icon_bitmaps,
                                web_application_info_->icon_bitmaps);
}

void ManifestUpdateTask::OnAllShortcutsMenuIconsRead(
    ShortcutsMenuIconBitmaps disk_shortcuts_menu_icon_bitmaps) {
  DCHECK_EQ(stage_, Stage::kPendingIconReadFromDisk);

  DCHECK(web_application_info_.has_value());

  if (IsUpdateNeededForShortcutsMenuIconsContents(
          disk_shortcuts_menu_icon_bitmaps)) {
    UpdateAfterWindowsClose();
    return;
  }

  NoManifestUpdateRequired();
}

bool ManifestUpdateTask::IsUpdateNeededForShortcutsMenuIconsContents(
    const ShortcutsMenuIconBitmaps& disk_shortcuts_menu_icon_bitmaps) const {
  DCHECK(web_application_info_.has_value());
  const ShortcutsMenuIconBitmaps& downloaded_shortcuts_menu_icon_bitmaps =
      web_application_info_->shortcuts_menu_icon_bitmaps;
  if (downloaded_shortcuts_menu_icon_bitmaps.size() !=
      disk_shortcuts_menu_icon_bitmaps.size()) {
    return true;
  }

  for (size_t i = 0; i < downloaded_shortcuts_menu_icon_bitmaps.size(); ++i) {
    const IconBitmaps& downloaded_icon_bitmaps =
        downloaded_shortcuts_menu_icon_bitmaps[i];
    const IconBitmaps& disk_icon_bitmaps = disk_shortcuts_menu_icon_bitmaps[i];
    if (HaveIconBitmapsChanged(disk_icon_bitmaps, downloaded_icon_bitmaps))
      return true;
  }

  return false;
}

bool ManifestUpdateTask::IsUpdateNeededForWebAppOriginAssociations() const {
  // Web app origin association update is tied to the manifest update process.
  // If there are url handlers for the current app, associations need to be
  // revalidated.
  DCHECK(web_application_info_.has_value());
  if (base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers) &&
      !web_application_info_->url_handlers.empty()) {
    return true;
  }

  return false;
}

void ManifestUpdateTask::NoManifestUpdateRequired() {
  DCHECK_EQ(stage_, Stage::kPendingIconReadFromDisk);
  stage_ = Stage::kPendingAssociationsUpdate;
  if (!IsUpdateNeededForWebAppOriginAssociations()) {
    DestroySelf(ManifestUpdateResult::kAppUpToDate);
    return;
  }

  os_integration_manager_.UpdateUrlHandlers(
      app_id_,
      base::BindOnce(&ManifestUpdateTask::OnWebAppOriginAssociationsUpdated,
                     AsWeakPtr()));
}

void ManifestUpdateTask::OnWebAppOriginAssociationsUpdated(bool success) {
  DCHECK_EQ(stage_, Stage::kPendingAssociationsUpdate);
  success ? DestroySelf(ManifestUpdateResult::kAppAssociationsUpdated)
          : DestroySelf(ManifestUpdateResult::kAppAssociationsUpdateFailed);
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
    case DisplayMode::kWindowControlsOverlay:
      NOTREACHED();
      break;
  }

  stage_ = Stage::kPendingMaybeReadExistingIcons;
  // Allow app icon updating if the existing icons are empty - this means the
  // app icon download during install failed.
  if (base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating)) {
    OnExistingIconsRead(IconBitmaps());
    return;
  }
  icon_manager_.ReadAllIcons(
      app_id_,
      base::BindOnce(&ManifestUpdateTask::OnExistingIconsRead, AsWeakPtr()));
}

void ManifestUpdateTask::OnExistingIconsRead(IconBitmaps icon_bitmaps) {
  DCHECK_EQ(stage_, Stage::kPendingMaybeReadExistingIcons);

  bool redownload_app_icons = icon_bitmaps.empty();
  if (!redownload_app_icons)
    web_application_info_->icon_bitmaps = std::move(icon_bitmaps);

  stage_ = Stage::kPendingInstallation;
  install_manager_.UpdateWebAppFromInfo(
      app_id_, std::make_unique<WebApplicationInfo>(*web_application_info_),
      /*redownload_app_icons=*/redownload_app_icons,
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
