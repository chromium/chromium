// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "manifest_update_data_fetch_command.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
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

ManifestUpdateDataFetchCommand::ManifestUpdateDataFetchCommand(
    const GURL& url,
    const AppId& app_id,
    base::WeakPtr<content::WebContents> web_contents,
    ManifestFetchCallback fetch_callback,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommandTemplate<AppLock>("ManifestUpdateDataFetchCommand"),
      lock_description_(std::make_unique<AppLockDescription>(app_id)),
      url_(url),
      app_id_(app_id),
      web_contents_(web_contents),
      fetch_callback_(std::move(fetch_callback)),
      data_retriever_(std::move(data_retriever)) {}

ManifestUpdateDataFetchCommand::~ManifestUpdateDataFetchCommand() = default;

const LockDescription& ManifestUpdateDataFetchCommand::lock_description()
    const {
  return *lock_description_;
}

void ManifestUpdateDataFetchCommand::OnShutdown() {
  CompleteCommand(ManifestUpdateResult::kAppUpdateFailed);
}

base::Value ManifestUpdateDataFetchCommand::ToDebugValue() const {
  base::Value::Dict data = debug_log_.Clone();
  data.Set("url", url_.spec());
  data.Set("app_id", app_id_);
  data.Set("stage", base::StreamableToString(stage_));
  return base::Value(std::move(data));
}

void ManifestUpdateDataFetchCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  stage_ = ManifestUpdateStage::kPendingInstallableData;
  webapps::InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = false;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), /*bypass_service_worker_check=*/false,
      base::BindOnce(&ManifestUpdateDataFetchCommand::OnDidGetInstallableData,
                     AsWeakPtr()),
      params);
}

bool ManifestUpdateDataFetchCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void ManifestUpdateDataFetchCommand::OnDidGetInstallableData(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingInstallableData);

  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    CompleteCommand(ManifestUpdateResult::kAppNotEligible);
    return;
  }

  install_info_.emplace();
  UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
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
    CompleteCommand(ManifestUpdateResult::kAppIdMismatch);
    return;
  }

  LoadAndCheckIconContents();
}

void ManifestUpdateDataFetchCommand::LoadAndCheckIconContents() {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingInstallableData);
  stage_ = ManifestUpdateStage::kPendingIconDownload;

  DCHECK(install_info_.has_value());
  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*install_info_);

  icon_downloader_.emplace(
      web_contents_.get(), std::move(icon_urls),
      base::BindOnce(&ManifestUpdateDataFetchCommand::OnIconsDownloaded,
                     AsWeakPtr()));
  icon_downloader_->SkipPageFavicons();
  icon_downloader_->FailAllIfAnyFail();
  icon_downloader_->Start();
}

void ManifestUpdateDataFetchCommand::OnIconsDownloaded(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingIconDownload);

  // TODO(crbug.com/1238622): Report `result` and `icons_http_results` in
  // internals.
  UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnUpdate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnUpdate", icons_http_results);

  if (result != IconsDownloadedResult::kCompleted) {
    CompleteCommand(ManifestUpdateResult::kIconDownloadFailed);
    return;
  }

  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnUpdate", result, icons_http_results);

  stage_ = ManifestUpdateStage::kPendingIconReadFromDisk;
  lock_->icon_manager().ReadAllIcons(
      app_id_, base::BindOnce(&ManifestUpdateDataFetchCommand::OnAllIconsRead,
                              AsWeakPtr(), std::move(icons_map)));
}

void ManifestUpdateDataFetchCommand::OnAllIconsRead(
    IconsMap downloaded_icons_map,
    IconBitmaps disk_icon_bitmaps) {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }

  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingIconReadFromDisk);

  if (disk_icon_bitmaps.empty()) {
    CompleteCommand(ManifestUpdateResult::kIconReadFromDiskFailed);
    return;
  }
  DCHECK(install_info_.has_value());

  stage_ = ManifestUpdateStage::kPendingAppIdentityCheck;

  // These calls populate the |install_info_| with all icon bitmap
  // data. If this data does not match what we already have on disk, then an
  // update is necessary.
  PopulateOtherIcons(&install_info_.value(), downloaded_icons_map);
  PopulateProductIcons(&install_info_.value(), &downloaded_icons_map);

  IconDiff icon_diff = IsUpdateNeededForIconContents(disk_icon_bitmaps);
  std::u16string old_title =
      base::UTF8ToUTF16(lock_->registrar().GetAppShortName(app_id_));
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
                                    lock_->registrar())) {
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
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);
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
  lock_->ui_manager().ShowWebAppIdentityUpdateDialog(
      app_id_, title_change, icon_change, old_title, new_title, *before_icon,
      *after_icon, web_contents_.get(),
      base::BindOnce(
          &ManifestUpdateDataFetchCommand::OnPostAppIdentityUpdateCheck,
          AsWeakPtr()));
}

void ManifestUpdateDataFetchCommand::OnPostAppIdentityUpdateCheck(
    AppIdentityUpdate app_identity_update_allowed) {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingAppIdentityCheck);

  app_identity_update_allowed_ =
      app_identity_update_allowed == AppIdentityUpdate::kAllowed;
  if (app_identity_update_allowed_) {
    CompleteCommand(absl::nullopt);
    return;
  }

  DCHECK(install_info_.has_value());
  if (IsUpdateNeededForManifest(app_id_, install_info_.value(),
                                lock_->registrar())) {
    CompleteCommand(absl::nullopt);
    return;
  }

  lock_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id_, base::BindOnce(
                   &ManifestUpdateDataFetchCommand::OnAllShortcutsMenuIconsRead,
                   AsWeakPtr()));
}

IconDiff ManifestUpdateDataFetchCommand::IsUpdateNeededForIconContents(
    const IconBitmaps& disk_icon_bitmaps) const {
  DCHECK(install_info_.has_value());
  const WebApp* app = lock_->registrar().GetAppById(app_id_);
  DCHECK(app);

  return HaveIconBitmapsChanged(disk_icon_bitmaps, install_info_->icon_bitmaps,
                                install_info_->manifest_icons,
                                app->manifest_icons(),
                                /* end_when_mismatch_detected= */ false);
}

void ManifestUpdateDataFetchCommand::OnAllShortcutsMenuIconsRead(
    ShortcutsMenuIconBitmaps disk_shortcuts_menu_icon_bitmaps) {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingAppIdentityCheck);

  DCHECK(install_info_.has_value());

  if (IsUpdateNeededForShortcutsMenuIconsContents(
          disk_shortcuts_menu_icon_bitmaps)) {
    CompleteCommand(absl::nullopt);
    return;
  }

  NoManifestUpdateRequired();
}

bool ManifestUpdateDataFetchCommand::
    IsUpdateNeededForShortcutsMenuIconsContents(
        const ShortcutsMenuIconBitmaps& disk_shortcuts_menu_icon_bitmaps)
        const {
  DCHECK(install_info_.has_value());
  const ShortcutsMenuIconBitmaps& downloaded_shortcuts_menu_icon_bitmaps =
      install_info_->shortcuts_menu_icon_bitmaps;
  if (downloaded_shortcuts_menu_icon_bitmaps.size() !=
      disk_shortcuts_menu_icon_bitmaps.size()) {
    return true;
  }

  const WebApp* app = lock_->registrar().GetAppById(app_id_);
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

void ManifestUpdateDataFetchCommand::NoManifestUpdateRequired() {
  if (IsWebContentsDestroyed()) {
    CompleteCommand(ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingAppIdentityCheck);
  CompleteCommand(ManifestUpdateResult::kAppUpToDate);
}

void ManifestUpdateDataFetchCommand::CompleteCommand(
    absl::optional<ManifestUpdateResult> early_exit_result) {
  if (early_exit_result.has_value()) {
    debug_log_.Set("result",
                   base::StreamableToString(early_exit_result.value()));
  } else {
    debug_log_.Set("result", "pending_manifest_data_write");
  }

  // TODO(crbug.com/1409710): Does success/failure make sense here? It should
  // probably be based on the exact result rather than if we early exit.
  CommandResult command_result = CommandResult::kSuccess;
  if (early_exit_result.has_value()) {
    command_result = CommandResult::kFailure;
  }
  SignalCompletionAndSelfDestruct(
      command_result,
      base::BindOnce(std::move(fetch_callback_), early_exit_result,
                     std::move(install_info_), app_identity_update_allowed_));
}

}  // namespace web_app
