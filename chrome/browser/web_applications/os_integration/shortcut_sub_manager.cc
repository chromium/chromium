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
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/time.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

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

}  // namespace

ShortcutSubManager::ShortcutSubManager(Profile& profile,
                                       WebAppProvider& provider)
    : profile_(profile), provider_(provider) {}

ShortcutSubManager::~ShortcutSubManager() = default;

void ShortcutSubManager::Configure(
    const webapps::AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_shortcut());

  desired_state.clear_shortcut();

  if (provider_->registrar_unsafe().GetInstallState(app_id) !=
      proto::INSTALLED_WITH_OS_INTEGRATION) {
    std::move(configure_done).Run();
    return;
  }

  auto* shortcut = desired_state.mutable_shortcut();
  shortcut->set_title(provider_->registrar_unsafe().GetAppShortName(app_id));
  shortcut->set_description(
      provider_->registrar_unsafe().GetAppDescription(app_id));
  provider_->icon_manager().ReadIconsLastUpdateTime(
      app_id, base::BindOnce(&ShortcutSubManager::StoreIconDataFromDisk,
                             weak_ptr_factory_.GetWeakPtr(), shortcut)
                  .Then(std::move(configure_done)));
}

void ShortcutSubManager::Execute(
    const webapps::AppId& app_id,
    const std::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  base::FilePath shortcut_data_dir = GetOsIntegrationResourcesDirectoryForApp(
      profile_->GetPath(), app_id,
      provider_->registrar_unsafe().GetAppStartUrl(app_id));

  const WebApp* app = provider_->registrar_unsafe().GetAppById(app_id);
  DCHECK(app);

  const bool force_update_shortcuts =
      synchronize_options.has_value() &&
      synchronize_options.value().force_update_shortcuts;

  const bool force_create_shortcuts =
      synchronize_options.has_value() &&
      synchronize_options.value().force_create_shortcuts;

  // First, handle the case where both current & desired state don't have
  // shortcuts, which should be a no-op.
  if (!desired_state.has_shortcut() && !current_state.has_shortcut()) {
    std::move(callback).Run();
    return;
  }

  CHECK_OS_INTEGRATION_ALLOWED();

  // Second, handle shortcut creation if either one of the following conditions
  // match:
  // 1. current_state is empty but desired_state has shortcut information.
  // 2. desired_state has value and force_create_shortcuts is set in the
  // synchronize_options. This is necessary for use-cases where the user might
  // have deleted shortcuts manually but the current_state has not been updated
  // to show that.
  if ((desired_state.has_shortcut() && !current_state.has_shortcut()) ||
      (desired_state.has_shortcut() && force_create_shortcuts)) {
#if BUILDFLAG(IS_MAC)
    AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                     profile_->GetPath());
#endif

    std::unique_ptr<ShortcutInfo> desired_shortcut_info =
        BuildShortcutInfoWithoutFavicon(
            app_id, provider_->registrar_unsafe().GetAppStartUrl(app_id),
            profile_->GetPath(),
            profile_->GetPrefs()->GetString(prefs::kProfileName),
            desired_state);
    PopulateFaviconForShortcutInfo(
        app, provider_->icon_manager(), std::move(desired_shortcut_info),
        base::BindOnce(&ShortcutSubManager::CreateShortcut,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       synchronize_options, std::move(callback)));
    return;
  }

  // Third, handle shortcut removal.
  if (!desired_state.has_shortcut() && current_state.has_shortcut()) {
    std::unique_ptr<ShortcutInfo> current_shortcut_info =
        BuildShortcutInfoWithoutFavicon(
            app_id, provider_->registrar_unsafe().GetAppStartUrl(app_id),
            profile_->GetPath(),
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
          app_id, provider_->registrar_unsafe().GetAppStartUrl(app_id),
          profile_->GetPath(),
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
      &PopulateFaviconForShortcutInfo, app, std::ref(provider_->icon_manager()),
      std::move(desired_shortcut_info),
      base::BindOnce(&ShortcutSubManager::UpdateShortcut,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     synchronize_options,
                     base::UTF8ToUTF16(current_state.shortcut().title()),
                     std::move(callback_for_update)));

  // Shortcut update detection.
  std::string desired, current;
  desired = desired_state.shortcut().SerializeAsString();
  current = current_state.shortcut().SerializeAsString();
  if (desired != current || force_update_shortcuts) {
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

#if BUILDFLAG(IS_MAC)
  // Protocol handler update detection. Shortcuts need to be updated in this
  // case on Mac because the shortcut itself includes the protocol
  // handling metadata.
  if (desired_state.has_file_handling() != current_state.has_file_handling()) {
    std::move(do_update).Run();
    return;
  }
  if (desired_state.has_file_handling() && current_state.has_file_handling()) {
    desired = desired_state.file_handling().SerializeAsString();
    current = current_state.file_handling().SerializeAsString();
    if (desired != current) {
      std::move(do_update).Run();
      return;
    }
  }
#endif

  // Fifth, no update is required.
  std::move(callback_for_no_update).Run();
}

void ShortcutSubManager::ForceUnregister(const webapps::AppId& app_id,
                                         base::OnceClosure callback) {
  base::FilePath shortcut_data_dir = GetOsIntegrationResourcesDirectoryForApp(
      profile_->GetPath(), app_id,
      provider_->registrar_unsafe().GetAppStartUrl(app_id));

  auto current_shortcut_info = std::make_unique<ShortcutInfo>();
  current_shortcut_info->app_id = app_id;
  current_shortcut_info->profile_path = profile_->GetPath();
  current_shortcut_info->title =
      base::UTF8ToUTF16(provider_->registrar_unsafe().GetAppShortName(app_id));

  internals::ScheduleDeletePlatformShortcuts(
      shortcut_data_dir, std::move(current_shortcut_info),
      base::BindOnce(&ShortcutSubManager::OnShortcutsDeleted,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void ShortcutSubManager::CreateShortcut(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> synchronize_options,
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
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> synchronize_options,
    const std::u16string& old_app_title,
    base::OnceClosure on_complete,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  std::optional<ShortcutLocations> locations = std::nullopt;
  if (synchronize_options.has_value()) {
    ShortcutLocations creation_locations;
    creation_locations.on_desktop =
        synchronize_options->add_shortcut_to_desktop;
    creation_locations.in_quick_launch_bar =
        synchronize_options->add_to_quick_launch_bar;
    locations = creation_locations;
  }

  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);

  internals::ScheduleUpdatePlatformShortcuts(
      shortcut_data_dir, old_app_title, locations,
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.Shortcuts.Update.Result",
                                  (result == Result::kOk));
      }).Then(std::move(on_complete)),
      std::move(shortcut_info));
}

void ShortcutSubManager::OnShortcutsDeleted(const webapps::AppId& app_id,
                                            base::OnceClosure final_callback,
                                            bool success) {
  ResultCallback final_result_callback =
      base::BindOnce([](Result result) {
        bool final_success = result == Result::kOk;
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
