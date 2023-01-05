// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_sub_manager.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/time.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace web_app {
namespace {

// Result of shortcuts creation process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreationResult {
  kSuccess = 0,
  kFailToCreateShortcut = 1,
  kMaxValue = kFailToCreateShortcut
};

gfx::ImageFamily PackageIconsIntoImageFamily(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  gfx::ImageFamily image_family;
  for (auto& size_and_bitmap : icon_bitmaps) {
    image_family.Add(gfx::ImageSkia(
        gfx::ImageSkiaRep(size_and_bitmap.second, /*scale=*/0.0f)));
  }

  // If the image failed to load, use the standard application icon.
  if (image_family.empty()) {
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
  }

  return image_family;
}

std::unique_ptr<ShortcutInfo> SetFavicon(
    std::unique_ptr<ShortcutInfo> shortcut_info,
    gfx::ImageFamily image_family) {
  shortcut_info->favicon = std::move(image_family);
  return shortcut_info;
}

void PopulateFaviconForShortcutInfo(
    const WebApp* app,
    WebAppIconManager& icon_manager,
    std::unique_ptr<ShortcutInfo> shortcut_info_to_populate,
    base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)> callback) {
  DCHECK(app);

  // Build a common intersection between desired and downloaded icons.
  auto icon_sizes_in_px = base::STLSetIntersection<std::vector<SquareSizePx>>(
      app->downloaded_icon_sizes(IconPurpose::ANY),
      GetDesiredIconSizesForShortcut());

  auto populate_and_return_shortcut_info =
      base::BindOnce(&SetFavicon, std::move(shortcut_info_to_populate))
          .Then(std::move(callback));

  if (!icon_sizes_in_px.empty()) {
    icon_manager.ReadIcons(
        app->app_id(), IconPurpose::ANY, icon_sizes_in_px,
        base::BindOnce(&PackageIconsIntoImageFamily)
            .Then(std::move(populate_and_return_shortcut_info)));
    return;
  }

  // If there is no single icon at the desired sizes, we will resize what we can
  // get.
  SquareSizePx desired_icon_size = GetDesiredIconSizesForShortcut().back();
  icon_manager.ReadIconAndResize(
      app->app_id(), IconPurpose::ANY, desired_icon_size,
      base::BindOnce(&PackageIconsIntoImageFamily)
          .Then(std::move(populate_and_return_shortcut_info)));
}
}  // namespace

ShortcutSubManager::ShortcutSubManager(Profile& profile,
                                       WebAppIconManager& icon_manager,
                                       WebAppRegistrar& registrar)
    : profile_(profile), icon_manager_(icon_manager), registrar_(registrar) {}

ShortcutSubManager::~ShortcutSubManager() = default;

void ShortcutSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_shortcut());

  desired_state.clear_shortcut();

  if (!registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  auto* shortcut = desired_state.mutable_shortcut();
  shortcut->set_title(registrar_->GetAppShortName(app_id));
  shortcut->set_description(registrar_->GetAppDescription(app_id));
  icon_manager_->ReadIconsLastUpdateTime(
      app_id, base::BindOnce(&ShortcutSubManager::StoreIconDataFromDisk,
                             weak_ptr_factory_.GetWeakPtr(), shortcut)
                  .Then(std::move(configure_done)));
}

void ShortcutSubManager::Start() {}

void ShortcutSubManager::Shutdown() {}

void ShortcutSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  base::FilePath shortcut_data_dir = GetOsIntegrationResourcesDirectoryForApp(
      profile_->GetPath(), app_id, registrar_->GetAppStartUrl(app_id));

  const WebApp* app = registrar_->GetAppById(app_id);
  DCHECK(app);

  // First, handle the case where both current & desired don't have shortcuts,
  // which should be a no-op.
  if (!desired_state.has_shortcut() && !current_state.has_shortcut()) {
    std::move(callback).Run();
    return;
  }

  // Second, handle shortcut creation.
  if (desired_state.has_shortcut() && !current_state.has_shortcut()) {
    // This is required to create the app shim registry for the current profile
    // on Mac, otherwise updates to the AppShimRegistry do not happen.
#if BUILDFLAG(IS_MAC)
    AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                     profile_->GetPath());
#endif
    std::unique_ptr<ShortcutInfo> desired_shortcut_info =
        BuildShortcutInfoWithoutFavicon(
            app_id, registrar_->GetAppStartUrl(app_id), profile_->GetPath(),
            profile_->GetPrefs()->GetString(prefs::kProfileName),
            desired_state);
    PopulateFaviconForShortcutInfo(
        app, *icon_manager_, std::move(desired_shortcut_info),
        base::BindOnce(&ShortcutSubManager::CreateShortcut,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       synchronize_options, std::move(callback)));
    return;
  }

  // Third, handle shortcut removal.
  if (!desired_state.has_shortcut() && current_state.has_shortcut()) {
    std::unique_ptr<ShortcutInfo> current_shortcut_info =
        BuildShortcutInfoWithoutFavicon(
            app_id, registrar_->GetAppStartUrl(app_id), profile_->GetPath(),
            profile_->GetPrefs()->GetString(prefs::kProfileName),
            current_state);

    internals::ScheduleDeletePlatformShortcuts(
        shortcut_data_dir, std::move(current_shortcut_info),
        base::BindOnce(&ShortcutSubManager::OnShortcutsDeleted,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(callback)));
    return;
  }

  // Fourth, handle update.
  std::unique_ptr<ShortcutInfo> desired_shortcut_info =
      BuildShortcutInfoWithoutFavicon(
          app_id, registrar_->GetAppStartUrl(app_id), profile_->GetPath(),
          profile_->GetPrefs()->GetString(prefs::kProfileName), desired_state);

  // The following section decides if an update needs to occur or not. To
  // optimize for the least number of serializations, the 'update' callback is
  // created first and then used below.

  // If no update is detected, we still need to call the `callback` argument, so
  // split the callback to allow us to pass ownership of one to the 'update'
  // part.
  auto [callback_for_update, callback_for_no_update] =
      base::SplitOnceCallback(std::move(callback));

  DCHECK(desired_state.has_shortcut());
  DCHECK(current_state.has_shortcut());

  // Note: This callback is either called immediately (and synchronously), or
  // not at all. This is why the usage of `std::ref` and `app` is safe.
  auto do_update = base::BindOnce(
      &PopulateFaviconForShortcutInfo, app, std::ref(*icon_manager_),
      std::move(desired_shortcut_info),
      base::BindOnce(&ShortcutSubManager::UpdateShortcut,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     base::UTF8ToUTF16(current_state.shortcut().title()),
                     std::move(callback_for_update)));

  // Shortcut update detection.
  std::string desired, current;
  desired = desired_state.shortcut().SerializeAsString();
  current = current_state.shortcut().SerializeAsString();
  if (desired != current) {
    std::move(do_update).Run();
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  // Protocol handler update detection. Shortcuts need to be updated in this
  // case on Linux & Mac because the shortcut itself includes the protocol
  // handling metadata.
  if (desired_state.has_protocols_handled() !=
      current_state.has_protocols_handled()) {
    std::move(do_update).Run();
    return;
  }
  if (desired_state.has_protocols_handled() &&
      current_state.has_protocols_handled()) {
    desired = desired_state.protocols_handled().SerializeAsString();
    current = current_state.protocols_handled().SerializeAsString();
    if (desired != current) {
      std::move(do_update).Run();
      return;
    }
  }
#endif

  // TODO: Add file handler change detection.

  // Fifth, no update is required.
  std::move(callback_for_no_update).Run();
}

void ShortcutSubManager::CreateShortcut(
    const AppId& app_id,
    absl::optional<SynchronizeOsOptions> synchronize_options,
    base::OnceClosure on_complete,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  SynchronizeOsOptions options =
      synchronize_options.value_or(SynchronizeOsOptions());

  ShortcutLocations locations;
  locations.on_desktop = options.add_shortcut_to_desktop;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  locations.in_quick_launch_bar = options.add_to_quick_launch_bar;

  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::ScheduleCreatePlatformShortcuts(
      shortcut_data_dir, locations, options.reason, std::move(shortcut_info),
      base::BindOnce([](bool success) {
        base::UmaHistogramEnumeration(
            "WebApp.Shortcuts.Creation.Result",
            success ? CreationResult::kSuccess
                    : CreationResult::kFailToCreateShortcut);
      }).Then(std::move(on_complete)));
}

void ShortcutSubManager::UpdateShortcut(
    const AppId& app_id,
    const std::u16string& old_app_title,
    base::OnceClosure on_complete,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTaskAndReplyWithResult(
      base::BindOnce(&internals::UpdatePlatformShortcuts,
                     std::move(shortcut_data_dir), std::move(old_app_title)),
      std::move(shortcut_info),
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.Shortcuts.Update.Result",
                                  (result == Result::kOk));
      }).Then(std::move(on_complete)));
}

void ShortcutSubManager::OnShortcutsDeleted(const AppId& app_id,
                                            base::OnceClosure final_callback,
                                            bool success) {
  ResultCallback final_result_callback =
      base::BindOnce([](Result result) {
        bool final_success = (result == Result::kOk) ? true : false;
        base::UmaHistogramBoolean("WebApp.Shortcuts.Delete.Result",
                                  final_success);
      }).Then(std::move(final_callback));

#if BUILDFLAG(IS_MAC)
  bool delete_multi_profile_shortcuts =
      AppShimRegistry::Get()->OnAppUninstalledForProfile(app_id,
                                                         profile_->GetPath());
  if (delete_multi_profile_shortcuts) {
    internals::ScheduleDeleteMultiProfileShortcutsForApp(
        app_id, std::move(final_result_callback));
  } else {
    std::move(final_result_callback)
        .Run(success ? Result::kOk : Result::kError);
  }
#else
  std::move(final_result_callback).Run(success ? Result::kOk : Result::kError);
#endif
}

void ShortcutSubManager::StoreIconDataFromDisk(
    proto::ShortcutDescription* shortcut,
    base::flat_map<SquareSizePx, base::Time> time_map) {
  for (const auto& [size, time] : time_map) {
    auto* shortcut_icon_data = shortcut->add_icon_data_any();
    shortcut_icon_data->set_icon_size(size);
    shortcut_icon_data->set_timestamp(syncer::TimeToProtoTime(time));
  }
}

}  // namespace web_app
