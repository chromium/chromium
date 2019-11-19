// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
#define CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/buildflags.h"

namespace chromeos {
class NetworkPortalNotificationController;
}

namespace policy {
class DisplaySettingsHandler;
}

class AccessibilityControllerClient;
class AppListClientImpl;
class ArcChromeActionsClient;
class AshShellInit;
class CastConfigControllerMediaRouter;
class ChromeNewWindowClient;
class ImeControllerClient;
class LoginScreenClient;
class MediaClientImpl;
class MobileDataNotifications;
class NetworkConnectDelegateChromeOS;
class NightLightClient;
class PhotoControllerImpl;
class ScreenOrientationDelegateChromeos;
class SessionControllerClientImpl;
class SystemTrayClient;
class TabletModePageBehavior;
class VpnListForwarder;
class WallpaperControllerClient;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
class ExoParts;
#endif

namespace internal {
class ChromeLauncherControllerInitializer;
}

// Browser initialization for Ash UI. Only use this for Ash specific
// intitialization (e.g. initialization of chrome/browser/ui/ash classes).
class ChromeBrowserMainExtraPartsAsh : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAsh();
  ~ChromeBrowserMainExtraPartsAsh() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

 private:
  class NotificationObserver;

  std::unique_ptr<NotificationObserver> notification_observer_;

  // Initialized in PreProfileInit in all configs before Shell init:
  std::unique_ptr<NetworkConnectDelegateChromeOS> network_connect_delegate_;
  std::unique_ptr<CastConfigControllerMediaRouter>
      cast_config_controller_media_router_;

  // Initialized in PreProfileInit if ash config != MASH:
  std::unique_ptr<AshShellInit> ash_shell_init_;

  // Initialized in PreProfileInit in all configs after Shell init:
  std::unique_ptr<AccessibilityControllerClient>
      accessibility_controller_client_;
  std::unique_ptr<AppListClientImpl> app_list_client_;
  std::unique_ptr<ArcChromeActionsClient> arc_chrome_actions_client_;
  std::unique_ptr<ChromeNewWindowClient> chrome_new_window_client_;
  std::unique_ptr<ImeControllerClient> ime_controller_client_;
  std::unique_ptr<ScreenOrientationDelegateChromeos>
      screen_orientation_delegate_;
  std::unique_ptr<SessionControllerClientImpl> session_controller_client_;
  std::unique_ptr<SystemTrayClient> system_tray_client_;
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;
  std::unique_ptr<VpnListForwarder> vpn_list_forwarder_;
  std::unique_ptr<WallpaperControllerClient> wallpaper_controller_client_;
  // TODO(stevenjb): Move NetworkPortalNotificationController to c/b/ui/ash and
  // elim chromeos:: namespace. https://crbug.com/798569.
  std::unique_ptr<chromeos::NetworkPortalNotificationController>
      network_portal_notification_controller_;

  std::unique_ptr<internal::ChromeLauncherControllerInitializer>
      chrome_launcher_controller_initializer_;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  std::unique_ptr<ExoParts> exo_parts_;
#endif

  // Initialized in PostProfileInit in all configs:
  std::unique_ptr<LoginScreenClient> login_screen_client_;
  std::unique_ptr<MediaClientImpl> media_client_;
  std::unique_ptr<policy::DisplaySettingsHandler> display_settings_handler_;

  // Initialized in PostBrowserStart in all configs:
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;
  std::unique_ptr<NightLightClient> night_light_client_;
  std::unique_ptr<PhotoControllerImpl> photo_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
