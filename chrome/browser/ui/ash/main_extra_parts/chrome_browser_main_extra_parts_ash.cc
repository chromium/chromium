// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/main_extra_parts/chrome_browser_main_extra_parts_ash.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/refresh_rate_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/webui/annotator/annotator_client_impl.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chrome/browser/ash/auth/active_session_fingerprint_client_impl.h"
#include "chrome/browser/ash/boca/boca_app_client_impl.h"
#include "chrome/browser/ash/geolocation/system_geolocation_source.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/ash/growth/campaigns_manager_session.h"
#include "chrome/browser/ash/input_device_settings/peripherals_app_delegate_impl.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier_factory.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ash/mahi/mahi_manager_impl.h"
#include "chrome/browser/ash/mahi/media_app/mahi_media_app_content_manager_impl.h"
#include "chrome/browser/ash/mahi/media_app/mahi_media_app_events_proxy_impl.h"
#include "chrome/browser/ash/net/vpn_list_forwarder.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/display/display_resolution_handler.h"
#include "chrome/browser/ash/policy/display/display_rotation_default_handler.h"
#include "chrome/browser/ash/policy/display/display_settings_handler.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/tablet_mode/tablet_mode_page_behavior.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_cleanup_manager.h"
#include "chrome/browser/exo_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"
#include "chrome/browser/ui/ash/app_access/app_access_notifier.h"
#include "chrome/browser/ui/ash/arc/arc_open_url_delegate_impl.h"
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/graduation/graduation_manager_impl.h"
#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_dialog_client.h"
#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_token_provider_impl.h"
#include "chrome/browser/ui/ash/input_method/ime_controller_client_impl.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_util_impl.h"
#include "chrome/browser/ui/ash/management_disclosure/management_disclosure_client_impl.h"
#include "chrome/browser/ui/ash/media_client/media_client_impl.h"
#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"
#include "chrome/browser/ui/ash/network/network_connect_delegate.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/picker/picker_client_impl.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/ash/projector/projector_client_impl.h"
#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/shelf/app_service/exo_app_type_resolver.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shell_init/ash_shell_init.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/ash/web_view/ash_web_view_factory_impl.h"
#include "chrome/browser/ui/ash/wm/tab_cluster_ui_client.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager_impl.h"
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension_factory.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "chromeos/ash/components/game_mode/game_mode_controller.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/heatmap/heatmap_palm_detector_impl.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ozone/evdev/heatmap_palm_detector.h"

namespace {
ChromeBrowserMainExtraPartsAsh* g_instance = nullptr;
}  // namespace

using GameMode = ash::ResourcedClient::GameMode;

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
    if (!chrome_shelf_controller_) {
      session_manager::SessionManager::Get()->RemoveObserver(this);
    }
  }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override {
    TRACE_EVENT0("ui",
                 "ChromeShelfControllerInitializer::OnSessionStateChanged");
    DCHECK(!chrome_shelf_controller_);
    DCHECK(!ChromeShelfController::instance());

    if (session_manager::SessionManager::Get()->session_state() ==
        session_manager::SessionState::ACTIVE) {
      chrome_shelf_controller_ = std::make_unique<ChromeShelfController>(
          nullptr, ash::ShelfModel::Get());
      chrome_shelf_controller_->Init();

      session_manager::SessionManager::Get()->RemoveObserver(this);
    }
  }

 private:
  std::unique_ptr<ChromeShelfController> chrome_shelf_controller_;
};

}  // namespace internal

// static
ChromeBrowserMainExtraPartsAsh* ChromeBrowserMainExtraPartsAsh::Get() {
  return g_instance;
}

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
  CHECK(!g_instance);
  g_instance = this;
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void ChromeBrowserMainExtraPartsAsh::PreCreateMainMessageLoop() {
  user_profile_loaded_observer_ = std::make_unique<UserProfileLoadedObserver>();
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (base::FeatureList::IsEnabled(arc::kEnableArcIdleManager) ||
      base::FeatureList::IsEnabled(arc::kVmmSwapPolicy)) {
    // Early init so that later objects can rely on this one.
    arc_window_watcher_ = std::make_unique<ash::ArcWindowWatcher>();
  }

  // NetworkConnect handles the network connection state machine for the UI.
  network_connect_delegate_ = std::make_unique<NetworkConnectDelegate>();
  ash::NetworkConnect::Initialize(network_connect_delegate_.get());

  cast_config_controller_media_router_ =
      std::make_unique<CastConfigControllerMediaRouter>();

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
  ash::Shell::Get()
      ->login_unlock_throughput_recorder()
      ->post_login_deferred_task_runner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &session_manager::SessionManager::
                         HandleUserSessionStartUpTaskCompleted,
                     // Safe because SessionManager singleton will be destroyed
                     // after message loops stops.
                     base::Unretained(session_manager::SessionManager::Get())));

  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateChromeos>();

  app_list_client_ = std::make_unique<AppListClientImpl>();

  // Must be available at login screen, so initialize before profile.
  accessibility_controller_client_ =
      std::make_unique<AccessibilityControllerClient>();

  chrome_new_window_client_ = std::make_unique<ChromeNewWindowClient>();

  arc_open_url_delegate_impl_ = std::make_unique<ArcOpenUrlDelegateImpl>();

  ime_controller_client_ = std::make_unique<ImeControllerClientImpl>(
      ash::input_method::InputMethodManager::Get());
  ime_controller_client_->Init();

  in_session_auth_dialog_client_ =
      std::make_unique<InSessionAuthDialogClient>();

  in_session_auth_token_provider_ =
      std::make_unique<ash::InSessionAuthTokenProviderImpl>();

  active_session_fingerprint_client_ =
      std::make_unique<ash::ActiveSessionFingerprintClientImpl>();

  // NOTE: The WallpaperControllerClientImpl must be initialized before the
  // session controller, because the session controller triggers the loading
  // of users, which itself calls a code path which eventually reaches the
  // WallpaperControllerClientImpl singleton instance via
  // user_manager::UserManagerImpl.
  wallpaper_controller_client_ =
      std::make_unique<WallpaperControllerClientImpl>(
          std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  wallpaper_controller_client_->Init();

  session_controller_client_ = std::make_unique<SessionControllerClientImpl>();
  session_controller_client_->Init();
  // By this point ash shell should have initialized its D-Bus signal
  // listeners, so inform the session manager that Ash is initialized.
  session_controller_client_->EmitAshInitialized();

  system_tray_client_ = std::make_unique<SystemTrayClientImpl>();
  network_connect_delegate_->SetSystemTrayClient(system_tray_client_.get());

  if (ash::features::IsBirchCoralEnabled()) {
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

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectFileDialogExtensionFactory>());

  exo_parts_ = ExoParts::CreateIfNecessary();
  if (exo_parts_) {
    exo::WMHelper::GetInstance()->RegisterAppPropertyResolver(
        std::make_unique<ExoAppTypeResolver>());
  }

  // Result is unused, but `TimezoneResolverManager` must be created here for
  // its internal initialization to succeed.
  g_browser_process->platform_part()->GetTimezoneResolverManager();

  annotator_client_ = std::make_unique<AnnotatorClientImpl>();

  if (ash::boca_util::IsEnabled()) {
    boca_client_ = std::make_unique<ash::boca::BocaAppClientImpl>();
  }

  projector_app_client_ = std::make_unique<ProjectorAppClientImpl>();
  projector_client_ = std::make_unique<ProjectorClientImpl>();

  desks_client_ = std::make_unique<DesksClient>();

  attestation_cleanup_manager_ =
      std::make_unique<enterprise_connectors::AshAttestationCleanupManager>();

  if (ash::features::IsGrowthCampaignsInDemoModeEnabled() ||
      ash::features::IsGrowthCampaignsInConsumerSessionEnabled()) {
    campaigns_manager_client_ = std::make_unique<CampaignsManagerClientImpl>();
  }

  // Requires UserManager.
  if (ash::features::IsGrowthCampaignsInConsumerSessionEnabled()) {
    campaigns_manager_session_ = std::make_unique<CampaignsManagerSession>();
  }

  ash::bluetooth_config::FastPairDelegate* delegate =
      ash::features::IsFastPairEnabled()
          ? ash::Shell::Get()->quick_pair_mediator()->GetFastPairDelegate()
          : nullptr;

  ash::bluetooth_config::Initialize(delegate);

  // Create GeolocationSystemPermissionManager.
  device::GeolocationSystemPermissionManager::SetInstance(
      ash::SystemGeolocationSource::
          CreateGeolocationSystemPermissionManagerOnAsh());

  ui::HeatmapPalmDetector::SetInstance(
      std::make_unique<ash::HeatmapPalmDetectorImpl>());

  // Required by `read_write_cards_manager_` and
  // `mahi_media_app_content_manager_`.
  mahi_media_app_events_proxy_ =
      std::make_unique<ash::MahiMediaAppEventsProxyImpl>();

  mahi_media_app_content_manager_ =
      std::make_unique<ash::MahiMediaAppContentManagerImpl>();

  // Needs to be initialized before `read_write_cards_manager_`. This is because
  // `QuickAnswersState` needs `MagicBoostState` to be initialized before it is
  // constructed.
  magic_boost_state_ash_ = std::make_unique<ash::MagicBoostStateAsh>();

  read_write_cards_manager_ =
      std::make_unique<chromeos::ReadWriteCardsManagerImpl>();
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit(Profile* profile,
                                                     bool is_initial_profile) {
  // The setup below is intended to run for only the initial profile.
  if (!is_initial_profile) {
    return;
  }

  login_screen_client_ = std::make_unique<LoginScreenClientImpl>();
  management_disclosure_client_ =
      std::make_unique<ManagementDisclosureClientImpl>(
          g_browser_process->platform_part()->browser_policy_connector_ash(),
          Profile::FromBrowserContext(
              ash::BrowserContextHelper::Get()->GetSigninBrowserContext())
              ->GetOriginalProfile());
  // https://crbug.com/884127 ensuring that LoginScreenClientImpl is initialized
  // before using it InitializeDeviceDisablingManager.
  g_browser_process->platform_part()->InitializeDeviceDisablingManager();

  media_client_ = std::make_unique<MediaClientImpl>();
  media_client_->Init();

  // Passes (and continues passing) the current camera count to the PrivacyHub.
  ash::privacy_hub_util::SetUpCameraCountObserver();

  app_access_notifier_ = std::make_unique<AppAccessNotifier>();
  ash::privacy_hub_util::SetAppAccessNotifier(app_access_notifier_.get());

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

  if (auto* picker_controller = ash::Shell::Get()->picker_controller()) {
    picker_client_ = std::make_unique<PickerClientImpl>(
        picker_controller, user_manager::UserManager::Get());
  }

  oobe_dialog_util_ = std::make_unique<ash::OobeDialogUtilImpl>();

  game_mode_controller_ = std::make_unique<game_mode::GameModeController>();

  game_mode_controller_->set_game_mode_changed_callback(
      base::BindRepeating([](aura::Window* window, GameMode game_mode) {
        ash::Shell::Get()->refresh_rate_controller()->SetGameMode(
            window, game_mode == GameMode::BOREALIS);
      }));

  if (ash::features::IsGraduationEnabled()) {
    graduation_manager_ =
        std::make_unique<ash::graduation::GraduationManagerImpl>();
  }

  if (ash::features::IsWelcomeExperienceEnabled()) {
    peripherals_app_delegate_ =
        std::make_unique<ash::PeripheralsAppDelegateImpl>();
    ash::Shell::Get()
        ->input_device_settings_controller()
        ->SetPeripheralsAppDelegate(peripherals_app_delegate_.get());
  }

  // Initialize TabScrubberChromeOS after the Ash Shell has been initialized.
  TabScrubberChromeOS::GetInstance();
}

void ChromeBrowserMainExtraPartsAsh::PostBrowserStart() {
  mobile_data_notifications_ = std::make_unique<MobileDataNotifications>();

  if (chromeos::features::IsMahiEnabled() &&
      !chromeos::features::IsSparkyEnabled()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kUseFakeMahiManager)) {
      mahi_manager_ = std::make_unique<ash::FakeMahiManager>();
    } else {
      mahi_manager_ = std::make_unique<ash::MahiManagerImpl>();
    }
  }
  CheckIfSanitizeCompleted();

  did_post_browser_start_ = true;
  if (post_browser_start_callback_) {
    std::move(post_browser_start_callback_).Run();
  }
}

void ChromeBrowserMainExtraPartsAsh::CheckIfSanitizeCompleted() {
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  if (base::FeatureList::IsEnabled(ash::features::kSanitize) &&
      prefs->GetBoolean(ash::settings::prefs::kSanitizeCompleted)) {
    prefs->SetBoolean(ash::settings::prefs::kSanitizeCompleted, false);
    prefs->CommitPendingWrite();

    // Blocks full restore UI from showing up. `FullRestoreService` object is
    // not created yet, so we use this static function.
    ash::full_restore::FullRestoreService::SetLastSessionSanitized();

    ash::SystemAppLaunchParams params;
    params.url = GURL(base::StrCat({ash::kChromeUISanitizeAppURL, "?done"}));
    params.launch_source = apps::LaunchSource::kUnknown;
    ash::LaunchSystemWebAppAsync(ProfileManager::GetPrimaryUserProfile(),
                                 ash::SystemWebAppType::OS_SANITIZE, params);
  }
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  ash::bluetooth_config::Shutdown();

  // Disable event dispatch before Exo starts closing windows to prevent
  // synthetic events from being dispatched. crbug.com/874156 and
  // crbug.com/1163269.
  ash::Shell::Get()->ShutdownEventDispatch();

  // ExoParts uses state from ash, delete it before ash so that exo can
  // uninstall correctly.
  exo_parts_.reset();

  mahi_manager_.reset();
  mobile_data_notifications_.reset();
  chrome_shelf_controller_initializer_.reset();
  attestation_cleanup_manager_.reset();

  campaigns_manager_session_.reset();
  campaigns_manager_client_.reset();

  desks_client_.reset();

  projector_client_.reset();
  projector_app_client_.reset();
  annotator_client_.reset();

  wallpaper_controller_client_.reset();
  vpn_list_forwarder_.reset();

  tab_cluster_ui_client_.reset();

  // Initialized in PostProfileInit (which may not get called in some tests).
  game_mode_controller_.reset();
  oobe_dialog_util_.reset();
  picker_client_.reset();
  ash_web_view_factory_.reset();
  network_portal_notification_controller_.reset();
  display_settings_handler_.reset();
  media_client_.reset();
  login_screen_client_.reset();
  management_disclosure_client_.reset();
  graduation_manager_.reset();

  ash::privacy_hub_util::SetAppAccessNotifier(nullptr);
  app_access_notifier_.reset();

  // Initialized in PreProfileInit (which may not get called in some tests).
  device::GeolocationSystemPermissionManager::SetInstance(nullptr);
  system_tray_client_.reset();
  session_controller_client_.reset();
  ime_controller_client_.reset();
  in_session_auth_dialog_client_.reset();
  arc_open_url_delegate_impl_.reset();
  chrome_new_window_client_.reset();
  accessibility_controller_client_.reset();
  // AppListClientImpl indirectly holds WebContents for answer card and
  // needs to be released before destroying the profile.
  app_list_client_.reset();
  ash_shell_init_.reset();

  // These instances must be destructed after `ash_shell_init_`.
  video_conference_tray_controller_.reset();
  read_write_cards_manager_.reset();

  // Must be destructed after `read_write_cards_manager_`.
  magic_boost_state_ash_.reset();

  mahi_media_app_content_manager_.reset();
  mahi_media_app_events_proxy_.reset();

  cast_config_controller_media_router_.reset();
  if (ash::NetworkConnect::IsInitialized()) {
    ash::NetworkConnect::Shutdown();
  }
  network_connect_delegate_.reset();
  user_profile_loaded_observer_.reset();
  arc_window_watcher_.reset();
}

void ChromeBrowserMainExtraPartsAsh::ResetChromeNewWindowClientForTesting() {
  chrome_new_window_client_.reset();
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
