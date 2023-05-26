// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_UI_H_

#include <memory>

#include "ash/webui/personalization_app/search/search.mojom-forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_factory.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/google_drive_page_handler_factory.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/google_drive_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/one_drive_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/one_drive_page_handler_factory.h"
#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_notification_handler.mojom-forward.h"
#include "chrome/browser/ui/webui/settings/ash/search/user_action_recorder.mojom-forward.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-forward.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-forward.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-forward.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
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

  // Instantiates implementor of the mojom::InputDeviceSettingsProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver);

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

 private:
  base::TimeTicks time_when_opened_;

  WebuiLoadTimer webui_load_timer_;

  std::unique_ptr<mojom::UserActionRecorder> user_action_recorder_;
  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;
  std::unique_ptr<GoogleDrivePageHandlerFactory>
      google_drive_page_handler_factory_;
  std::unique_ptr<OneDrivePageHandlerFactory> one_drive_page_handler_factory_;

  // This handler notifies the WebUI when the color provider changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_UI_H_
