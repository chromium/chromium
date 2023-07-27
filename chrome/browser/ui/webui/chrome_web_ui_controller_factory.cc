// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/device_log_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/gcm_internals_ui.h"
#include "chrome/browser/ui/webui/internals/internals_ui.h"
#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/invalidations/invalidations_ui.h"
#include "chrome/browser/ui/webui/local_state/local_state_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/media_history_ui.h"
#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"
#include "chrome/browser/ui/webui/memory_internals_ui.h"
#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h"
#include "chrome/browser/ui/webui/net_export_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"
#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/lens/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/security_interstitials/content/connection_help_ui.h"
#include "components/security_interstitials/content/known_interception_disclosure_ui.h"
#include "components/security_interstitials/content/urls.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "crypto/crypto_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/favicon_size.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/ui/webui/nacl_ui.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "chrome/browser/ui/webui/offline/offline_internals_ui.h"
#include "chrome/browser/ui/webui/webapks/webapks_ui.h"
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/commander/commander_ui.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/identity_internals_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_internals_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_ui.h"
#include "chrome/browser/ui/webui/profile_internals/profile_internals_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_ui.h"
#include "chrome/browser/ui/webui/system_info_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "media/base/media_switches.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/file_manager/url_constants.h"
#include "ash/webui/files_internals/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/multidevice_debug/url_constants.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/ash/extensions/url_constants.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/mobile_setup_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "url/url_util.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/chrome_url_disabled/chrome_url_disabled_ui.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/app_launcher_page_ui.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/webui_js_error/webui_js_error_ui.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/app_home/app_home_ui.h"
#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"
#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/welcome/welcome_ui.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/signin_error_ui.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/sandbox/sandbox_internals_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_CERTIFICATE_VIEWER)
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
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/welcome/welcome_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/media_router/cast_feedback_ui.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/webui/lens/lens_ui.h"
#endif

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/ui/webui/ash/chromebox_for_meetings/network_settings_dialog.h"
#endif  // BUILDFLAG(PLATFORM_CFM)

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::WebDialogUI;

namespace {

// TODO(crbug.com/1295080): Allow a way to disable CSP in tests.
void SetUpWebUIDataSource(WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
}

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

#if !BUILDFLAG(IS_ANDROID)
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

template <>
WebUIController* NewWebUI<commerce::CommerceInternalsUI>(WebUI* web_ui,
                                                         const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return new commerce::CommerceInternalsUI(
      web_ui,
      base::BindOnce(&SetUpWebUIDataSource, web_ui,
                     commerce::kChromeUICommerceInternalsHost),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile));
}

template <>
WebUIController* NewWebUI<OptimizationGuideInternalsUI>(WebUI* web_ui,
                                                        const GURL& url) {
  return OptimizationGuideInternalsUI::MaybeCreateOptimizationGuideInternalsUI(
      web_ui, base::BindOnce(&SetUpWebUIDataSource, web_ui,
                             optimization_guide_internals::
                                 kChromeUIOptimizationGuideInternalsHost));
}

template <>
WebUIController* NewWebUI<HistoryClustersInternalsUI>(WebUI* web_ui,
                                                      const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return new HistoryClustersInternalsUI(
      web_ui, HistoryClustersServiceFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      base::BindOnce(
          &SetUpWebUIDataSource, web_ui,
          history_clusters_internals::kChromeUIHistoryClustersInternalsHost));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
template <>
WebUIController* NewWebUI<WelcomeUI>(WebUI* web_ui, const GURL& url) {
  return new WelcomeUI(web_ui, url);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

bool IsAboutUI(const GURL& url) {
  return (url.host_piece() == chrome::kChromeUIChromeURLsHost ||
          url.host_piece() == chrome::kChromeUICreditsHost
#if !BUILDFLAG(IS_ANDROID)
          || url.host_piece() == chrome::kChromeUITermsHost
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
          || url.host_piece() == chrome::kChromeUILinuxProxyConfigHost
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
          || url.host_piece() == chrome::kChromeUIOSCreditsHost ||
          url.host_piece() == chrome::kChromeUIBorealisCreditsHost ||
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (url.host_piece() == chrome::kChromeUIBluetoothInternalsHost)
    return &NewWebUI<BluetoothInternalsUI>;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

  if (url.host_piece() == chrome::kChromeUIBrowsingTopicsInternalsHost)
    return &NewWebUI<BrowsingTopicsInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIComponentsHost)
    return &NewWebUI<ComponentsUI>;
  if (url.host_piece() == commerce::kChromeUICommerceInternalsHost) {
    return &NewWebUI<commerce::CommerceInternalsUI>;
  }
  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedWebDialogUI>;
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (url.host_piece() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
#endif
  if (url.host_piece() == chrome::kChromeUIDeviceLogHost)
    return &NewWebUI<chromeos::DeviceLogUI>;
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
  if (url.host_piece() == chrome::kChromeUILocationInternalsHost) {
    return &NewWebUI<LocationInternalsUI>;
  }
  if (url.host_piece() == chrome::kChromeUIMemoryInternalsHost)
    return &NewWebUI<MemoryInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIMetricsInternalsHost)
    return &NewWebUI<MetricsInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINetExportHost)
    return &NewWebUI<NetExportUI>;
  if (url.host_piece() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINTPTilesInternalsHost)
    return &NewWebUI<NTPTilesInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIOmniboxHost)
    return &NewWebUI<OmniboxUI>;
  if (url.host_piece() ==
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost) {
    return &NewWebUI<OptimizationGuideInternalsUI>;
  }
  if (url.host_piece() == chrome::kChromeUIPasswordManagerInternalsHost)
    return &NewWebUI<PasswordManagerInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIPredictorsHost)
    return &NewWebUI<PredictorsUI>;
  if (url.host_piece() == safe_browsing::kChromeUISafeBrowsingHost)
    return &NewWebUI<safe_browsing::SafeBrowsingUI>;
  if (url.host_piece() == chrome::kChromeUISegmentationInternalsHost)
    return &NewWebUI<SegmentationInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISignInInternalsHost)
    return &NewWebUI<SignInInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISupervisedUserPassphrasePageHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host_piece() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host_piece() == chrome::kChromeUITranslateInternalsHost)
    return &NewWebUI<TranslateInternalsUI>;
  if (url.host_piece() ==
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost) {
    return &NewWebUI<HistoryClustersInternalsUI>;
  }
  if (url.host_piece() == chrome::kChromeUIUsbInternalsHost)
    return &NewWebUI<UsbInternalsUI>;
  if (url.host_piece() == chrome::kChromeUIUserActionsHost)
    return &NewWebUI<UserActionsUI>;
  if (url.host_piece() == chrome::kChromeUIVersionHost)
    return &NewWebUI<VersionUI>;

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
  // AppLauncherPage is not needed on Android or ChromeOS.
  if (url.host_piece() == chrome::kChromeUIAppLauncherPageHost && profile &&
      extensions::ExtensionSystem::Get(profile)->extension_service() &&
      !profile->IsGuestSession()) {
    if (base::FeatureList::IsEnabled(features::kDesktopPWAsAppHomePage)) {
      return &NewWebUI<webapps::AppHomeUI>;
    } else {
      return &NewWebUI<AppLauncherPageUI>;
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  if (profile->IsGuestSession() &&
      (url.host_piece() == chrome::kChromeUIAppLauncherPageHost ||
       url.host_piece() == chrome::kChromeUIBookmarksHost ||
       url.host_piece() == chrome::kChromeUIHistoryHost ||
       url.host_piece() == chrome::kChromeUIExtensionsHost ||
       url.host_piece() == chrome::kChromeUINewTabPageHost ||
       url.host_piece() == chrome::kChromeUINewTabPageThirdPartyHost ||
       url.host_piece() == password_manager::kChromeUIPasswordManagerHost)) {
    return &NewWebUI<PageNotAvailableForGuestUI>;
  }
  if (url.host_piece() == chrome::kChromeUIAppServiceInternalsHost)
    return &NewWebUI<AppServiceInternalsUI>;
  // Bookmarks are part of NTP on Android.
  if (url.host_piece() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  if (url.host_piece() == password_manager::kChromeUIPasswordManagerHost) {
    return &NewWebUI<PasswordManagerUI>;
  }
  if (url.host_piece() == chrome::kChromeUICommanderHost)
    return &NewWebUI<CommanderUI>;
  // Downloads list on Android uses the built-in download manager.
  if (url.host_piece() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  // Identity API is not available on Android.
  if (url.host_piece() == chrome::kChromeUIIdentityInternalsHost)
    return &NewWebUI<IdentityInternalsUI>;
  if (url.host_piece() == chrome::kChromeUINewTabHost) {
    // The URL chrome://newtab/ can be either a virtual or a real URL,
    // depending on the context. In this case, it is always a real URL that
    // points to the New Tab page for the incognito profile only. For other
    // profile types, this URL must already be redirected to a different URL
    // that matches the profile type.
    //
    // Returning NewWebUI<NewTabUI> for the wrong profile type will lead to
    // crash in NTPResourceCache::GetNewTabHTML (Check: false), so here we add
    // a sanity check to prevent further crashes.
    //
    // The switch statement below must be consistent with the code in
    // NTPResourceCache::GetNewTabHTML!
    switch (NTPResourceCache::GetWindowType(profile)) {
      case NTPResourceCache::NORMAL:
        LOG(ERROR) << "Requested load of chrome://newtab/ for incorrect "
                      "profile type.";
        // TODO(crbug.com/1380151): Add DumpWithoutCrashing() here.
        return nullptr;
      case NTPResourceCache::INCOGNITO:
        [[fallthrough]];
      case NTPResourceCache::GUEST:
        [[fallthrough]];
      case NTPResourceCache::NON_PRIMARY_OTR:
        return &NewWebUI<NewTabUI>;
    }
  }
  if (!profile->IsOffTheRecord()) {
    if (url.host_piece() == chrome::kChromeUINewTabPageHost)
      return &NewWebUI<NewTabPageUI>;
    if (url.host_piece() == chrome::kChromeUINewTabPageThirdPartyHost)
      return &NewWebUI<NewTabPageThirdPartyUI>;
  }
  if (url.host_piece() == chrome::kChromeUIFeedbackHost &&
      FeedbackUI::IsFeedbackEnabled(profile)) {
    return &NewWebUI<FeedbackUI>;
  }
  if (url.host_piece() == chrome::kChromeUIReadLaterHost)
    return &NewWebUI<ReadingListUI>;
  if (url.host_piece() == chrome::kChromeUIBookmarksSidePanelHost)
    return &NewWebUI<BookmarksSidePanelUI>;
  if (url.host_piece() == commerce::kChromeUIShoppingInsightsSidePanelHost) {
    return &NewWebUI<ShoppingInsightsSidePanelUI>;
  }
  if (url.host_piece() == chrome::kChromeUICustomizeChromeSidePanelHost &&
      customize_chrome::IsSidePanelEnabled()) {
    return &NewWebUI<CustomizeChromeUI>;
  }
  if (base::FeatureList::IsEnabled(history_clusters::kSidePanelJourneys)) {
    if (url.host_piece() == chrome::kChromeUIHistoryClustersSidePanelHost)
      return &NewWebUI<HistoryClustersSidePanelUI>;
  }
  if (url.host_piece() == chrome::kChromeUIUserNotesSidePanelHost)
    return &NewWebUI<UserNotesSidePanelUI>;
  // Settings are implemented with native UI elements on Android.
  if (url.host_piece() == chrome::kChromeUISettingsHost)
    return &NewWebUI<settings::SettingsUI>;
  if (url.host_piece() == chrome::kChromeUITabSearchHost)
    return &NewWebUI<TabSearchUI>;
  if (url.host_piece() == chrome::kChromeUIExtensionsHost)
    return &NewWebUI<extensions::ExtensionsUI>;
  if (url.host_piece() == chrome::kChromeUIHistoryHost)
    return &NewWebUI<HistoryUI>;
  if (url.host_piece() == chrome::kChromeUIProfileInternalsHost)
    return &NewWebUI<ProfileInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISyncFileSystemInternalsHost)
    return &NewWebUI<SyncFileSystemInternalsUI>;
  if (url.host_piece() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<SystemInfoUI>;
  if (url.host_piece() == chrome::kChromeUIAccessCodeCastHost) {
    if (!base::FeatureList::IsEnabled(features::kAccessCodeCastUI)) {
      return nullptr;
    }
    if (!media_router::GetAccessCodeCastEnabledPref(profile)) {
      return nullptr;
    }
    return &NewWebUI<media_router::AccessCodeCastUI>;
  }
  if (base::FeatureList::IsEnabled(features::kSupportTool) &&
      url.host_piece() == chrome::kChromeUISupportToolHost &&
      SupportToolUI::IsEnabled(profile)) {
    return &NewWebUI<SupportToolUI>;
  }
  if (url.host_piece() == chrome::kChromeUIWebAppInternalsHost) {
    return &NewWebUI<WebAppInternalsUI>;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  if (url.host_piece() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.host_piece() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<ash::cellular_setup::MobileSetupUI>;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUIWebUIJsErrorHost)
    return &NewWebUI<WebUIJsErrorUI>;
#endif
#if BUILDFLAG(IS_ANDROID)
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
#else   // BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIEnterpriseProfileWelcomeHost)
    return &NewWebUI<EnterpriseProfileWelcomeUI>;
  if (url.host_piece() == chrome::kChromeUIIntroHost &&
      base::FeatureList::IsEnabled(kForYouFre))
    return &NewWebUI<IntroUI>;
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
#endif

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
  if (url.host_piece() == chrome::kChromeUISearchEngineChoiceHost &&
      base::FeatureList::IsEnabled(switches::kSearchEngineChoice)) {
    return &NewWebUI<SearchEngineChoiceUI>;
  }
#endif

#if BUILDFLAG(ENABLE_NACL)
  if (url.host_piece() == chrome::kChromeUINaClHost)
    return &NewWebUI<NaClUI>;
#endif
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(TOOLKIT_VIEWS)) ||                         \
    defined(USE_AURA)
  if (url.host_piece() == chrome::kChromeUITabModalConfirmDialogHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
#endif

#if BUILDFLAG(ENABLE_WEBUI_CERTIFICATE_VIEWER)
  if (url.host_piece() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#endif  // ENABLE_WEBUI_CERTIFICATE_VIEWER

  if (url.host_piece() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;
#if !BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIManagementHost)
    return &NewWebUI<ManagementUI>;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (url.host_piece() == chrome::kChromeUIPrintHost) {
    if (profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled))
      return nullptr;
    // Filter out everything except chrome://print/ and test_loader.html.
    if (url.path() != "/" && url.path() != "/test_loader.html") {
      return nullptr;
    }
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
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (url.host_piece() == chrome::kChromeUICastFeedbackHost &&
      media_router::MediaRouterEnabled(profile) &&
      FeedbackUI::IsFeedbackEnabled(profile)) {
    return &NewWebUI<media_router::CastFeedbackUI>;
  }
#endif
  if (url.host_piece() == chrome::kChromeUIWebuiGalleryHost) {
    return &NewWebUI<WebuiGalleryUI>;
  }
  if (url.host_piece() == chrome::kChromeUIWhatsNewHost &&
      base::FeatureList::IsEnabled(features::kChromeWhatsNewUI)) {
    return &NewWebUI<WhatsNewUI>;
  }
  if (url.host_piece() == chrome::kChromeUIOmniboxPopupHost &&
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    return &NewWebUI<OmniboxPopupUI>;
  }
  if (url.host_piece() == chrome::kChromeUISuggestInternalsHost) {
    return &NewWebUI<SuggestInternalsUI>;
  }
  if (url.host_piece() == chrome::kChromeUIMediaRouterInternalsHost &&
      media_router::MediaRouterEnabled(profile)) {
    return &NewWebUI<media_router::MediaRouterInternalsUI>;
  }
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == chrome::kChromeUISandboxHost) {
    return &NewWebUI<SandboxInternalsUI>;
  }
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.host_piece() == chrome::kChromeUIConnectorsInternalsHost)
    return &NewWebUI<enterprise_connectors::ConnectorsInternalsUI>;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (url.host_piece() == chrome::kChromeUIDiscardsHost)
    return &NewWebUI<DiscardsUI>;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (url.host_piece() == chrome::kChromeUIBrowserSwitchHost)
    return &NewWebUI<BrowserSwitchUI>;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
  if (url.host_piece() == chrome::kChromeUIWebAppSettingsHost)
    return &NewWebUI<WebAppSettingsUI>;
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
  if (url.host_piece() == chrome::kChromeUIWelcomeHost &&
      welcome::IsEnabled(profile)) {
    return &NewWebUI<WelcomeUI>;
  }
  if (url.host_piece() == chrome::kChromeUIDiceWebSigninInterceptHost)
    return &NewWebUI<DiceWebSigninInterceptUI>;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Inline login UI is available on all platforms except Android and Lacros.
  if (url.host_piece() == chrome::kChromeUIChromeSigninHost)
    return &NewWebUI<InlineLoginUI>;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (url.host_piece() == chrome::kChromeUISigninReauthHost &&
      !profile->IsOffTheRecord()) {
    return &NewWebUI<SigninReauthUI>;
  }
#endif

#if BUILDFLAG(PLATFORM_CFM)
  if (url.host_piece() == chrome::kCfmNetworkSettingsHost)
    return &NewWebUI<ash::cfm::NetworkSettingsDialogUi>;
#endif  // BUILDFLAG(PLATFORM_CFM)

#if !BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == chrome::kChromeUIPrivacySandboxDialogHost)
    return &NewWebUI<PrivacySandboxDialogUI>;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (url.host_piece() == chrome::kChromeUILensHost) {
    return &NewWebUI<LensUI>;
  }
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

  if (web_ui->GetWebContents()->GetPrimaryMainFrame())
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

  // Use ui::GetSupportedResourceScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  std::vector<ui::ResourceScaleFactor> resource_scale_factors =
      ui::GetSupportedResourceScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (auto scale_factor : resource_scale_factors) {
    float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
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
    ui::ResourceScaleFactor selected_resource_scale =
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  static base::NoDestructor<ChromeWebUIControllerFactory> instance;
  return instance.get();
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
      origin.host() == chrome::kChromeUIDownloadsHost;
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() = default;

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() = default;

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url,
    ui::ResourceScaleFactor scale_factor) const {
#if !BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_WIN)
  if (page_url.host_piece() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
#endif

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (page_url.host_piece() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);
#endif

  if (page_url.host_piece() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
  // The Apps launcher page is not available on android or ChromeOS.
  if (page_url.host_piece() == chrome::kChromeUIAppLauncherPageHost)
    return AppLauncherPageUI::GetFaviconResourceBytes(scale_factor);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (page_url.host_piece() == chrome::kChromeUINewTabPageHost)
    return NewTabPageUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == chrome::kChromeUIWhatsNewHost)
    return WhatsNewUI::GetFaviconResourceBytes(scale_factor);

  // Bookmarks are part of NTP on Android.
  if (page_url.host_piece() == chrome::kChromeUIBookmarksHost)
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == password_manager::kChromeUIPasswordManagerHost)
    return PasswordManagerUI::GetFaviconResourceBytes(scale_factor);

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
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (page_url.host_piece() == chrome::kChromeUIOSSettingsHost)
    return settings_utils::GetFaviconResourceBytes(scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
const std::vector<GURL>&
ChromeWebUIControllerFactory::GetListOfAcceptableURLs() {
  // clang-format off
  static const base::NoDestructor<std::vector<GURL>> urls({
    // Pages that exist in Ash and in Lacros (separately), with both instances
    // accessible. The Lacros instance is reachable via chrome:// and the Ash
    // instance is reachable via os:// (from Lacros). For convenience and to
    // avoid confusion, the two instances should provide a link to each other.
    GURL(chrome::kChromeUIAboutURL),
    GURL(chrome::kChromeUIAppServiceInternalsURL),
    GURL(chrome::kChromeUIComponentsUrl),
    GURL(chrome::kChromeUICreditsURL),
    GURL(chrome::kChromeUIDeviceLogUrl),
    GURL(chrome::kChromeUIFlagsURL),
    GURL(chrome::kChromeUIGpuURL),
    GURL(chrome::kChromeUIHistogramsURL),
    GURL(chrome::kChromeUIInvalidationsUrl),
    GURL(chrome::kChromeUIManagementURL),
    GURL(chrome::kChromeUIPrefsInternalsURL),
    GURL(chrome::kChromeUIRestartURL),
    GURL(chrome::kChromeUISignInInternalsUrl),
    GURL(chrome::kChromeUISyncInternalsUrl),
    GURL(chrome::kChromeUISystemURL),
    GURL(chrome::kChromeUITermsURL),
    GURL(chrome::kChromeUIVersionURL),
    GURL(chrome::kChromeUIWebAppInternalsURL),

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Pages that exist only in Ash, i.e. have no immediate counterpart in
    // Lacros. They are reachable via both chrome:// and os:// (from Lacros).
    // Note: chrome://os-settings is also reachable via os://settings.
    GURL(ash::file_manager::kChromeUIFileManagerUntrustedURL),
    GURL(ash::file_manager::kChromeUIFileManagerURL),
    GURL(ash::kChromeUICameraAppURL),
    GURL(ash::kChromeUIFilesInternalsURL),
    GURL(ash::kChromeUIHelpAppURL),
    GURL(ash::multidevice::kChromeUIProximityAuthURL),
    GURL(chrome::kChromeUIAccountManagerErrorURL),
    GURL(chrome::kChromeUIAccountMigrationWelcomeURL),
    GURL(chrome::kChromeUIAddSupervisionURL),
    GURL(chrome::kChromeUIAppDisabledURL),
    GURL(chrome::kChromeUIArcOverviewTracingURL),
    GURL(chrome::kChromeUIArcPowerControlURL),
    GURL(chrome::kChromeUIAssistantOptInURL),
    GURL(chrome::kChromeUIBluetoothInternalsURL),
    GURL(chrome::kChromeUIBluetoothPairingURL),
    GURL(chrome::kChromeUIBorealisCreditsURL),
    GURL(chrome::kChromeUICloudUploadURL),
    GURL(chrome::kChromeUIConnectivityDiagnosticsAppURL),
    GURL(chrome::kChromeUICrashesUrl),
    GURL(chrome::kChromeUICrostiniCreditsURL),
    GURL(chrome::kChromeUICrostiniInstallerUrl),
    GURL(chrome::kChromeUICrostiniUpgraderUrl),
    GURL(chrome::kChromeUICryptohomeURL),
    GURL(chrome::kChromeUIDeviceEmulatorURL),
    GURL(chrome::kChromeUIDiagnosticsAppURL),
    GURL(chrome::kChromeUIDriveInternalsUrl),
    GURL(chrome::kChromeUIEmojiPickerURL),
    GURL(chrome::kChromeUIExtensionsInternalsURL),
    GURL(chrome::kChromeUIFirmwareUpdaterAppURL),
    GURL(chrome::kChromeUIHealthdInternalsURL),
    GURL(chrome::kChromeUIHumanPresenceInternalsURL),
    GURL(chrome::kChromeUIInternetConfigDialogURL),
    GURL(chrome::kChromeUIInternetDetailDialogURL),
    GURL(chrome::kChromeUILauncherInternalsURL),
    GURL(chrome::kChromeUILockScreenNetworkURL),
    GURL(chrome::kChromeUILockScreenStartReauthURL),
    GURL(chrome::kChromeUIManageMirrorSyncURL),
    GURL(chrome::kChromeUIMultiDeviceInternalsURL),
    GURL(chrome::kChromeUIMultiDeviceSetupUrl),
    GURL(chrome::kChromeUINearbyInternalsURL),
    GURL(chrome::kChromeUINetExportURL),
    GURL(chrome::kChromeUINetworkUrl),
    GURL(chrome::kChromeUINotificationTesterURL),
    GURL(chrome::kChromeUIOfficeFallbackURL),
    GURL(chrome::kChromeUIOSCreditsURL),
    GURL(chrome::kChromeUIOSSettingsURL),
    GURL(chrome::kChromeUIPowerUrl),
    GURL(chrome::kChromeUIPrintManagementUrl),
    GURL(chrome::kChromeUIScanningAppURL),
    GURL(chrome::kChromeUISensorInfoURL),
    GURL(chrome::kChromeUISetTimeURL),
    GURL(chrome::kChromeUISlowURL),
    GURL(chrome::kChromeUISmbShareURL),
    GURL(chrome::kChromeUISysInternalsUrl),
    GURL(chrome::kChromeUIUntrustedCroshURL),
    GURL(chrome::kChromeUIUntrustedTerminalURL),
    GURL(chrome::kChromeUIUserImageURL),
    GURL(chrome::kChromeUIVmUrl),

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // IME extension's Japanese options page. Opened via OS_URL_HANDLER SWA
    // by InputMethodPrivateOpenOptionsPageFunction when Lacros is the only
    // browser.
    // TODO(b/250997017): Remove this once the Japanese options are
    // in-settings.
    GURL(extensions::kIMEJPOptionsURL),

    // Option pages of accessibility extensions. Opened via OS_URL_HANDLER
    // SWA by ash::settings::AccessibilityHandler when Lacros is the only
    // browser.
    GURL(base::StrCat({extensions::kExtensionScheme,
                       url::kStandardSchemeSeparator,
                       extension_misc::kChromeVoxExtensionId,
                       extension_misc::kChromeVoxOptionsPath})),
    GURL(base::StrCat({extensions::kExtensionScheme,
                       url::kStandardSchemeSeparator,
                       extension_misc::kEspeakSpeechSynthesisExtensionId,
                       extension_misc::kEspeakSpeechSynthesisOptionsPath})),
    GURL(base::StrCat({extensions::kExtensionScheme,
                       url::kStandardSchemeSeparator,
                       extension_misc::kGoogleSpeechSynthesisExtensionId,
                       extension_misc::kGoogleSpeechSynthesisOptionsPath})),
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // Pages that only exist in Lacros, where they are reachable via chrome://.
    // TODO(neis): Some of these still exist in Ash (but are inaccessible) and
    // should be removed.
    GURL(chrome::kChromeUIPolicyURL),
    GURL(chrome::kChromeUISettingsURL),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  });
  // clang-format on
  return *urls;
}

bool ChromeWebUIControllerFactory::CanHandleUrl(const GURL& url) {
  return crosapi::gurl_os_handler_utils::IsUrlInList(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      crosapi::gurl_os_handler_utils::GetTargetURLFromLacrosURL(url),
#else
      crosapi::gurl_os_handler_utils::SanitizeAshURL(url),
#endif
      GetListOfAcceptableURLs());
}

#endif
