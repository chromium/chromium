// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/config/ash_web_ui_config_manager.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_receiver_app_ui/boca_receiver_ui.h"
#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_ui.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/color_internals/color_internals_ui.h"
#include "ash/webui/connectivity_diagnostics/connectivity_diagnostics_ui.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/eche_app_ui/eche_app_ui.h"
#include "ash/webui/eche_app_ui/untrusted_eche_app_ui.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "ash/webui/file_manager/file_manager_untrusted_ui.h"
#include "ash/webui/files_internals/files_internals_ui.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/focus_mode/focus_mode_ui.h"
#include "ash/webui/focus_mode/focus_mode_untrusted_ui.h"
#include "ash/webui/graduation/graduation_ui.h"
#include "ash/webui/growth_internals/growth_internals_ui.h"
#include "ash/webui/help_app_ui/help_app_kids_magazine_untrusted_ui.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/mall/mall_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui.h"
#include "ash/webui/sanitize_ui/sanitize_ui.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/status_area_internals/status_area_internals_ui.h"
#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/annotator/untrusted_annotator_ui_config.h"
#include "chrome/browser/ash/boca/receiver/receiver_handler_delegate_impl.h"
#include "chrome/browser/ash/borealis/borealis_motd_ui_impl.h"
#include "chrome/browser/ash/diagnostics/system_routine_controller_delegate_impl.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/mall/chrome_mall_ui_delegate.h"
#include "chrome/browser/ash/multidevice_debug/proximity_auth_ui_config.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/sanitize/chrome_sanitize_ui_delegate.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_app_untrusted_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/chrome_demo_mode_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/chrome_file_manager_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/crosh_ui.h"
#include "chrome/browser/ash/system_web_apps/apps/files_internals_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_untrusted_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/chrome_media_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/projector_app/untrusted_projector_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/terminal_ui.h"
#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_manager_error_ui.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"
#include "chrome/browser/ui/webui/ash/arc_overview_tracing/arc_overview_tracing_ui.h"
#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_ui.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/config/ash_web_ui_config_manager.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/ash/cryptohome/cryptohome_ui.h"
#include "chrome/browser/ui/webui/ash/curtain_ui/remote_maintenance_curtain_ui.h"
#include "chrome/browser/ui/webui/ash/drive_internals/drive_internals_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"
#include "chrome/browser/ui/webui/ash/healthd_internals/healthd_internals_ui.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_ui.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_ui.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/network_ui/network_ui.h"
#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_ui.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/power_ui/power_ui.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor_info_ui.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"
#include "chrome/browser/ui/webui/ash/slow/slow_trace_ui.h"
#include "chrome/browser/ui/webui/ash/slow/slow_ui.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"
#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/chrome_url_disabled/chrome_url_disabled_ui.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_ui.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_untrusted_ui.h"
#endif  // !defined(OFFICIAL_BUILD)

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace ash {

namespace {

AshWebUIConfigManager* g_instance = nullptr;

GURL GetURLFromWebUIConfig(content::WebUIConfig& config) {
  return GURL(base::StrCat(
      {config.scheme(), url::kStandardSchemeSeparator, config.host()}));
}

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
            !IsRunningInAppMode());
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
                kDiagnosticsLogDirectoryName),
            std::make_unique<
                diagnostics::SystemRoutineControllerDelegateImpl>());
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

std::unique_ptr<content::WebUIConfig> MakeHelpAppUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        Profile* profile = Profile::FromWebUI(web_ui);

        auto delegate = std::make_unique<ChromeHelpAppUIDelegate>(web_ui);
        return std::make_unique<ash::HelpAppUI>(web_ui, std::move(delegate),
                                                profile->GetPrefs());
      });

  return std::make_unique<HelpAppUIConfig>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeRecorderAppUIConfig() {
  CreateWebUIControllerFunc create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        Profile* profile = Profile::FromWebUI(web_ui);
        const AccountId& account_id =
            CHECK_DEREF(AnnotatedAccountId::Get(profile));
        signin::IdentityManager* identity_manager =
            IdentityManagerProvider::Get().Find(account_id);
        consent_auditor::ConsentAuditor* consent_auditor =
            ConsentAuditorFactory::GetForProfile(profile);

        auto delegate = std::make_unique<ChromeRecorderAppUIDelegate>(
            g_browser_process->local_state(),
            g_browser_process->GetFeatures()->application_locale_storage(),
            g_browser_process->variations_service(),
            user_manager::UserManager::Get(), account_id, identity_manager,
            consent_auditor);
        return std::make_unique<RecorderAppUI>(web_ui, std::move(delegate));
      });

  return std::make_unique<RecorderAppUIConfig>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeDemoModeAppUntrustedUIConfig() {
  auto create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        return std::make_unique<DemoModeAppUntrustedUI>(
            web_ui, DemoSession::Get()->GetDemoAppComponentPath(),
            std::make_unique<ChromeDemoModeAppDelegate>(web_ui));
      });
  return std::make_unique<DemoModeAppUntrustedUIConfig>(create_controller_func);
}

std::unique_ptr<content::WebUIConfig> MakeBocaReceiverUntrustedUIConfig() {
  auto create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        auto delegate =
            std::make_unique<boca_receiver::ReceiverHandlerDelegateImpl>(
                web_ui);
        return std::make_unique<BocaReceiverUntrustedUI>(web_ui,
                                                         std::move(delegate));
      });

  return std::make_unique<BocaReceiverUntrustedUIConfig>(
      create_controller_func);
}

}  // namespace

// static
AshWebUIConfigManager* AshWebUIConfigManager::GetInstance() {
  return g_instance;
}

AshWebUIConfigManager::AshWebUIConfigManager() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AshWebUIConfigManager::~AshWebUIConfigManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, this);
  Unregister();
  g_instance = nullptr;
}

void AshWebUIConfigManager::RegisterWebUIConfigs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Add trusted `WebUIConfig`s (chrome://) for Ash ChromeOS to the list here.
  //
  // All `WebUIConfig`s should be registered here, irrespective of whether their
  // `WebUI` is enabled or not. To conditionally enable/disable a WebUI,
  // developers should override `WebUIConfig::IsWebUIEnabled()`.
  AddWebUIConfig(MakeComponentConfigWithDelegate<CameraAppUIConfig, CameraAppUI,
                                                 ChromeCameraAppUIDelegate>());
  AddWebUIConfig(std::make_unique<cellular_setup::MobileSetupUIConfig>());
  AddWebUIConfig(std::make_unique<chromeos::ChromeURLDisabledUIConfig>());
  AddWebUIConfig(std::make_unique<AccountManagerErrorUIConfig>());
  AddWebUIConfig(std::make_unique<AccountMigrationWelcomeUIConfig>());
  AddWebUIConfig(std::make_unique<AddSupervisionUIConfig>());
  AddWebUIConfig(std::make_unique<app_install::AppInstallDialogUIConfig>());
  AddWebUIConfig(std::make_unique<ArcOverviewTracingUIConfig>());
  AddWebUIConfig(std::make_unique<ArcPowerControlUIConfig>());
  AddWebUIConfig(std::make_unique<BluetoothPairingDialogUIConfig>());
  AddWebUIConfig(std::make_unique<BocaReceiverUIConfig>());
  AddWebUIConfig(std::make_unique<borealis::BorealisMOTDUIConfig>());
  AddWebUIConfig(std::make_unique<cloud_upload::CloudUploadUIConfig>());
  AddWebUIConfig(std::make_unique<ColorInternalsUIConfig>());
  AddWebUIConfig(std::make_unique<ConfirmPasswordChangeUIConfig>());
  AddWebUIConfig(MakeConnectivityDiagnosticsUIConfig());
  AddWebUIConfig(std::make_unique<CrostiniCreditsUI>());
  AddWebUIConfig(std::make_unique<CrostiniInstallerUIConfig>());
  AddWebUIConfig(std::make_unique<CryptohomeUIConfig>());
  AddWebUIConfig(MakeDiagnosticsUIConfig());
  AddWebUIConfig(std::make_unique<DriveInternalsUIConfig>());
  AddWebUIConfig(MakeEcheAppUIConfig());
  AddWebUIConfig(std::make_unique<SensorInfoUIConfig>());
  AddWebUIConfig(std::make_unique<EmojiUIConfig>());
  AddWebUIConfig(std::make_unique<extended_updates::ExtendedUpdatesUIConfig>());
  AddWebUIConfig(
      MakeComponentConfigWithDelegate<FilesInternalsUIConfig, FilesInternalsUI,
                                      ChromeFilesInternalsUIDelegate>());
  AddWebUIConfig(
      MakeComponentConfigWithDelegate<file_manager::FileManagerUIConfig,
                                      file_manager::FileManagerUI,
                                      ChromeFileManagerUIDelegate>());
  AddWebUIConfig(std::make_unique<FirmwareUpdateAppUIConfig>());
  AddWebUIConfig(std::make_unique<FocusModeUIConfig>());
  AddWebUIConfig(std::make_unique<graduation::GraduationUIConfig>());
  AddWebUIConfig(std::make_unique<HealthdInternalsUIConfig>());
  AddWebUIConfig(MakeHelpAppUIConfig());
  AddWebUIConfig(std::make_unique<InternetConfigDialogUIConfig>());
  AddWebUIConfig(std::make_unique<InternetDetailDialogUIConfig>());
  AddWebUIConfig(std::make_unique<KerberosInBrowserUIConfig>());
  AddWebUIConfig(std::make_unique<LauncherInternalsUIConfig>());
  AddWebUIConfig(std::make_unique<LockScreenNetworkUIConfig>());
  AddWebUIConfig(std::make_unique<LockScreenStartReauthUIConfig>());
  AddWebUIConfig(MakeComponentConfigWithDelegate<MallUIConfig, MallUI,
                                                 ChromeMallUIDelegate>());
  AddWebUIConfig(std::make_unique<ManageMirrorSyncUIConfig>());
  AddWebUIConfig(MakeComponentConfigWithDelegate<MediaAppUIConfig, MediaAppUI,
                                                 ChromeMediaAppUIDelegate>());
  AddWebUIConfig(std::make_unique<MultideviceInternalsUIConfig>());
  AddWebUIConfig(
      std::make_unique<multidevice_setup::MultiDeviceSetupDialogUIConfig>());
  AddWebUIConfig(std::make_unique<NearbyInternalsUIConfig>());
  AddWebUIConfig(std::make_unique<nearby_share::NearbyShareDialogUIConfig>());
  AddWebUIConfig(std::make_unique<NetworkUIConfig>());
  AddWebUIConfig(std::make_unique<NotificationTesterUIConfig>());
  AddWebUIConfig(std::make_unique<office_fallback::OfficeFallbackUIConfig>());
  AddWebUIConfig(std::make_unique<OobeUIConfig>());
  AddWebUIConfig(std::make_unique<OSCreditsUI>());
  AddWebUIConfig(
      MakeComponentConfigWithDelegate<OSFeedbackUIConfig, OSFeedbackUI,
                                      ChromeOsFeedbackDelegate>());
  AddWebUIConfig(std::make_unique<settings::OSSettingsUIConfig>());
  AddWebUIConfig(std::make_unique<ParentAccessUIConfig>());
  AddWebUIConfig(std::make_unique<PasswordChangeUIConfig>());
  AddWebUIConfig(std::make_unique<reporting::EnterpriseReportingUIConfig>());
  AddWebUIConfig(
      std::make_unique<personalization_app::PersonalizationAppUIConfig>(
          base::BindRepeating(
              personalization_app::CreatePersonalizationAppUI)));
  AddWebUIConfig(std::make_unique<PowerUIConfig>());
  AddWebUIConfig(
      std::make_unique<printing::printing_manager::PrintManagementUIConfig>(
          base::BindRepeating(
              &printing::print_management::PrintingManagerFactory::
                  CreatePrintManagementUIController)));
  AddWebUIConfig(std::make_unique<multidevice::ProximityAuthUIConfig>());
  AddWebUIConfig(MakeRecorderAppUIConfig());
  AddWebUIConfig(std::make_unique<RemoteMaintenanceCurtainUIConfig>());
  AddWebUIConfig(
      MakeComponentConfigWithDelegate<SanitizeDialogUIConfig, SanitizeDialogUI,
                                      ChromeSanitizeUIDelegate>());
  AddWebUIConfig(MakeComponentConfigWithDelegate<ScanningUIConfig, ScanningUI,
                                                 ChromeScanningAppDelegate>());
  AddWebUIConfig(std::make_unique<SetTimeUIConfig>());
  AddWebUIConfig(MakeComponentConfigWithDelegate<
                 ShimlessRMADialogUIConfig, ShimlessRMADialogUI,
                 shimless_rma::ChromeShimlessRmaDelegate>());
  AddWebUIConfig(std::make_unique<ShortcutCustomizationAppUIConfig>());
  AddWebUIConfig(std::make_unique<SlowTraceControllerConfig>());
  AddWebUIConfig(std::make_unique<SlowUIConfig>());
  AddWebUIConfig(std::make_unique<smb_dialog::SmbCredentialsDialogUIConfig>());
  AddWebUIConfig(std::make_unique<smb_dialog::SmbShareDialogUIConfig>());
  AddWebUIConfig(std::make_unique<SysInternalsUIConfig>());
  AddWebUIConfig(std::make_unique<
                 policy::local_user_files::LocalFilesMigrationUIConfig>());
  AddWebUIConfig(std::make_unique<UrgentPasswordExpiryNotificationUIConfig>());
  AddWebUIConfig(std::make_unique<vc_background_ui::VcBackgroundUIConfig>(
      base::BindRepeating(vc_background_ui::CreateVcBackgroundUI)));
  AddWebUIConfig(std::make_unique<GrowthInternalsUIConfig>());
  AddWebUIConfig(std::make_unique<FloatingWorkspaceUIConfig>());
#if !defined(OFFICIAL_BUILD)
  AddWebUIConfig(std::make_unique<SampleSystemWebAppUIConfig>());
  AddWebUIConfig(std::make_unique<StatusAreaInternalsUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}

void AshWebUIConfigManager::RegisterUntrustedWebUIConfigs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Add untrusted `WebUIConfig`s for Ash ChromeOS to the list here.
  //
  // All `WebUIConfig`s should be registered here, irrespective of whether their
  // `WebUI` is enabled or not. To conditionally enable/disable a WebUI,
  // developers should override `WebUIConfig::IsWebUIEnabled()`.
  AddUntrustedWebUIConfig(std::make_unique<BocaUIConfig>());
  AddUntrustedWebUIConfig(MakeBocaReceiverUntrustedUIConfig());
  AddUntrustedWebUIConfig(std::make_unique<CroshUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<TerminalUIConfig>());
  AddUntrustedWebUIConfig(
      std::make_unique<eche_app::UntrustedEcheAppUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<MediaAppGuestUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<HelpAppUntrustedUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<CameraAppUntrustedUIConfig>());
  AddUntrustedWebUIConfig(
      std::make_unique<HelpAppKidsMagazineUntrustedUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<UntrustedProjectorUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<UntrustedAnnotatorUIConfig>());
  AddUntrustedWebUIConfig(
      std::make_unique<file_manager::FileManagerUntrustedUIConfig>());
  AddUntrustedWebUIConfig(
      std::make_unique<feedback::OsFeedbackUntrustedUIConfig>());
  AddUntrustedWebUIConfig(MakeDemoModeAppUntrustedUIConfig());
  AddUntrustedWebUIConfig(std::make_unique<MakoUntrustedUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<FocusModeUntrustedUIConfig>());
  AddUntrustedWebUIConfig(std::make_unique<ScannerFeedbackUntrustedUIConfig>());
#if !defined(OFFICIAL_BUILD)
  AddUntrustedWebUIConfig(
      std::make_unique<SampleSystemWebAppUntrustedUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}

void AshWebUIConfigManager::AddWebUIConfig(
    std::unique_ptr<content::WebUIConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config);
  registered_urls_to_unregister_.push_back(GetURLFromWebUIConfig(*config));
  content::WebUIConfigMap::GetInstance().AddWebUIConfig(std::move(config));
}

void AshWebUIConfigManager::AddUntrustedWebUIConfig(
    std::unique_ptr<content::WebUIConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config);
  registered_urls_to_unregister_.push_back(GetURLFromWebUIConfig(*config));
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::move(config));
}

void AshWebUIConfigManager::Unregister() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& url : base::Reversed(registered_urls_to_unregister_)) {
    CHECK(content::WebUIConfigMap::GetInstance().RemoveConfig(url));
  }
  registered_urls_to_unregister_.clear();
}

}  // namespace ash
