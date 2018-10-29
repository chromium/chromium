// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/mus_property_mirror_ash.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/process_creation_time_recorder.mojom.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/night_light/night_light_client.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"
#include "chrome/browser/ui/ash/ash_shell_init.h"
#include "chrome/browser/ui/ash/cast_config_client_media_router.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/media_client.h"
#include "chrome/browser/ui/ash/network/data_promo_notification.h"
#include "chrome/browser/ui/ash/network/network_connect_delegate_chromeos.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/tab_scrubber.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/ash/volume_controller.h"
#include "chrome/browser/ui/ash/vpn_list_forwarder.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/views/frame/immersive_context_mus.h"
#include "chrome/browser/ui/views/frame/immersive_handler_factory_mus.h"
#include "chrome/browser/ui/views/ime_driver/ime_driver_mus.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
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
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/user_activity_monitor.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/user_activity_forwarder.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/mus/mus_client.h"

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
#include "chrome/browser/exo_parts.h"
#endif

namespace {

void PushProcessCreationTimeToAsh() {
  ash::mojom::ProcessCreationTimeRecorderPtr recorder;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &recorder);
  DCHECK(!startup_metric_utils::MainEntryPointTicks().is_null());
  recorder->SetMainProcessCreationTime(
      startup_metric_utils::MainEntryPointTicks());
}

}  // namespace

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
      // Chrome keeps its own ShelfModel copy in sync with Ash's ShelfModel.
      chrome_shelf_model_ = std::make_unique<ash::ShelfModel>();
      chrome_launcher_controller_ = std::make_unique<ChromeLauncherController>(
          nullptr, chrome_shelf_model_.get());
      chrome_launcher_controller_->Init();

      session_manager::SessionManager::Get()->RemoveObserver(this);
    }
  }

 private:
  // By default |chrome_shelf_model_| is synced with Ash's ShelfController
  // instance in Mash and in Classic Ash; otherwise this is not created and
  // Ash's ShelfModel instance is used directly.
  std::unique_ptr<ash::ShelfModel> chrome_shelf_model_;
  std::unique_ptr<ChromeLauncherController> chrome_launcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerInitializer);
};

}  // namespace internal

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh()
    : notification_observer_(std::make_unique<NotificationObserver>()) {}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
  // Views code observes TabletModeClient and may not be destroyed until
  // ash::Shell is, so destroy |tablet_mode_client_| after ash::Shell.
  // Also extensions need to remove observers after PostMainMessageLoopRun().
  tablet_mode_client_.reset();
}

void ChromeBrowserMainExtraPartsAsh::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  if (features::IsMultiProcessMash()) {
    // ash::Shell will not be created because ash is running out-of-process.
    ash::Shell::SetIsBrowserProcessWithMash();
  }

  if (features::IsUsingWindowService()) {
    // Start up the window service and the ash system UI service.
    // NOTE: ash::Shell is still created below for SingleProcessMash.
    connection->GetConnector()->StartService(
        service_manager::Identity(ws::mojom::kServiceName));
    connection->GetConnector()->StartService(
        service_manager::Identity(ash::mojom::kServiceName));

    views::MusClient::InitParams params;
    params.connector = connection->GetConnector();
    params.io_task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
    // WMState has already been created, so don't have MusClient create it.
    params.create_wm_state = false;
    params.running_in_ws_process = features::IsSingleProcessMash();
    params.create_cursor_factory = !features::IsSingleProcessMash();
    mus_client_ = std::make_unique<views::MusClient>(params);
    // Register ash-specific window properties with Chrome's property converter.
    // Values of registered properties will be transported between the services.
    ash::RegisterWindowProperties(mus_client_->property_converter());
    mus_client_->SetMusPropertyMirror(
        std::make_unique<ash::MusPropertyMirrorAsh>());
  }
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  // IME driver must be available at login screen, so initialize before profile.
  IMEDriver::Register();

  // NetworkConnect handles the network connection state machine for the UI.
  network_connect_delegate_ =
      std::make_unique<NetworkConnectDelegateChromeOS>();
  chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());

  if (!features::IsMultiProcessMash()) {
    ash_shell_init_ = std::make_unique<AshShellInit>();
  } else {
    immersive_handler_factory_ = std::make_unique<ImmersiveHandlerFactoryMus>();

    // Enterprise support in the browser can monitor user activity. Connect to
    // the UI service to monitor activity. The ash process has its own monitor.
    // TODO(jamescook): Figure out if we need this for SingleProcessMash.
    // https://crbug.com/626899
    user_activity_detector_ = std::make_unique<ui::UserActivityDetector>();
    ws::mojom::UserActivityMonitorPtr user_activity_monitor;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ws::mojom::kServiceName, &user_activity_monitor);
    user_activity_forwarder_ = std::make_unique<aura::UserActivityForwarder>(
        std::move(user_activity_monitor), user_activity_detector_.get());
  }

  if (features::IsUsingWindowService())
    immersive_context_ = std::make_unique<ImmersiveContextMus>();

  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateChromeos>();

  app_list_client_ = std::make_unique<AppListClientImpl>();

  // Must be available at login screen, so initialize before profile.
  accessibility_controller_client_ =
      std::make_unique<AccessibilityControllerClient>();
  accessibility_controller_client_->Init();

  chrome_new_window_client_ = std::make_unique<ChromeNewWindowClient>();

  ime_controller_client_ = std::make_unique<ImeControllerClient>(
      chromeos::input_method::InputMethodManager::Get());
  ime_controller_client_->Init();

  session_controller_client_ = std::make_unique<SessionControllerClient>();
  session_controller_client_->Init();

  system_tray_client_ = std::make_unique<SystemTrayClient>();

  // Makes mojo request to TabletModeController in ash.
  tablet_mode_client_ = std::make_unique<TabletModeClient>();
  tablet_mode_client_->Init();

  volume_controller_ = std::make_unique<VolumeController>();

  vpn_list_forwarder_ = std::make_unique<VpnListForwarder>();

  wallpaper_controller_client_ = std::make_unique<WallpaperControllerClient>();
  wallpaper_controller_client_->Init();

  chrome_launcher_controller_initializer_ =
      std::make_unique<internal::ChromeLauncherControllerInitializer>();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  exo_parts_ = ExoParts::CreateIfNecessary();
#endif

  PushProcessCreationTimeToAsh();
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  cast_config_client_media_router_ =
      std::make_unique<CastConfigClientMediaRouter>();
  login_screen_client_ = std::make_unique<LoginScreenClient>();
  media_client_ = std::make_unique<MediaClient>();

  // Instantiate DisplayRotationDefaultHandler after CrosSettings has been
  // initialized.
  display_rotation_handler_ =
      std::make_unique<policy::DisplayRotationDefaultHandler>();

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

  // TODO(mash): Port TabScrubber. This depends on where gesture recognition
  // happens because TabScrubber uses 3-finger scrolls. https://crbug.com/796366
  if (!features::IsMultiProcessMash()) {
    // Initialize TabScrubber after the Ash Shell has been initialized.
    TabScrubber::GetInstance();
  }
}

void ChromeBrowserMainExtraPartsAsh::PostBrowserStart() {
  data_promo_notification_ = std::make_unique<DataPromoNotification>();

  if (ash::features::IsNightLightEnabled()) {
    night_light_client_ = std::make_unique<NightLightClient>(
        g_browser_process->shared_url_loader_factory());
    night_light_client_->Start();
  }
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  // ExoParts uses state from ash, delete it before ash so that exo can
  // uninstall correctly.
  exo_parts_.reset();
#endif

  night_light_client_.reset();
  data_promo_notification_.reset();
  chrome_launcher_controller_initializer_.reset();

  wallpaper_controller_client_.reset();
  vpn_list_forwarder_.reset();
  volume_controller_.reset();

  // Initialized in PostProfileInit:
  network_portal_notification_controller_.reset();
  display_rotation_handler_.reset();
  media_client_.reset();
  login_screen_client_.reset();
  cast_config_client_media_router_.reset();

  // Initialized in PreProfileInit:
  system_tray_client_.reset();
  session_controller_client_.reset();
  ime_controller_client_.reset();
  chrome_new_window_client_.reset();
  accessibility_controller_client_.reset();
  // AppListClientImpl indirectly holds WebContents for answer card and
  // needs to be released before destroying the profile.
  app_list_client_.reset();
  ash_shell_init_.reset();

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
