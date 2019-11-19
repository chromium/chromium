// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"

#include <memory>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/camera/camera_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/crx_file/id_util.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const std::vector<InternalApp>& GetInternalAppListImpl(bool get_all,
                                                       const Profile* profile) {
  DCHECK(get_all || profile);
  static const base::NoDestructor<std::vector<InternalApp>>
      internal_app_list_static(
          {{ash::kInternalAppIdKeyboardShortcutViewer,
            IDS_INTERNAL_APP_KEYBOARD_SHORTCUT_VIEWER,
            IDR_SHORTCUT_VIEWER_LOGO_192,
            /*recommendable=*/false,
            /*searchable=*/true,
            /*show_in_launcher=*/false,
            apps::BuiltInAppName::kKeyboardShortcutViewer,
            IDS_LAUNCHER_SEARCHABLE_KEYBOARD_SHORTCUT_VIEWER},

           {ash::kInternalAppIdContinueReading,
            IDS_INTERNAL_APP_CONTINUOUS_READING, IDR_PRODUCT_LOGO_256,
            /*recommendable=*/true,
            /*searchable=*/false,
            /*show_in_launcher=*/false, apps::BuiltInAppName::kContinueReading,
            /*searchable_string_resource_id=*/0},

           {ash::kReleaseNotesAppId, IDS_RELEASE_NOTES_NOTIFICATION_TITLE,
            IDR_RELEASE_NOTES_APP_192,
            /*recommendable=*/true,
            /*searchable=*/false,
            /*show_in_launcher=*/false, apps::BuiltInAppName::kReleaseNotes,
            /*searchable_string_resource_id=*/0}});

  static base::NoDestructor<std::vector<InternalApp>> internal_app_list;
  internal_app_list->clear();
  internal_app_list->insert(internal_app_list->begin(),
                            internal_app_list_static->begin(),
                            internal_app_list_static->end());

  const bool add_camera_app = get_all || !profile->IsGuestSession();
  if (add_camera_app && !chromeos::CameraUI::IsEnabled()) {
    internal_app_list->push_back({ash::kInternalAppIdCamera,
                                  IDS_INTERNAL_APP_CAMERA, IDR_CAMERA_LOGO_192,
                                  /*recommendable=*/true,
                                  /*searchable=*/true,
                                  /*show_in_launcher=*/true,
                                  apps::BuiltInAppName::kCamera,
                                  /*searchable_string_resource_id=*/0});
  }

  const bool add_discover_app =
      get_all || !chromeos::ProfileHelper::IsEphemeralUserProfile(profile);
  if (base::FeatureList::IsEnabled(chromeos::features::kDiscoverApp) &&
      add_discover_app) {
    internal_app_list->push_back(
        {ash::kInternalAppIdDiscover, IDS_INTERNAL_APP_DISCOVER,
         IDR_DISCOVER_APP_192,
         /*recommendable=*/false,
         /*searchable=*/true,
         /*show_in_launcher=*/true, apps::BuiltInAppName::kDiscover,
         /*searchable_string_resource_id=*/IDS_INTERNAL_APP_DISCOVER});
  }

  // TODO(calamity/nigeltao): when removing the
  // web_app::SystemWebAppManager::IsEnabled condition, we can probably also
  // remove the apps::BuiltInChromeOsApps::SetHideSettingsAppForTesting hack.
  if (!web_app::SystemWebAppManager::IsEnabled()) {
    internal_app_list->push_back(
        {ash::kInternalAppIdSettings, IDS_INTERNAL_APP_SETTINGS,
         IDR_SETTINGS_LOGO_192,
         /*recommendable=*/true,
         /*searchable=*/true,
         /*show_in_launcher=*/true, apps::BuiltInAppName::kSettings,
         /*searchable_string_resource_id=*/0});
  }

  if (get_all || plugin_vm::IsPluginVmAllowedForProfile(profile)) {
    internal_app_list->push_back(
        {plugin_vm::kPluginVmAppId, IDS_PLUGIN_VM_APP_NAME,
         IDR_LOGO_PLUGIN_VM_DEFAULT_192,
         /*recommendable=*/true,
         /*searchable=*/true,
         /*show_in_launcher=*/true, apps::BuiltInAppName::kPluginVm,
         /*searchable_string_resource_id=*/0});
  }
  return *internal_app_list;
}

}  // namespace

const std::vector<InternalApp>& GetInternalAppList(const Profile* profile) {
  return GetInternalAppListImpl(false, profile);
}

bool IsSuggestionChip(const std::string& app_id) {
  // App IDs for internal apps which should only be shown as suggestion chips.
  static const char* kSuggestionChipIds[] = {ash::kInternalAppIdContinueReading,
                                             ash::kReleaseNotesAppId};

  for (size_t i = 0; i < base::size(kSuggestionChipIds); ++i) {
    if (base::LowerCaseEqualsASCII(app_id, kSuggestionChipIds[i]))
      return true;
  }
  return false;
}

const InternalApp* FindInternalApp(const std::string& app_id) {
  for (const auto& app : GetInternalAppListImpl(true, nullptr)) {
    if (app_id == app.app_id)
      return &app;
  }
  return nullptr;
}

bool IsInternalApp(const std::string& app_id) {
  return !!FindInternalApp(app_id);
}

base::string16 GetInternalAppNameById(const std::string& app_id) {
  const auto* app = FindInternalApp(app_id);
  return app ? l10n_util::GetStringUTF16(app->name_string_resource_id)
             : base::string16();
}

int GetIconResourceIdByAppId(const std::string& app_id) {
  const auto* app = FindInternalApp(app_id);
  return app ? app->icon_resource_id : 0;
}

void OpenChromeCameraApp(Profile* profile, int event_flags) {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(extension_misc::kChromeCameraAppId);
  if (extension) {
    AppListClientImpl* controller = AppListClientImpl::GetInstance();
    apps::AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
        profile, extension, event_flags,
        apps::mojom::AppLaunchSource::kSourceAppLauncher,
        controller->GetAppListDisplayId());
    params.launch_id = ash::ShelfID(extension->id()).launch_id;
    apps::LaunchService::Get(profile)->OpenApplication(params);
    VLOG(1) << "Launched CCA.";
  } else {
    LOG(ERROR) << "CCA not found on device";
  }
}

void OpenInternalApp(const std::string& app_id,
                     Profile* profile,
                     int event_flags) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    ash::ToggleKeyboardShortcutViewer();
  } else if (app_id == ash::kInternalAppIdSettings) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile);
  } else if (app_id == ash::kInternalAppIdCamera) {
    // In case Camera app is already running, use it to prevent appearing double
    // apps, from Chrome and Android domains.
    const ash::ShelfID shelf_id(ash::kInternalAppIdCamera);
    AppWindowLauncherItemController* const app_controller =
        ChromeLauncherController::instance()
            ->shelf_model()
            ->GetAppWindowLauncherItemController(shelf_id);
    if (app_controller) {
      VLOG(1)
          << "Camera app controller already exists, activating existing app.";
      app_controller->ActivateIndexedApp(0 /* index */);
    } else {
      OpenChromeCameraApp(profile, event_flags);
    }
  } else if (app_id == ash::kInternalAppIdDiscover) {
    base::RecordAction(base::UserMetricsAction("ShowDiscover"));
    chromeos::DiscoverWindowManager::GetInstance()
        ->ShowChromeDiscoverPageForProfile(profile);
  } else if (app_id == plugin_vm::kPluginVmAppId) {
    if (plugin_vm::IsPluginVmEnabled(profile)) {
      plugin_vm::PluginVmManager::GetForProfile(profile)->LaunchPluginVm();
    } else {
      plugin_vm::ShowPluginVmLauncherView(profile);
    }
  } else if (app_id == ash::kReleaseNotesAppId) {
    base::RecordAction(
        base::UserMetricsAction("ReleaseNotes.SuggestionChipLaunched"));
    chrome::LaunchReleaseNotes(profile);
  }
}

gfx::ImageSkia GetIconForResourceId(int resource_id, int resource_size_in_dip) {
  if (resource_id == 0)
    return gfx::ImageSkia();

  gfx::ImageSkia* source =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      *source, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip, resource_size_in_dip));
}

bool HasRecommendableForeignTab(
    Profile* profile,
    base::string16* title,
    GURL* url,
    sync_sessions::OpenTabsUIDelegate* test_delegate) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(profile);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  sync_sessions::OpenTabsUIDelegate* delegate =
      test_delegate ? test_delegate : service->GetOpenTabsUIDelegate();
  if (delegate != nullptr)
    delegate->GetAllForeignSessions(&foreign_sessions);

  constexpr int kMaxForeignTabAgeInMinutes = 120;
  base::Time latest_timestamp;
  bool has_recommendation = false;
  for (const sync_sessions::SyncedSession* session : foreign_sessions) {
    if (latest_timestamp > session->modified_time)
      continue;

    auto device_type = session->device_type;
    if (device_type != sync_pb::SyncEnums::TYPE_PHONE &&
        device_type != sync_pb::SyncEnums::TYPE_TABLET) {
      continue;
    }

    for (const auto& key_value : session->windows) {
      for (const std::unique_ptr<sessions::SessionTab>& tab :
           key_value.second->wrapped_window.tabs) {
        if (tab->navigations.empty())
          continue;

        const sessions::SerializedNavigationEntry& navigation =
            tab->navigations.back();
        const GURL& virtual_url = navigation.virtual_url();

        // Only show pages with http or https.
        if (!virtual_url.SchemeIsHTTPOrHTTPS())
          continue;

        // Only show pages recently opened.
        const base::TimeDelta tab_age = base::Time::Now() - tab->timestamp;
        if (tab_age > base::TimeDelta::FromMinutes(kMaxForeignTabAgeInMinutes))
          continue;

        if (latest_timestamp < tab->timestamp) {
          has_recommendation = true;
          latest_timestamp = tab->timestamp;
          if (title) {
            *title = navigation.title().empty()
                         ? base::UTF8ToUTF16(virtual_url.spec())
                         : navigation.title();
          }

          if (url)
            *url = virtual_url;
        }
      }
    }
  }
  return has_recommendation;
}

size_t GetNumberOfInternalAppsShowInLauncherForTest(std::string* apps_name,
                                                    const Profile* profile) {
  size_t num_of_internal_apps_show_in_launcher = 0u;
  std::vector<std::string> internal_apps_name;
  for (const auto& app : GetInternalAppList(profile)) {
    if (app.show_in_launcher) {
      ++num_of_internal_apps_show_in_launcher;
      if (apps_name) {
        internal_apps_name.emplace_back(
            l10n_util::GetStringUTF8(app.name_string_resource_id));
      }
    }
  }
  if (apps_name)
    *apps_name = base::JoinString(internal_apps_name, ",");
  return num_of_internal_apps_show_in_launcher;
}

}  // namespace app_list
