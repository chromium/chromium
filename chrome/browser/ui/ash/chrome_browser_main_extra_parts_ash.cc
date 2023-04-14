// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/shell.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/game_mode/game_mode_controller.h"
#include "chrome/browser/ash/geolocation/system_geolocation_source.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier_factory.h"
#include "chrome/browser/ash/night_light/night_light_client.h"
#include "chrome/browser/ash/policy/display/display_resolution_handler.h"
#include "chrome/browser/ash/policy/display/display_rotation_default_handler.h"
#include "chrome/browser/ash/policy/display/display_settings_handler.h"
#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/tablet_mode/tablet_mode_page_behavior.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"
#include "chrome/browser/ui/ash/ambient/ambient_client_impl.h"
#include "chrome/browser/ui/ash/app_access_notifier.h"
#include "chrome/browser/ui/ash/arc_open_url_delegate_impl.h"
#include "chrome/browser/ui/ash/ash_shell_init.h"
#include "chrome/browser/ui/ash/ash_web_view_factory_impl.h"
#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate_provider.h"
#include "chrome/browser/ui/ash/crosapi_new_window_delegate.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/ime_controller_client_impl.h"
#include "chrome/browser/ui/ash/in_session_auth_dialog_client.h"
#include "chrome/browser/ui/ash/in_session_auth_token_provider_impl.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/media_client_impl.h"
#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"
#include "chrome/browser/ui/ash/network/network_connect_delegate.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/ash/projector/projector_client_impl.h"
#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/shelf/app_service/exo_app_type_resolver.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/tab_cluster_ui_client.h"
#include "chrome/browser/ui/ash/vpn_list_forwarder.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "components/crash/core/common/crash_key.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/ash/input_method_manager.h"

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
#include "chrome/browser/exo_parts.h"
#endif

namespace internal {

// Creates a ChromeShelfController on the first active session notification.
// Used to avoid constructing a ChromeShelfController with no active profile.
class ChromeShelfControllerInitializer
    : public session_manager::SessionManagerObserver {
 public:
  ChromeShelfControllerInitializer() {
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  ChromeShelfControllerInitializer(const ChromeShelfControllerInitializer&) =
      delete;
  ChromeShelfControllerInitializer& operator=(
      const ChromeShelfControllerInitializer&) = delete;

  ~ChromeShelfControllerInitializer() override {
    if (!chrome_shelf_controller_)
      session_manager::SessionManager::Get()->RemoveObserver(this);
    if (chrome_shelf_item_factory_)
      ash::ShelfModel::Get()->SetShelfItemFactory(nullptr);
  }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override {
    DCHECK(!chrome_shelf_controller_);
    DCHECK(!ChromeShelfController::instance());

    if (session_manager::SessionManager::Get()->session_state() ==
        session_manager::SessionState::ACTIVE) {
      chrome_shelf_item_factory_ = std::make_unique<ChromeShelfItemFactory>();
      ash::ShelfModel::Get()->SetShelfItemFactory(
          chrome_shelf_item_factory_.get());
      chrome_shelf_controller_ = std::make_unique<ChromeShelfController>(
          nullptr, ash::ShelfModel::Get(), chrome_shelf_item_factory_.get());
      chrome_shelf_controller_->Init();

      session_manager::SessionManager::Get()->RemoveObserver(this);
    }
  }

 private:
  std::unique_ptr<ChromeShelfItemFactory> chrome_shelf_item_factory_;
  std::unique_ptr<ChromeShelfController> chrome_shelf_controller_;
};

}  // namespace internal

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() = default;

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() = default;

void ChromeBrowserMainExtraPartsAsh::PreCreateMainMessageLoop() {
  user_profile_loaded_observer_ = std::make_unique<UserProfileLoadedObserver>();
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (base::FeatureList::IsEnabled(arc::kEnableArcIdleManager)) {
    // Early init so that later objects can rely on this one.
    arc_window_watcher_ = std::make_unique<ash::ArcWindowWatcher>();
  }

  // NetworkConnect handles the network connection state machine for the UI.
  network_connect_delegate_ = std::make_unique<NetworkConnectDelegate>();
  ash::NetworkConnect::Initialize(network_connect_delegate_.get());

  cast_config_controller_media_router_ =
      std::make_unique<CastConfigControllerMediaRouter>();

  // Needed by AmbientController in ash.
  ambient_client_ = std::make_unique<AmbientClientImpl>();

  // This controller MUST be initialized before the UI (AshShellInit) is
  // constructed. The video conferencing views will observe and have their own
  // reference to this controller, and will assume it exists for as long as they
  // themselves exist.
  if (ash::features::IsVideoConferenceEnabled()) {
    // `VideoConferenceTrayController` relies on audio and camera services to
    // function properly, so we will use the fake version when system bus is not
    // available so that this works on linux-chromeos and unit tests.
    if (ash::DBusThreadManager::Get()->GetSystemBus()) {
      video_conference_tray_controller_ =
          std::make_unique<ash::VideoConferenceTrayController>();
    } else {
      video_conference_tray_controller_ =
          std::make_unique<ash::FakeVideoConferenceTrayController>();
    }
  }

  ash_shell_init_ = std::make_unique<AshShellInit>();

  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateChromeos>();

  app_list_client_ = std::make_unique<AppListClientImpl>();

  // Must be available at login screen, so initialize before profile.
  accessibility_controller_client_ =
      std::make_unique<AccessibilityControllerClient>();

  {
    auto chrome_new_window_client = std::make_unique<ChromeNewWindowClient>();
    auto crosapi_new_window_delegate =
        std::make_unique<CrosapiNewWindowDelegate>(
            chrome_new_window_client.get());
    new_window_delegate_provider_ =
        std::make_unique<ChromeNewWindowDelegateProvider>(
            std::move(chrome_new_window_client),
            std::move(crosapi_new_window_delegate));
    arc_open_url_delegate_impl_ = std::make_unique<ArcOpenUrlDelegateImpl>();
  }

  ime_controller_client_ = std::make_unique<ImeControllerClientImpl>(
      ash::input_method::InputMethodManager::Get());
  ime_controller_client_->Init();

  in_session_auth_dialog_client_ =
      std::make_unique<InSessionAuthDialogClient>();

  in_session_auth_token_provider_ =
      std::make_unique<ash::InSessionAuthTokenProviderImpl>();

  // NOTE: The WallpaperControllerClientImpl must be initialized before the
  // session controller, because the session controller triggers the loading
  // of users, which itself calls a code path which eventually reaches the
  // WallpaperControllerClientImpl singleton instance via
  // ash::ChromeUserManagerImpl.
  wallpaper_controller_client_ =
      std::make_unique<WallpaperControllerClientImpl>(
          std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  wallpaper_controller_client_->Init();

  if (ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    screensaver_images_policy_handler_ =
        std::make_unique<policy::ScreensaverImagesPolicyHandler>();
  }

  session_controller_client_ = std::make_unique<SessionControllerClientImpl>();
  session_controller_client_->Init();
  // By this point ash shell should have initialized its D-Bus signal
  // listeners, so inform the session manager that Ash is initialized.
  session_controller_client_->EmitAshInitialized();

  system_tray_client_ = std::make_unique<SystemTrayClientImpl>();
  network_connect_delegate_->SetSystemTrayClient(system_tray_client_.get());

  if (ash::features::IsTabClusterUIEnabled()) {
    ash::TabClusterUIController* tab_cluster_ui_controller =
        ash::Shell::Get()->tab_cluster_ui_controller();
    DCHECK(tab_cluster_ui_controller);
    tab_cluster_ui_client_ =
        std::make_unique<TabClusterUIClient>(tab_cluster_ui_controller);
  }

  tablet_mode_page_behavior_ = std::make_unique<TabletModePageBehavior>();
  vpn_list_forwarder_ = std::make_unique<VpnListForwarder>();

  chrome_shelf_controller_initializer_ =
      std::make_unique<internal::ChromeShelfControllerInitializer>();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  exo_parts_ = ExoParts::CreateIfNecessary();
  if (exo_parts_) {
    exo::WMHelper::GetInstance()->RegisterAppPropertyResolver(
        std::make_unique<ExoAppTypeResolver>());
  }
#endif

  night_light_client_ = std::make_unique<ash::NightLightClient>(
      g_browser_process->platform_part()->GetTimezoneResolverManager(),
      g_browser_process->shared_url_loader_factory());
  night_light_client_->Start();

  if (ash::features::IsProjectorEnabled()) {
    projector_app_client_ = std::make_unique<ProjectorAppClientImpl>();
    projector_client_ = std::make_unique<ProjectorClientImpl>();
  }

  desks_client_ = std::make_unique<DesksClient>();

  ash::bluetooth_config::FastPairDelegate* delegate =
      ash::features::IsFastPairEnabled()
          ? ash::Shell::Get()->quick_pair_mediator()->GetFastPairDelegate()
          : nullptr;

  ash::bluetooth_config::Initialize(delegate);

  // Create geolocation manager
  g_browser_process->SetGeolocationManager(
      ash::SystemGeolocationSource::CreateGeolocationManagerOnAsh());
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit(Profile* profile,
                                                     bool is_initial_profile) {
  // The setup below is intended to run for only the initial profile.
  if (!is_initial_profile)
    return;

  login_screen_client_ = std::make_unique<LoginScreenClientImpl>();
  // https://crbug.com/884127 ensuring that LoginScreenClientImpl is initialized
  // before using it InitializeDeviceDisablingManager.
  g_browser_process->platform_part()->InitializeDeviceDisablingManager();

  media_client_ = std::make_unique<MediaClientImpl>();
  media_client_->Init();

  // Passes (and continues passing) the current camera count to the PrivacyHub.
  ash::privacy_hub_util::SetUpCameraCountObserver();

  if (ash::features::IsMicMuteNotificationsEnabled()) {
    app_access_notifier_ = std::make_unique<AppAccessNotifier>();
  }

  // Check if Lacros is enabled for crash reporting here to give the user
  // manager a chance to be initialized first.
  constexpr char kLacrosEnabledDataKey[] = "lacros-enabled";
  static crash_reporter::CrashKeyString<4> key(kLacrosEnabledDataKey);
  key.Set(crosapi::browser_util::IsLacrosEnabled() ? "yes" : "no");

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
    network_portal_notification_controller_ =
        std::make_unique<ash::NetworkPortalNotificationController>();
  }

  ash_web_view_factory_ = std::make_unique<AshWebViewFactoryImpl>();

  quick_answers_controller_ = std::make_unique<QuickAnswersControllerImpl>();
  QuickAnswersController::Get()->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          g_browser_process->shared_url_loader_factory(),
          QuickAnswersController::Get()->GetQuickAnswersDelegate()));
  game_mode_controller_ = std::make_unique<game_mode::GameModeController>();

  // Initialize TabScrubberChromeOS after the Ash Shell has been initialized.
  TabScrubberChromeOS::GetInstance();
}

void ChromeBrowserMainExtraPartsAsh::PostBrowserStart() {
  mobile_data_notifications_ = std::make_unique<MobileDataNotifications>();
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  ash::bluetooth_config::Shutdown();

  // Disable event dispatch before Exo starts closing windows to prevent
  // synthetic events from being dispatched. crbug.com/874156 and
  // crbug.com/1163269.
  ash::Shell::Get()->ShutdownEventDispatch();

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  // ExoParts uses state from ash, delete it before ash so that exo can
  // uninstall correctly.
  exo_parts_.reset();
#endif

  night_light_client_.reset();
  mobile_data_notifications_.reset();
  chrome_shelf_controller_initializer_.reset();
  desks_client_.reset();

  projector_client_.reset();
  projector_app_client_.reset();

  wallpaper_controller_client_.reset();
  vpn_list_forwarder_.reset();

  tab_cluster_ui_client_.reset();

  // Initialized in PostProfileInit (which may not get called in some tests).
  game_mode_controller_.reset();
  quick_answers_controller_.reset();
  ash_web_view_factory_.reset();
  network_portal_notification_controller_.reset();
  display_settings_handler_.reset();
  media_client_.reset();
  login_screen_client_.reset();

  if (ash::features::IsMicMuteNotificationsEnabled()) {
    app_access_notifier_.reset();
  }

  // Initialized in PreProfileInit (which may not get called in some tests).
  g_browser_process->SetGeolocationManager(nullptr);
  system_tray_client_.reset();
  session_controller_client_.reset();
  screensaver_images_policy_handler_.reset();
  ime_controller_client_.reset();
  in_session_auth_dialog_client_.reset();
  arc_open_url_delegate_impl_.reset();
  new_window_delegate_provider_.reset();
  accessibility_controller_client_.reset();
  // AppListClientImpl indirectly holds WebContents for answer card and
  // needs to be released before destroying the profile.
  app_list_client_.reset();
  ash_shell_init_.reset();

  // This instance must be destructed after `ash_shell_init_`.
  video_conference_tray_controller_.reset();

  ambient_client_.reset();

  cast_config_controller_media_router_.reset();
  if (ash::NetworkConnect::IsInitialized())
    ash::NetworkConnect::Shutdown();
  network_connect_delegate_.reset();
  user_profile_loaded_observer_.reset();
  arc_window_watcher_.reset();
}

class ChromeBrowserMainExtraPartsAsh::UserProfileLoadedObserver
    : public session_manager::SessionManagerObserver {
 public:
  UserProfileLoadedObserver() {
    session_observation_.Observe(session_manager::SessionManager::Get());
  }

  UserProfileLoadedObserver(const UserProfileLoadedObserver&) = delete;
  UserProfileLoadedObserver& operator=(const UserProfileLoadedObserver&) =
      delete;

  ~UserProfileLoadedObserver() override = default;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override {
    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
    if (ash::ProfileHelper::IsUserProfile(profile) &&
        !profile->IsGuestSession()) {
      // Start the error notifier services to show auth/sync notifications.
      ash::SigninErrorNotifierFactory::GetForProfile(profile);
      ash::SyncErrorNotifierFactory::GetForProfile(profile);
    }

    if (ChromeShelfController::instance()) {
      ChromeShelfController::instance()->OnUserProfileReadyToSwitch(profile);
    }
  }

 private:
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
};
