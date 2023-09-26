// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"

namespace chrome {

const char kAccessCodeCastLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=cast_to_class_teacher";

const char kAccessibilityLabelsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=image_descriptions";

const char kAdPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ad_privacy";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_automatic_settings_reset";

const char kAdvancedProtectionDownloadLearnMoreURL[] =
    "https://support.google.com/accounts/accounts?p=safe-browsing";

const char kAppNotificationsBrowserSettingsURL[] =
    "chrome://settings/content/notifications";

const char kBatterySaverModeLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=chrome_battery_saver";

const char kBluetoothAdapterOffHelpURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kCastCloudServicesHelpURL[] =
    "https://support.google.com/chromecast/?p=casting_cloud_services";

const char kCastNoDestinationFoundURL[] =
    "https://support.google.com/chromecast/?p=no_cast_destination";

const char kChooserHidOverviewUrl[] =
    "https://support.google.com/chrome?p=webhid";

const char kChooserSerialOverviewUrl[] =
    "https://support.google.com/chrome?p=webserial";

const char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";

const char kChromeBetaForumURL[] =
    "https://support.google.com/chrome/?p=beta_forum";

const char kChromeFixUpdateProblems[] =
    "https://support.google.com/chrome?p=fix_chrome_updates";

const char kChromeHelpViaKeyboardURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaMenuURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=menu";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome/?p=help&ctx=menu";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaWebUIURL[] =
    "https://support.google.com/chrome/?p=help&ctx=settings";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kChromeOsHelpViaWebUIURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=settings";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kIsolatedAppScheme[] = "isolated-app";

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSafePageURL[] = "https://www.google.com/chrome/#safe";

const char kChromeSearchLocalNtpHost[] = "local-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kChromeUIUntrustedNewTabPageBackgroundUrl[] =
    "chrome-untrusted://new-tab-page/background.jpg";
const char kChromeUIUntrustedNewTabPageBackgroundFilename[] = "background.jpg";

const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchScheme[] = "chrome-search";

const char kChromeUIUntrustedNewTabPageUrl[] =
    "chrome-untrusted://new-tab-page/";

const char kChromiumProjectURL[] = "https://www.chromium.org/";

const char kContentSettingsExceptionsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_manage_exceptions";

const char kCookiesSettingsHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_cookies";

const char kCrashReasonURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=e_awsnap";
#else
    "https://support.google.com/chrome/?p=e_awsnap";
#endif

const char kCrashReasonFeedbackDisplayedURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=e_awsnap_rl";
#else
    "https://support.google.com/chrome/?p=e_awsnap_rl";
#endif

const char kDoNotTrackLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_do_not_track";
#else
    "https://support.google.com/chrome/?p=settings_do_not_track";
#endif

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_download_errors";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

// Note: This is the same as the above URL. This is done to decouple the URLs,
// in case the support page is split apart into separate pages in the future.
const char kDownloadBlockedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_settings_api_extension";

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";

const char kFamilyGroupCreateURL[] =
    "https://myaccount.google.com/family/create?utm_source=cpwd";

const char kFamilyGroupViewURL[] =
    "https://myaccount.google.com/family/details?utm_source=cpwd";

const char kFirstPartySetsLearnMoreURL[] =
    "https://support.google.com/chrome?p=cpn_cookies"
    "#zippy=%2Callow-related-sites-to-access-your-activity";

const char kFlashDeprecationLearnMoreURL[] =
    "https://blog.chromium.org/2017/07/so-long-and-thanks-for-all-flash.html";

const char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.google.com/activitycontrols/search";

const char kGoogleAccountActivityControlsURLInPrivacyGuide[] =
    "https://myaccount.google.com/activitycontrols/"
    "search&utm_source=chrome&utm_medium=privacy-guide";

const char kGoogleAccountLanguagesURL[] =
    "https://myaccount.google.com/language";

const char kGoogleAccountURL[] = "https://myaccount.google.com";

const char kGoogleAccountChooserURL[] =
    "https://accounts.google.com/AccountChooser";

const char kGoogleAccountDeviceActivityURL[] =
    "https://myaccount.google.com/device-activity?utm_source=chrome";

const char kGooglePasswordManagerURL[] = "https://passwords.google.com";

const char kGooglePhotosURL[] = "https://photos.google.com";

const char kHighEfficiencyModeLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=chrome_memory_saver";

const char kHighEfficiencyModeTabDiscardingHelpUrl[] =
    "https://support.google.com/chrome/?p=performance_site_exclusion";

const char kIncognitoHelpCenterURL[] =
    "https://support.google.com/chrome/answer/9845881";

// TODO(crbug.com/1480695): Update this URL with proper user-facing explainer.
const char kIsolatedWebAppsLearnMoreUrl[] =
    "https://github.com/WICG/isolated-web-apps/blob/main/README.md";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

const char kManage3pcHelpCenterURL[] =
    "https://support.google.com/chrome/?p=manage_tp_cookies";

const char kManagedUiLearnMoreUrl[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=is_chrome_managed";
#else
    "https://support.google.com/chrome/?p=is_chrome_managed";
#endif

const char kInsecureDownloadBlockingLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=mixed_content_downloads";

const char kMyActivityUrlInClearBrowsingData[] =
    "https://myactivity.google.com/myactivity?utm_source=chrome_cbd";

const char kOmniboxLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_omnibox";
#else
    "https://support.google.com/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=ui_security_indicator";
#else
    "https://support.google.com/chrome/?p=ui_security_indicator";
#endif

const char kPasswordCheckLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/"
    "?p=settings_password#leak_detection_privacy";
#else
    "https://support.google.com/chrome/"
    "?p=settings_password#leak_detection_privacy";
#endif

const char kPasswordGenerationLearnMoreURL[] =
    "https://support.google.com/chrome/answer/7570435";

const char kPasswordManagerLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_password";
#else
    "https://support.google.com/chrome/?p=settings_password";
#endif

const char kPasswordManagerImportLearnMoreURL[] =
    "https://support.google.com/chrome/?p=import-passwords-desktop";

const char kPasswordSharingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=password_sharing";

const char kPasswordSharingTroubleshootURL[] =
    "https://support.google.com/chrome/?p=password_sharing_troubleshoot";

const char kPaymentMethodsURL[] =
    "https://pay.google.com/payments/"
    "home?utm_source=chrome&utm_medium=settings&utm_campaign=chrome-payment#"
    "paymentMethods";

const char kAddressesAndPaymentMethodsLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#else
    "https://support.google.com/chrome/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#endif

const char kPrivacyLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_privacy";
#else
    "https://support.google.com/chrome/?p=settings_privacy";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome/?p=ui_remove_non_cws_extensions";

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";

const char kSafeBrowsingHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_safe_browsing";

const char kSafeBrowsingHelpCenterUpdatedURL[] =
    "https://support.google.com/chrome?p=safe_browsing_preferences";

const char kSafeBrowsingInChromeHelpCenterURL[] =
    "https://support.google.com/chrome?p=safebrowsing_in_chrome";

const char kSafeBrowsingPTourURL[] =
    "https://support.google.com/chrome/answer/13844634";

const char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome/?p=safety_tip";

const char kSearchHistoryUrlInClearBrowsingData[] =
    "https://myactivity.google.com/product/search?utm_source=chrome_cbd";

const char kSeeMoreSecurityTipsURL[] =
    "https://support.google.com/accounts/answer/32040";

const char kSettingsSearchHelpURL[] =
    "https://support.google.com/chrome/?p=settings_search_help";

const char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.google.com/chrome?p=syncgoogleservices";

const char kSyncEncryptionHelpURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_encryption";
#else
    "https://support.google.com/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome/?p=settings_sync_error";

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kSigninInterceptManagedDisclaimerLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=profile_separation";

#if !BUILDFLAG(IS_ANDROID)
const char kSyncTrustedVaultOptInURL[] =
    "https://passwords.google.com/encryption/enroll?"
    "utm_source=chrome&utm_medium=desktop&utm_campaign=encryption_enroll";
#endif

const char kSyncTrustedVaultLearnMoreURL[] =
    "https://support.google.com/accounts?p=settings_password_ode";

const char kTrackingProtectionHelpCenterURL[] =
    "https://support.google.com/chrome/?p=tracking_protection";

const char kUserBypassHelpCenterURL[] =
    "https://support.google.com/chrome/?p=user_bypass";

const char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

const char kWhoIsMyAdministratorHelpURL[] =
    "https://support.google.com/chrome?p=your_administrator";

const char kCwsEnhancedSafeBrowsingLearnMoreURL[] =
    "https://support.google.com/chrome?p=cws_enhanced_safe_browsing";

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
const char kEnhancedPlaybackNotificationLearnMoreURL[] =
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=enhanced_playback";
#elif BUILDFLAG(IS_ANDROID)
// Keep in sync with chrome/browser/ui/android/strings/android_chrome_strings.grd
    "https://support.google.com/chrome/?p=mobile_protected_content";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
const char kChromeOSDefaultMailtoHandler[] =
    "https://mail.google.com/mail/?extsrc=mailto&amp;url=%s";
const char kChromeOSDefaultWebcalHandler[] =
    "https://www.google.com/calendar/render?cid=%s";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccountManagerLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=google_accounts";

const char kAccountRecoveryURL[] =
    "https://accounts.google.com/signin/recovery";

const char kAddNewUserURL[] =
    "https://www.google.com/chromebook/howto/add-another-account";

const char kAndroidAppsLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=playapps";

const char kArcAdbSideloadingLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=develop_android_apps";

const char kArcExternalStorageLearnMoreURL[] =
    "https://support.google.com/chromebook?p=open_files";

const char kArcPrivacyPolicyURLPath[] = "arc/privacy_policy";

const char kArcTermsURLPath[] = "arc/terms";

// TODO(crbug.com/1010321): Remove 'm100' prefix from link once Bluetooth Revamp
// has shipped.
const char kBluetoothPairingLearnMoreUrl[] =
    "https://support.google.com/chromebook?p=bluetooth_revamp_m100";

const char kChromeAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/topic/6323347";

const char kChromeOSAssetHost[] = "chromeos-asset";
const char kChromeOSAssetPath[] = "/usr/share/chromeos-assets/";

const char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

const char kChromeOSCreditsCompressedPath[] =
    "/opt/google/chrome/resources/about_os_credits.html.gz";

// TODO(carpenterr): Have a solution for plink mapping in Help App.
// The magic numbers in this url are the topic and article ids currently
// required to navigate directly to a help article in the Help App.
const char kChromeOSGestureEducationHelpURL[] =
    "chrome://help-app/help/sub/3399710/id/9739838";

const char kChromePaletteHelpURL[] =
    "https://support.google.com/chromebook?p=stylus_help";

const char kCupsPrintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_printing";

const char kCupsPrintPPDLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=printing_advancedconfigurations";

const char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=smart_lock";

const char kEchoLearnMoreURL[] =
    "chrome://help-app/help/sub/3399709/id/2703646";

const char kArcTermsPathFormat[] = "arc_tos/%s/terms.html";

const char kArcPrivacyPolicyPathFormat[] = "arc_tos/%s/privacy_policy.pdf";

const char kEolNotificationURL[] = "https://www.google.com/chromebook/older/";

const char kEolIncentiveNotificationOfferURL[] =
    "https://www.google.com/chromebook/renew-chromebook-offer";

const char kEolIncentiveNotificationNoOfferURL[] =
    "https://www.google.com/chromebook/renew-chromebook";

const char kAutoUpdatePolicyURL[] =
    "https://support.google.com/chrome/a?p=auto-update-policy";

const char kGoogleNameserversLearnMoreURL[] =
    "https://developers.google.com/speed/public-dns";

const char kInstantTetheringLearnMoreURL[] =
    "https://support.google.com/chromebook?p=instant_tethering";

const char kKerberosAccountsLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=kerberos_accounts";

const char kLanguageSettingsLearnMoreUrl[] =
    "https://support.google.com/chromebook/answer/1059490";

const char kLanguagePacksLearnMoreURL[] =
    "https://support.google.com/chromebook?p=language_packs";

const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook/?p=managed";

const char kLinuxAppsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_linuxapps";

const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromebook/?p=simple_scrolling";

// TODO(zhangwenyu): Update link once confirmed.
const char kControlledScrollingHelpURL[] =
    "https://support.google.com/chromebook/?p=simple_scrolling";

const char kHapticFeedbackHelpURL[] =
    "https://support.google.com/chromebook?p=haptic_feedback_m100";

const char kOemEulaURLPath[] = "oem";

const char kGoogleEulaOnlineURLPath[] =
    "https://policies.google.com/terms/embedded?hl=%s";

const char kCrosEulaOnlineURLPath[] =
    "https://www.google.com/intl/%s/chrome/terms/";

const char kArcTosOnlineURLPath[] =
    "https://play.google.com/about/play-terms/embedded/";

const char kPrivacyPolicyOnlineURLPath[] =
    "https://policies.google.com/privacy/embedded";

const char kOsSettingsSearchHelpURL[] =
    "https://support.google.com/chromebook/?p=settings_search_help";

const char kPeripheralDataAccessHelpURL[] =
    "https://support.google.com/chromebook?p=connect_thblt_usb4_accy";

const char kSelectToSpeakLearnMoreURL[] =
    "https://support.google.com/chromebook?p=select_to_speak";

const char kTPMFirmwareUpdateLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=tpm_update";

const char kTimeZoneSettingsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_timezone&hl=%s";

const char kSmartPrivacySettingsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=screen_privacy_m100";

const char kSmbSharesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=network_file_shares";

const char kGoogleDriveCleanUpStorageLearnMoreURL[] =
    "https://support.google.com/chromebook?p=cleanup_offline_files";

const char kGoogleDriveOfflineLearnMoreURL[] =
    "https://support.google.com/chromebook?p=my_drive_cbx";

const char kSpeakOnMuteDetectionLearnMoreURL[] =
    "https://support.google.com/chromebook?p=mic-mute";

const char kGeolocationToggleLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/142065";

const char kSuggestedContentLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=explorecontent";

const char kTabletModeGesturesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=tablet_mode_gestures";

const char kWifiSyncLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=wifisync";

const char kWifiHiddenNetworkURL[] =
    "https://support.google.com/chromebook?p=hidden_networks";

const char kWifiPasspointURL[] =
    "https://support.google.com/chromebook?p=wifi_passpoint";

const char kNearbyShareLearnMoreURL[] =
    "https://support.google.com/chromebook?p=nearby_share";

extern const char kNearbyShareManageContactsURL[] =
    "https://contacts.google.com";

extern const char kFingerprintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_fingerprint";

extern const char kRecoveryLearnMoreURL[] =
    "https://support.google.com/chrome?p=local_data_recovery";

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
const char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/1331549";

const char kMacOsObsoleteURL[] =
    "https://support.google.com/chrome/?p=unsupported_mac";
#endif

#if BUILDFLAG(IS_WIN)
const char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";

const char kWindows78DeprecationURL[] =
    "https://support.google.com/chrome/?p=unsupported_windows";
#endif  // BUILDFLAG(IS_WIN)

const char kChromeSyncLearnMoreURL[] =
    "https://support.google.com/chrome/answer/165139";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";
#endif

// TODO (b/184137843): Use real link to phone hub notifications and apps access.
const char kPhoneHubPermissionLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multidevice";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
const char kChromeAppsDeprecationLearnMoreURL[] =
    "https://support.google.com/chrome/?p=chrome_app_deprecation";
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
const char kChromeRootStoreSettingsHelpCenterURL[] =
    "https://support.google.com/chrome?p=root_store";
#endif

}  // namespace chrome
