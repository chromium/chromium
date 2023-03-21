// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.
// Except for WebUI UI/Host/SubPage constants. Those go in
// chrome/common/webui_url_constants.h.
//
// - The constants are divided into sections: Cross platform, platform-specific,
//   and feature-specific.
// - When adding platform/feature specific constants, if there already exists an
//   appropriate #if block, use that.
// - Keep the constants sorted by name within its section.
// - Use the same order in this header and url_constants.cc.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "net/net_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

namespace chrome {

// "Learn more" URL linked in the dialog to cast using a code.
extern const char kAccessCodeCastLearnMoreURL[];

// "Learn more" URL for accessibility image labels, linked from the permissions
// dialog shown when a user enables the feature.
extern const char kAccessibilityLabelsLearnMoreURL[];

// "Learn more" URL for Ad Privacy.
extern const char kAdPrivacyLearnMoreURL[];

// "Learn more" URL for when profile settings are automatically reset.
extern const char kAutomaticSettingsResetLearnMoreURL[];

// "Learn more" URL for Advanced Protection download warnings.
extern const char kAdvancedProtectionDownloadLearnMoreURL[];

// "Chrome Settings" URL for website notifications linked out from OSSettings.
extern const char kAppNotificationsBrowserSettingsURL[];

// "Learn more" URL for Battery Saver Mode.
extern const char kBatterySaverModeLearnMoreUrl[];

// The URL for providing help when the Bluetooth adapter is off.
extern const char kBluetoothAdapterOffHelpURL[];

// "Learn more" URL shown in the dialog to enable cloud services for Cast.
extern const char kCastCloudServicesHelpURL[];

// The URL for the help center article to show when no Cast destination has been
// found.
extern const char kCastNoDestinationFoundURL[];

// The URL for the WebHID API help center article.
extern const char kChooserHidOverviewUrl[];

// The URL for the Web Serial API help center article.
extern const char kChooserSerialOverviewUrl[];

// The URL for the WebUsb help center article.
extern const char kChooserUsbOverviewURL[];

// Link to the forum for Chrome Beta.
extern const char kChromeBetaForumURL[];

// The URL for the help center article to fix Chrome update problems.
extern const char kChromeFixUpdateProblems[];

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kChromeOsHelpViaWebUIURL[];
#endif

// The isolated-app: scheme is used for Isolated Web Apps. A public explainer
// can be found here: https://github.com/reillyeon/isolated-web-apps
extern const char kIsolatedAppScheme[];

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
extern const char kChromeNativeScheme[];

// Pages under chrome-search.
extern const char kChromeSearchLocalNtpHost[];

// Host and URL for most visited iframes used on the Instant Extended NTP.
extern const char kChromeSearchMostVisitedHost[];
extern const char kChromeSearchMostVisitedUrl[];

// URL for NTP custom background image selected from the user's machine and
// filename for the version of the file in the Profile directory
extern const char kChromeUIUntrustedNewTabPageBackgroundUrl[];
extern const char kChromeUIUntrustedNewTabPageBackgroundFilename[];

// Page under chrome-search.
extern const char kChromeSearchRemoteNtpHost[];

// The chrome-search: scheme is served by the same backend as chrome:.  However,
// only specific URLDataSources are enabled to serve requests via the
// chrome-search: scheme.  See |InstantIOContext::ShouldServiceRequest| and its
// callers for details.  Note that WebUIBindings should never be granted to
// chrome-search: pages.  chrome-search: pages are displayable but not readable
// by external search providers (that are rendered by Instant renderer
// processes), and neither displayable nor readable by normal (non-Instant) web
// pages.  To summarize, a non-Instant process, when trying to access
// 'chrome-search://something', will bump up against the following:
//
//  1. Renderer: The display-isolated check in WebKit will deny the request,
//  2. Browser: Assuming they got by #1, the scheme checks in
//     URLDataSource::ShouldServiceRequest will deny the request,
//  3. Browser: for specific sub-classes of URLDataSource, like ThemeSource
//     there are additional Instant-PID checks that make sure the request is
//     coming from a blessed Instant process, and deny the request.
extern const char kChromeSearchScheme[];

// This is the base URL of content that can be embedded in chrome://new-tab-page
// using an <iframe>. The embedded untrusted content can make web requests and
// can include content that is from an external source.
extern const char kChromeUIUntrustedNewTabPageUrl[];

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

extern const char kContentSettingsExceptionsLearnMoreURL[];

// "Learn more" URL for cookies.
extern const char kCookiesSettingsHelpCenterURL[];

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
extern const char kCrashReasonURL[];

// "Learn more" URL for "Aw snap" page when showing "Send feedback" button.
extern const char kCrashReasonFeedbackDisplayedURL[];

// "Learn more" URL for the "Do not track" setting in the privacy section.
extern const char kDoNotTrackLearnMoreURL[];

// The URL for the "Learn more" page for interrupted downloads.
extern const char kDownloadInterruptedLearnMoreURL[];

// The URL for the "Learn more" page for download scanning.
extern const char kDownloadScanningLearnMoreURL[];

// "Learn more" URL for the Settings API, NTP bubble and other settings bubbles
// showing which extension is controlling them.
extern const char kExtensionControlledSettingLearnMoreURL[];

// URL used to indicate that an extension resource load request was invalid.
extern const char kExtensionInvalidRequestURL[];

// "Learn more" URL for first party sets.
extern const char kFirstPartySetsLearnMoreURL[];

// Url to a blogpost about Flash deprecation.
extern const char kFlashDeprecationLearnMoreURL[];

// URL of the Google account language selection page.
extern const char kGoogleAccountLanguagesURL[];

// URL of the 'Activity controls' section of the privacy settings page.
extern const char kGoogleAccountActivityControlsURL[];

// URL of the 'Activity controls' section of the privacy settings page, with
// privacy guide parameters and a link for users to manage data.
extern const char kGoogleAccountActivityControlsURLInPrivacyGuide[];

// URL of the Google Account.
extern const char kGoogleAccountURL[];

// URL of the Google Account chooser.
extern const char kGoogleAccountChooserURL[];

// URL of the Google Account page showing the known user devices.
extern const char kGoogleAccountDeviceActivityURL[];

// URL of the Google Password Manager.
extern const char kGooglePasswordManagerURL[];

// URL of the Google Photos.
extern const char kGooglePhotosURL[];

// The URL for the "Learn more" link for the High Efficiency Mode.
extern const char kHighEfficiencyModeLearnMoreUrl[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

// The URL for the Learn More page about policies and enterprise enrollment.
extern const char kManagedUiLearnMoreUrl[];

// The URL for the "Learn more" page for insecure download blocking.
extern const char kInsecureDownloadBlockingLearnMoreUrl[];

// "myactivity.google.com" URL for the history checkbox in ClearBrowsingData.
extern const char kMyActivityUrlInClearBrowsingData[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// Help URL for the bulk password check.
extern const char kPasswordCheckLearnMoreURL[];

// Help URL for password generation.
extern const char kPasswordGenerationLearnMoreURL[];

extern const char kPasswordManagerLearnMoreURL[];

// Help URL for the Payment methods page of the Google Pay site.
extern const char kPaymentMethodsURL[];

// The URL for the "Fill out forms automatically" support page.
extern const char kAddressesAndPaymentMethodsLearnMoreURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// The URL for the Learn More link of the non-CWS bubble.
extern const char kRemoveNonCWSExtensionURL[];

// "Learn more" URL for resetting profile preferences.
extern const char kResetProfileSettingsLearnMoreURL[];

// "Learn more" URL for Safebrowsing
extern const char kSafeBrowsingHelpCenterURL[];

// "Learn more" URL for safety tip bubble.
extern const char kSafetyTipHelpCenterURL[];

// Google search history URL that leads users of the CBD dialog to their search
// history in their Google account.
extern const char kSearchHistoryUrlInClearBrowsingData[];

// The URL for the "See more security tips" with advices how to create a strong
// password.
extern const char kSeeMoreSecurityTipsURL[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

// The URL for the Learn More page about Sync and Google services.
extern const char kSyncAndGoogleServicesLearnMoreURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

extern const char kSyncGoogleDashboardURL[];

// The URL for the "Learn more" page for sync setup on the personal stuff page.
extern const char kSyncLearnMoreURL[];

// The URL for the "Learn more" link in the enterprise disclaimer for managed
// profile in the Signin Intercept bubble.
extern const char kSigninInterceptManagedDisclaimerLearnMoreURL[];

#if !BUILDFLAG(IS_ANDROID)
// The URL for the trusted vault sync passphrase opt in.
extern const char kSyncTrustedVaultOptInURL[];
#endif

// The URL for the "Learn more" link for the trusted vault sync passphrase.
extern const char kSyncTrustedVaultLearnMoreURL[];

extern const char kUpgradeHelpCenterBaseURL[];

// The URL for the "Learn more" link for nearby share.
extern const char kNearbyShareLearnMoreURL[];

// Help center URL for who the account administrator is.
extern const char kWhoIsMyAdministratorHelpURL[];

// The URL for the "Learn more" link about CWS Enhanced Safe Browsing.
extern const char kCwsEnhancedSafeBrowsingLearnMoreURL[];

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
// "Learn more" URL for the enhanced playback notification dialog.
extern const char kEnhancedPlaybackNotificationLearnMoreURL[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Chrome OS default pre-defined custom handlers
extern const char kChromeOSDefaultMailtoHandler[];
extern const char kChromeOSDefaultWebcalHandler[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Help center URL for Chrome OS Account Manager.
extern const char kAccountManagerLearnMoreURL[];

// The URL for the "Account recovery" page.
extern const char kAccountRecoveryURL[];

// The URL for the "How to add a new user account on a Chromebook" page.
extern const char kAddNewUserURL[];

// The URL for the "learn more" link for Google Play Store (ARC) settings.
extern const char kAndroidAppsLearnMoreURL[];

// Help center URL for ARC ADB sideloading.
extern const char kArcAdbSideloadingLearnMoreURL[];

// The URL for the "Learn more" link in the External storage preferences
// settings.
extern const char kArcExternalStorageLearnMoreURL[];

// The path format to the localized offline ARC++ Privacy Policy.
// Relative to |kChromeOSAssetPath|.
extern const char kArcPrivacyPolicyPathFormat[];

// The path format to the localized offline ARC++ Terms of Service.
// Relative to |kChromeOSAssetPath|.
extern const char kArcTermsPathFormat[];

// The URL for the "Learn more" link during Bluetooth pairing.
extern const char kBluetoothPairingLearnMoreUrl[];

// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];

extern const char kChromeOSAssetHost[];
extern const char kChromeOSAssetPath[];

extern const char kChromeOSCreditsPath[];

// Chrome OS tablet gestures education help link for Chrome.
extern const char kChromeOSGestureEducationHelpURL[];

// Palette help link for Chrome.
extern const char kChromePaletteHelpURL[];

extern const char kCupsPrintLearnMoreURL[];

extern const char kCupsPrintPPDLearnMoreURL[];

// The URL for the "Learn more" link the the Easy Unlock settings.
extern const char kEasyUnlockLearnMoreUrl[];

// The URL for the help center article about redeeming Chromebook offers.
extern const char kEchoLearnMoreURL[];

// The URL for EOL notification
extern const char kEolNotificationURL[];

// The URL for the EOL incentive with offer.
extern const char kEolIncentiveNotificationOfferURL[];

// The URL for the EOL incentive with no offer.
extern const char kEolIncentiveNotificationNoOfferURL[];

// The URL for Auto Update Policy.
extern const char kAutoUpdatePolicyURL[];

// The URL for providing more information about Google nameservers.
extern const char kGoogleNameserversLearnMoreURL[];

// The URL for the "learn more" link for Instant Tethering.
extern const char kInstantTetheringLearnMoreURL[];

// The URL for the "Learn more" link for Kerberos accounts.
extern const char kKerberosAccountsLearnMoreURL[];

// The URL for the "Learn more" link in the language settings.
extern const char kLanguageSettingsLearnMoreUrl[];

// The URL for the "Learn more" link in language settings regarding language
// packs.
extern const char kLanguagePacksLearnMoreURL[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];

// The URL for the Learn More page about Linux for Chromebooks.
extern const char kLinuxAppsLearnMoreURL[];

// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

// The URL for the "Learn more" link for touchpad haptic feedback on Chrome OS.
extern const char kHapticFeedbackHelpURL[];

// The URL path to offline OEM EULA.
extern const char kOemEulaURLPath[];

// Help URL for the OS settings page's search feature.
extern const char kOsSettingsSearchHelpURL[];

// The URL for the "Learn more" link in the peripheral data access protection
// settings.
extern const char kPeripheralDataAccessHelpURL[];

// The URL for the "Learn more" link for Enhanced network voices in Chrome OS
// settings for Select-to-speak.
extern const char kSelectToSpeakLearnMoreURL[];

// The URL path to offline ARC++ Terms of Service.
extern const char kArcTermsURLPath[];

// The URL path to offline ARC++ Privacy Policy.
extern const char kArcPrivacyPolicyURLPath[];

// The URL path to Online Google EULA.
extern const char kGoogleEulaOnlineURLPath[];

// The URL path to Online Chrome and Chrome OS terms of service.
extern const char kCrosEulaOnlineURLPath[];

// The URL path to online ARC++ terms of service.
extern const char kArcTosOnlineURLPath[];

// The URL path to online privacy policy.
extern const char kPrivacyPolicyOnlineURLPath[];

// The URL for the "learn more" link for TPM firmware update.
extern const char kTPMFirmwareUpdateLearnMoreURL[];

// The URL for the "Learn more" page for the time zone settings page.
extern const char kTimeZoneSettingsLearnMoreURL[];

// The URL for the "Learn more" page for screen privacy protections.
extern const char kSmartPrivacySettingsLearnMoreURL[];

// The URL for the "Learn more" page for the network file shares settings page.
extern const char kSmbSharesLearnMoreURL[];

// The URL for the "Learn more" page for Suggested Content in the privacy page.
extern const char kSuggestedContentLearnMoreURL[];

// The URL to a support article with more information about gestures available
// in tablet mode on Chrome OS (gesture to go to home screen, overview, or to go
// back). Used as a "Learn more" link URL for the accessibility option to shelf
// navigation buttons in tablet mode (the buttons are hidden by default in
// favour of the gestures in question).
extern const char kTabletModeGesturesLearnMoreURL[];

// The URL for the help center article about Wi-Fi sync.
extern const char kWifiSyncLearnMoreURL[];

// The URL for the help center article about hidden Wi-Fi networks.
extern const char kWifiHiddenNetworkURL[];

// The URL for the help center article about Passpoint.
extern const char kWifiPasspointURL[];

// The URL for contacts management in Nearby Share feature.
extern const char kNearbyShareManageContactsURL[];

// The URL for the help center article about fingerprint on Chrome OS devices.
extern const char kFingerprintLearnMoreURL[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
// "Learn more" URL for the enterprise sign-in confirmation dialog.
extern const char kChromeEnterpriseSignInLearnMoreURL[];

// The URL for the "learn more" link on the macOS version obsolescence infobar.
extern const char kMacOsObsoleteURL[];
#endif

#if BUILDFLAG(IS_WIN)
// The URL for the Learn More link in the Chrome Cleanup settings card.
extern const char kChromeCleanerLearnMoreURL[];

// The URL for the Windows XP/Vista deprecation help center article.
extern const char kWindowsXPVistaDeprecationURL[];

// The URL for the Windows 7/8.1 deprecation help center article.
extern const char kWindows78DeprecationURL[];
#endif

// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];

#if BUILDFLAG(ENABLE_PLUGINS)

// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];
#endif

// "Learn more" URL for the phone hub notifications and apps access setup.
extern const char kPhoneHubPermissionLearnMoreURL[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)

// "Learn more" URL for the chrome apps deprecation dialog.
extern const char kChromeAppsDeprecationLearnMoreURL[];
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
extern const char kChromeRootStoreSettingsHelpCenterURL[];
#endif

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
