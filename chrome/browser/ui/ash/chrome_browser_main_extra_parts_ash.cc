// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/night_light/night_light_client.h"
#include "chrome/browser/chromeos/policy/display_resolution_handler.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/policy/display_settings_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"
#include "chrome/browser/ui/ash/ambient/photo_controller_impl.h"
#include "chrome/browser/ui/ash/arc_chrome_actions_client.h"
#include "chrome/browser/ui/ash/ash_shell_init.h"
#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/media_client_impl.h"
#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"
#include "chrome/browser/ui/ash/network/network_connect_delegate_chromeos.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/tab_scrubber.h"
#include "chrome/browser/ui/ash/tablet_mode_page_behavior.h"
#include "chrome/browser/ui/ash/vpn_list_forwarder.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
#include "chrome/browser/exo_parts.h"
#endif

namespace internal {

// Creates a ChromeLauncherController on the first active session notification.
// Used to avoid constructing a ChromeLauncherController with no active profile.
class ChromeLauncherControllerInitializer
    : public session_manager::SessionManagerObserver {
 public:
  ChromeLauncherControllerInitializer() {
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  ~ChromeLauncherControllerInitializer() override {
    if (!chrome_launcher_controller_)
      session_manager::SessionManager::Get()->RemoveObserver(this);
  }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override {
    DCHECK(!chrome_launcher_controller_);
    DCHECK(!ChromeLauncherController::instance());

    if (session_manager::SessionManager::Get()->session_state() ==
        session_manager::SessionState::ACTIVE) {
      chrome_launcher_controller_ = std::make_unique<ChromeLauncherController>(
          nullptr, ash::ShelfModel::Get());
      chrome_launcher_controller_->Init();

      session_manager::SessionManager::Get()->RemoveObserver(this);
    }
  }

 private:
  std::unique_ptr<ChromeLauncherController> chrome_launcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerInitializer);
};

}  // namespace internal

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh()
    : notification_observer_(std::make_unique<NotificationObserver>()) {}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() = default;

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  // NetworkConnect handles the network connection state machine for the UI.
  network_connect_delegate_ =
      std::make_unique<NetworkConnectDelegateChromeOS>();
  chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());

  cast_config_controller_media_router_ =
      std::make_unique<CastConfigControllerMediaRouter>();

  ash_shell_init_ = std::make_unique<AshShellInit>();

  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateChromeos>();

  app_list_client_ = std::make_unique<AppListClientImpl>();

  // Must be available at login screen, so initialize before profile.
  accessibility_controller_client_ =
      std::make_unique<AccessibilityControllerClient>();

  chrome_new_window_client_ = std::make_unique<ChromeNewWindowClient>();

  ime_controller_client_ = std::make_unique<ImeControllerClient>(
      chromeos::input_method::InputMethodManager::Get());
  ime_controller_client_->Init();

  // NOTE: The WallpaperControllerClient must be initialized before the
  // session controller, because the session controller triggers the loading
  // of users, which itself calls a code path which eventually reaches the
  // WallpaperControllerClient singleton instance via
  // chromeos::ChromeUserManagerImpl.
  wallpaper_controller_client_ = std::make_unique<WallpaperControllerClient>();
  wallpaper_controller_client_->Init();

  session_controller_client_ = std::make_unique<SessionControllerClientImpl>();
  session_controller_client_->Init();

  system_tray_client_ = std::make_unique<SystemTrayClient>();
  tablet_mode_page_behavior_ = std::make_unique<TabletModePageBehavior>();
  vpn_list_forwarder_ = std::make_unique<VpnListForwarder>();

  chrome_launcher_controller_initializer_ =
      std::make_unique<internal::ChromeLauncherControllerInitializer>();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);

  arc_chrome_actions_client_ = std::make_unique<ArcChromeActionsClient>();

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  exo_parts_ = ExoParts::CreateIfNecessary();
#endif
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  login_screen_client_ = std::make_unique<LoginScreenClient>();
  // https://crbug.com/884127 ensuring that LoginScreenClient is initialized
  // before using it InitializeDeviceDisablingManager.
  g_browser_process->platform_part()->InitializeDeviceDisablingManager();

  media_client_ = std::make_unique<MediaClientImpl>();
  media_client_->Init();

  // Instantiate DisplaySettingsHandler after CrosSettings has been
  // initialized.
  display_settings_handler_ =
      std::make_unique<policy::DisplaySettingsHandler>();
  display_settings_handler_->RegisterHandler(
      std::make_unique<policy::DisplayResolutionHandler>());
  display_settings_handler_->RegisterHandler(
      std::make_unique<policy::DisplayRotationDefaultHandler>());
  display_settings_handler_->Start();

  // Do not create a NetworkPortalNotificationController for tests since the
  // NetworkPortalDetector instance may be replaced.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    chromeos::NetworkPortalDetector* detector =
        chromeos::network_portal_detector::GetInstance();
    CHECK(detector);
    network_portal_notification_controller_ =
        std::make_unique<chromeos::NetworkPortalNotificationController>(
            detector);
  }

  // Initialize TabScrubber after the Ash Shell has been initialized.
  TabScrubber::GetInstance();
}

void ChromeBrowserMainExtraPartsAsh::PostBrowserStart() {
  mobile_data_notifications_ = std::make_unique<MobileDataNotifications>();

  night_light_client_ = std::make_unique<NightLightClient>(
      g_browser_process->shared_url_loader_factory());
  night_light_client_->Start();

  if (chromeos::features::IsAmbientModeEnabled())
    photo_controller_ = std::make_unique<PhotoControllerImpl>();
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  // ExoParts uses state from ash, delete it before ash so that exo can
  // uninstall correctly.
  exo_parts_.reset();
#endif

  if (chromeos::features::IsAmbientModeEnabled())
    photo_controller_.reset();

  night_light_client_.reset();
  mobile_data_notifications_.reset();
  chrome_launcher_controller_initializer_.reset();

  wallpaper_controller_client_.reset();
  vpn_list_forwarder_.reset();

  // Initialized in PostProfileInit (which may not get called in some tests).
  network_portal_notification_controller_.reset();
  display_settings_handler_.reset();
  media_client_.reset();
  login_screen_client_.reset();

  // Initialized in PreProfileInit (which may not get called in some tests).
  system_tray_client_.reset();
  session_controller_client_.reset();
  ime_controller_client_.reset();
  chrome_new_window_client_.reset();
  accessibility_controller_client_.reset();
  // AppListClientImpl indirectly holds WebContents for answer card and
  // needs to be released before destroying the profile.
  app_list_client_.reset();
  ash_shell_init_.reset();
  cast_config_controller_media_router_.reset();
  if (chromeos::NetworkConnect::IsInitialized())
    chromeos::NetworkConnect::Shutdown();
  network_connect_delegate_.reset();
}

class ChromeBrowserMainExtraPartsAsh::NotificationObserver
    : public content::NotificationObserver {
 public:
  NotificationObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                   content::NotificationService::AllSources());
  }
  ~NotificationObserver() override = default;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
        Profile* profile = content::Details<Profile>(details).ptr();
        if (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
            !chromeos::ProfileHelper::IsLockScreenAppProfile(profile) &&
            !profile->IsGuestSession() && !profile->IsSupervised()) {
          // Start the error notifier services to show auth/sync notifications.
          SigninErrorNotifierFactory::GetForProfile(profile);
          SyncErrorNotifierFactory::GetForProfile(profile);
        }
        // Do not use chrome::NOTIFICATION_PROFILE_ADDED because the
        // profile is not fully initialized by user_manager.  Use
        // chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED instead.
        if (ChromeLauncherController::instance()) {
          ChromeLauncherController::instance()->OnUserProfileReadyToSwitch(
              profile);
        }
        break;
      }
      default:
        NOTREACHED() << "Unexpected notification " << type;
    }
  }

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NotificationObserver);
};
