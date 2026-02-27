// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "build/build_config.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/chrome_url_constants.h"
#include "ash/constants/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome {

// Note: Add hosts to `ChromeURLHosts()` at the bottom of this file to be listed
// by chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

#if BUILDFLAG(IS_CHROMEOS)

// static assertions to keep the consistency with URLs that ash as OS system
// refers.
static_assert(std::string_view(kChromeUICertificateManagerDialogURL) ==
              ash::chrome_urls::kChromeUICertificateManagerDialogURL);
static_assert(std::string_view(kChromeUIFlagsURL) ==
              ash::chrome_urls::kChromeUIFlagsURL);
static_assert(std::string_view(kChromeUIFeedbackURL) ==
              ash::chrome_urls::kChromeUIFeedbackURL);
static_assert(std::u16string_view(kChromeUIManagementURL16) ==
              ash::chrome_urls::kChromeUIManagementURL16);
static_assert(std::string_view(kChromeUINewTabURL) ==
              ash::chrome_urls::kChromeUINewTabURL);
static_assert(std::string_view(kChromeUISettingsHost) ==
              ash::chrome_urls::kChromeUISettingsHost);
static_assert(std::string_view(kChromeUISettingsURL) ==
              ash::chrome_urls::kChromeUISettingsURL);
static_assert(std::string_view(kChromeUITermsHost) ==
              ash::chrome_urls::kChromeUITermsHost);
static_assert(std::string_view(kChromeUITermsURL) ==
              ash::chrome_urls::kChromeUITermsURL);

static_assert(std::string_view(kAppearanceSubPage) ==
              ash::chrome_urls::kAppearanceSubPage);
static_assert(std::string_view(kAutofillSubPage) ==
              ash::chrome_urls::kAutofillSubPage);
static_assert(std::string_view(kClearBrowserDataSubPage) ==
              ash::chrome_urls::kClearBrowserDataSubPage);
static_assert(std::string_view(kDownloadsSubPage) ==
              ash::chrome_urls::kDownloadsSubPage);
static_assert(std::string_view(kLanguagesSubPage) ==
              ash::chrome_urls::kLanguagesSubPage);
static_assert(std::string_view(kOnStartupSubPage) ==
              ash::chrome_urls::kOnStartupSubPage);
static_assert(std::string_view(kPasswordManagerSubPage) ==
              ash::chrome_urls::kPasswordManagerSubPage);
static_assert(std::string_view(kPrivacySubPage) ==
              ash::chrome_urls::kPrivacySubPage);
static_assert(std::string_view(kResetSubPage) ==
              ash::chrome_urls::kResetSubPage);
static_assert(std::string_view(kSearchSubPage) ==
              ash::chrome_urls::kSearchSubPage);
static_assert(std::string_view(kSyncSetupSubPage) ==
              ash::chrome_urls::kSyncSetupSubPage);

bool IsSystemWebUIHost(std::string_view host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  constexpr auto kHosts = base::MakeFixedFlatSet<std::string_view>({
      ash::kChromeUIAccountManagerErrorHost,
      ash::kChromeUIAccountMigrationWelcomeHost,
      ash::kChromeUIAddSupervisionHost,
      ash::kChromeUIAppInstallDialogHost,
      ash::kChromeUIBluetoothPairingHost,
      ash::kChromeUIBorealisCreditsHost,
      ash::kChromeUIBorealisInstallerHost,
      ash::kChromeUIBorealisMOTDHost,
      kChromeUICertificateManagerHost,
      ash::kChromeUICloudUploadHost,
      ash::kChromeUICrostiniCreditsHost,
      ash::kChromeUICrostiniInstallerHost,
      ash::kChromeUICryptohomeHost,
      ash::kChromeUIDeviceEmulatorHost,
      ash::kChromeUIEmojiPickerHost,
      ash::kChromeUIExtendedUpdatesDialogHost,
      ash::kChromeUIInternetConfigDialogHost,
      ash::kChromeUIInternetDetailDialogHost,
      ash::kChromeUILockScreenNetworkHost,
      ash::kChromeUILockScreenStartReauthHost,
      ash::kChromeUIMobileSetupHost,
      ash::kChromeUIMultiDeviceSetupHost,
      ash::kChromeUINetworkHost,
      ash::kChromeUINotificationTesterHost,
      ash::kChromeUIOobeHost,
      ash::kChromeUIOSCreditsHost,
      ash::kChromeUIOSSettingsHost,
      ash::kChromeUIPasswordChangeHost,
      ash::kChromeUIPowerHost,
      ash::kChromeUISetTimeHost,
      ash::kChromeUISmbCredentialsHost,
      ash::kChromeUISmbShareHost,
  });

  return kHosts.contains(host);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
base::span<const base::cstring_view> ChromeURLHosts() {
  static constexpr auto kChromeURLHosts = std::to_array<base::cstring_view>({
      kChromeUIAboutHost,
      kChromeUIAccessibilityHost,
      kChromeUIActorInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIAppServiceInternalsHost,
#endif
      kChromeUIAutofillInternalsHost,
      kChromeUIBluetoothInternalsHost,
      kChromeUIBrowsingTopicsInternalsHost,
      kChromeUIChromeFindsInternalsHost,
      kChromeUIChromeURLsHost,
      kChromeUIComponentsHost,
      commerce::kChromeUICommerceInternalsHost,
      kChromeUIConnectorsInternalsHost,
      kChromeUICrashesHost,
      kChromeUICreditsHost,
#if BUILDFLAG(IS_CHROMEOS) && !defined(OFFICIAL_BUILD)
      ash::kChromeUIDeviceEmulatorHost,
#endif
      kChromeUIDeviceLogHost,
      kChromeUIDownloadInternalsHost,
      kChromeUIFamilyLinkUserInternalsHost,
      kChromeUIFlagsHost,
      kChromeUIGCMInternalsHost,
      kChromeUIHistoryHost,
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost,
      kChromeUIInterstitialHost,
      kChromeUILocalStateHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIManagementHost,
#endif
      kChromeUIMediaEngagementHost,
      kChromeUIMetricsInternalsHost,
      kChromeUINetExportHost,
      kChromeUINetInternalsHost,
      kChromeUINewTabHost,
      kChromeUIOmniboxHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIOnDeviceInternalsHost,
#endif
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost,
      kChromeUIPasswordManagerInternalsHost,
      password_manager::kChromeUIPasswordManagerHost,
      kChromeUIPolicyHost,
      kChromeUIPredictorsHost,
      kChromeUIPrefsInternalsHost,
      kChromeUIProfileInternalsHost,
      content::kChromeUIQuotaInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIWebUIToolbarHost,
#endif
      kChromeUISignInInternalsHost,
      kChromeUISiteEngagementHost,
      kChromeUISkillsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUISuggestInternalsHost,
#endif
      kChromeUINTPTilesInternalsHost,
      safe_browsing::kChromeUISafeBrowsingHost,
      kChromeUISyncInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUITabSearchHost,
      kChromeUITermsHost,
#endif
      kChromeUITranslateInternalsHost,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
      kChromeUIUpdaterHost,
#endif
      kChromeUIUsbInternalsHost,
      kChromeUIUserActionsHost,
      kChromeUIVersionHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIWebAppInternalsHost,
#endif
      content::kChromeUIPrivateAggregationInternalsHost,
      content::kChromeUIAttributionInternalsHost,
      content::kChromeUIBlobInternalsHost,
      content::kChromeUIDinoHost,
      content::kChromeUIGpuHost,
      content::kChromeUIHistogramHost,
      content::kChromeUIIndexedDBInternalsHost,
      content::kChromeUIMediaInternalsHost,
      content::kChromeUINetworkErrorsListingHost,
      content::kChromeUIProcessInternalsHost,
      content::kChromeUIServiceWorkerInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      content::kChromeUITracingHost,
#endif
      content::kChromeUIUkmHost,
      content::kChromeUIWebRTCInternalsHost,
#if BUILDFLAG(ENABLE_VR)
      content::kChromeUIWebXrInternalsHost,
#endif
#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
      kChromeUIAppLauncherPageHost,
#endif
      kChromeUIBookmarksHost,
      kChromeUIDownloadsHost,
      kChromeUIHelpHost,
      kChromeUIInspectHost,
      kChromeUINewTabPageHost,
      kChromeUINewTabPageThirdPartyHost,
#if BUILDFLAG(IS_ANDROID)
      kChromeUINotificationsInternalsHost,
#endif
      kChromeUISettingsHost,
      kChromeUISystemInfoHost,
#if !BUILDFLAG(IS_CHROMEOS)
      kChromeUIWhatsNewHost,
#endif
#endif
#if BUILDFLAG(IS_ANDROID)
      kChromeUISnippetsInternalsHost,
      kChromeUIWebApksHost,
#endif
#if BUILDFLAG(IS_CHROMEOS)
      ash::kChromeUIBorealisCreditsHost,
      kChromeUICertificateManagerHost,
      ash::kChromeUICrostiniCreditsHost,
      ash::kChromeUICryptohomeHost,
      ash::kChromeUIDriveInternalsHost,
      ash::kChromeUINetworkHost,
      ash::kChromeUILockScreenNetworkHost,
      ash::kChromeUIOobeHost,
      ash::kChromeUIOSCreditsHost,
      ash::kChromeUIOSSettingsHost,
      ash::kChromeUIPowerHost,
      ash::kChromeUISysInternalsHost,
      ash::kChromeUIInternetConfigDialogHost,
      ash::kChromeUIInternetDetailDialogHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
      kChromeUIDiscardsHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      kChromeUIWebAppSettingsHost,
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
      kChromeUILinuxProxyConfigHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
      kChromeUISandboxHost,
#endif
#if BUILDFLAG(IS_WIN)
      kChromeUIConflictsHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      kChromeUIExtensionsHost,
      kChromeUIExtensionsInternalsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      kChromeUIPrintHost,
#endif
      kChromeUIWebRtcLogsHost,
      kChromeUIWebNNInternalsHost,
#if BUILDFLAG(IS_CHROMEOS)
      ash::kChromeUIDlpInternalsHost,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIWebuiBrowserHost,
#endif  // !BUILDFLAG(IS_ANDROID)
  });

  return base::span(kChromeURLHosts);
}

base::span<const base::cstring_view> ChromeDebugURLs() {
  // TODO(crbug.com/40253037): make this list comprehensive
  static constexpr auto kChromeDebugURLs = std::to_array<base::cstring_view>(
      {blink::kChromeUIBadCastCrashURL,
       blink::kChromeUIBrowserCrashURL,
       blink::kChromeUIBrowserDcheckURL,
       blink::kChromeUICrashURL,
       blink::kChromeUICrashRustURL,
#if defined(ADDRESS_SANITIZER)
       blink::kChromeUICrashRustOverflowURL,
#endif
       blink::kChromeUIDumpURL,
       blink::kChromeUIKillURL,
       blink::kChromeUIHangURL,
       blink::kChromeUIShorthangURL,
       blink::kChromeUIGpuCleanURL,
       blink::kChromeUIGpuCrashURL,
       blink::kChromeUIGpuHangURL,
       blink::kChromeUIMemoryExhaustURL,
       blink::kChromeUIMemoryPressureCriticalURL,
       blink::kChromeUIMemoryPressureModerateURL,
#if BUILDFLAG(IS_WIN)
       blink::kChromeUIBrowserHeapCorruptionURL,
       blink::kChromeUICfgViolationCrashURL,
       blink::kChromeUIHeapCorruptionCrashURL,
#endif
#if BUILDFLAG(IS_ANDROID)
       blink::kChromeUIGpuJavaCrashURL,
       kChromeUIJavaCrashURL,
#else
       kChromeUIWebUIJsErrorURL,
#endif  // BUILDFLAG(IS_ANDROID)
       kChromeUIQuitURL,
       kChromeUIRestartURL});

  return base::span(kChromeDebugURLs);
}

}  // namespace chrome
