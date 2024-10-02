// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MAIN_EXTRA_PARTS_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
#define CHROME_BROWSER_UI_ASH_MAIN_EXTRA_PARTS_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/buildflags.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"

namespace ash {
class ArcWindowWatcher;
class ActiveSessionFingerprintClient;
class InSessionAuthTokenProviderImpl;
class MagicBoostStateAsh;
class NetworkPortalNotificationController;
class OobeDialogUtil;
class PeripheralsAppDelegateImpl;
class VideoConferenceTrayController;

namespace boca {
class BocaAppClientImpl;
}

namespace graduation {
class GraduationManager;
}

}  // namespace ash

namespace chromeos {
class MahiManager;
class MahiMediaAppEventsProxy;
class MahiMediaAppContentManager;
class ReadWriteCardsManager;
}  // namespace chromeos

namespace enterprise_connectors {
class AshAttestationCleanupManager;
}

namespace game_mode {
class GameModeController;
}

namespace policy {
class DisplaySettingsHandler;
}

class AccessibilityControllerClient;
class AppAccessNotifier;
class AppListClientImpl;
class ArcOpenUrlDelegateImpl;
class AshShellInit;
class AshWebViewFactoryImpl;
class CampaignsManagerClientImpl;
class CampaignsManagerSession;
class CastConfigControllerMediaRouter;
class ChromeNewWindowClient;
class DesksClient;
class ExoParts;
class ImeControllerClientImpl;
class InSessionAuthDialogClient;
class LoginScreenClientImpl;
class ManagementDisclosureClientImpl;
class MediaClientImpl;
class MobileDataNotifications;
class NetworkConnectDelegate;
class PickerClientImpl;
class ProjectorAppClientImpl;
class ProjectorClientImpl;
class AnnotatorClientImpl;
class ScreenOrientationDelegateChromeos;
class SessionControllerClientImpl;
class SystemTrayClientImpl;
class TabClusterUIClient;
class TabletModePageBehavior;
class VpnListForwarder;
class WallpaperControllerClientImpl;

namespace internal {
class ChromeShelfControllerInitializer;
}

// Browser initialization for Ash UI. Only use this for Ash specific
// initialization (e.g. initialization of chrome/browser/ui/ash classes).
class ChromeBrowserMainExtraPartsAsh : public ChromeBrowserMainExtraParts {
 public:
  // Returns the single instance. Returns null early in startup and late in
  // shutdown.
  static ChromeBrowserMainExtraPartsAsh* Get();

  ChromeBrowserMainExtraPartsAsh();

  ChromeBrowserMainExtraPartsAsh(const ChromeBrowserMainExtraPartsAsh&) =
      delete;
  ChromeBrowserMainExtraPartsAsh& operator=(
      const ChromeBrowserMainExtraPartsAsh&) = delete;

  ~ChromeBrowserMainExtraPartsAsh() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreCreateMainMessageLoop() override;
  void PreProfileInit() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

  void set_post_browser_start_callback(base::OnceClosure callback) {
    post_browser_start_callback_ = std::move(callback);
  }

  bool did_post_browser_start() const { return did_post_browser_start_; }

  void ResetChromeNewWindowClientForTesting();

 private:
  class UserProfileLoadedObserver;

  std::unique_ptr<UserProfileLoadedObserver> user_profile_loaded_observer_;

  // Initialized in PreProfileInit in all configs before Shell init:
  std::unique_ptr<NetworkConnectDelegate> network_connect_delegate_;
  std::unique_ptr<CastConfigControllerMediaRouter>
      cast_config_controller_media_router_;

  // Initialized in PreProfileInit if ash config != MASH:
  std::unique_ptr<AshShellInit> ash_shell_init_;

  // Initialized in PreProfileInit in all configs after Shell init:
  std::unique_ptr<AccessibilityControllerClient>
      accessibility_controller_client_;
  std::unique_ptr<AppListClientImpl> app_list_client_;
  std::unique_ptr<ChromeNewWindowClient> chrome_new_window_client_;
  std::unique_ptr<ash::ArcWindowWatcher> arc_window_watcher_;
  std::unique_ptr<ArcOpenUrlDelegateImpl> arc_open_url_delegate_impl_;
  std::unique_ptr<ash::boca::BocaAppClientImpl> boca_client_;
  std::unique_ptr<ImeControllerClientImpl> ime_controller_client_;
  std::unique_ptr<InSessionAuthDialogClient> in_session_auth_dialog_client_;
  std::unique_ptr<ash::ActiveSessionFingerprintClient>
      active_session_fingerprint_client_;
  std::unique_ptr<ash::InSessionAuthTokenProviderImpl>
      in_session_auth_token_provider_;
  std::unique_ptr<ScreenOrientationDelegateChromeos>
      screen_orientation_delegate_;
  std::unique_ptr<SessionControllerClientImpl> session_controller_client_;
  std::unique_ptr<SystemTrayClientImpl> system_tray_client_;
  std::unique_ptr<TabClusterUIClient> tab_cluster_ui_client_;
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;
  std::unique_ptr<VpnListForwarder> vpn_list_forwarder_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  std::unique_ptr<ProjectorClientImpl> projector_client_;
  std::unique_ptr<ProjectorAppClientImpl> projector_app_client_;
  std::unique_ptr<AnnotatorClientImpl> annotator_client_;
  std::unique_ptr<game_mode::GameModeController> game_mode_controller_;
  std::unique_ptr<ash::NetworkPortalNotificationController>
      network_portal_notification_controller_;
  std::unique_ptr<ash::VideoConferenceTrayController>
      video_conference_tray_controller_;
  std::unique_ptr<enterprise_connectors::AshAttestationCleanupManager>
      attestation_cleanup_manager_;
  std::unique_ptr<ash::MagicBoostStateAsh> magic_boost_state_ash_;
  std::unique_ptr<chromeos::MahiManager> mahi_manager_;
  std::unique_ptr<chromeos::MahiMediaAppEventsProxy>
      mahi_media_app_events_proxy_;
  std::unique_ptr<chromeos::MahiMediaAppContentManager>
      mahi_media_app_content_manager_;

  std::unique_ptr<internal::ChromeShelfControllerInitializer>
      chrome_shelf_controller_initializer_;
  std::unique_ptr<DesksClient> desks_client_;
  std::unique_ptr<CampaignsManagerClientImpl> campaigns_manager_client_;
  std::unique_ptr<CampaignsManagerSession> campaigns_manager_session_;
  std::unique_ptr<ash::PeripheralsAppDelegateImpl> peripherals_app_delegate_;

  std::unique_ptr<ExoParts> exo_parts_;

  // Initialized in PostProfileInit in all configs:
  std::unique_ptr<LoginScreenClientImpl> login_screen_client_;
  std::unique_ptr<ManagementDisclosureClientImpl> management_disclosure_client_;
  std::unique_ptr<MediaClientImpl> media_client_;
  std::unique_ptr<AppAccessNotifier> app_access_notifier_;
  std::unique_ptr<policy::DisplaySettingsHandler> display_settings_handler_;
  std::unique_ptr<AshWebViewFactoryImpl> ash_web_view_factory_;
  std::unique_ptr<PickerClientImpl> picker_client_;
  std::unique_ptr<ash::OobeDialogUtil> oobe_dialog_util_;
  std::unique_ptr<chromeos::ReadWriteCardsManager> read_write_cards_manager_;
  std::unique_ptr<ash::graduation::GraduationManager> graduation_manager_;

  // Initialized in PostBrowserStart in all configs:
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;

  // Boolean that is set to true after PostBrowserStart() executes.
  bool did_post_browser_start_ = false;

  // Callback invoked at the end of PostBrowserStart().
  base::OnceClosure post_browser_start_callback_;

  // Once Sanitize is completed, ash is restarted. After ash has restarted, we
  // should check if the restart has happened right after a sanitize. If that is
  // the case, sanitize done dialog should be shown to the user.
  void CheckIfSanitizeCompleted();
};

#endif  // CHROME_BROWSER_UI_ASH_MAIN_EXTRA_PARTS_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
