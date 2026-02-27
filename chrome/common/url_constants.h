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

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include <stddef.h>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/url_constants.h"
#include "net/net_buildflags.h"

namespace chrome {

// "Learn more" URL linked in the dialog to cast using a code.
inline constexpr char kAccessCodeCastLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=cast_to_class_teacher";

// "Learn more" URL for accessibility image labels, linked from the permissions
// dialog shown when a user enables the feature.
inline constexpr char kAccessibilityLabelsLearnMoreURL[] =
    "https://support.google.com/chrome?p=image_descriptions";

// "Learn more" URL for Ad Privacy.
inline constexpr char kAdPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome?p=ad_privacy";

// "Learn more" URL for when profile settings are automatically reset.
inline constexpr char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome?p=ui_automatic_settings_reset";

// "Learn more" URL for Advanced Protection download warnings.
inline constexpr char kAdvancedProtectionDownloadLearnMoreURL[] =
    "https://support.google.com/accounts/accounts?p=safe-browsing";

// "Chrome Settings" URL for website notifications linked out from OSSettings.
inline constexpr char kAppNotificationsBrowserSettingsURL[] =
    "chrome://settings/content/notifications";

// "Chrome Settings" URL for the appearance page.
inline constexpr char kBrowserSettingsSearchEngineURL[] =
    "chrome://settings/search";

// "Learn more" URL for Battery Saver Mode.
inline constexpr const char16_t kBatterySaverModeLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=chrome_battery_saver";

// The URL for providing help when the Bluetooth adapter is off.
inline constexpr char kBluetoothAdapterOffHelpURL[] =
    "https://support.google.com/chrome?p=bluetooth";

// "Chrome Settings" URL for website camera access permissions.
inline constexpr char kBrowserCameraPermissionsSettingsURL[] =
    "chrome://settings/content/camera";

// "Chrome Settings" URL for website location access permissions.
inline constexpr char kBrowserLocationPermissionsSettingsURL[] =
    "chrome://settings/content/location";

// "Chrome Settings" URL for website microphone access permissions.
inline constexpr char kBrowserMicrophonePermissionsSettingsURL[] =
    "chrome://settings/content/microphone";

// "Learn more" URL shown in the dialog to enable cloud services for Cast.
inline constexpr char kCastCloudServicesHelpURL[] =
    "https://support.google.com/chromecast/?p=casting_cloud_services";

// The URL for the help center article to show when no Cast destination has been
// found.
inline constexpr char kCastNoDestinationFoundURL[] =
    "https://support.google.com/chromecast/?p=no_cast_destination";

// The URL for the WebHID API help center article.
inline constexpr char kChooserHidOverviewUrl[] =
    "https://support.google.com/chrome?p=webhid";

// The URL for the Web Serial API help center article.
inline constexpr char kChooserSerialOverviewUrl[] =
    "https://support.google.com/chrome?p=webserial";

// The URL for the WebUsb help center article.
inline constexpr char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";

// Link to the forum for Chrome Beta.
inline constexpr char kChromeBetaForumURL[] =
    "https://support.google.com/chrome?p=beta_forum";

// The URL for the help center article to fix Chrome update problems.
inline constexpr char16_t kChromeFixUpdateProblems[] =
    u"https://support.google.com/chrome?p=fix_chrome_updates";

// General help links for Chrome, opened using various actions.
inline constexpr char kChromeHelpViaKeyboardURL[] =
#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=keyboard";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome?p=help&ctx=keyboard";
#endif  // BUILDFLAG(IS_CHROMEOS)

inline constexpr char kChromeHelpViaMenuURL[] =
#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=menu";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome?p=help&ctx=menu";
#endif  // BUILDFLAG(IS_CHROMEOS)

inline constexpr char kChromeHelpViaWebUIURL[] =
    "https://support.google.com/chrome?p=help&ctx=settings";
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kChromeOsHelpViaWebUIURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=settings";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS)

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
inline constexpr char kChromeNativeScheme[] = "chrome-native";

// The URL of safe section in Chrome page (https://www.google.com/chrome).
inline constexpr char16_t kChromeSafePageURL[] =
    u"https://www.google.com/chrome/#safe";

// Host and URL for most visited iframes used on the Instant Extended NTP.
inline constexpr char kChromeSearchMostVisitedHost[] = "most-visited";
inline constexpr char kChromeSearchMostVisitedUrl[] =
    "chrome-search://most-visited/";

// URL for NTP custom background image selected from the user's machine and
// filename for the version of the file in the Profile directory
inline constexpr char kChromeUIUntrustedNewTabPageBackgroundUrl[] =
    "chrome-untrusted://new-tab-page/background.jpg";
inline constexpr char kChromeUIUntrustedNewTabPageBackgroundFilename[] =
    "background.jpg";

// Page under chrome-search.
inline constexpr char kChromeSearchRemoteNtpHost[] = "remote-ntp";

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
inline constexpr char kChromeSearchScheme[] = "chrome-search";

// This is the base URL of content that can be embedded in chrome://new-tab-page
// using an <iframe>. The embedded untrusted content can make web requests and
// can include content that is from an external source.
inline constexpr char kChromeUIUntrustedNewTabPageUrl[] =
    "chrome-untrusted://new-tab-page/";

// The URL for the Chromium project used in the About dialog.
inline constexpr char16_t kChromiumProjectURL[] = u"https://www.chromium.org/";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// The URL for the "Clear browsing data in Chrome" help center article.
inline constexpr char16_t kClearBrowsingDataHelpCenterURL[] =
    u"https://support.google.com/chrome?p=delete_browsing_data";
#endif

inline constexpr char16_t kContentSettingsExceptionsLearnMoreURL[] =
    u"https://support.google.com/chrome?p=settings_manage_exceptions";

// "Learn more" URL for cookies.
inline constexpr char kCookiesSettingsHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_cookies";

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
inline constexpr char kCrashReasonURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=e_awsnap";
#else
    "https://support.google.com/chrome?p=e_awsnap";
#endif

// "Learn more" URL for "Aw snap" page when showing "Send feedback" button.
inline constexpr char kCrashReasonFeedbackDisplayedURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=e_awsnap_rl";
#else
    "https://support.google.com/chrome?p=e_awsnap_rl";
#endif

// "Learn more" URL for the inactive tabs appearance setting.
inline constexpr const char16_t kDiscardRingTreatmentLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=performance_personalization";

// "Learn more" URL for the "Do not track" setting in the privacy section.
inline constexpr char16_t kDoNotTrackLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=settings_do_not_track";
#else
    u"https://support.google.com/chrome?p=settings_do_not_track";
#endif

// The URL for the "Learn more" page for interrupted downloads.
inline constexpr char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome?p=ui_download_errors";

// The URL for the "Learn more" page for download scanning.
inline constexpr char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome?p=ib_download_blocked";

// The URL for the "Learn more" page for blocked downloads.
// Note: This is the same as the above URL. This is done to decouple the URLs,
// in case the support page is split apart into separate pages in the future.
inline constexpr char kDownloadBlockedLearnMoreURL[] =
    "https://support.google.com/chrome?p=ib_download_blocked";

// "Learn more" URL for the Settings API, NTP bubble and other settings bubbles
// showing which extension is controlling them.
inline constexpr char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome?p=ui_settings_api_extension";

// Link for creating family group with Google Families.
inline constexpr char16_t kFamilyGroupCreateURL[] =
    u"https://myaccount.google.com/family/create?utm_source=cpwd";

// Link for viewing family group with Google Families.
inline constexpr char16_t kFamilyGroupViewURL[] =
    u"https://myaccount.google.com/family/details?utm_source=cpwd";

// "Learn more" URL for related website sets.
inline constexpr char kRelatedWebsiteSetsLearnMoreURL[] =
    "https://support.google.com/"
    "chrome?p=cpn_cookies&rd=1#allow_block_cookies&zippy=%2Callow-or-block-"
    "third-party-cookies%2Callow-related-sites-to-access-your-activity%2Cabout-"
    "embedded-content";

// Url to a blogpost about Flash deprecation.
inline constexpr char kFlashDeprecationLearnMoreURL[] =
    "https://blog.chromium.org/2017/07/so-long-and-thanks-for-all-flash.html";

// URL of the Google account language selection page.
inline constexpr char kGoogleAccountLanguagesURL[] =
    "https://myaccount.google.com/language";

// URL of the 'Activity controls' section of the privacy settings page.
inline constexpr char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.google.com/activitycontrols/search";

// URL of the 'Activity controls' section of the privacy settings page, with
// privacy guide parameters and a link for users to manage data.
inline constexpr char kGoogleAccountActivityControlsURLInPrivacyGuide[] =
    "https://myaccount.google.com/activitycontrols/"
    "search&utm_source=chrome&utm_medium=privacy-guide";

// URL of the 'Linked services' section of the privacy settings page.
inline constexpr char kGoogleAccountLinkedServicesURL[] =
    "https://myaccount.google.com/linked-services?utm_source=chrome_s";

// URL of the Google Account.
inline constexpr char kGoogleAccountURL[] = "https://myaccount.google.com";

// URL of the Google Account chooser.
inline constexpr char kGoogleAccountChooserURL[] =
    "https://accounts.google.com/AccountChooser";

// URL of the Google Account page showing the known user devices.
inline constexpr char kGoogleAccountDeviceActivityURL[] =
    "https://myaccount.google.com/device-activity?utm_source=chrome";

// URL of the Google Account home address page.
inline constexpr char kGoogleAccountHomeAddressURL[] =
    "https://myaccount.google.com/address/"
    "home?utm_source=chrome&utm_campaign=manage_addresses";

// URL of the Google Account work address page.
inline constexpr char kGoogleAccountWorkAddressURL[] =
    "https://myaccount.google.com/address/"
    "work?utm_source=chrome&utm_campaign=manage_addresses";

// URL of the change Google Account name page.
inline constexpr char kGoogleAccountNameEmailAddressEditURL[] =
    "https://myaccount.google.com/"
    "personal-info?utm_source=chrome-settings&utm_medium=autofill";

// URL of the two factor authentication setup required intersitial.
inline constexpr char kGoogleTwoFactorIntersitialURL[] =
    "https://myaccount.google.com/interstitials/twosvrequired";

// URL of the Google Password Manager.
inline constexpr char kGooglePasswordManagerURL[] =
    "https://passwords.google.com";

// URL of the Google Photos.
inline constexpr char kGooglePhotosURL[] = "https://photos.google.com";

// The URL for the "Learn more" link for the Memory Saver Mode.
inline constexpr const char16_t kMemorySaverModeLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=chrome_memory_saver";

// The URL in the help text for the Memory Saver Mode tab discarding
// exceptions add dialog.
inline constexpr char16_t kMemorySaverModeTabDiscardingHelpUrl[] =
    u"https://support.google.com/chrome?p=performance_site_exclusion";

// The URL to the help center article of Incognito mode.
inline constexpr char16_t kIncognitoHelpCenterURL[] =
    u"https://support.google.com/chrome?p=incognito";

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
inline constexpr char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome?p=ui_usagestat";

// The URL for the tab group sync help center page.
inline constexpr char kTabGroupsLearnMoreURL[] =
    "https://support.google.com/chrome?p=desktop_tab_groups";

// The URL for the Learn More page about policies and enterprise enrollment.
inline constexpr char16_t kManagedUiLearnMoreUrl[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=is_chrome_managed";
#else
    u"https://support.google.com/chrome?p=is_chrome_managed";
#endif

// The URL for the "Learn more" page for insecure download blocking.
inline constexpr char kInsecureDownloadBlockingLearnMoreUrl[] =
    "https://support.google.com/chrome?p=mixed_content_downloads";

// "myactivity.google.com" URLs with their respective UTM sources.
// - In the Clear Browsing Data footer.
// - In the Clear Browsing Data "notice about other forms of history".
// - On the history page.
inline constexpr char16_t kMyActivityUrlInClearBrowsingData[] =
    u"https://myactivity.google.com/myactivity?utm_source=chrome_cbd";
inline constexpr char16_t kMyActivityUrlInClearBrowsingDataNotice[] =
    u"https://myactivity.google.com/myactivity/?utm_source=chrome_n";
inline constexpr char16_t kMyActivityUrlInHistory[] =
    u"https://myactivity.google.com/myactivity/?utm_source=chrome_h";

// The URL for the Gemini Personal Context page.
inline constexpr char16_t kGeminiPersonalContextUrl[] =
    u"https://gemini.google.com/saved-info";

// The URL for "Your Gemini Apps Activity" page.
inline constexpr char16_t kMyActivityGeminiAppsUrl[] =
    u"https://myactivity.google.com/product/gemini";

#if !BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// "Learn more" URL for On-Device AI.
inline constexpr char16_t kOnDeviceAiLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=on_device_genAI";
#endif

// Help URL for the Omnibox setting.
inline constexpr char16_t kOmniboxLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=settings_omnibox";
#else
    u"https://support.google.com/chrome?p=settings_omnibox";
#endif

// "What do these mean?" URL for the Page Info bubble.
inline constexpr char kPageInfoHelpCenterURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=ui_security_indicator";
#else
    "https://support.google.com/chrome?p=ui_security_indicator";
#endif

// Help center article URL for automated password change.
inline constexpr char16_t kPasswordChangeLearnMoreURL[] =
    u"https://support.google.com/chrome?p=automatedpasswordchange";

// Help URL for the bulk password check.
inline constexpr char kPasswordCheckLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook/"
    "?p=settings_password#leak_detection_privacy";
#else
    "https://support.google.com/chrome/"
    "?p=settings_password#leak_detection_privacy";
#endif

// Help URL for password generation.
inline constexpr char kPasswordGenerationLearnMoreURL[] =
    "https://support.google.com/chrome?p=generate_password";

inline constexpr char16_t kPasswordManagerLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=settings_password";
#else
    u"https://support.google.com/chrome?p=settings_password";
#endif

// Help URL for passwords import.
inline constexpr char kPasswordManagerImportLearnMoreURL[] =
    "https://support.google.com/chrome?p=import-passwords-desktop";

// Help URL for password sharing.
inline constexpr char kPasswordSharingLearnMoreURL[] =
    "https://support.google.com/chrome?p=password_sharing";

// Help URL for troubleshooting password sharing.
inline constexpr char kPasswordSharingTroubleshootURL[] =
    "https://support.google.com/chrome?p=password_sharing_troubleshoot";

// The URL for the "Fill out forms automatically" support page.
inline constexpr char kAddressesAndPaymentMethodsLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=settings_autofill";
#else
    "https://support.google.com/chrome?p=settings_autofill";
#endif

// The URL for the "Pay over time" support page.
// TODO(crbug.com/395027230): Change URL once official support page is
// finalized.
inline constexpr char16_t kPayOverTimeLearnMoreUrl[] =
    u"https://support.google.com/googlepay?p=bnpl_autofill_chrome";

// The URL for the Wallet website.
inline constexpr char16_t kWalletUrl[] = u"https://wallet.google.com";

// Help URL for Autofill AI.
inline constexpr char16_t kAutofillAiLearnMoreURL[] =
    u"https://support.google.com/chrome?p=autofill_ai";

// "Learn more" URL for the autofill show card benefits setting.
inline constexpr char16_t kCardBenefitsLearnMoreURL[] =
    u"https://support.google.com/googlepay?p=card_benefits_chrome";

// "Learn more" URL for the performance intervention notification setting.
inline constexpr const char16_t kPerformanceInterventionLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=performance_personalization";

// "Learn more" URL for the preloading section in Performance settings.
inline constexpr const char16_t kPreloadingLearnMoreUrl[] =
    u"https://support.google.com/chrome/?p=performance_preload_pages";

// "Learn more" URL for the Privacy section under Options.
inline constexpr char kPrivacyLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=settings_privacy";
#else
    "https://support.google.com/chrome?p=settings_privacy";
#endif

// "Chrome Settings" URL for Ad Topics page
inline constexpr char kPrivacySandboxAdTopicsURL[] =
    "chrome://settings/adPrivacy/interests";

// "Chrome Settings" URL for Managing Topics page
inline constexpr char kPrivacySandboxManageTopicsURL[] =
    "chrome://settings/adPrivacy/interests/manage";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// The Privacy Sandbox homepage.
inline constexpr char16_t kPrivacySandboxURL[] =
    u"https://www.privacysandbox.com/";
#endif

// The URL for the Learn More link of the non-CWS bubble.
inline constexpr char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome?p=ui_remove_non_cws_extensions";

// "Learn more" URL for resetting profile preferences.
inline constexpr char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome?p=ui_reset_settings";

// "Learn more" URL for Safebrowsing
inline constexpr char kSafeBrowsingHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_safe_browsing";

// Updated "Info icon" URL for Safebrowsing
inline constexpr char kSafeBrowsingHelpCenterUpdatedURL[] =
    "https://support.google.com/chrome?p=safe_browsing_preferences";

// "Learn more" URL for Enhanced Protection
inline constexpr char16_t kSafeBrowsingInChromeHelpCenterURL[] =
    u"https://support.google.com/chrome?p=safebrowsing_in_chrome";

// The URL for Safe Browsing link in Safety Check page.
inline constexpr char16_t kSafeBrowsingUseInChromeURL[] =
    u"https://support.google.com/chrome/answer/9890866";

// "Learn more" URL for Safety Check page.
inline constexpr char16_t kSafetyHubHelpCenterURL[] =
    u"https://support.google.com/chrome?p=safety_check";

// "Learn more" URL for safety tip bubble.
inline constexpr char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome?p=safety_tip";

// Google search history URL that leads users of the CBD dialog to their search
// history in their Google account.
inline constexpr char16_t kSearchHistoryUrlInClearBrowsingData[] =
    u"https://myactivity.google.com/product/search?utm_source=chrome_cbd";

// The URL for the "See more security tips" with advices how to create a strong
// password.
inline constexpr char kSeeMoreSecurityTipsURL[] =
    "https://support.google.com/accounts/answer/32040";

// Help URL for the settings page's search feature.
inline constexpr char16_t kSettingsSearchHelpURL[] =
    u"https://support.google.com/chrome?p=settings_search_help";

// The URL for the Learn More page about Sync and Google services.
inline constexpr char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.google.com/chrome?p=syncgoogleservices";

// The URL for the "Learn more" page on sync encryption.
inline constexpr char16_t kSyncEncryptionHelpURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=settings_encryption";
#else
    u"https://support.google.com/chrome?p=settings_encryption";
#endif

// The URL for the "Learn more" link when there is a sync error.
inline constexpr char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome?p=settings_sync_error";

// Legacy URL to the sync google dashboard.
inline constexpr char kLegacySyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";

// New URL to the sync google dashboard.
inline constexpr char kNewSyncGoogleDashboardURL[] =
    "https://chrome.google.com/data";

// The URL for the "Learn more" page for sync setup on the personal stuff page.
inline constexpr char16_t kSyncLearnMoreURL[] =
    u"https://support.google.com/chrome?p=settings_sign_in";

// The URL for the "Learn more" page for signing in to chrome with expanded
// section on "Sign in and turn on sync" in the Computer/Desktop tab.
inline constexpr char kSigninOnDesktopLearnMoreURL[] =
    "https://support.google.com/"
    "chrome?p=settings_sign_in#zippy=sign-in-turn-on-sync";

// The URL for the "Learn more" page for adding a new profile to Chrome.
inline constexpr char kAddNewProfileOnDesktopLearnMoreURL[] =
    "https://support.google.com/chrome/?p=add_profile";

// The URL for the "Learn more" page for AI settings for managed users.
inline constexpr char16_t kAiSettingsLearnMorePageManagedUrl[] =
    u"https://support.google.com/chrome/a?p=generative_ai_settings";

// The URL for the "Learn more" page for Help me Write.
inline constexpr char kComposeLearnMorePageURL[] =
    "https://support.google.com/chrome?p=help_me_write";

// The URL for the "Learn more" page for Help me Write for managed users.
inline constexpr char kComposeLearnMorePageManagedURL[] =
    "https://support.google.com/chrome/a?p=help_me_write_settings";

// The URL for the "Learn more" links for pages related to History search.
inline constexpr char kHistorySearchLearnMorePageURL[] =
    "https://support.google.com/chrome?p=ai_history_search";

// The URL for the "Learn more" links for pages related to History search for
// managed users.
inline constexpr char kHistorySearchLearnMorePageManagedURL[] =
    "https://support.google.com/chrome/a?p=search_history";

// The URL for the Settings page to enable history search.
inline constexpr char16_t kHistorySearchSettingURL[] =
    u"chrome://settings/ai/historySearch";

// The URL for the "Learn more" page for Wallpaper Search.
inline constexpr char kWallpaperSearchLearnMorePageURL[] =
    "https://support.google.com/chrome?p=create_themes_with_ai";

// The URL for the passed in Google Wallet.
inline constexpr char kWalletPassesPageURL[] =
    "https://wallet.google.com/wallet/passes";

// The URL for the "Learn more" page for Tab Organization.
inline constexpr char kTabOrganizationLearnMorePageURL[] =
    "https://support.google.com/chrome?p=auto_tab_group";

// The URL for the "Learn more" page for Tab Organization for managed users.
inline constexpr char kTabOrganizationLearnMorePageManagedURL[] =
    "https://support.google.com/chrome/a?p=tab_organizer_settings";

// The URL for the "Learn more" link in the enterprise disclaimer for managed
// profile in the Signin Intercept bubble.
inline constexpr char kSigninInterceptManagedDisclaimerLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=profile_separation";

#if !BUILDFLAG(IS_ANDROID)
// The URL for the trusted vault sync passphrase opt in.
inline constexpr char kSyncTrustedVaultOptInURL[] =
    "https://passwords.google.com/encryption/enroll?"
    "utm_source=chrome&utm_medium=desktop&utm_campaign=encryption_enroll";
#endif

// The URL for the "Learn more" link for the trusted vault sync passphrase.
inline constexpr char kSyncTrustedVaultLearnMoreURL[] =
    "https://support.google.com/accounts?p=settings_password_ode";

// The URL for the Help Center page about User Bypass.
inline constexpr char16_t kUserBypassHelpCenterURL[] =
    u"https://support.google.com/chrome?p=pause_protections";

inline constexpr char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

// The URL for the "Learn more" link for nearby share.
inline constexpr char16_t kNearbyShareLearnMoreURL[] =
    u"https://support.google.com/chromebook?p=nearby_share";

// Help center URL for who the account administrator is.
inline constexpr char16_t kWhoIsMyAdministratorHelpURL[] =
    u"https://support.google.com/chrome?p=your_administrator";

// The URL for the "Learn more" link about CWS Enhanced Safe Browsing.
inline constexpr char16_t kCwsEnhancedSafeBrowsingLearnMoreURL[] =
    u"https://support.google.com/chrome?p=cws_enhanced_safe_browsing";

// The URL path to Google's Privacy Policy page.
inline constexpr char kPrivacyPolicyURL[] =
    "https://policies.google.com/privacy";

// The URL path to Google's Privacy Policy page for users in China.
inline constexpr char kPrivacyPolicyURLChina[] =
    "https://policies.google.cn/privacy";

// The URL path to Google's Embedded Privacy Policy page.
inline constexpr char kPrivacyPolicyOnlineURLPath[] =
    "https://policies.google.com/privacy/embedded";

// The URL path to Google's Embedded Privacy Policy page for users in China.
inline constexpr char kPrivacyPolicyEmbeddedURLPathChina[] =
    "https://policies.google.cn/privacy/embedded";

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// "Learn more" URL for the enhanced playback notification dialog.
inline constexpr char kEnhancedPlaybackNotificationLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=enhanced_playback";
#else
    // Keep in sync with
    // chrome/browser/ui/android/strings/android_chrome_strings.grd
    "https://support.google.com/chrome?p=mobile_protected_content";
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Chrome OS default pre-defined custom handlers
inline constexpr char kChromeOSDefaultMailtoHandler[] =
    "https://mail.google.com/mail/?extsrc=mailto&amp;url=%s";
inline constexpr char kChromeOSDefaultWebcalHandler[] =
    "https://www.google.com/calendar/render?cid=%s";

// The URL for the "Account recovery" page.
inline constexpr char kAccountRecoveryURL[] =
    "https://accounts.google.com/signin/recovery";

// The URL for the "How to add a new user account on a Chromebook" page.
inline constexpr char16_t kAddNewUserURL[] =
    u"https://www.google.com/chromebook/howto/add-another-account";

// Help center URL for ARC ADB sideloading.
inline constexpr char16_t kArcAdbSideloadingLearnMoreURL[] =
    u"https://support.google.com/chromebook?p=develop_android_apps";

// The path format to the localized offline ARC++ Privacy Policy.
// Relative to |kChromeOSAssetPath|.
inline constexpr char kArcPrivacyPolicyPathFormat[] =
    "arc_tos/%s/privacy_policy.pdf";

// The path format to the localized offline ARC++ Terms of Service.
// Relative to |kChromeOSAssetPath|.
inline constexpr char kArcTermsPathFormat[] = "arc_tos/%s/terms.html";

// Source for chrome://os-credits. On some devices, this will be compressed.
// Check both.
inline constexpr char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

inline constexpr char kChromeOSCreditsCompressedPath[] =
    "/opt/google/chrome/resources/about_os_credits.html.gz";

// The URL for the help center article about redeeming Chromebook offers.
inline constexpr char kEchoLearnMoreURL[] =
    "chrome://help-app/help/sub/3399709/id/2703646";

// The URL for the Learn More page about enterprise enrolled devices.
inline constexpr char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook?p=managed";

// The URL path to offline OEM EULA.
inline constexpr char kOemEulaURLPath[] = "oem";

inline constexpr char kOrcaSuggestionLearnMoreURL[] =
    "https://support.google.com/chromebook?p=copyeditor";

// Help URL for the OS settings page's search feature.
inline constexpr char kOsSettingsSearchHelpURL[] =
    "https://support.google.com/chromebook?p=settings_search_help";

// The URL path to offline ARC++ Terms of Service.
inline constexpr char kArcTermsURLPath[] = "arc/terms";

// The URL path to offline ARC++ Privacy Policy.
inline constexpr char kArcPrivacyPolicyURLPath[] = "arc/privacy_policy";

// The URL for contacts management in Nearby Share feature.
inline constexpr char16_t kNearbyShareManageContactsURL[] =
    u"https://contacts.google.com";

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
// "Learn more" URL for the enterprise sign-in confirmation dialog.
inline constexpr char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.google.com/chromebook?p=is_chrome_managed";

// The URL for the "learn more" link on the macOS version obsolescence infobar.
inline constexpr char kMacOsObsoleteURL[] =
    "https://support.google.com/chrome?p=unsupported_mac";
#endif

#if BUILDFLAG(IS_WIN)
// The URL for the Windows XP/Vista deprecation help center article.
inline constexpr char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/"
    "updates-to-chrome-platform-support.html";

// The URL for the Windows 7/8.1 deprecation help center article.
inline constexpr char kWindows78DeprecationURL[] =
    "https://support.google.com/chrome?p=unsupported_windows";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// The URL for the "Learn more" page for the outdated plugin infobar.
inline constexpr char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome?p=ib_outdated_plugin";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// "Learn more" URL for the chrome apps deprecation dialog.
inline constexpr char kChromeAppsDeprecationLearnMoreURL[] =
    "https://support.google.com/chrome?p=chrome_app_deprecation";
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
inline constexpr char kChromeRootStoreSettingsHelpCenterURL[] =
    "https://support.google.com/chrome?p=root_store";
#endif

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
// "Learn more" URL for how to save PDF to Google Drive from the PDF viewer.
inline constexpr char kPdfViewerSaveToDriveHelpCenterURL[] =
    "https://support.google.com/drive?p=save_from_chrome";
#endif

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
