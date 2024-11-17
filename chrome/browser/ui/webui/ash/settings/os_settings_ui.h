// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_UI_H_

#include <memory>

#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom-forward.h"
#include "ash/webui/common/mojom/accelerator_fetcher.mojom.h"
#include "ash/webui/common/mojom/shortcut_input_provider.mojom.h"
#include "ash/webui/personalization_app/search/search.mojom-forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_notification_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/date_time_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/google_drive_page_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/one_drive_page_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/mojom/graduation_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/mojom/app_permission_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/magic_boost_notice_page_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/mojom/magic_boost_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/user_action_recorder.mojom-forward.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-forward.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-forward.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-forward.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom-forward.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash::settings {

namespace mojom {
class SearchHandler;
}  // namespace mojom

class OSSettingsUI;

// WebUIConfig for chrome://os-settings
//
// Even though OSSettings is a System Web App, it is used in profiles where SWAs
// are not installed (e.g. kiosk mode) so it can't use
// ash::SystemWebAppUIConfig.
class OSSettingsUIConfig : public content::DefaultWebUIConfig<OSSettingsUI> {
 public:
  OSSettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIOSSettingsHost) {}
};

// The WebUI handler for chrome://os-settings.
class OSSettingsUI : public ui::MojoWebUIController {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit OSSettingsUI(content::WebUI* web_ui);

  OSSettingsUI(const OSSettingsUI&) = delete;
  OSSettingsUI& operator=(const OSSettingsUI&) = delete;

  ~OSSettingsUI() override;

  // Instantiates implementor of the mojom::CellularSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::CellularSetup> receiver);

  // Instantiates implementor of the mojom::ESimManager mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver);

  // Instantiates implementor of the mojom::CrosAudioConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<audio_config::mojom::CrosAudioConfig> receiver);

  // Instantiates implementor of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  // Instantiates implementor of the mojom::UserActionRecorder mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::UserActionRecorder> receiver);

  // Instantiates implementor of the mojom::SearchHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::SearchHandler> receiver);

  // Instantiates implementor of the personalization app mojom::SearchHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::SearchHandler>
          receiver);

  // Instantiates implementor of the mojom::AppNotificationsHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
          receiver);

  // Instantiates implementor of the mojom::AppParentalControlsHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          app_parental_controls::mojom::AppParentalControlsHandler> receiver);

  // Instantiates implementor of the mojom::AppPermissionsHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_permission::mojom::AppPermissionsHandler>
          receiver);

  // Instantiates implementor of the mojom::GraduationHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<graduation::mojom::GraduationHandler> receiver);

  // Instantiates implementor of the mojom::InputDeviceSettingsProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver);

  // Instantiates implementor of the mojom::DisplaySettingsProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::DisplaySettingsProvider> receiver);

  // Instantiates implementor of the mojom::AcceleratorFetcher mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<::ash::common::mojom::AcceleratorFetcher> receiver);

  // Instantiates implementor of the mojom::ShortcutInputProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<::ash::common::mojom::ShortcutInputProvider>
          receiver);

  // Instantiates implementor of the mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
          receiver);

  // Binds to the existing settings instance owned by the nearby share keyed
  // service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver);

  // Creates and binds a new receive manager.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ReceiveManager> receiver);

  // Binds to the existing contacts manager instance owned by the nearby share
  // keyed service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver);

  // Instantiates implementor of the mojom::CrosBluetoothConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<bluetooth_config::mojom::CrosBluetoothConfig>
          receiver);

  // Instantiates implementor of the mojom::CrosHotspotConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<hotspot_config::mojom::CrosHotspotConfig> receiver);

  // Binds to the cros authentication factor editing services.
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::AuthFactorConfig> receiver);
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::RecoveryFactorEditor> receiver);
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::PinFactorEditor> receiver);
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::PasswordFactorEditor> receiver);

  // Binds to the in session auth service, for authenticating sensitive
  // operations.
  void BindInterface(
      mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver);

  // Binds to the Jelly dynamic color Mojo
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Binds to the Google Drive page handler mojo.
  void BindInterface(
      mojo::PendingReceiver<google_drive::mojom::PageHandlerFactory> receiver);

  // Binds to the OneDrive page handler mojo.
  void BindInterface(
      mojo::PendingReceiver<one_drive::mojom::PageHandlerFactory> receiver);

  // Binds to the cros Passpoint service.
  void BindInterface(
      mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
          receiver);

  // Binds HidPreservingBluetoothStateController service.
  void BindInterface(
      mojo::PendingReceiver<ash::mojom::HidPreservingBluetoothStateController>
          receiver);

  // Binds InputMethodUserDataService service.
  void BindInterface(
      mojo::PendingReceiver<ash::ime::mojom::InputMethodUserDataService>
          receiver);

  // Binds to the DateTimeHandler mojo.
  void BindInterface(
      mojo::PendingReceiver<date_time::mojom::PageHandlerFactory> receiver);

  // Binds to the MagicBoostNoticePageHandler mojo.
  void BindInterface(
      mojo::PendingReceiver<magic_boost_handler::mojom::PageHandlerFactory>
          receiver);

 private:
  base::TimeTicks time_when_opened_;

  WebuiLoadTimer webui_load_timer_;

  std::unique_ptr<mojom::UserActionRecorder> user_action_recorder_;
  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;
  std::unique_ptr<GoogleDrivePageHandlerFactory>
      google_drive_page_handler_factory_;
  std::unique_ptr<OneDrivePageHandlerFactory> one_drive_page_handler_factory_;
  std::unique_ptr<DateTimeHandlerFactory> date_time_handler_factory_;
  std::unique_ptr<MagicBoostNoticePageHandlerFactory>
      magic_boost_notice_page_handler_factory_;

  // This handler notifies the WebUI when the color provider changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_UI_H_
