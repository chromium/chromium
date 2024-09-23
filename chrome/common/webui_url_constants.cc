// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/lens/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

namespace chrome {

// Note: Add hosts to `ChromeURLHosts()` at the bottom of this file to be listed
// by chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

#if BUILDFLAG(IS_CHROMEOS_ASH)

bool IsSystemWebUIHost(std::string_view host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  constexpr auto kHosts = base::MakeFixedFlatSet<std::string_view>({
      kChromeUIAccountManagerErrorHost,
      kChromeUIAccountMigrationWelcomeHost,
      kChromeUIAddSupervisionHost,
      kChromeUIAppInstallDialogHost,
      kChromeUIAssistantOptInHost,
      kChromeUIBluetoothPairingHost,
      kChromeUIBorealisCreditsHost,
      kChromeUIBorealisInstallerHost,
      kChromeUICertificateManagerHost,
      kChromeUICloudUploadHost,
      kChromeUICrostiniCreditsHost,
      kChromeUICrostiniInstallerHost,
      kChromeUICryptohomeHost,
      kChromeUIDeviceEmulatorHost,
      kChromeUIEmojiPickerHost,
      kChromeUIExtendedUpdatesDialogHost,
      kChromeUIInternetConfigDialogHost,
      kChromeUIInternetDetailDialogHost,
      kChromeUILockScreenNetworkHost,
      kChromeUILockScreenStartReauthHost,
      kChromeUIMobileSetupHost,
      kChromeUIMultiDeviceSetupHost,
      kChromeUINetworkHost,
      kChromeUINotificationTesterHost,
      kChromeUIOobeHost,
      kChromeUIOSCreditsHost,
      kChromeUIOSSettingsHost,
      kChromeUIPasswordChangeHost,
      kChromeUIPowerHost,
      kChromeUISetTimeHost,
      kChromeUISmbCredentialsHost,
      kChromeUISmbShareHost,
      kChromeUIVcTrayTesterHost,
  });

  return kHosts.contains(host);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
base::span<const base::cstring_view> ChromeURLHosts() {
  static constexpr auto kChromeURLHosts = std::to_array<base::cstring_view>({
      kChromeUIAboutHost,
      kChromeUIAccessibilityHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIAppServiceInternalsHost,
#endif
      kChromeUIAutofillInternalsHost,
      kChromeUIBluetoothInternalsHost,
      kChromeUIBrowsingTopicsInternalsHost,
      kChromeUIChromeURLsHost,
      kChromeUIComponentsHost,
      commerce::kChromeUICommerceInternalsHost,
      kChromeUICrashesHost,
      kChromeUICreditsHost,
#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)
      kChromeUIDeviceEmulatorHost,
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
      kChromeUISignInInternalsHost,
      kChromeUISiteEngagementHost,
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
      kChromeUISettingsHost,
      kChromeUISystemInfoHost,
#if !BUILDFLAG(IS_CHROMEOS)
      kChromeUIWhatsNewHost,
#endif
#endif
#if BUILDFLAG(IS_ANDROID)
      kChromeUIOfflineInternalsHost,
      kChromeUISnippetsInternalsHost,
      kChromeUIWebApksHost,
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      kChromeUIBorealisCreditsHost,
      kChromeUICertificateManagerHost,
      kChromeUICrostiniCreditsHost,
      kChromeUICryptohomeHost,
      kChromeUIDriveInternalsHost,
      kChromeUINetworkHost,
      kChromeUILockScreenNetworkHost,
      kChromeUIOobeHost,
      kChromeUIOSCreditsHost,
      kChromeUIOSSettingsHost,
      kChromeUIPowerHost,
      kChromeUISysInternalsHost,
      kChromeUIInternetConfigDialogHost,
      kChromeUIInternetDetailDialogHost,
      kChromeUIAssistantOptInHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
      kChromeUIConnectorsInternalsHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(ENABLE_NACL)
      kChromeUINaClHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
      kChromeUIExtensionsHost,
      kChromeUIExtensionsInternalsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      kChromeUIPrintHost,
#endif
      kChromeUIWebRtcLogsHost,
#if BUILDFLAG(IS_CHROMEOS)
      kChromeUIDlpInternalsHost,
#endif  // BUILDFLAG(IS_CHROMEOS)
  });

  return base::span(kChromeURLHosts);
}

// Add chrome://internals/* subpages here to be included in chrome://chrome-urls
// (about:about).
base::span<const base::cstring_view> ChromeInternalsURLPaths() {
  static constexpr auto kChromeInternalsURLPaths =
      std::to_array<base::cstring_view>({
#if BUILDFLAG(IS_ANDROID)
          kChromeUIInternalsQueryTilesPath,
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
          kChromeUISessionServiceInternalsPath,
#endif
      });

  return base::span(kChromeInternalsURLPaths);
}

base::span<const base::cstring_view> ChromeDebugURLs() {
  // TODO(crbug.com/40253037): make this list comprehensive
  static constexpr auto kChromeDebugURLs = std::to_array<base::cstring_view>(
      {blink::kChromeUIBadCastCrashURL,
       blink::kChromeUIBrowserCrashURL,
       blink::kChromeUIBrowserDcheckURL,
       blink::kChromeUICrashURL,
#if BUILDFLAG(ENABLE_RUST_CRASH)
       blink::kChromeUICrashRustURL,
#if defined(ADDRESS_SANITIZER)
       blink::kChromeUICrashRustOverflowURL,
#endif
#endif  // BUILDFLAG(ENABLE_RUST_CRASH)
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
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
       kChromeUIWebUIJsErrorURL,
#endif
       kChromeUIQuitURL,
       kChromeUIRestartURL});

  return base::span(kChromeDebugURLs);
}

}  // namespace chrome
