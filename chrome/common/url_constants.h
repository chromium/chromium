// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/buildflags/buildflags.h"

namespace chrome {

// "Learn more" URL for accessibility image labels, linked from the permissions
// dialog shown when a user enables the feature.
extern const char kAccessibilityLabelsLearnMoreURL[];

// "Learn more" URL for when profile settings are automatically reset.
extern const char kAutomaticSettingsResetLearnMoreURL[];

// "Learn more" URL for Advanced Protection download warnings.
extern const char kAdvancedProtectionDownloadLearnMoreURL[];

// The URL for providing help when the Bluetooth adapter is off.
extern const char kBluetoothAdapterOffHelpURL[];

// "Learn more" URL shown in the dialog to enable cloud services for Cast.
extern const char kCastCloudServicesHelpURL[];

// The URL for the help center article to show when no Cast destination has been
// found.
extern const char kCastNoDestinationFoundURL[];

// The URL for the Bluetooth Overview help center article in the Web Bluetooth
// Chooser.
extern const char kChooserBluetoothOverviewURL[];

// The URL for the WebUsb help center article.
extern const char kChooserUsbOverviewURL[];

// Link to the forum for Chrome Beta.
extern const char kChromeBetaForumURL[];

// Link to the release notes page managed by marketing.
extern const char kChromeReleaseNotesURL[];

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];
#if defined(OS_CHROMEOS)
extern const char kChromeOsHelpViaWebUIURL[];
#endif

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
extern const char kChromeNativeScheme[];

// Pages under chrome-search.
extern const char kChromeSearchLocalNtpHost[];
extern const char kChromeSearchLocalNtpUrl[];

// Host and URL for most visited iframes used on the Instant Extended NTP.
extern const char kChromeSearchMostVisitedHost[];
extern const char kChromeSearchMostVisitedUrl[];

// URL for NTP custom background image selected from the user's machine and
// filename for the version of the file in the Profile directory
extern const char kChromeSearchLocalNtpBackgroundUrl[];
extern const char kChromeSearchLocalNtpBackgroundFilename[];

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

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];

// "Learn more" URL for the Cloud Print Preview certificate error.
extern const char kCloudPrintCertificateErrorLearnMoreURL[];

extern const char kContentSettingsExceptionsLearnMoreURL[];

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

// URL of the 'Activity controls' section of the privacy settings page.
extern const char kGoogleAccountActivityControlsURL[];

// URL of the Google Account.
extern const char kGoogleAccountURL[];

// URL of the Google Account chooser.
extern const char kGoogleAccountChooserURL[];

// URL of the Google Password Manager.
extern const char kGooglePasswordManagerURL[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

// Management URL for Chrome Supervised Users - version without scheme, used
// for display.
extern const char kLegacySupervisedUserManagementDisplayURL[];

// Management URL for Chrome Supervised Users.
extern const char kLegacySupervisedUserManagementURL[];

// The URL for the Learn More page about policies and enterprise enrollment.
extern const char kManagedUiLearnMoreUrl[];

// "myactivity.google.com" URL for the history checkbox in ClearBrowsingData.
extern const char kMyActivityUrlInClearBrowsingData[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

extern const char kPasswordManagerLearnMoreURL[];

// Help URL for the Payment methods page of the Google Pay site.
extern const char kPaymentMethodsURL[];

extern const char kPaymentMethodsLearnMoreURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// The URL for the Learn More link of the non-CWS bubble.
extern const char kRemoveNonCWSExtensionURL[];

// "Learn more" URL for resetting profile preferences.
extern const char kResetProfileSettingsLearnMoreURL[];

// "Learn more" URL for safety tip bubble.
extern const char kSafetyTipHelpCenterURL[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

// URL to use as the 'Learn More' link when the interstitial is caused by
// a "ERR_CERT_SYMANTEC_LEGACY" error, -202 fragment is included so
// chrome://connection-help expands the right section if the user can't reach
// the help center.
extern const char kSymantecSupportUrl[];

// The URL for the Learn More page about Sync and Google services.
extern const char kSyncAndGoogleServicesLearnMoreURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

extern const char kSyncGoogleDashboardURL[];

// The URL for the "Learn more" page for sync setup on the personal stuff page.
extern const char kSyncLearnMoreURL[];

extern const char kUpgradeHelpCenterBaseURL[];

#if defined(OS_ANDROID)
extern const char kAndroidAppScheme[];
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
// "Learn more" URL for the enhanced playback notification dialog.
extern const char kEnhancedPlaybackNotificationLearnMoreURL[];
#endif

#if defined(OS_CHROMEOS)
// Help center URL for Chrome OS Account Manager.
extern const char kAccountManagerLearnMoreURL[];

// The URL for the "learn more" link for Google Play Store (ARC) settings.
extern const char kAndroidAppsLearnMoreURL[];

// The URL for the "Learn more" link in the External storage preferences
// settings.
extern const char kArcExternalStorageLearnMoreURL[];

// The path format to the localized offline ARC++ Privacy Policy.
// Relative to |kChromeOSAssetPath|.
extern const char kArcPrivacyPolicyPathFormat[];

// The path format to the localized offline ARC++ Terms of Service.
// Relative to |kChromeOSAssetPath|.
extern const char kArcTermsPathFormat[];

// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];

extern const char kChromeOSAssetHost[];
extern const char kChromeOSAssetPath[];

extern const char kChromeOSCreditsPath[];

// Palette help link for Chrome.
extern const char kChromePaletteHelpURL[];

extern const char kCrosScheme[];

extern const char kCupsPrintLearnMoreURL[];

extern const char kCupsPrintPPDLearnMoreURL[];

// The URL for the "Learn more" link the the Easy Unlock settings.
extern const char kEasyUnlockLearnMoreUrl[];

// The path to the offline Chrome OS EULA.
extern const char kEULAPathFormat[];

// The URL for EOL notification
extern const char kEolNotificationURL[];

// The URL for providing more information about Google nameservers.
extern const char kGoogleNameserversLearnMoreURL[];

// The URL for the "learn more" link for Instant Tethering.
extern const char kInstantTetheringLearnMoreURL[];

// The URL for the "Learn more" link for Kerberos accounts.
extern const char kKerberosAccountsLearnMoreURL[];

// The URL for the "Learn more" link in the connected devices.
extern const char kMultiDeviceLearnMoreURL[];

// The URL for the "Learn more" link for Android Messages.
extern const char kAndroidMessagesLearnMoreURL[];

// The URL for the "Learn more" link in the language settings.
extern const char kLanguageSettingsLearnMoreUrl[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];

// The URL for the Learn More page about Linux for Chromebooks.
extern const char kLinuxAppsLearnMoreURL[];

// The URL for additional help that is given when Linux export/import fails.
extern const char kLinuxExportImportHelpURL[];

// Credits for Linux for Chromebooks.
extern const char kLinuxCreditsPath[];

// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

// The URL path to offline OEM EULA.
extern const char kOemEulaURLPath[];

// The URL path to offline ARC++ Terms of Service.
extern const char kArcTermsURLPath[];

// The URL path to offline ARC++ Privacy Policy.
extern const char kArcPrivacyPolicyURLPath[];

extern const char kOnlineEulaURLPath[];

// The URL for the "learn more" link for TPM firmware update.
extern const char kTPMFirmwareUpdateLearnMoreURL[];

// The URL for the "Learn more" page for the time zone settings page.
extern const char kTimeZoneSettingsLearnMoreURL[];

// The URL for the "Learn more" page for the network file shares settings page.
extern const char kSmbSharesLearnMoreURL[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// "Learn more" URL for the enterprise sign-in confirmation dialog.
extern const char kChromeEnterpriseSignInLearnMoreURL[];

// The URL for the "learn more" link on the 10.9 obsolescence infobar.
extern const char kMac10_9_ObsoleteURL[];
#endif

#if defined(OS_WIN)
// The URL for the Learn More link in the Chrome Cleanup settings card.
extern const char kChromeCleanerLearnMoreURL[];

// The URL for the Windows XP/Vista deprecation help center article.
extern const char kWindowsXPVistaDeprecationURL[];
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// The URL for the "Learn more" page for the blocked plugin infobar.
extern const char kBlockedPluginLearnMoreURL[];

// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];
#endif

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
