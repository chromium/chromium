// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/suggestions/suggestions_ui.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/components_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/device_log_ui.h"
#include "chrome/browser/ui/webui/domain_reliability_internals_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/flash_ui.h"
#include "chrome/browser/ui/webui/gcm_internals_ui.h"
#include "chrome/browser/ui/webui/identity_internals_ui.h"
#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"
#include "chrome/browser/ui/webui/invalidations_ui.h"
#include "chrome/browser/ui/webui/local_state/local_state_ui.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"
#include "chrome/browser/ui/webui/memory_internals_ui.h"
#include "chrome/browser/ui/webui/net_export_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"
#include "chrome/browser/ui/webui/policy_tool_ui.h"
#include "chrome/browser/ui/webui/policy_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_ui.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
#include "chrome/browser/ui/webui/settings_utils.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/supervised_user_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals_ui.h"
#include "chrome/browser/ui/webui/task_scheduler_internals/task_scheduler_internals_ui.h"
#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "chrome/browser/ui/webui/ukm/ukm_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/dom_distiller_constants.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/webui/dom_distiller_ui.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "components/security_interstitials/content/connection_help_ui.h"
#include "components/security_interstitials/content/urls.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"
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

#if !defined(OS_ANDROID)
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_internals_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/cast/cast_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals_ui.h"
#include "chrome/browser/ui/webui/offline/offline_internals_ui.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_ui.h"
#include "chrome/browser/ui/webui/webapks_ui.h"
#include "components/feed/feed_feature_list.h"
#else
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"
#include "chrome/browser/ui/webui/md_downloads/md_downloads_ui.h"
#include "chrome/browser/ui/webui/md_history_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_ui.h"
#include "chrome/browser/ui/webui/system_info_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/chromeos/certificate_manager_dialog_ui.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"
#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/chromeos/power_ui.h"
#include "chrome/browser/ui/webui/chromeos/set_time_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_trace_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_ui.h"
#include "chrome/browser/ui/webui/chromeos/sys_internals/sys_internals_ui.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/webui/proximity_auth_ui.h"
#include "chromeos/components/proximity_auth/webui/url_constants.h"
#endif

#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_ui.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/app_launcher_page_ui.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/md_user_manager_ui.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/signin_error_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/welcome/welcome_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"
#include "chrome/browser/ui/webui/set_as_default_browser_ui_win.h"
#include "chrome/browser/ui/webui/welcome/welcome_win10_ui.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "chrome/browser/ui/webui/sandbox_internals_ui.h"
#endif

#if defined(USE_NSS_CERTS) && defined(USE_AURA)
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
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

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#include "components/safe_browsing/features.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::WebDialogUI;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template<class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

#if !defined(OS_ANDROID)
template <>
WebUIController* NewWebUI<PageNotAvailableForGuestUI>(WebUI* web_ui,
                                                      const GURL& url) {
  return new PageNotAvailableForGuestUI(web_ui, url.host());
}
#endif

// Special case for older about: handlers.
template<>
WebUIController* NewWebUI<AboutUI>(WebUI* web_ui, const GURL& url) {
  return new AboutUI(web_ui, url.host());
}

#if defined(OS_CHROMEOS)
template<>
WebUIController* NewWebUI<chromeos::OobeUI>(WebUI* web_ui, const GURL& url) {
  return new chromeos::OobeUI(web_ui, url);
}

// Special case for chrome://proximity_auth.
template <>
WebUIController* NewWebUI<proximity_auth::ProximityAuthUI>(WebUI* web_ui,
                                                           const GURL& url) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  // TODO(crbug.com/848956): Only pass DeviceSyncClient once Smart Lock has
  // fully migrated to the DeviceSync API.
  return new proximity_auth::ProximityAuthUI(
      web_ui,
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? nullptr
          : chromeos::EasyUnlockServiceFactory::GetForBrowserContext(
                browser_context)
                ->proximity_auth_client(),
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? chromeos::device_sync::DeviceSyncClientFactory::GetForProfile(
                Profile::FromBrowserContext(browser_context))
          : nullptr,
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? chromeos::secure_channel::SecureChannelClientProvider::GetInstance()
                ->GetClient()
          : nullptr);
}
#endif

// Special cases for DOM distiller.
template<>
WebUIController* NewWebUI<dom_distiller::DomDistillerUi>(WebUI* web_ui,
                                                         const GURL& url) {
  // The DomDistillerUi can not depend on components/dom_distiller/content,
  // so inject the correct DomDistillerService from chrome/.
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          browser_context);
  return new dom_distiller::DomDistillerUi(
      web_ui, service, dom_distiller::kDomDistillerScheme);
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
template <>
WebUIController* NewWebUI<WelcomeUI>(WebUI* web_ui, const GURL& url) {
  return new WelcomeUI(web_ui, url);
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_WIN)
template <>
WebUIController* NewWebUI<WelcomeWin10UI>(WebUI* web_ui, const GURL& url) {
  return new WelcomeWin10UI(web_ui, url);
}
#endif  // defined(OS_WIN)

bool IsAboutUI(const GURL& url) {
  return (url.host_piece() == chrome::kChromeUIChromeURLsHost ||
          url.host_piece() == chrome::kChromeUICreditsHost
#if !defined(OS_ANDROID)
          || url.host_piece() == chrome::kChromeUITermsHost
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
          || url.host_piece() == chrome::kChromeUILinuxProxyConfigHost
#endif
#if defined(OS_CHROMEOS)
          || url.host_piece() == chrome::kChromeUIOSCreditsHost ||
          url.host_piece() == chrome::kChromeUILinuxCreditsHost
#endif
          );  // NOLINT
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(content::kChromeDevToolsScheme) &&
      !url.SchemeIs(content::kChromeUIScheme)) {
    return NULL;
  }

  /****************************************************************************
   * Please keep this in alphabetical order. If #ifs or special logics are
   * required, add it below in the appropriate section.
   ***************************************************************************/
  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host_piece() == chrome::kChromeUIAccessibilityHost)
    return &NewWebUI<AccessibilityUI>;
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
  if (url.host_piece() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
  if (url.host_piece() == chrome::kChromeUIGCMInternalsHost)
    return &NewWebUI<GCMInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIInterstitialHost)
    return &NewWebUI<InterstitialUI>;
  if (url.host_piece() == chrome::kChromeUIInterventionsInternalsHost)
    return &NewWebUI<InterventionsInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIInvalidationsHost)
    return &NewWebUI<InvalidationsUI>;
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
  if (url.host_piece() == chrome::kChromeUISupervisedUserInternalsHost)
    return &NewWebUI<SupervisedUserInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISupervisedUserPassphrasePageHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host_piece() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISyncResourcesHost)
    return &NewWebUI<WebDialogUI>;
  if (url.host_piece() == chrome::kChromeUITaskSchedulerInternalsHost)
    return &NewWebUI<TaskSchedulerInternalsUI>;
  if (url.host_piece() == chrome::kChromeUITranslateInternalsHost)
    return &NewWebUI<TranslateInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUkmHost)
    return &NewWebUI<UkmInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUsbInternalsHost)
    return &NewWebUI<UsbInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUserActionsHost)
    return &NewWebUI<UserActionsUI>;
  if (url.host_piece() == chrome::kChromeUIVersionHost)
    return &NewWebUI<VersionUI>;

  /****************************************************************************
   * OS Specific #defines
   ***************************************************************************/
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
  // AppLauncherPage is not needed on Android or ChromeOS.
  if (url.host_piece() == chrome::kChromeUIAppLauncherPageHost && profile &&
      extensions::ExtensionSystem::Get(profile)->extension_service() &&
      !profile->IsGuestSession()) {
    return &NewWebUI<AppLauncherPageUI>;
  }
#endif  // defined(OS_CHROMEOS)

  if (profile->IsGuestSession() &&
      (url.host_piece() == chrome::kChromeUIAppLauncherPageHost ||
       url.host_piece() == chrome::kChromeUIBookmarksHost ||
       url.host_piece() == chrome::kChromeUIHistoryHost ||
       url.host_piece() == chrome::kChromeUIExtensionsHost)) {
    return &NewWebUI<PageNotAvailableForGuestUI>;
  }
  // Bookmarks are part of NTP on Android.
  if (url.host_piece() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<MdBookmarksUI>;
  // Downloads list on Android uses the built-in download manager.
  if (url.host_piece() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<MdDownloadsUI>;
  // Identity API is not available on Android.
  if (url.host_piece() == chrome::kChromeUIIdentityInternalsHost)
    return &NewWebUI<IdentityInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINewTabHost)
    return &NewWebUI<NewTabUI>;
  // Settings are implemented with native UI elements on Android.
  if (url.host_piece() == chrome::kChromeUISettingsHost)
    return &NewWebUI<settings::MdSettingsUI>;
  if (url.host_piece() == chrome::kChromeUIExtensionsHost)
    return &NewWebUI<extensions::ExtensionsUI>;
  if (url.host_piece() == chrome::kChromeUIHistoryHost)
    return &NewWebUI<MdHistoryUI>;
  if (url.host_piece() == chrome::kChromeUISyncFileSystemInternalsHost)
    return &NewWebUI<SyncFileSystemInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<SystemInfoUI>;
  // Inline login UI is available on all platforms except Android.
  if (url.host_piece() == chrome::kChromeUIChromeSigninHost)
    return &NewWebUI<InlineLoginUI>;
#endif  // !defined(OS_ANDROID)
#if defined(OS_WIN)
  if (url.host_piece() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
  if (url.host_piece() == chrome::kChromeUIMetroFlowHost)
    return &NewWebUI<SetAsDefaultBrowserUI>;
#endif
#if defined(OS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUIBluetoothPairingHost)
    return &NewWebUI<chromeos::BluetoothPairingDialogUI>;
  if (url.host_piece() == chrome::kChromeUICertificateManagerHost)
    return &NewWebUI<chromeos::CertificateManagerDialogUI>;
  if (url.host_piece() == chrome::kChromeUICryptohomeHost)
    return &NewWebUI<chromeos::CryptohomeUI>;
  if (url.host_piece() == chrome::kChromeUIDriveInternalsHost)
    return &NewWebUI<chromeos::DriveInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIFirstRunHost)
    return &NewWebUI<chromeos::FirstRunUI>;
  if (url.host_piece() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<chromeos::MobileSetupUI>;
  if (url.host_piece() == chrome::kChromeUIMultiDeviceSetupHost)
    return &NewWebUI<chromeos::multidevice_setup::MultiDeviceSetupDialogUI>;
  if (url.host_piece() == chrome::kChromeUINetworkHost)
    return &NewWebUI<chromeos::NetworkUI>;
  if (url.host_piece() == chrome::kChromeUIOobeHost)
    return &NewWebUI<chromeos::OobeUI>;
  if (url.host_piece() == chrome::kChromeUIPowerHost)
    return &NewWebUI<chromeos::PowerUI>;
  if (url.host_piece() == proximity_auth::kChromeUIProximityAuthHost)
    return &NewWebUI<proximity_auth::ProximityAuthUI>;
  if (url.host_piece() == chrome::kChromeUIInternetConfigDialogHost)
    return &NewWebUI<chromeos::InternetConfigDialogUI>;
  if (url.host_piece() == chrome::kChromeUIInternetDetailDialogHost)
    return &NewWebUI<chromeos::InternetDetailDialogUI>;
  if (url.host_piece() == chrome::kChromeUISetTimeHost)
    return &NewWebUI<chromeos::SetTimeUI>;
  if (url.host_piece() == chrome::kChromeUISlowHost)
    return &NewWebUI<chromeos::SlowUI>;
  if (url.host_piece() == chrome::kChromeUISlowTraceHost)
    return &NewWebUI<chromeos::SlowTraceController>;
  if (url.host_piece() == chrome::kChromeUISysInternalsHost &&
      SysInternalsUI::IsEnabled())
    return &NewWebUI<SysInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIAssistantOptInHost)
    return &NewWebUI<chromeos::AssistantOptInUI>;
#if !defined(OFFICIAL_BUILD)
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    if (url.host_piece() == chrome::kChromeUIDeviceEmulatorHost)
      return &NewWebUI<DeviceEmulatorUI>;
  }
#endif  // !defined(OFFICIAL_BUILD)
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIEocInternalsHost &&
      !profile->IsOffTheRecord())
    return &NewWebUI<EocInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIOfflineInternalsHost)
    return &NewWebUI<OfflineInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISnippetsInternalsHost &&
      !profile->IsOffTheRecord() &&
      !base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions))
    return &NewWebUI<SnippetsInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIWebApksHost)
    return &NewWebUI<WebApksUI>;
#else
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
#endif
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIMdUserManagerHost)
    return &NewWebUI<MDUserManagerUI>;
  if (url.host_piece() == chrome::kChromeUISigninErrorHost &&
      (!profile->IsOffTheRecord() ||
       profile->GetOriginalProfile()->IsSystemProfile()))
    return &NewWebUI<SigninErrorUI>;
  if (url.host_piece() == chrome::kChromeUISyncConfirmationHost &&
      !profile->IsOffTheRecord())
    return &NewWebUI<SyncConfirmationUI>;
  if (url.host_piece() == chrome::kChromeUISigninEmailConfirmationHost &&
      !profile->IsOffTheRecord())
    return &NewWebUI<SigninEmailConfirmationUI>;
  if (url.host_piece() == chrome::kChromeUIWelcomeHost)
    return &NewWebUI<WelcomeUI>;
#endif
#if defined(OS_WIN)
  if (url.host_piece() == chrome::kChromeUIWelcomeWin10Host)
    return &NewWebUI<WelcomeWin10UI>;
#endif  // defined(OS_WIN)

  /****************************************************************************
   * Other #defines and special logics.
   ***************************************************************************/
#if BUILDFLAG(ENABLE_NACL)
  if (url.host_piece() == chrome::kChromeUINaClHost)
    return &NewWebUI<NaClUI>;
#endif
#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
  if (url.host_piece() == chrome::kChromeUITabModalConfirmDialogHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
#endif
#if defined(USE_NSS_CERTS) && defined(USE_AURA)
  if (url.host_piece() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#if defined(OS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUICertificateViewerDialogHost)
    return &NewWebUI<CertificateViewerModalDialogUI>;
#endif
#endif  // USE_NSS_CERTS && USE_AURA

  if (url.host_piece() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;
#if !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIPolicyToolHost &&
      PolicyToolUI::IsEnabled()) {
    return &NewWebUI<PolicyToolUI>;
  }
  if (url.host_piece() == chrome::kChromeUIManagementHost)
    return &NewWebUI<ManagementUI>;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.host_piece() == chrome::kChromeUIExtensionsFrameHost)
    return &NewWebUI<extensions::ExtensionsUI>;
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  if (url.host_piece() == chrome::kChromeUIFlashHost)
    return &NewWebUI<FlashUI>;
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (url.host_piece() == chrome::kChromeUIPrintHost &&
      !profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled)) {
    return &NewWebUI<PrintPreviewUI>;
  }
#endif
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (url.host_piece() == chrome::kChromeUIDevicesHost) {
    return &NewWebUI<LocalDiscoveryUI>;
  }
#endif
  if (url.host_piece() == chrome::kChromeUIWebRtcLogsHost)
    return &NewWebUI<WebRtcLogsUI>;
#if !defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIMediaRouterHost &&
      media_router::MediaRouterEnabled(profile)) {
    return &NewWebUI<media_router::MediaRouterUI>;
  }
  if (url.host_piece() == chrome::kChromeUIMediaRouterInternalsHost &&
      media_router::MediaRouterEnabled(profile)) {
    return &NewWebUI<media_router::MediaRouterInternalsUI>;
  }
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUICastHost &&
      media_router::MediaRouterEnabled(profile)) {
    return &NewWebUI<CastUI>;
  }
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (url.host_piece() == chrome::kChromeUISandboxHost) {
    return &NewWebUI<SandboxInternalsUI>;
  }
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  if (url.host_piece() == chrome::kChromeUIDiscardsHost)
    return &NewWebUI<DiscardsUI>;
#endif
  if (IsAboutUI(url))
    return &NewWebUI<AboutUI>;

  if (base::FeatureList::IsEnabled(features::kBundledConnectionHelpFeature) &&
      url.host_piece() == security_interstitials::kChromeUIConnectionHelpHost) {
    return &NewWebUI<security_interstitials::ConnectionHelpUI>;
  }

  if (dom_distiller::IsEnableDomDistillerSet() &&
      url.host_piece() == dom_distiller::kChromeUIDomDistillerHost) {
    return &NewWebUI<dom_distiller::DomDistillerUi>;
  }

  if (SiteEngagementService::IsEnabled() &&
      url.host_piece() == chrome::kChromeUISiteEngagementHost) {
    return &NewWebUI<SiteEngagementUI>;
  }

  if (MediaEngagementService::IsEnabled() &&
      url.host_piece() == chrome::kChromeUIMediaEngagementHost) {
    return &NewWebUI<MediaEngagementUI>;
  }

#if defined(SAFE_BROWSING_DB_LOCAL)
  bool enable_reset_password =
      base::FeatureList::IsEnabled(
          safe_browsing::kForceEnableResetPasswordWebUI) ||
      safe_browsing::ChromePasswordProtectionService::
          IsPasswordReuseProtectionConfigured(profile);
  if (url.host_piece() == chrome::kChromeUIResetPasswordHost &&
      enable_reset_password) {
    return &NewWebUI<ResetPasswordUI>;
  }
#endif

  return NULL;
}

void RunFaviconCallbackAsync(
    const favicon_base::FaviconResultsCallback& callback,
    const std::vector<favicon_base::FaviconRawBitmapResult>* results) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&favicon::FaviconService::FaviconResultsCallbackRunner,
                     callback, base::Owned(results)));
}

}  // namespace

WebUI::TypeID ChromeWebUIControllerFactory::GetWebUIType(
      content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(NULL, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return UseWebUIForURL(browser_context, url);
}

std::unique_ptr<WebUIController>
ChromeWebUIControllerFactory::CreateWebUIControllerForURL(
    WebUI* web_ui,
    const GURL& url) const {
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
    const favicon_base::FaviconResultsCallback& callback) const {
  // Before determining whether page_url is an extension url, we must handle
  // overrides. This changes urls in |kChromeUIScheme| to extension urls, and
  // allows to use ExtensionWebUI::GetFaviconForURL.
  GURL url(page_url);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::HandleChromeURLOverride(&url, profile);

  // All extensions get their favicon from the icons part of the manifest.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionWebUI::GetFaviconForURL(profile, url, callback);
    return;
  }
#endif

  std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();

  // Use ui::GetSupportedScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  std::vector<ui::ScaleFactor> resource_scale_factors =
      ui::GetSupportedScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (size_t i = 0; i < resource_scale_factors.size(); ++i) {
    float scale = ui::GetScaleForScaleFactor(resource_scale_factors[i]);
    // Assume that GetFaviconResourceBytes() returns favicons which are
    // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP.
    int candidate_edge_size =
        static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(
        gfx::Size(candidate_edge_size, candidate_edge_size));
  }
  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(
      candidate_sizes, desired_sizes_in_pixel, &selected_indices, NULL);
  for (size_t i = 0; i < selected_indices.size(); ++i) {
    size_t selected_index = selected_indices[i];
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
      favicon_bitmap_results->push_back(bitmap_result);
    }
  }

  RunFaviconCallbackAsync(callback, favicon_bitmap_results);
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  return base::Singleton<ChromeWebUIControllerFactory>::get();
}

// static
bool ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  // Whitelist to work around exceptional cases.
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
      origin.host() == chrome::kChromeUIDownloadsHost;
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() {
}

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() {
}

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url, ui::ScaleFactor scale_factor) const {
#if !defined(OS_ANDROID)
  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED();
    return NULL;
  }
#endif

  if (!content::HasWebUIScheme(page_url))
    return NULL;

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

  if (page_url.host_piece() == chrome::kChromeUIHistoryHost) {
    return ui::ResourceBundle::GetSharedInstance()
        .LoadDataResourceBytesForScale(IDR_HISTORY_FAVICON, scale_factor);
  }

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
  // The Apps launcher page is not available on android or ChromeOS.
  if (page_url.host_piece() == chrome::kChromeUIAppLauncherPageHost)
    return AppLauncherPageUI::GetFaviconResourceBytes(scale_factor);
#endif  // !defined(OS_CHROMEOS)

  // Bookmarks are part of NTP on Android.
  if (page_url.host_piece() == chrome::kChromeUIBookmarksHost)
    return MdBookmarksUI::GetFaviconResourceBytes(scale_factor);

  // Flash is not available on android.
  if (page_url.host_piece() == chrome::kChromeUIFlashHost)
    return FlashUI::GetFaviconResourceBytes(scale_factor);

  // Android uses the native download manager.
  if (page_url.host_piece() == chrome::kChromeUIDownloadsHost)
    return MdDownloadsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the Options/Settings pages.
  if (page_url.host_piece() == chrome::kChromeUISettingsHost)
    return settings_utils::GetFaviconResourceBytes(scale_factor);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (page_url.host_piece() == chrome::kChromeUIExtensionsHost ||
      page_url.host_piece() == chrome::kChromeUIExtensionsFrameHost) {
    return extensions::ExtensionsUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !defined(OS_ANDROID)

  return NULL;
}
