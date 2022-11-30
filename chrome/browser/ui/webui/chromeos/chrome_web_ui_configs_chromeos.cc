// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/chrome_untrusted_web_ui_configs_chromeos.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/color_internals/color_internals_ui.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/system_extensions_internals_ui/system_extensions_internals_ui.h"
#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_manager_error_ui.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing.h"
#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_ui.h"
#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_ui.h"
#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_ui.h"
#include "chrome/browser/ui/webui/ash/audio/audio_ui.h"
#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/certificate_manager_dialog_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/ash/cryptohome_ui.h"
#include "chrome/browser/ui/webui/ash/drive_internals_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/human_presence_internals_ui.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_ui.h"
#include "chrome/browser/ui/webui/ash/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/network_ui.h"
#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_ui.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/power_ui.h"
#include "chrome/browser/ui/webui/ash/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/slow_trace_ui.h"
#include "chrome/browser/ui/webui/ash/slow_ui.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"
#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_ui.h"
#include "chrome/browser/ui/webui/ash/vm/vm_ui.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_ui.h"
#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#if !defined(USE_REAL_DBUS_CLIENTS)
#include "chrome/browser/ui/webui/ash/emulator/device_emulator_ui.h"
#endif  // !defined(USE_REAL_DBUS_CLIENTS)
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace {
using CreateWebUIControllerFunc =
    std::unique_ptr<content::WebUIController> (*)(content::WebUI*);

template <class Config, class Controller, class Delegate>
std::unique_ptr<content::WebUIConfig> MakeComponentConfig() {
  CreateWebUIControllerFunc create_controller_func =
      [](content::WebUI* web_ui) -> std::unique_ptr<content::WebUIController> {
    auto delegate = std::make_unique<Delegate>(web_ui);
    return std::make_unique<Controller>(web_ui, std::move(delegate));
  };

  return std::make_unique<Config>(create_controller_func);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterAshChromeWebUIConfigs() {
  // Add `WebUIConfig`s for Ash ChromeOS to the list here.
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(
      MakeComponentConfig<ash::CameraAppUIConfig, ash::CameraAppUI,
                          ChromeCameraAppUIDelegate>());
  map.AddWebUIConfig(std::make_unique<ash::AccountManagerErrorUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AccountMigrationWelcomeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AddSupervisionUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ArcGraphicsTracingUIConfig<
                         ash::ArcGraphicsTracingMode::kFull>>());
  map.AddWebUIConfig(std::make_unique<ash::ArcGraphicsTracingUIConfig<
                         ash::ArcGraphicsTracingMode::kOverview>>());
  map.AddWebUIConfig(std::make_unique<ash::ArcPowerControlUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AssistantOptInUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AudioUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::BluetoothPairingDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CertificateManagerDialogUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::cloud_upload::CloudUploadUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ColorInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ConfirmPasswordChangeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CrostiniInstallerUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CrostiniUpgraderUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CryptohomeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::DriveInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::EmojiUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::FirmwareUpdateAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::HumanPresenceInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::InternetConfigDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::InternetDetailDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::LauncherInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::LockScreenNetworkUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::LockScreenStartReauthUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ManageMirrorSyncUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::MultideviceInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<
                     ash::multidevice_setup::MultiDeviceSetupDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<NearbyInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<nearby_share::NearbyShareDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::NetworkUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::NotificationTesterUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::office_fallback::OfficeFallbackUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::settings::OSSettingsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ParentAccessUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::PasswordChangeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::PowerUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::SetTimeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::ShortcutCustomizationAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::SlowTraceControllerConfig>());
  map.AddWebUIConfig(std::make_unique<ash::SlowUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::smb_dialog::SmbCredentialsDialogUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::smb_dialog::SmbShareDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::SysInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::SystemExtensionsInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<ash::UrgentPasswordExpiryNotificationUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::VmUIConfig>());
#if !defined(OFFICIAL_BUILD)
  map.AddWebUIConfig(std::make_unique<ash::SampleSystemWebAppUIConfig>());
#if !defined(USE_REAL_DBUS_CLIENTS)
  map.AddWebUIConfig(std::make_unique<ash::DeviceEmulatorUIConfig>());
#endif  // !defined(USE_REAL_DBUS_CLIENTS)
#endif  // !defined(OFFICIAL_BUILD)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

void RegisterChromeOSChromeWebUIConfigs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterAshChromeWebUIConfigs();
#endif
}
