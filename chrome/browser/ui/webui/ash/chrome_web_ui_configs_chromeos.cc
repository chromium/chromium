// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/chrome_untrusted_web_ui_configs_chromeos.h"

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"
#include "url/gurl.h"

// Headers that are part of the //chrome/browser target.
//
// Depending on //chrome/browser would cause a circular dependency because it
// depends on //chrome/browser/ui which depends on this file's target. So we
// suppress gn warnings.
#include "chrome/browser/app_mode/app_mode_utils.h"         // nogncheck
#include "chrome/browser/feedback/feedback_dialog_utils.h"  // nogncheck

#include "ash/constants/ash_features.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/color_internals/color_internals_ui.h"
#include "ash/webui/connectivity_diagnostics/connectivity_diagnostics_ui.h"
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/eche_app_ui/eche_app_ui.h"
#include "ash/webui/face_ml_app_ui/face_ml_app_ui.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "ash/webui/files_internals/files_internals_ui.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/guest_os_installer/guest_os_installer_ui.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/system_extensions_internals_ui/system_extensions_internals_ui.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/guest_os/public/installer_delegate_factory.h"
#include "chrome/browser/ash/multidevice_debug/proximity_auth_ui_config.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/chrome_file_manager_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/face_ml/chrome_face_ml_user_provider.h"
#include "chrome/browser/ash/system_web_apps/apps/files_internals_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/chrome_media_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_manager_error_ui.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_ui.h"
#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_ui.h"
#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_ui.h"
#include "chrome/browser/ui/webui/ash/audio/audio_ui.h"
#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_ui.h"
#include "chrome/browser/ui/webui/ash/certificate_manager_dialog_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/ash/cryptohome_ui.h"
#include "chrome/browser/ui/webui/ash/drive_internals_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"
#include "chrome/browser/ui/webui/ash/healthd_internals/healthd_internals_ui.h"
#include "chrome/browser/ui/webui/ash/human_presence_internals_ui.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_ui.h"
#include "chrome/browser/ui/webui/ash/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_ui.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/network_ui.h"
#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_ui.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/power_ui.h"
#include "chrome/browser/ui/webui/ash/remote_maintenance_curtain_ui.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor_info_ui.h"
#include "chrome/browser/ui/webui/ash/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/slow_trace_ui.h"
#include "chrome/browser/ui/webui/ash/slow_ui.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"
#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_ui.h"
#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_ui.h"
#include "chrome/browser/ui/webui/ash/vc_tray_tester/vc_tray_tester_ui.h"
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

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace ash {
using CreateWebUIControllerFunc = base::RepeatingCallback<std::unique_ptr<
    content::WebUIController>(content::WebUI*, const GURL& url)>;

// Consider the following approaches when registering component
// WebUIConfigs, in order of preference:
//   1. Using a Delegate with MakeComponentConfigWithDelegate,
//   2. Using a custom MakeConfig() method (least preferred, avoid if possible).

template <class Config, class Controller, class Delegate>
std::unique_ptr<content::WebUIConfig> MakeComponentConfigWithDelegate() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        auto delegate = std::make_unique<Delegate>(web_ui);
        return std::make_unique<Controller>(web_ui, std::move(delegate));
      });

  return std::make_unique<Config>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeConnectivityDiagnosticsUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        return std::make_unique<ConnectivityDiagnosticsUI>(
            web_ui,
            /* BindNetworkDiagnosticsServiceCallback */
            base::BindRepeating(&network_health::NetworkHealthManager::
                                    NetworkDiagnosticsServiceCallback),
            /* BindNetworkHealthServiceCallback */
            base::BindRepeating(&network_health::NetworkHealthManager::
                                    NetworkHealthServiceCallback),
            /* SendFeedbackReportCallback */
            base::BindRepeating(
                &chrome::ShowFeedbackDialogForWebUI,
                chrome::WebUIFeedbackSource::kConnectivityDiagnostics),
            /*show_feedback_button=*/
            !chrome::IsRunningInAppMode());
      });

  return std::make_unique<ConnectivityDiagnosticsUIConfig>(
      create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeDiagnosticsUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        HoldingSpaceKeyedService* holding_space_keyed_service =
            HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
                web_ui->GetWebContents()->GetBrowserContext());
        // This directory stores routine and network event logs for a given
        // |profile|.
        static constexpr base::FilePath::CharType
            kDiagnosticsLogDirectoryName[] = FILE_PATH_LITERAL("diagnostics");
        return std::make_unique<DiagnosticsDialogUI>(
            web_ui,
            base::BindRepeating([](content::WebContents* web_contents)
                                    -> std::unique_ptr<ui::SelectFilePolicy> {
              return std::make_unique<ChromeSelectFilePolicy>(web_contents);
            }),
            holding_space_keyed_service->client(),
            Profile::FromWebUI(web_ui)->GetPath().Append(
                kDiagnosticsLogDirectoryName));
      });

  return std::make_unique<DiagnosticsDialogUIConfig>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeEcheAppUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        Profile* profile = Profile::FromWebUI(web_ui);
        eche_app::EcheAppManager* manager =
            eche_app::EcheAppManagerFactory::GetForProfile(profile);
        return std::make_unique<eche_app::EcheAppUI>(web_ui, manager);
      });

  return std::make_unique<eche_app::EcheAppUIConfig>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeGuestOSInstallerUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        return std::make_unique<GuestOSInstallerUI>(
            web_ui, base::BindRepeating(&guest_os::InstallerDelegateFactory));
      });

  return std::make_unique<GuestOSInstallerUIConfig>(create_controller_func);
}

void RegisterAshChromeWebUIConfigs() {
  // Add `WebUIConfig`s for Ash ChromeOS to the list here.
  //
  // All `WebUIConfig`s should be registered here, irrespective of whether their
  // `WebUI` is enabled or not. To conditionally enable/disable a WebUI,
  // developers should override `WebUIConfig::IsWebUIEnabled()`.
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<CameraAppUIConfig, CameraAppUI,
                                      ChromeCameraAppUIDelegate>());
  map.AddWebUIConfig(std::make_unique<AccountManagerErrorUIConfig>());
  map.AddWebUIConfig(std::make_unique<AccountMigrationWelcomeUIConfig>());
  map.AddWebUIConfig(std::make_unique<AddSupervisionUIConfig>());
  map.AddWebUIConfig(std::make_unique<ArcGraphicsTracingUIConfig>());
  map.AddWebUIConfig(std::make_unique<ArcPowerControlUIConfig>());
  map.AddWebUIConfig(std::make_unique<AssistantOptInUIConfig>());
  map.AddWebUIConfig(std::make_unique<AudioUIConfig>());
  map.AddWebUIConfig(std::make_unique<BluetoothPairingDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<BorealisInstallerUIConfig>());
  map.AddWebUIConfig(std::make_unique<CertificateManagerDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<cloud_upload::CloudUploadUIConfig>());
  map.AddWebUIConfig(std::make_unique<ColorInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ConfirmPasswordChangeUIConfig>());
  map.AddWebUIConfig(MakeConnectivityDiagnosticsUIConfig());
  map.AddWebUIConfig(std::make_unique<CrostiniInstallerUIConfig>());
  map.AddWebUIConfig(std::make_unique<CrostiniUpgraderUIConfig>());
  map.AddWebUIConfig(std::make_unique<CryptohomeUIConfig>());
  map.AddWebUIConfig(MakeDiagnosticsUIConfig());
  map.AddWebUIConfig(std::make_unique<DriveInternalsUIConfig>());
  map.AddWebUIConfig(MakeEcheAppUIConfig());
  map.AddWebUIConfig(std::make_unique<SensorInfoUIConfig>());
  map.AddWebUIConfig(std::make_unique<EmojiUIConfig>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<FaceMLAppUIConfig, FaceMLAppUI,
                                      ChromeFaceMLUserProvider>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<FilesInternalsUIConfig, FilesInternalsUI,
                                      ChromeFilesInternalsUIDelegate>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<file_manager::FileManagerUIConfig,
                                      file_manager::FileManagerUI,
                                      ChromeFileManagerUIDelegate>());
  map.AddWebUIConfig(std::make_unique<FirmwareUpdateAppUIConfig>());
  map.AddWebUIConfig(MakeGuestOSInstallerUIConfig());
  map.AddWebUIConfig(std::make_unique<HealthdInternalsUIConfig>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<HelpAppUIConfig, HelpAppUI,
                                      ChromeHelpAppUIDelegate>());
  map.AddWebUIConfig(std::make_unique<HumanPresenceInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<InternetConfigDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<InternetDetailDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<KerberosInBrowserUIConfig>());
  map.AddWebUIConfig(std::make_unique<LauncherInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<LockScreenNetworkUIConfig>());
  map.AddWebUIConfig(std::make_unique<LockScreenStartReauthUIConfig>());
  map.AddWebUIConfig(std::make_unique<ManageMirrorSyncUIConfig>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<MediaAppUIConfig, MediaAppUI,
                                      ChromeMediaAppUIDelegate>());
  map.AddWebUIConfig(std::make_unique<MultideviceInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<multidevice_setup::MultiDeviceSetupDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<NearbyInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<nearby_share::NearbyShareDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetworkUIConfig>());
  map.AddWebUIConfig(std::make_unique<NotificationTesterUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<office_fallback::OfficeFallbackUIConfig>());
  map.AddWebUIConfig(std::make_unique<OobeUIConfig>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<OSFeedbackUIConfig, OSFeedbackUI,
                                      ChromeOsFeedbackDelegate>());
  map.AddWebUIConfig(std::make_unique<settings::OSSettingsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ParentAccessUIConfig>());
  map.AddWebUIConfig(std::make_unique<PasswordChangeUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<reporting::EnterpriseReportingUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<personalization_app::PersonalizationAppUIConfig>(
          base::BindRepeating(
              personalization_app::CreatePersonalizationAppUI)));
  map.AddWebUIConfig(std::make_unique<PowerUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<printing::printing_manager::PrintManagementUIConfig>(
          base::BindRepeating(
              &printing::print_management::PrintingManagerFactory::
                  CreatePrintManagementUIController)));
  map.AddWebUIConfig(std::make_unique<multidevice::ProximityAuthUIConfig>());
  map.AddWebUIConfig(std::make_unique<RemoteMaintenanceCurtainUIConfig>());
  map.AddWebUIConfig(
      MakeComponentConfigWithDelegate<ScanningUIConfig, ScanningUI,
                                      ChromeScanningAppDelegate>());
  map.AddWebUIConfig(std::make_unique<SetTimeUIConfig>());
  map.AddWebUIConfig(MakeComponentConfigWithDelegate<
                     ShimlessRMADialogUIConfig, ShimlessRMADialogUI,
                     shimless_rma::ChromeShimlessRmaDelegate>());
  map.AddWebUIConfig(std::make_unique<ShortcutCustomizationAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<SlowTraceControllerConfig>());
  map.AddWebUIConfig(std::make_unique<SlowUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<smb_dialog::SmbCredentialsDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<smb_dialog::SmbShareDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<SysInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<SystemExtensionsInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<UrgentPasswordExpiryNotificationUIConfig>());
  map.AddWebUIConfig(std::make_unique<VcTrayTesterUIConfig>());
  map.AddWebUIConfig(std::make_unique<VmUIConfig>());
#if !defined(OFFICIAL_BUILD)
  map.AddWebUIConfig(std::make_unique<SampleSystemWebAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<StatusAreaInternalsUIConfig>());
#if !defined(USE_REAL_DBUS_CLIENTS)
  map.AddWebUIConfig(std::make_unique<DeviceEmulatorUIConfig>());
#endif  // !defined(USE_REAL_DBUS_CLIENTS)
#endif  // !defined(OFFICIAL_BUILD)
}

}  // namespace ash
