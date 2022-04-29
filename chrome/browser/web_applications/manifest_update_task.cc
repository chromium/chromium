// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_task.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

namespace {

// This is used for metrics, so do not remove or reorder existing entries.
enum class AppIdentityDisplayMetric {
  kNoAppIdentityChange = 0,
  kIconChanging = 1,
  // Values 2 and 3 are reserved for Android (icon mask).
  kAppNameChanging = 4,
  kAppNameAndIconChanging = 5,
  // Values 6 through 15 (inclusive) are reserved for Android (icon mask/app
  // short name).
  kLastAndroidSpecificValue = 15,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = 15
};

// Returns a shared instance of UpdatePendingCallback.
ManifestUpdateTask::UpdatePendingCallback* GetUpdatePendingCallbackMutable() {
  static base::NoDestructor<ManifestUpdateTask::UpdatePendingCallback>
      g_update_pending_callback;
  return g_update_pending_callback.get();
}

void HaveIconContentsChanged(
    const std::map<SquareSizePx, SkBitmap>& disk_icon_bitmaps,
    const std::map<SquareSizePx, SkBitmap>& downloaded_icon_bitmaps,
    IconDiff* icon_diff,
    const std::vector<SquareSizePx>& on_disk_sizes,
    const std::vector<SquareSizePx>& downloaded_sizes,
    bool end_when_mismatch_detected) {
  if (downloaded_icon_bitmaps.size() != disk_icon_bitmaps.size()) {
    icon_diff->diff_results |= MISMATCHED_IMAGE_SIZES;
    if (end_when_mismatch_detected)
      return;
  }

  if (on_disk_sizes != downloaded_sizes) {
    icon_diff->diff_results |= MISMATCHED_IMAGE_SIZES;
    if (end_when_mismatch_detected)
      return;
  }

  for (const std::pair<const SquareSizePx, SkBitmap>& entry :
       downloaded_icon_bitmaps) {
    SquareSizePx size = entry.first;
    const SkBitmap& downloaded_bitmap = entry.second;

    auto it = disk_icon_bitmaps.find(size);
    if (it == disk_icon_bitmaps.end()) {
      icon_diff->diff_results |= MISMATCHED_IMAGE_SIZES;
      if (end_when_mismatch_detected)
        return;
      continue;
    }

    const SkBitmap& disk_bitmap = it->second;
    if (!gfx::BitmapsAreEqual(downloaded_bitmap, disk_bitmap)) {
      if (end_when_mismatch_detected) {
        icon_diff->diff_results |= ONE_OR_MORE_ICONS_CHANGED;
        return;
      }

      if (size == kInstallIconSize) {
        icon_diff->diff_results |= INSTALL_ICON_CHANGED;
        icon_diff->before = disk_bitmap;
        icon_diff->after = downloaded_bitmap;
      } else if (size == kLauncherIconSize) {
        icon_diff->diff_results |= LAUNCHER_ICON_CHANGED;
        if (icon_diff->before.drawsNothing() &&
            icon_diff->after.drawsNothing()) {
          icon_diff->before = disk_bitmap;
          icon_diff->after = downloaded_bitmap;
        }
      } else {
        icon_diff->diff_results |= UNIMPORTANT_ICON_CHANGED;
      }
    }
  }
}

// Some apps, such as pre-installed apps, have been vetted and are therefore
// considered safe and permitted to update their names.
bool AllowUnpromptedNameUpdate(const AppId& app_id,
                               const WebAppRegistrar& registrar) {
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app)
    return false;
  return CanWebAppUpdateIdentity(web_app);
}

// Some apps, such as pre-installed apps, have been vetted and are therefore
// considered safe and permitted to update their icon. For others, the feature
// flag needs to be on.
bool AllowUnpromptedIconUpdate(const AppId& app_id,
                               const WebAppRegistrar& registrar) {
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app)
    return false;
  return CanWebAppUpdateIdentity(web_app) ||
         base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating);
}

bool NeedsAppIdentityUpdateDialog(bool title_changing,
                                  bool icons_changing,
                                  const AppId& app_id,
                                  const WebAppRegistrar& registrar) {
  if (title_changing && !AllowUnpromptedNameUpdate(app_id, registrar))
    return true;
  if (icons_changing && !AllowUnpromptedIconUpdate(app_id, registrar))
    return true;
  return false;
}

}  // namespace

IconDiff HaveIconBitmapsChanged(
    const IconBitmaps& disk_icon_bitmaps,
    const IconBitmaps& downloaded_icon_bitmaps,
    const std::vector<apps::IconInfo>& disk_icon_info,
    const std::vector<apps::IconInfo>& downloaded_icon_info,
    bool end_when_mismatch_detected) {
  // The manifest information associated with the icons is a flat vector of
  // IconInfo types. This needs to be split into vectors and keyed by purpose
  // (any, masked, monochrome) so that it can be read by the icon diff.
  std::map<apps::IconInfo::Purpose, std::vector<SquareSizePx>> on_disk_sizes;
  std::map<apps::IconInfo::Purpose, std::vector<SquareSizePx>> downloaded_sizes;
  on_disk_sizes[apps::IconInfo::Purpose::kAny] = std::vector<SquareSizePx>();
  downloaded_sizes[apps::IconInfo::Purpose::kAny] = std::vector<SquareSizePx>();
  on_disk_sizes[apps::IconInfo::Purpose::kMaskable] =
      std::vector<SquareSizePx>();
  downloaded_sizes[apps::IconInfo::Purpose::kMaskable] =
      std::vector<SquareSizePx>();
  on_disk_sizes[apps::IconInfo::Purpose::kMonochrome] =
      std::vector<SquareSizePx>();
  downloaded_sizes[apps::IconInfo::Purpose::kMonochrome] =
      std::vector<SquareSizePx>();
  // Put each entry found into the right map (sort by purpose).
  for (const auto& entry : disk_icon_info) {
    on_disk_sizes[entry.purpose].push_back(entry.square_size_px.value_or(-1));
  }
  for (const auto& entry : downloaded_icon_info) {
    downloaded_sizes[entry.purpose].push_back(
        entry.square_size_px.value_or(-1));
  }

  IconDiff icon_diff;
  HaveIconContentsChanged(disk_icon_bitmaps.any, downloaded_icon_bitmaps.any,
                          &icon_diff,
                          on_disk_sizes[apps::IconInfo::Purpose::kAny],
                          downloaded_sizes[apps::IconInfo::Purpose::kAny],
                          end_when_mismatch_detected);
  if (icon_diff.mismatch() && end_when_mismatch_detected)
    return icon_diff;

  HaveIconContentsChanged(disk_icon_bitmaps.maskable,
                          downloaded_icon_bitmaps.maskable, &icon_diff,
                          on_disk_sizes[apps::IconInfo::Purpose::kMaskable],
                          downloaded_sizes[apps::IconInfo::Purpose::kMaskable],
                          end_when_mismatch_detected);
  if (icon_diff.mismatch() && end_when_mismatch_detected)
    return icon_diff;

  HaveIconContentsChanged(
      disk_icon_bitmaps.monochrome, downloaded_icon_bitmaps.monochrome,
      &icon_diff, on_disk_sizes[apps::IconInfo::Purpose::kMonochrome],
      downloaded_sizes[apps::IconInfo::Purpose::kMonochrome],
      end_when_mismatch_detected);
  return icon_diff;
}

// static
void ManifestUpdateTask::SetUpdatePendingCallbackForTesting(
    UpdatePendingCallback callback) {
  *GetUpdatePendingCallbackMutable() = std::move(callback);
}

// static
bool& ManifestUpdateTask::BypassWindowCloseWaitingForTesting() {
  static bool bypass_window_close_waiting_for_testing_ = false;
  return bypass_window_close_waiting_for_testing_;
}

ManifestUpdateTask::ManifestUpdateTask(
    const GURL& url,
    const AppId& app_id,
    content::WebContents* web_contents,
    StoppedCallback stopped_callback,
    bool hang_for_testing,
    const WebAppRegistrar& registrar,
    const WebAppIconManager& icon_manager,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* install_finalizer,
    OsIntegrationManager& os_integration_manager,
    WebAppSyncBridge* sync_bridge)
    : content::WebContentsObserver(web_contents),
      registrar_(registrar),
      icon_manager_(icon_manager),
      ui_manager_(*ui_manager),
      install_finalizer_(*install_finalizer),
      os_integration_manager_(os_integration_manager),
      sync_bridge_(sync_bridge),
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

  if (render_frame_host->GetParentOrOuterDocument())
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
    case Stage::kPendingIconReadFromDisk:
    case Stage::kPendingAppIdentityCheck:
      DestroySelf(ManifestUpdateResult::kWebContentsDestroyed);
      return;
    case Stage::kPendingWindowsClosed:
    case Stage::kPendingMaybeReadExistingIcons:
    case Stage::kPendingInstallation:
    case Stage::kPendingAssociationsUpdate:
      // These stages should have stopped listening to the web contents.
      NOTREACHED() << static_cast<int>(stage_);
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

  install_info_.emplace();
  UpdateWebAppInfoFromManifest(data.manifest, data.manifest_url,
                               &install_info_.value());

  // We cannot allow the app ID to change via the manifest changing. We rely on
  // fixed app IDs to determine whether web apps installed in the user sync
  // profile has been sync installed across devices. If we allowed the app ID to
  // change then the sync system would try to redeploy the old app indefinitely,
  // additionally the new app ID would get added to the sync profile. This has
  // the potential to flood the user sync profile with an infinite number of
  // apps should the site be serving a random start_url on every navigation.
  if (app_id_ !=
      GenerateAppId(install_info_->manifest_id, install_info_->start_url)) {
    DestroySelf(ManifestUpdateResult::kAppIdMismatch);
    return;
  }

  LoadAndCheckIconContents();
}

bool ManifestUpdateTask::IsUpdateNeededForManifest() const {
  DCHECK(install_info_.has_value());
  const WebApp* app = registrar_.GetAppById(app_id_);
  DCHECK(app);

  // TODO(crbug.com/1259777): Check whether translations have been updated.
  bool title_changing =
      install_info_->title != base::UTF8ToUTF16(app->untranslated_name());
  bool icons_changing = install_info_->manifest_icons != app->manifest_icons();
  if (!NeedsAppIdentityUpdateDialog(title_changing, icons_changing, app_id_,
                                    registrar_)) {
    if (title_changing && AllowUnpromptedNameUpdate(app_id_, registrar_)) {
      return true;
    }
    if (icons_changing && AllowUnpromptedIconUpdate(app_id_, registrar_)) {
      return true;
    }
  }

  // Allows updating start_url and manifest_id when kWebAppEnableManifestId is
  // enabled. Both fields are allowed to change as long as the app_id generated
  // from them doesn't change.
  if (base::FeatureList::IsEnabled(blink::features::kWebAppEnableManifestId)) {
    if (install_info_->manifest_id != app->manifest_id())
      return true;
    if (install_info_->start_url != app->start_url())
      return true;
  }

  if (install_info_->theme_color != app->theme_color())
    return true;

  if (install_info_->scope != app->scope())
    return true;

  if (install_info_->display_mode != app->display_mode())
    return true;

  if (install_info_->display_override != app->display_mode_override())
    return true;

  if (install_info_->shortcuts_menu_item_infos !=
      app->shortcuts_menu_item_infos()) {
    return true;
  }

  if (install_info_->share_target != app->share_target())
    return true;

  if (install_info_->protocol_handlers != app->protocol_handlers())
    return true;

  if (install_info_->url_handlers != app->url_handlers())
    return true;

  if (install_info_->note_taking_new_note_url !=
      app->note_taking_new_note_url()) {
    return true;
  }

  if (install_info_->capture_links != app->capture_links())
    return true;

  if (install_info_->handle_links != app->handle_links())
    return true;

  if (app->file_handlers() != install_info_->file_handlers)
    return true;

  if (install_info_->background_color != app->background_color())
    return true;

  if (install_info_->dark_mode_theme_color != app->dark_mode_theme_color()) {
    return true;
  }

  if (install_info_->dark_mode_background_color !=
      app->dark_mode_background_color()) {
    return true;
  }

  if (install_info_->manifest_url != app->manifest_url())
    return true;

  if (install_info_->launch_handler != app->launch_handler())
    return true;

  if (install_info_->permissions_policy != app->permissions_policy())
    return true;

  // TODO(crbug.com/1212849): Handle changes to is_storage_isolated.

  // TODO(crbug.com/926083): Check more manifest fields.
  return false;
}

void ManifestUpdateTask::UpdateAfterWindowsClose() {
  DCHECK(stage_ == Stage::kPendingInstallableData ||
         stage_ == Stage::kPendingAppIdentityCheck);
  stage_ = Stage::kPendingWindowsClosed;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_MANIFEST_UPDATE, KeepAliveRestartOption::DISABLED);
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kWebAppUpdate);

  Observe(nullptr);

  if (BypassWindowCloseWaitingForTesting()) {
    OnAllAppWindowsClosed();
  } else {
    ui_manager_.NotifyOnAllAppWindowsClosed(
        app_id_, base::BindOnce(&ManifestUpdateTask::OnAllAppWindowsClosed,
                                AsWeakPtr()));
    UpdatePendingCallback* callback = GetUpdatePendingCallbackMutable();
    if (!callback->is_null())
      std::move(*callback).Run(url_);
  }
}

void ManifestUpdateTask::LoadAndCheckIconContents() {
  DCHECK_EQ(stage_, Stage::kPendingInstallableData);
  stage_ = Stage::kPendingIconDownload;

  DCHECK(install_info_.has_value());
  std::vector<GURL> icon_urls = GetValidIconUrlsToDownload(*install_info_);
  icon_downloader_.emplace(
      web_contents(), std::move(icon_urls),
      base::BindOnce(&ManifestUpdateTask::OnIconsDownloaded, AsWeakPtr()));
  icon_downloader_->SkipPageFavicons();
  icon_downloader_->FailAllIfAnyFail();
  icon_downloader_->Start();
}

void ManifestUpdateTask::OnIconsDownloaded(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK_EQ(stage_, Stage::kPendingIconDownload);

  // TODO(crbug.com/1238622): Report `result` and `icons_http_results` in
  // internals.
  UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnUpdate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnUpdate", icons_http_results);

  if (result != IconsDownloadedResult::kCompleted) {
    DestroySelf(ManifestUpdateResult::kIconDownloadFailed);
    return;
  }

  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnUpdate", result, icons_http_results);

  stage_ = Stage::kPendingIconReadFromDisk;
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
  DCHECK(install_info_.has_value());

  stage_ = Stage::kPendingAppIdentityCheck;

  // These calls populate the |install_info_| with all icon bitmap
  // data. If this data does not match what we already have on disk, then an
  // update is necessary.
  PopulateOtherIcons(&install_info_.value(), downloaded_icons_map);
  PopulateProductIcons(&install_info_.value(), &downloaded_icons_map);

  IconDiff icon_diff = IsUpdateNeededForIconContents(disk_icon_bitmaps);
  std::u16string old_title =
      base::UTF8ToUTF16(registrar_.GetAppShortName(app_id_));
  std::u16string new_title = install_info_->title;

  bool title_change = old_title != new_title;
  bool icon_change = icon_diff.mismatch();

  AppIdentityDisplayMetric app_id_changes =
      AppIdentityDisplayMetric::kNoAppIdentityChange;
  if (title_change && icon_change) {
    app_id_changes = AppIdentityDisplayMetric::kAppNameAndIconChanging;
  } else if (title_change || icon_change) {
    app_id_changes = title_change ? AppIdentityDisplayMetric::kAppNameChanging
                                  : AppIdentityDisplayMetric::kIconChanging;
  }

  // This catches the cases where the App Identity Dialog is not needed. That
  // includes:
  // - All Default-installed apps (since they are pre-approved for all updates).
  // - Policy-installed apps w/kWebAppManifestPolicyAppIdentityUpdate exemption.
  // - All icon changes when the kWebAppManifestIconUpdating override is set.
  // - ... and apps that simply aren't requesting any app identity changes.
  if (!NeedsAppIdentityUpdateDialog(title_change, icon_change, app_id_,
                                    registrar_)) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.AlreadyApproved",
                              app_id_changes);
    OnPostAppIdentityUpdateCheck(AppIdentityUpdate::kSkipped);
    return;
  }

  SkBitmap* before_icon = nullptr;
  SkBitmap* after_icon = nullptr;
  if (icon_change &&
      base::FeatureList::IsEnabled(features::kPwaUpdateDialogForIcon)) {
    before_icon = &icon_diff.before;
    after_icon = &icon_diff.after;
  } else {
    auto it = disk_icon_bitmaps.any.find(kInstallIconSize);
    if (it == disk_icon_bitmaps.any.end())
      it = disk_icon_bitmaps.any.find(kLauncherIconSize);
    if (it == disk_icon_bitmaps.any.end())
      it = disk_icon_bitmaps.any.begin();
    if (it != disk_icon_bitmaps.any.end()) {
      before_icon = &it->second;
      after_icon = &it->second;
    }
  }

  // If there are any cases of Default-installed or Policy-installed apps that
  // haven't been granted exceptions above (such as Policy apps without the
  // special exemption), they should bail out now (with the icon set reset) so
  // as to avoid showing the app identity dialog and allow other non-app
  // identity changes to occur.
  const WebApp* web_app = registrar_.GetAppById(app_id_);
  if (web_app->IsPreinstalledApp() || web_app->IsPolicyInstalledApp()) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.NotShowing",
                              app_id_changes);
    install_info_->icon_bitmaps = std::move(disk_icon_bitmaps);
    install_info_->manifest_icons = web_app->manifest_icons();
    install_info_->is_generated_icon = web_app->is_generated_icon();
    OnPostAppIdentityUpdateCheck(AppIdentityUpdate::kSkipped);
    return;
  }

  // At this point we are only dealing with user-installed apps. Apps that don't
  // ask for any identity updates are dealt with above, so this needs to handle
  // updates to either the app title or icons.
  if (icon_change &&
      !base::FeatureList::IsEnabled(features::kPwaUpdateDialogForIcon)) {
    // Icon changes are not supported, revert them and continue.
    install_info_->icon_bitmaps = std::move(disk_icon_bitmaps);
    install_info_->manifest_icons = web_app->manifest_icons();
    install_info_->is_generated_icon = web_app->is_generated_icon();
    icon_change = false;
  }

  if (title_change &&
      !base::FeatureList::IsEnabled(features::kPwaUpdateDialogForName)) {
    // Title changes are not supported, revert and continue.
    install_info_->title = old_title;
    new_title = old_title;
    title_change = false;
  }

  // A title change requires showing the dialog, but unimportant icon changes
  // are allowed to proceed.
  if (!title_change && icon_change &&
      !icon_diff.requires_app_identity_check()) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.AlreadyApproved",
                              app_id_changes);
    OnPostAppIdentityUpdateCheck(AppIdentityUpdate::kAllowed);
    return;
  }

  if (!title_change && !icon_change) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.NotShowing",
                              app_id_changes);
    OnPostAppIdentityUpdateCheck(AppIdentityUpdate::kSkipped);
    return;
  }

  if (before_icon == nullptr || after_icon == nullptr ||
      before_icon->drawsNothing() || after_icon->drawsNothing()) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.NotShowing",
                              app_id_changes);
    OnPostAppIdentityUpdateCheck(AppIdentityUpdate::kSkipped);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Webapp.AppIdentityDialog.Showing", app_id_changes);

  ui_manager_.ShowWebAppIdentityUpdateDialog(
      app_id_, title_change, icon_change, old_title, new_title, *before_icon,
      *after_icon, web_contents(),
      base::BindOnce(&ManifestUpdateTask::OnPostAppIdentityUpdateCheck,
                     AsWeakPtr()));

  // Flow continues in OnPostAppIdentityUpdateCheck, once an action has been
  // taken in the dialog.
}

void ManifestUpdateTask::OnPostAppIdentityUpdateCheck(
    AppIdentityUpdate app_identity_update_allowed) {
  DCHECK_EQ(stage_, Stage::kPendingAppIdentityCheck);

  app_identity_update_allowed_ =
      app_identity_update_allowed == AppIdentityUpdate::kAllowed;
  if (app_identity_update_allowed_) {
    UpdateAfterWindowsClose();
    return;
  }

  if (IsUpdateNeededForManifest()) {
    UpdateAfterWindowsClose();
    return;
  }

  icon_manager_.ReadAllShortcutsMenuIcons(
      app_id_, base::BindOnce(&ManifestUpdateTask::OnAllShortcutsMenuIconsRead,
                              AsWeakPtr()));
}

IconDiff ManifestUpdateTask::IsUpdateNeededForIconContents(
    const IconBitmaps& disk_icon_bitmaps) const {
  DCHECK(install_info_.has_value());
  const WebApp* app = registrar_.GetAppById(app_id_);
  DCHECK(app);

  return HaveIconBitmapsChanged(disk_icon_bitmaps, install_info_->icon_bitmaps,
                                install_info_->manifest_icons,
                                app->manifest_icons(),
                                /* end_when_mismatch_detected= */ false);
}

void ManifestUpdateTask::OnAllShortcutsMenuIconsRead(
    ShortcutsMenuIconBitmaps disk_shortcuts_menu_icon_bitmaps) {
  DCHECK_EQ(stage_, Stage::kPendingAppIdentityCheck);

  DCHECK(install_info_.has_value());

  if (IsUpdateNeededForShortcutsMenuIconsContents(
          disk_shortcuts_menu_icon_bitmaps)) {
    UpdateAfterWindowsClose();
    return;
  }

  NoManifestUpdateRequired();
}

bool ManifestUpdateTask::IsUpdateNeededForShortcutsMenuIconsContents(
    const ShortcutsMenuIconBitmaps& disk_shortcuts_menu_icon_bitmaps) const {
  DCHECK(install_info_.has_value());
  const ShortcutsMenuIconBitmaps& downloaded_shortcuts_menu_icon_bitmaps =
      install_info_->shortcuts_menu_icon_bitmaps;
  if (downloaded_shortcuts_menu_icon_bitmaps.size() !=
      disk_shortcuts_menu_icon_bitmaps.size()) {
    return true;
  }

  const WebApp* app = registrar_.GetAppById(app_id_);
  DCHECK(app);
  for (size_t i = 0; i < downloaded_shortcuts_menu_icon_bitmaps.size(); ++i) {
    const IconBitmaps& downloaded_icon_bitmaps =
        downloaded_shortcuts_menu_icon_bitmaps[i];
    const IconBitmaps& disk_icon_bitmaps = disk_shortcuts_menu_icon_bitmaps[i];
    if (HaveIconBitmapsChanged(disk_icon_bitmaps, downloaded_icon_bitmaps,
                               install_info_->manifest_icons,
                               app->manifest_icons(),
                               /* end_when_mismatch_detected= */ true)
            .mismatch())
      return true;
  }

  return false;
}

bool ManifestUpdateTask::IsUpdateNeededForWebAppOriginAssociations() const {
  // Web app origin association update is tied to the manifest update process.
  // If there are url handlers for the current app, associations need to be
  // revalidated.
  DCHECK(install_info_.has_value());
  return !install_info_->url_handlers.empty();
}

void ManifestUpdateTask::NoManifestUpdateRequired() {
  DCHECK_EQ(stage_, Stage::kPendingAppIdentityCheck);
  stage_ = Stage::kPendingAssociationsUpdate;

  Observe(nullptr);

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

  DCHECK(install_info_.has_value());

  if (!AllowUnpromptedNameUpdate(app_id_, registrar_) &&
      !app_identity_update_allowed_) {
    // The app's name must not change due to an automatic update, except for
    // default installed apps (that have been vetted).
    install_info_->title =
        base::UTF8ToUTF16(registrar_.GetAppShortName(app_id_));
  }

  // Preserve the user's choice of form factor to open the app with.
  install_info_->user_display_mode = registrar_.GetAppUserDisplayMode(app_id_);

  stage_ = Stage::kPendingInstallation;

  install_finalizer_.FinalizeUpdate(
      *install_info_,
      base::BindOnce(&ManifestUpdateTask::OnInstallationComplete, AsWeakPtr()));
}

void ManifestUpdateTask::OnInstallationComplete(const AppId& app_id,
                                                webapps::InstallResultCode code,
                                                OsHooksErrors os_hooks_errors) {
  DCHECK_EQ(stage_, Stage::kPendingInstallation);

  if (!IsSuccess(code)) {
    DestroySelf(ManifestUpdateResult::kAppUpdateFailed);
    return;
  }

  DCHECK_EQ(app_id_, app_id);
  DCHECK(!IsUpdateNeededForManifest());
  DCHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);

  sync_bridge_->SetAppManifestUpdateTime(app_id, base::Time::Now());

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
