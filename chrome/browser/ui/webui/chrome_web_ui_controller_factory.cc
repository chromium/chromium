// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/suggestions/suggestions_ui.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/device_log_ui.h"
#include "chrome/browser/ui/webui/domain_reliability_internals_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/gcm_internals_ui.h"
#include "chrome/browser/ui/webui/internals/internals_ui.h"
#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#include "chrome/browser/ui/webui/invalidations/invalidations_ui.h"
#include "chrome/browser/ui/webui/local_state/local_state_ui.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/media_history_ui.h"
#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"
#include "chrome/browser/ui/webui/memory_internals_ui.h"
#include "chrome/browser/ui/webui/net_export_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_ui.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"
#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/web_ui/constants.h"
#include "components/search/ntp_features.h"
#include "components/security_interstitials/content/connection_help_ui.h"
#include "components/security_interstitials/content/known_interception_disclosure_ui.h"
#include "components/security_interstitials/content/urls.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/ui/webui/nacl_ui.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_ui.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "chrome/browser/ui/webui/offline/offline_internals_ui.h"
#include "chrome/browser/ui/webui/webapks/webapks_ui.h"
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#else  // defined(OS_ANDROID)
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/commander/commander_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/identity_internals_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_internals_ui.h"
#include "chrome/browser/ui/webui/memories/memories_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_ui.h"
#include "chrome/browser/ui/webui/system_info_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/web_footer_experiment_ui.h"
#include "media/base/media_switches.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/content/scanning/scanning_ui.h"
#include "ash/content/scanning/url_constants.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/ash/scanning/scan_service_factory.h"
#include "chrome/browser/ash/web_applications/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ash/web_applications/chrome_help_app_ui_delegate.h"
#include "chrome/browser/ash/web_applications/chrome_media_app_ui_delegate.h"
#include "chrome/browser/ash/web_applications/chrome_personalization_app_ui_delegate.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/chromeos/net/network_health/network_health_service.h"
#include "chrome/browser/chromeos/printing/print_management/printing_manager.h"
#include "chrome/browser/chromeos/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_error_ui.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_welcome_ui.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_migration_welcome_ui.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_ui.h"
#include "chrome/browser/ui/webui/chromeos/arc_power_control/arc_power_control_ui.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/certificate_manager_dialog_ui.h"
#include "chrome/browser/ui/webui/chromeos/chrome_url_disabled/chrome_url_disabled_ui.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"
#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/chromeos/power_ui.h"
#include "chrome/browser/ui/webui/chromeos/set_time_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_trace_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_ui.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_share_dialog.h"
#include "chrome/browser/ui/webui/chromeos/sys_internals/sys_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/vm/vm_ui.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chromeos/components/camera_app_ui/camera_app_ui.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "chromeos/components/connectivity_diagnostics/connectivity_diagnostics_ui.h"
#include "chromeos/components/connectivity_diagnostics/url_constants.h"
#include "chromeos/components/diagnostics_ui/diagnostics_ui.h"
#include "chromeos/components/diagnostics_ui/url_constants.h"
#include "chromeos/components/eche_app_ui/eche_app_manager.h"
#include "chromeos/components/eche_app_ui/eche_app_ui.h"
#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/components/help_app_ui/help_app_ui.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/media_app_ui/media_app_guest_ui.h"
#include "chromeos/components/media_app_ui/media_app_ui.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/multidevice/debug_webui/proximity_auth_ui.h"
#include "chromeos/components/multidevice/debug_webui/url_constants.h"
#include "chromeos/components/personalization_app/personalization_app_ui.h"
#include "chromeos/components/personalization_app/personalization_app_url_constants.h"
#include "chromeos/components/print_management/print_management_ui.h"
#include "chromeos/components/print_management/url_constants.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)
#include "chrome/browser/ash/web_applications/chrome_file_manager_ui_delegate.h"
#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_ui.h"
#include "chromeos/components/file_manager/file_manager_ui.h"
#include "chromeos/components/file_manager/url_constants.h"
#include "chromeos/components/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "chromeos/components/sample_system_web_app_ui/url_constants.h"
#include "chromeos/components/telemetry_extension_ui/telemetry_extension_ui.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/app_launcher_page_ui.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/webui_js_error/webui_js_error_ui.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/signin_error_ui.h"
#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/welcome/welcome_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#endif

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
#include "chrome/browser/ui/webui/sandbox/sandbox_internals_ui.h"
#endif

#if defined(USE_NSS_CERTS) && defined(USE_AURA)
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/sessions/session_service_internals_ui.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::WebDialogUI;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be nullptr (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template <class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

// Template for handlers defined in a component layer, that take an instance of
// a delegate implemented in the chrome layer.
template <class WEB_UI_CONTROLLER, class DELEGATE>
WebUIController* NewComponentUI(WebUI* web_ui, const GURL& url) {
  auto delegate = std::make_unique<DELEGATE>(web_ui);
  return new WEB_UI_CONTROLLER(web_ui, std::move(delegate));
}

#if !defined(OS_ANDROID)
template <>
WebUIController* NewWebUI<PageNotAvailableForGuestUI>(WebUI* web_ui,
                                                      const GURL& url) {
  return new PageNotAvailableForGuestUI(web_ui, url.host());
}
#endif

// Special case for older about: handlers.
template <>
WebUIController* NewWebUI<AboutUI>(WebUI* web_ui, const GURL& url) {
  return new AboutUI(web_ui, url.host());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
template <>
WebUIController* NewWebUI<chromeos::OobeUI>(WebUI* web_ui, const GURL& url) {
  return new chromeos::OobeUI(web_ui, url);
}

void BindPrintManagement(
    Profile* profile,
    mojo::PendingReceiver<
        chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
        receiver) {
  chromeos::printing::print_management::PrintingManager* handler =
      chromeos::printing::print_management::PrintingManagerFactory::
          GetForProfile(profile);
  if (handler)
    handler->BindInterface(std::move(receiver));
}

template <>
WebUIController*
NewWebUI<chromeos::printing::printing_manager::PrintManagementUI>(
    WebUI* web_ui,
    const GURL& url) {
  return new chromeos::printing::printing_manager::PrintManagementUI(
      web_ui,
      base::BindRepeating(&BindPrintManagement, Profile::FromWebUI(web_ui)));
}

void BindEcheSignalingMessageExchanger(
    Profile* profile,
    mojo::PendingReceiver<chromeos::eche_app::mojom::SignalingMessageExchanger>
        receiver) {
  chromeos::eche_app::EcheAppManager* manager =
      chromeos::eche_app::EcheAppManagerFactory::GetForProfile(profile);
  if (manager) {
    manager->BindInterface(std::move(receiver));
  }
}

template <>
WebUIController* NewWebUI<chromeos::eche_app::EcheAppUI>(WebUI* web_ui,
                                                         const GURL& url) {
  return new chromeos::eche_app::EcheAppUI(
      web_ui, base::BindRepeating(&BindEcheSignalingMessageExchanger,
                                  Profile::FromWebUI(web_ui)));
}

void BindScanService(
    Profile* profile,
    mojo::PendingReceiver<ash::scanning::mojom::ScanService> pending_receiver) {
  ash::ScanService* service =
      ash::ScanServiceFactory::GetForBrowserContext(profile);
  if (service)
    service->BindInterface(std::move(pending_receiver));
}

std::unique_ptr<ui::SelectFilePolicy> CreateChromeSelectFilePolicy(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeSelectFilePolicy>(web_contents);
}

template <>
WebUIController* NewWebUI<ash::ScanningUI>(WebUI* web_ui, const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return new ash::ScanningUI(
      web_ui, base::BindRepeating(&BindScanService, profile),
      std::make_unique<ash::ChromeScanningAppDelegate>(web_ui));
}

template <>
WebUIController* NewWebUI<chromeos::DiagnosticsDialogUI>(WebUI* web_ui,
                                                         const GURL& url) {
  return new chromeos::DiagnosticsDialogUI(
      web_ui, base::BindRepeating(&CreateChromeSelectFilePolicy));
}

void BindMultiDeviceSetup(
    Profile* profile,
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  chromeos::multidevice_setup::MultiDeviceSetupService* service = chromeos::
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(profile);
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

// Special case for chrome://proximity_auth.
template <>
WebUIController* NewWebUI<chromeos::multidevice::ProximityAuthUI>(
    WebUI* web_ui,
    const GURL& url) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  return new chromeos::multidevice::ProximityAuthUI(
      web_ui,
      chromeos::device_sync::DeviceSyncClientFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context)),
      chromeos::secure_channel::SecureChannelClientProvider::GetInstance()
          ->GetClient(),
      base::BindRepeating(&BindMultiDeviceSetup, Profile::FromWebUI(web_ui)));
}

template <>
WebUIController* NewWebUI<chromeos::ConnectivityDiagnosticsUI>(
    WebUI* web_ui,
    const GURL& url) {
  return new chromeos::ConnectivityDiagnosticsUI(
      web_ui,
      /* BindNetworkDiagnosticsServiceCallback */
      base::BindRepeating(
          [](mojo::PendingReceiver<
              chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
                 receiver) {
            chromeos::network_health::NetworkHealthService::GetInstance()
                ->BindDiagnosticsReceiver(std::move(receiver));
          }),
      /* BindNetworkHealthServiceCallback */
      base::BindRepeating(
          [](mojo::PendingReceiver<
              chromeos::network_health::mojom::NetworkHealthService> receiver) {
            chromeos::network_health::NetworkHealthService::GetInstance()
                ->BindHealthReceiver(std::move(receiver));
          }),
      /* SendFeedbackReportCallback */
      base::BindRepeating(
          &chrome::ShowFeedbackDialogForWebUI,
          chrome::WebUIFeedbackSource::kConnectivityDiagnostics));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
template <>
WebUIController* NewWebUI<WelcomeUI>(WebUI* web_ui, const GURL& url) {
  return new WelcomeUI(web_ui, url);
}
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

bool IsAboutUI(const GURL& url) {
  return (url.host_piece() == chrome::kChromeUIChromeURLsHost ||
          url.host_piece() == chrome::kChromeUICreditsHost
#if !defined(OS_ANDROID)
          || url.host_piece() == chrome::kChromeUITermsHost
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_OPENBSD)
          || url.host_piece() == chrome::kChromeUILinuxProxyConfigHost
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
          || url.host_piece() == chrome::kChromeUIOSCreditsHost ||
          url.host_piece() == chrome::kChromeUICrostiniCreditsHost
#endif
  );  // NOLINT
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns nullptr if the URL doesn't have WebUI
// associated with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!content::HasWebUIScheme(url))
    return nullptr;

  // This factory doesn't support chrome-untrusted:// WebUIs.
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return nullptr;

  // Please keep this in alphabetical order. If #ifs or special logics are
  // required, add it below in the appropriate section.
  //
  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host_piece() == chrome::kChromeUIAccessibilityHost)
    return &NewWebUI<AccessibilityUI>;
  if (url.host_piece() == chrome::kChromeUIAutofillInternalsHost)
    return &NewWebUI<AutofillInternalsUI>;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.host_piece() == chrome::kChromeUIAppDisabledHost)
    return &NewWebUI<chromeos::ChromeURLDisabledUI>;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (url.host_piece() == chrome::kChromeUIBluetoothInternalsHost)
    return &NewWebUI<BluetoothInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIComponentsHost)
    return &NewWebUI<ComponentsUI>;
  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host_piece() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
  if (url.host_piece() == chrome::kChromeUIDeviceLogHost)
    return &NewWebUI<chromeos::DeviceLogUI>;
  if (url.host_piece() == chrome::kChromeUIDomainReliabilityInternalsHost)
    return &NewWebUI<DomainReliabilityInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIDownloadInternalsHost)
    return &NewWebUI<DownloadInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIFlagsHost &&
      FlagsDeprecatedUI::IsDeprecatedUrl(url))
    return &NewWebUI<FlagsDeprecatedUI>;
  if (url.host_piece() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
  if (url.host_piece() == chrome::kChromeUIGCMInternalsHost)
    return &NewWebUI<GCMInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIInternalsHost)
    return &NewWebUI<InternalsUI>;
  if (url.host_piece() == chrome::kChromeUIInterstitialHost)
    return &NewWebUI<InterstitialUI>;
  if (url.host_piece() == chrome::kChromeUIInvalidationsHost)
    return &NewWebUI<InvalidationsUI>;
  if (url.host_piece() ==
      security_interstitials::kChromeUIConnectionMonitoringDetectedHost) {
    return &NewWebUI<security_interstitials::KnownInterceptionDisclosureUI>;
  }
  if (url.host_piece() == chrome::kChromeUILocalStateHost)
    return &NewWebUI<LocalStateUI>;
  if (url.host_piece() == chrome::kChromeUIMemoryInternalsHost)
    return &NewWebUI<MemoryInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINetExportHost)
    return &NewWebUI<NetExportUI>;
  if (url.host_piece() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINTPTilesInternalsHost)
    return &NewWebUI<NTPTilesInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIOmniboxHost)
    return &NewWebUI<OmniboxUI>;
  if (url.host_piece() == chrome::kChromeUIPasswordManagerInternalsHost)
    return &NewWebUI<PasswordManagerInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIPredictorsHost)
    return &NewWebUI<PredictorsUI>;
  if (url.host_piece() == chrome::kChromeUIQuotaInternalsHost)
    return &NewWebUI<QuotaInternalsUI>;
  if (url.host_piece() == safe_browsing::kChromeUISafeBrowsingHost)
    return &NewWebUI<safe_browsing::SafeBrowsingUI>;
  if (url.host_piece() == chrome::kChromeUISignInInternalsHost)
    return &NewWebUI<SignInInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISuggestionsHost)
    return &NewWebUI<suggestions::SuggestionsUI>;
  if (url.host_piece() == chrome::kChromeUISupervisedUserPassphrasePageHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host_piece() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host_piece() == chrome::kChromeUITranslateInternalsHost)
    return &NewWebUI<TranslateInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUsbInternalsHost)
    return &NewWebUI<UsbInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUserActionsHost)
    return &NewWebUI<UserActionsUI>;
  if (url.host_piece() == chrome::kChromeUIVersionHost)
    return &NewWebUI<VersionUI>;

#if !defined(OS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // AppLauncherPage is not needed on Android or ChromeOS.
  if (url.host_piece() == chrome::kChromeUIAppLauncherPageHost && profile &&
      extensions::ExtensionSystem::Get(profile)->extension_service() &&
      !profile->IsGuestSession()) {
    return &NewWebUI<AppLauncherPageUI>;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile->IsGuestSession() &&
      (url.host_piece() == chrome::kChromeUIAppLauncherPageHost ||
       url.host_piece() == chrome::kChromeUIBookmarksHost ||
       url.host_piece() == chrome::kChromeUIHistoryHost ||
       url.host_piece() == chrome::kChromeUIExtensionsHost ||
       url.host_piece() == chrome::kChromeUINewTabPageHost ||
       url.host_piece() == chrome::kChromeUINewTabPageThirdPartyHost)) {
    return &NewWebUI<PageNotAvailableForGuestUI>;
  }
  if (profile->IsEphemeralGuestProfile() &&
      (url.host_piece() == chrome::kChromeUIBookmarksHost ||
       url.host_piece() == chrome::kChromeUIExtensionsHost ||
       url.host_piece() == chrome::kChromeUINewTabPageHost ||
       url.host_piece() == chrome::kChromeUINewTabPageThirdPartyHost)) {
    return &NewWebUI<PageNotAvailableForGuestUI>;
  }
  // Bookmarks are part of NTP on Android.
  if (url.host_piece() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  if (url.host_piece() == chrome::kChromeUICommanderHost)
    return &NewWebUI<CommanderUI>;
  if (url.host_piece() == chrome::kChromeUIDownloadShelfHost &&
      base::FeatureList::IsEnabled(features::kWebUIDownloadShelf)) {
    return &NewWebUI<DownloadShelfUI>;
  }
  // Downloads list on Android uses the built-in download manager.
  if (url.host_piece() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  // Identity API is not available on Android.
  if (url.host_piece() == chrome::kChromeUIIdentityInternalsHost)
    return &NewWebUI<IdentityInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINewTabHost)
    return &NewWebUI<NewTabUI>;
  if (url.host_piece() == chrome::kChromeUINewTabPageHost)
    return &NewWebUI<NewTabPageUI>;
  if (url.host_piece() == chrome::kChromeUINewTabPageThirdPartyHost)
    return &NewWebUI<NewTabPageThirdPartyUI>;
  if (base::FeatureList::IsEnabled(features::kWebUIFeedback)) {
    if (url.host_piece() == chrome::kChromeUIFeedbackHost)
      return &NewWebUI<FeedbackUI>;
  }
  if (base::FeatureList::IsEnabled(memories::kMemories)) {
    if (url.host_piece() == chrome::kChromeUIMemoriesHost)
      return &NewWebUI<MemoriesUI>;
  }
  if (base::FeatureList::IsEnabled(reading_list::switches::kReadLater)) {
    if (url.host_piece() == chrome::kChromeUIReadLaterHost)
      return &NewWebUI<ReadLaterUI>;
  }
  // Settings are implemented with native UI elements on Android.
  if (url.host_piece() == chrome::kChromeUISettingsHost)
    return &NewWebUI<settings::SettingsUI>;
  if (url.host_piece() == chrome::kChromeUITabSearchHost)
    return &NewWebUI<TabSearchUI>;
  if (url.host_piece() == chrome::kChromeUIExtensionsHost)
    return &NewWebUI<extensions::ExtensionsUI>;
  if (url.host_piece() == chrome::kChromeUIHistoryHost)
    return &NewWebUI<HistoryUI>;
  if (url.host_piece() == chrome::kChromeUISyncFileSystemInternalsHost)
    return &NewWebUI<SyncFileSystemInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<SystemInfoUI>;
  if (url.host_piece() == chrome::kChromeUIWebFooterExperimentHost)
    return &NewWebUI<WebFooterExperimentUI>;
  // Inline login UI is available on all platforms except Android.
  if (url.host_piece() == chrome::kChromeUIChromeSigninHost)
    return &NewWebUI<InlineLoginUI>;
#endif  // !defined(OS_ANDROID)
#if defined(OS_WIN)
  if (url.host_piece() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.host_piece() == chrome::kChromeUIPasswordChangeHost) {
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kSamlInSessionPasswordChangeEnabled)) {
      return nullptr;
    }
    return &NewWebUI<chromeos::PasswordChangeUI>;
  }
  if (url.host_piece() == chrome::kChromeUIConfirmPasswordChangeHost) {
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kSamlInSessionPasswordChangeEnabled)) {
      return nullptr;
    }
    return &NewWebUI<chromeos::ConfirmPasswordChangeUI>;
  }
  if (url.host_piece() ==
      chrome::kChromeUIUrgentPasswordExpiryNotificationHost) {
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kSamlInSessionPasswordChangeEnabled)) {
      return nullptr;
    }
    return &NewWebUI<chromeos::UrgentPasswordExpiryNotificationUI>;
  }
  if (url.host_piece() == chrome::kChromeUILockScreenStartReauthHost) {
    if (!ash::features::IsSamlReauthenticationOnLockscreenEnabled()) {
      return nullptr;
    }
    return &NewWebUI<chromeos::LockScreenStartReauthUI>;
  }
  if (url.host_piece() == chrome::kChromeUILockScreenNetworkHost) {
    if (!ash::features::IsSamlReauthenticationOnLockscreenEnabled()) {
      return nullptr;
    }
    return &NewWebUI<chromeos::LockScreenNetworkUI>;
  }
  if (url.host_piece() == chrome::kChromeUIAccountManagerErrorHost)
    return &NewWebUI<chromeos::AccountManagerErrorUI>;
  if (url.host_piece() == chrome::kChromeUIAccountManagerWelcomeHost)
    return &NewWebUI<chromeos::AccountManagerWelcomeUI>;
  if (url.host_piece() == chrome::kChromeUIAccountMigrationWelcomeHost)
    return &NewWebUI<chromeos::AccountMigrationWelcomeUI>;
  if (url.host_piece() == chrome::kChromeUIAddSupervisionHost)
    return &NewWebUI<chromeos::AddSupervisionUI>;
  if (url.host_piece() == chrome::kChromeUIBluetoothPairingHost)
    return &NewWebUI<chromeos::BluetoothPairingDialogUI>;
// TODO(crbug.com/1147032): The certificates settings page is temporarily
// disabled for Lacros-Chrome until a better solution is found.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (url.host_piece() == chrome::kChromeUICertificateManagerHost)
    return &NewWebUI<chromeos::CertificateManagerDialogUI>;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (url.host_piece() == chromeos::kChromeUIConnectivityDiagnosticsHost)
    return &NewWebUI<chromeos::ConnectivityDiagnosticsUI>;
  if (url.host_piece() == chrome::kChromeUICrostiniInstallerHost)
    return &NewWebUI<chromeos::CrostiniInstallerUI>;
  if (url.host_piece() == chrome::kChromeUICrostiniUpgraderHost)
    return &NewWebUI<chromeos::CrostiniUpgraderUI>;
  if (url.host_piece() == chrome::kChromeUICryptohomeHost)
    return &NewWebUI<chromeos::CryptohomeUI>;
  if (url.host_piece() == chrome::kChromeUIDriveInternalsHost)
    return &NewWebUI<chromeos::DriveInternalsUI>;
  if (url.host_piece() == chromeos::kChromeUIHelpAppHost)
    return &NewComponentUI<chromeos::HelpAppUI, ChromeHelpAppUIDelegate>;
  if (url.host_piece() == chrome::kChromeUIMachineLearningInternalsHost)
    return &NewWebUI<chromeos::machine_learning::MachineLearningInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<chromeos::cellular_setup::MobileSetupUI>;
  if (url.host_piece() == chrome::kChromeUIMultiDeviceInternalsHost)
    return &NewWebUI<chromeos::MultideviceInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIMultiDeviceSetupHost)
    return &NewWebUI<chromeos::multidevice_setup::MultiDeviceSetupDialogUI>;
  if (url.host_piece() == chrome::kChromeUINetworkHost)
    return &NewWebUI<chromeos::NetworkUI>;
  if (url.host_piece() == chrome::kChromeUIOobeHost)
    return &NewWebUI<chromeos::OobeUI>;
  if (url.host_piece() == chrome::kChromeUIOSSettingsHost)
    return &NewWebUI<chromeos::settings::OSSettingsUI>;
  if (url.host_piece() == chrome::kChromeUIPowerHost)
    return &NewWebUI<chromeos::PowerUI>;
  if (base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp) &&
      url.host_piece() == chromeos::kChromeUIDiagnosticsAppHost) {
    return &NewWebUI<chromeos::DiagnosticsDialogUI>;
  }
  if (url.host_piece() == chromeos::kChromeUIPrintManagementHost)
    return &NewWebUI<chromeos::printing::printing_manager::PrintManagementUI>;
  if (url.host_piece() == ash::kChromeUIScanningAppHost)
    return &NewWebUI<ash::ScanningUI>;
  if (base::FeatureList::IsEnabled(chromeos::features::kMediaApp)) {
    if (url.host_piece() == chromeos::kChromeUIMediaAppHost)
      return &NewComponentUI<chromeos::MediaAppUI, ChromeMediaAppUIDelegate>;
  }
  if (url.host_piece() == chromeos::multidevice::kChromeUIProximityAuthHost &&
      profile->IsRegularProfile()) {
    return &NewWebUI<chromeos::multidevice::ProximityAuthUI>;
  }
  if (url.host_piece() == chrome::kChromeUIInternetConfigDialogHost)
    return &NewWebUI<chromeos::InternetConfigDialogUI>;
  if (url.host_piece() == chrome::kChromeUIInternetDetailDialogHost)
    return &NewWebUI<chromeos::InternetDetailDialogUI>;
  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile) &&
      url.host_piece() == chrome::kChromeUINearbyShareHost &&
      profile->IsRegularProfile()) {
    return &NewWebUI<nearby_share::NearbyShareDialogUI>;
  }
  if (url.host_piece() == chrome::kChromeUISetTimeHost)
    return &NewWebUI<chromeos::SetTimeUI>;
  if (url.host_piece() == chrome::kChromeUISlowHost)
    return &NewWebUI<chromeos::SlowUI>;
  if (url.host_piece() == chrome::kChromeUISlowTraceHost)
    return &NewWebUI<chromeos::SlowTraceController>;
  if (url.host_piece() == chrome::kChromeUISmbCredentialsHost)
    return &NewWebUI<chromeos::smb_dialog::SmbCredentialsDialogUI>;
  if (url.host_piece() == chrome::kChromeUISmbShareHost)
    return &NewWebUI<chromeos::smb_dialog::SmbShareDialogUI>;
  if (url.host_piece() == chrome::kChromeUISysInternalsHost)
    return &NewWebUI<SysInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIAssistantOptInHost)
    return &NewWebUI<chromeos::AssistantOptInUI>;
  if (url.host_piece() == chromeos::kChromeUICameraAppHost &&
      web_app::SystemWebAppManager::IsAppEnabled(
          web_app::SystemAppType::CAMERA)) {
    return &NewComponentUI<chromeos::CameraAppUI, ChromeCameraAppUIDelegate>;
  }
  if (url.host_piece() == chrome::kChromeUINearbyInternalsHost)
    return &NewWebUI<NearbyInternalsUI>;
  if (arc::IsArcAllowedForProfile(profile)) {
    if (url.host_piece() == chrome::kChromeUIArcGraphicsTracingHost) {
      return &NewWebUI<chromeos::ArcGraphicsTracingUI<
          chromeos::ArcGraphicsTracingMode::kFull>>;
    }
    if (url.host_piece() == chrome::kChromeUIArcOverviewTracingHost) {
      return &NewWebUI<chromeos::ArcGraphicsTracingUI<
          chromeos::ArcGraphicsTracingMode::kOverview>>;
    }
    if (url.host_piece() == chrome::kChromeUIArcPowerControlHost) {
      return &NewWebUI<chromeos::ArcPowerControlUI>;
    }
  }
  if (url.host_piece() == chrome::kChromeUIEmojiPickerHost &&
      base::FeatureList::IsEnabled(chromeos::features::kImeSystemEmojiPicker)) {
    return &NewWebUI<chromeos::EmojiUI>;
  }
  if (url.host_piece() == chromeos::eche_app::kChromeUIEcheAppHost &&
      base::FeatureList::IsEnabled(chromeos::features::kEcheSWA)) {
    return &NewWebUI<chromeos::eche_app::EcheAppUI>;
  }
  if (url.host_piece() == chrome::kChromeUIVmHost &&
      base::FeatureList::IsEnabled(chromeos::features::kVmStatusPage)) {
    return &NewWebUI<chromeos::VmUI>;
  }
  if (url.host_piece() == chromeos::kChromeUIPersonalizationAppHost &&
      chromeos::features::IsWallpaperWebUIEnabled()) {
    return &NewComponentUI<chromeos::PersonalizationAppUI,
                           ChromePersonalizationAppUiDelegate>;
  }

#if !defined(OFFICIAL_BUILD)
#if !defined(USE_REAL_DBUS_CLIENTS)
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    if (url.host_piece() == chrome::kChromeUIDeviceEmulatorHost)
      return &NewWebUI<DeviceEmulatorUI>;
  }
#endif  // !defined(USE_REAL_DBUS_CLIENTS)
  if (url.host_piece() == chromeos::file_manager::kChromeUIFileManagerHost) {
    return &NewComponentUI<chromeos::file_manager::FileManagerUI,
                           ChromeFileManagerUIDelegate>;
  }
  if (url.host_piece() == chromeos::kChromeUISampleSystemWebAppHost)
    return &NewWebUI<chromeos::SampleSystemWebAppUI>;
  if (url.host_piece() == chromeos::kChromeUITelemetryExtensionHost) {
    if (base::FeatureList::IsEnabled(chromeos::features::kTelemetryExtension)) {
      return &NewWebUI<chromeos::TelemetryExtensionUI>;
    }
  }
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUIWebUIJsErrorHost)
    return &NewWebUI<WebUIJsErrorUI>;
#endif
#if defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIExploreSitesInternalsHost &&
      !profile->IsOffTheRecord())
    return &NewWebUI<explore_sites::ExploreSitesInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIOfflineInternalsHost)
    return &NewWebUI<OfflineInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISnippetsInternalsHost &&
      !profile->IsOffTheRecord()) {
#if BUILDFLAG(ENABLE_FEED_V2)
    return &NewWebUI<FeedInternalsUI>;
#else
    return nullptr;
#endif  // BUILDFLAG(ENABLE_FEED_V2)
  }
  if (url.host_piece() == chrome::kChromeUIWebApksHost)
    return &NewWebUI<WebApksUI>;
#else  // !defined(OS_ANDROID)
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    if (!DevToolsUIBindings::IsValidFrontendURL(url))
      return nullptr;
    return &NewWebUI<DevToolsUI>;
  }
  // chrome://inspect isn't supported on Android nor iOS. Page debugging is
  // handled by a remote devtools on the host machine, and other elements, i.e.
  // extensions aren't supported.
  if (url.host_piece() == chrome::kChromeUIInspectHost)
    return &NewWebUI<InspectUI>;
  if (url.host_piece() == chrome::kChromeUISyncConfirmationHost &&
      !profile->IsOffTheRecord()) {
    return &NewWebUI<SyncConfirmationUI>;
  }
#endif  // defined(OS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIEnterpriseProfileWelcomeHost)
    return &NewWebUI<EnterpriseProfileWelcomeUI>;
  if (url.host_piece() == chrome::kChromeUIProfileCustomizationHost)
    return &NewWebUI<ProfileCustomizationUI>;
  if (url.host_piece() == chrome::kChromeUIProfilePickerHost)
    return &NewWebUI<ProfilePickerUI>;
  if (url.host_piece() == chrome::kChromeUISigninErrorHost &&
      (!profile->IsOffTheRecord() || profile->IsSystemProfile()))
    return &NewWebUI<SigninErrorUI>;
  if (url.host_piece() == chrome::kChromeUISigninEmailConfirmationHost &&
      !profile->IsOffTheRecord())
    return &NewWebUI<SigninEmailConfirmationUI>;
  if (url.host_piece() == chrome::kChromeUISigninReauthHost &&
      !profile->IsOffTheRecord()) {
    return &NewWebUI<SigninReauthUI>;
  }
  if (url.host_piece() == chrome::kChromeUIWelcomeHost &&
      welcome::IsEnabled(profile))
    return &NewWebUI<WelcomeUI>;
#endif

#if BUILDFLAG(ENABLE_NACL)
  if (url.host_piece() == chrome::kChromeUINaClHost)
    return &NewWebUI<NaClUI>;
#endif
#if ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(TOOLKIT_VIEWS)) || \
    defined(USE_AURA)
  if (url.host_piece() == chrome::kChromeUITabModalConfirmDialogHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
#endif
#if defined(USE_NSS_CERTS) && defined(USE_AURA)
  if (url.host_piece() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#endif  // USE_NSS_CERTS && USE_AURA

  if (url.host_piece() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;
#if !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIManagementHost)
    return &NewWebUI<ManagementUI>;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (url.host_piece() == chrome::kChromeUIPrintHost) {
    if (profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled))
      return nullptr;
    // Filter out iframes that just display the preview PDF. Ideally, this would
    // filter out anything other than chrome://print/, but that does not work
    // for PrintPreviewUI tests that inject test_loader.html.
    if (url.path() == "/pdf/index.html")
      return nullptr;
    return &NewWebUI<printing::PrintPreviewUI>;
  }
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (url.host_piece() == chrome::kChromeUITabStripHost) {
    return &NewWebUI<TabStripUI>;
  }
#endif

  if (url.host_piece() == chrome::kChromeUIWebRtcLogsHost)
    return &NewWebUI<WebRtcLogsUI>;
#if !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIMediaRouterInternalsHost &&
      media_router::MediaRouterEnabled(profile)) {
    return &NewWebUI<media_router::MediaRouterInternalsUI>;
  }
#endif
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUISandboxHost) {
    return &NewWebUI<SandboxInternalsUI>;
  }
#endif
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUIDiscardsHost)
    return &NewWebUI<DiscardsUI>;
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  if (url.host_piece() == chrome::kChromeUIBrowserSwitchHost)
    return &NewWebUI<BrowserSwitchUI>;
#endif
  if (IsAboutUI(url))
    return &NewWebUI<AboutUI>;

  if (url.host_piece() == security_interstitials::kChromeUIConnectionHelpHost) {
    return &NewWebUI<security_interstitials::ConnectionHelpUI>;
  }

  if (site_engagement::SiteEngagementService::IsEnabled() &&
      url.host_piece() == chrome::kChromeUISiteEngagementHost) {
    return &NewWebUI<SiteEngagementUI>;
  }

  if (MediaEngagementService::IsEnabled() &&
      url.host_piece() == chrome::kChromeUIMediaEngagementHost) {
    return &NewWebUI<MediaEngagementUI>;
  }

  if (media_history::MediaHistoryKeyedService::IsEnabled() &&
      url.host_piece() == chrome::kChromeUIMediaHistoryHost) {
    return &NewWebUI<MediaHistoryUI>;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (url.host_piece() == chrome::kChromeUIResetPasswordHost) {
    return &NewWebUI<ResetPasswordUI>;
  }
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (url.host_piece() == chrome::kChromeUIFamilyLinkUserInternalsHost)
    return &NewWebUI<FamilyLinkUserInternalsUI>;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (url.host_piece() == chrome::kChromeUIDiceWebSigninInterceptHost)
    return &NewWebUI<DiceWebSigninInterceptUI>;
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (url.host_piece() == chrome::kChromeUISessionServiceInternalsHost)
    return &NewWebUI<SessionServiceInternalsUI>;
#endif

  return nullptr;
}

}  // namespace

WebUI::TypeID ChromeWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function =
      GetWebUIFactoryFunction(nullptr, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
ChromeWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                          const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(web_ui, profile, url);
  if (!function)
    return nullptr;

  if (web_ui->GetWebContents()->GetMainFrame())
    webui::LogWebUIUrl(url);

  return base::WrapUnique((*function)(web_ui, url));
}

void ChromeWebUIControllerFactory::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    favicon_base::FaviconResultsCallback callback) const {
  // Before determining whether page_url is an extension url, we must handle
  // overrides. This changes urls in |kChromeUIScheme| to extension urls, and
  // allows to use ExtensionWebUI::GetFaviconForURL.
  GURL url(page_url);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::HandleChromeURLOverride(&url, profile);

  // All extensions get their favicon from the icons part of the manifest.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionWebUI::GetFaviconForURL(profile, url, std::move(callback));
    return;
  }
#endif

  std::vector<favicon_base::FaviconRawBitmapResult> favicon_bitmap_results;

  // Use ui::GetSupportedScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  std::vector<ui::ScaleFactor> resource_scale_factors =
      ui::GetSupportedScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (auto scale_factor : resource_scale_factors) {
    float scale = ui::GetScaleForScaleFactor(scale_factor);
    // Assume that GetFaviconResourceBytes() returns favicons which are
    // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP.
    int candidate_edge_size =
        static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(
        gfx::Size(candidate_edge_size, candidate_edge_size));
  }
  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(candidate_sizes, desired_sizes_in_pixel,
                            &selected_indices, nullptr);
  for (size_t selected_index : selected_indices) {
    ui::ScaleFactor selected_resource_scale =
        resource_scale_factors[selected_index];

    scoped_refptr<base::RefCountedMemory> bitmap(
        GetFaviconResourceBytes(url, selected_resource_scale));
    if (bitmap.get() && bitmap->size()) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap;
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::IconType::kFavicon;
      bitmap_result.pixel_size = candidate_sizes[selected_index];
      favicon_bitmap_results.push_back(bitmap_result);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  return base::Singleton<ChromeWebUIControllerFactory>::get();
}

// static
bool ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  // Allowlist to work around exceptional cases.
  //
  // If you are adding a new host to this list, please file a corresponding bug
  // to track its removal. See https://crbug.com/829412 for the metabug.
  return
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      // https://crbug.com/829414
      origin.host() == chrome::kChromeUIPrintHost ||
#endif
      // https://crbug.com/831812
      origin.host() == chrome::kChromeUISyncConfirmationHost ||
      // https://crbug.com/831813
      origin.host() == chrome::kChromeUIInspectHost ||
      // https://crbug.com/859345
      origin.host() == chrome::kChromeUIDownloadsHost ||
      // TODO(crbug.com/1076506): remove when change to iframed OneGoogleBar.
      (origin.host() == chrome::kChromeUINewTabPageHost &&
       !base::FeatureList::IsEnabled(ntp_features::kIframeOneGoogleBar));
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() = default;

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() = default;

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url,
    ui::ScaleFactor scale_factor) const {
#if !defined(OS_ANDROID)
  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED();
    return nullptr;
  }
#endif

  if (!content::HasWebUIScheme(page_url))
    return nullptr;

  if (page_url.host_piece() == chrome::kChromeUIComponentsHost)
    return ComponentsUI::GetFaviconResourceBytes(scale_factor);

#if defined(OS_WIN)
  if (page_url.host_piece() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
#endif

  if (page_url.host_piece() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);

#if !defined(OS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // The Apps launcher page is not available on android or ChromeOS.
  if (page_url.host_piece() == chrome::kChromeUIAppLauncherPageHost)
    return AppLauncherPageUI::GetFaviconResourceBytes(scale_factor);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Bookmarks are part of NTP on Android.
  if (page_url.host_piece() == chrome::kChromeUIBookmarksHost)
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes(scale_factor);

  // Android uses the native download manager.
  if (page_url.host_piece() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the Options/Settings pages.
  if (page_url.host_piece() == chrome::kChromeUISettingsHost) {
    if (page_url.path() == chrome::kPrivacySandboxSubPagePath) {
      return settings_utils::GetPrivacySandboxFaviconResourceBytes(
          scale_factor);
    }
    return settings_utils::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == chrome::kChromeUIManagementHost)
    return ManagementUI::GetFaviconResourceBytes(scale_factor);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (page_url.host_piece() == chrome::kChromeUIExtensionsHost) {
    return extensions::ExtensionsUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (page_url.host_piece() == chrome::kChromeUIOSSettingsHost)
    return settings_utils::GetFaviconResourceBytes(scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return nullptr;
}
