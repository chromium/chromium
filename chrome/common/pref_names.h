// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <iterator>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "components/compose/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// This preference determines if the browser will use the Compact Mode UI.
inline constexpr char kCompactModeEnabled[] = "compact_mode";

// A string property indicating whether default apps should be installed
// in this profile.  Use the value "install" to enable defaults apps, or
// "noinstall" to disable them.  This property is usually set in the
// master_preferences and copied into the profile preferences on first run.
// Defaults apps are installed only when creating a new profile.
inline constexpr char kPreinstalledApps[] = "default_apps";

// Disable SafeBrowsing checks for files coming from trusted URLs when false.
inline constexpr char kSafeBrowsingForTrustedSourcesEnabled[] =
    "safebrowsing_for_trusted_sources_enabled";

// Disables screenshot accelerators and extension APIs.
// This setting resides both in profile prefs and local state. Accelerator
// handling code reads local state, while extension APIs use profile pref.
inline constexpr char kDisableScreenshots[] = "disable_screenshots";

// Prevents certain types of downloads based on integer value, which corresponds
// to DownloadPrefs::DownloadRestriction.
// 0 - No special restrictions (default)
// 1 - Block dangerous downloads
// 2 - Block potentially dangerous downloads
// 3 - Block all downloads
// 4 - Block malicious downloads
inline constexpr char kDownloadRestrictions[] = "download_restrictions";

// A boolean specifying whether the partial download bubble (which shows up
// automatically when downloads are complete) should be enabled. True (partial
// bubble will show automatically) by default.
// The following two prefs are ignored on ChromeOS Lacros if SysUI integration
// is enabled.
// TODO(chlily): Clean them up once SysUI integration is enabled by default.
inline constexpr char kDownloadBubblePartialViewEnabled[] =
    "download_bubble.partial_view_enabled";

// An integer counting the number of download bubble partial view impressions.
// The partial view shows up automatically when downloads are complete. This
// is used to decide whether to show the setting for suppressing the partial
// view in the partial view itself. Only counts up to 6; any further impressions
// will not increment the count.
inline constexpr char kDownloadBubblePartialViewImpressions[] =
    "download_bubble.partial_view_impressions";

#if BUILDFLAG(IS_ANDROID)
// Records the timestamp of each time we show a prompt to the user
// suggesting they enable app verification on Android. We use this pref
// to limit the number of times users see a prompt in a given window.
inline constexpr char kDownloadAppVerificationPromptTimestamps[] =
    "download.app_verification_prompt_timestamps";
#endif

// If set to true profiles are created in ephemeral mode and do not store their
// data in the profile folder on disk but only in memory.
inline constexpr char kForceEphemeralProfiles[] = "profile.ephemeral_mode";

// A boolean specifying whether the New Tab page is the home page or not.
inline constexpr char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
inline constexpr char kHomePage[] = "homepage";

// A boolean specifying whether HTTPS-Only Mode is enabled by the user.
inline constexpr char kHttpsOnlyModeEnabled[] = "https_only_mode_enabled";

// A boolean specifying whether HTTPS-First Mode is enabled in Balanced Mode.
inline constexpr char kHttpsFirstBalancedMode[] =
    "https_first_balanced_mode_enabled";

// A boolean specifying whether HTTPS-First Mode (aka "HTTPS-Only Mode") is
// enabled in Incognito Mode.
inline constexpr char kHttpsFirstModeIncognito[] =
    "https_first_mode_incognito_enabled";

// A boolean specifying whether HTTPS-First Balanced Mode is automatically
// enabled by the Typically Secure User heuristic. Can only be set to true if
// this pref, kHttpsOnlyModeEnabled and kHttpsFirstBalancedMode have never been
// set before (true or false).
// If any of the prefs is modified, this will be set to false, disabling
// automatic enabling of HTTPS-First Balanced Mode forever for this profile.
inline constexpr char kHttpsOnlyModeAutoEnabled[] =
    "https_only_mode_auto_enabled";

// A dictionary containing information about HTTPS Upgrade failures in the
// recent days. Failure entries are stored in a list with a timestamp. Old
// entries are evicted from the list and new entries are added when a new HTTPS
// Upgrade fallback happens.
inline constexpr char kHttpsUpgradeFallbacks[] = "https_upgrade_fallbacks";

// A dictionary containing information about HTTPS Upgrade related navigations.
inline constexpr char kHttpsUpgradeNavigations[] = "https_upgrade_navigations";

// Stores information about the important sites dialog, including the time and
// frequency it has been ignored.
inline constexpr char kImportantSitesDialogHistory[] = "important_sites_dialog";

// This is the profile creation time.
inline constexpr char kProfileCreationTime[] = "profile.creation_time";

#if BUILDFLAG(IS_WIN)
// This is a timestamp of the last time this profile was reset by a third party
// tool. On Windows, a third party tool may set a registry value that will be
// compared to this value and if different will result in a profile reset
// prompt. See triggered_profile_resetter.h for more information.
inline constexpr char kLastProfileResetTimestamp[] =
    "profile.last_reset_timestamp";
#endif

// The URL to open the new tab page to. Only set by Group Policy.
inline constexpr char kNewTabPageLocationOverride[] =
    "newtab_page_location_override";

// An integer that keeps track of the profile icon version. This allows us to
// determine the state of the profile icon for icon format changes.
inline constexpr char kProfileIconVersion[] = "profile.icon_version";

// A string pref whose values is one of the values defined by
// |ProfileImpl::kPrefExitTypeXXX|. Set to |kPrefExitTypeCrashed| on startup and
// one of |kPrefExitTypeNormal| or |kPrefExitTypeSessionEnded| during
// shutdown. Used to determine the exit type the last time the profile was open.
inline constexpr char kSessionExitType[] = "profile.exit_type";

// An integer pref. Holds one of several values:
// 0: unused, previously indicated to open the homepage on startup
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
// 5: open the New Tab Page on startup.
inline constexpr char kRestoreOnStartup[] = "session.restore_on_startup";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
inline constexpr char kURLsToRestoreOnStartup[] = "session.startup_urls";

// Boolean that is true when user feedback to Google is allowed.
inline constexpr char kUserFeedbackAllowed[] = "feedback_allowed";

#if BUILDFLAG(ENABLE_RLZ)
// Integer. RLZ ping delay in seconds.
inline constexpr char kRlzPingDelaySeconds[] = "rlz_ping_delay";
#endif  // BUILDFLAG(ENABLE_RLZ)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Locale preference of device' owner.  ChromeOS device appears in this locale
// after startup/wakeup/signout.
inline constexpr char kOwnerLocale[] = "intl.owner_locale";
// Locale accepted by user.  Non-syncable.
// Used to determine whether we need to show Locale Change notification.
inline constexpr char kApplicationLocaleAccepted[] = "intl.app_locale_accepted";
// Non-syncable item.
// It is used in two distinct ways.
// (1) Used for two-step initialization of locale in ChromeOS
//     because synchronization of kApplicationLocale is not instant.
// (2) Used to detect locale change.  Locale change is detected by
//     LocaleChangeGuard in case values of kApplicationLocaleBackup and
//     kApplicationLocale are both non-empty and differ.
// Following is a table showing how state of those prefs may change upon
// common real-life use cases:
//                                  AppLocale Backup Accepted
// Initial login                       -        A       -
// Sync                                B        A       -
// Accept (B)                          B        B       B
// -----------------------------------------------------------
// Initial login                       -        A       -
// No sync and second login            A        A       -
// Change options                      B        B       -
// -----------------------------------------------------------
// Initial login                       -        A       -
// Sync                                A        A       -
// Locale changed on login screen      A        C       -
// Accept (A)                          A        A       A
// -----------------------------------------------------------
// Initial login                       -        A       -
// Sync                                B        A       -
// Revert                              A        A       -
inline constexpr char kApplicationLocaleBackup[] = "intl.app_locale_backup";

// List of locales the UI is allowed to be displayed in by policy. The list is
// empty if no restriction is being enforced.
inline constexpr char kAllowedLanguages[] = "intl.allowed_languages";
#endif

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
inline constexpr char kDefaultCharset[] = "intl.charset_default";

// If these change, the corresponding enums in the extension API
// experimental.fontSettings.json must also change.
inline constexpr const char* const kWebKitScriptsForFontFamilyMaps[] = {
#define EXPAND_SCRIPT_FONT(x, script_name) script_name,
#include "chrome/common/pref_font_script_names-inl.h"
    ALL_FONT_SCRIPTS("unused param")
#undef EXPAND_SCRIPT_FONT
};

inline constexpr size_t kWebKitScriptsForFontFamilyMapsLength =
    std::size(kWebKitScriptsForFontFamilyMaps);

// Strings for WebKit font family preferences. If these change, the pref prefix
// in pref_names_util.cc and the pref format in font_settings_api.cc must also
// change.
inline constexpr char kWebKitStandardFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_STANDARD;
inline constexpr char kWebKitFixedFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_FIXED;
inline constexpr char kWebKitSerifFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_SERIF;
inline constexpr char kWebKitSansSerifFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_SANSERIF;
inline constexpr char kWebKitCursiveFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_CURSIVE;
inline constexpr char kWebKitFantasyFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_FANTASY;
inline constexpr char kWebKitMathFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_MATH;
inline constexpr char kWebKitStandardFontFamilyArabic[] =
    "webkit.webprefs.fonts.standard.Arab";
#if BUILDFLAG(IS_WIN)
inline constexpr char kWebKitFixedFontFamilyArabic[] =
    "webkit.webprefs.fonts.fixed.Arab";
#endif
inline constexpr char kWebKitSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.serif.Arab";
inline constexpr char kWebKitSansSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.sansserif.Arab";
#if BUILDFLAG(IS_WIN)
inline constexpr char kWebKitStandardFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.standard.Cyrl";
inline constexpr char kWebKitFixedFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.fixed.Cyrl";
inline constexpr char kWebKitSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.serif.Cyrl";
inline constexpr char kWebKitSansSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.sansserif.Cyrl";
inline constexpr char kWebKitStandardFontFamilyGreek[] =
    "webkit.webprefs.fonts.standard.Grek";
inline constexpr char kWebKitFixedFontFamilyGreek[] =
    "webkit.webprefs.fonts.fixed.Grek";
inline constexpr char kWebKitSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.serif.Grek";
inline constexpr char kWebKitSansSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.sansserif.Grek";
#endif
inline constexpr char kWebKitStandardFontFamilyJapanese[] =
    "webkit.webprefs.fonts.standard.Jpan";
inline constexpr char kWebKitFixedFontFamilyJapanese[] =
    "webkit.webprefs.fonts.fixed.Jpan";
inline constexpr char kWebKitSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.serif.Jpan";
inline constexpr char kWebKitSansSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.sansserif.Jpan";
inline constexpr char kWebKitStandardFontFamilyKorean[] =
    "webkit.webprefs.fonts.standard.Hang";
inline constexpr char kWebKitFixedFontFamilyKorean[] =
    "webkit.webprefs.fonts.fixed.Hang";
inline constexpr char kWebKitSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.serif.Hang";
inline constexpr char kWebKitSansSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.sansserif.Hang";
#if BUILDFLAG(IS_WIN)
inline constexpr char kWebKitCursiveFontFamilyKorean[] =
    "webkit.webprefs.fonts.cursive.Hang";
#endif
inline constexpr char kWebKitStandardFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.standard.Hans";
inline constexpr char kWebKitFixedFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.fixed.Hans";
inline constexpr char kWebKitSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.serif.Hans";
inline constexpr char kWebKitSansSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.sansserif.Hans";
inline constexpr char kWebKitStandardFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.standard.Hant";
inline constexpr char kWebKitFixedFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.fixed.Hant";
inline constexpr char kWebKitSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.serif.Hant";
inline constexpr char kWebKitSansSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.sansserif.Hant";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
inline constexpr char kWebKitCursiveFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.cursive.Hans";
inline constexpr char kWebKitCursiveFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.cursive.Hant";
#endif

// WebKit preferences.
inline constexpr char kWebKitWebSecurityEnabled[] =
    "webkit.webprefs.web_security_enabled";
inline constexpr char kWebKitDomPasteEnabled[] =
    "webkit.webprefs.dom_paste_enabled";
inline constexpr char kWebKitTextAreasAreResizable[] =
    "webkit.webprefs.text_areas_are_resizable";
inline constexpr char kWebKitJavascriptCanAccessClipboard[] =
    "webkit.webprefs.javascript_can_access_clipboard";
inline constexpr char kWebkitTabsToLinks[] = "webkit.webprefs.tabs_to_links";
inline constexpr char kWebKitAllowRunningInsecureContent[] =
    "webkit.webprefs.allow_running_insecure_content";
#if BUILDFLAG(IS_ANDROID)
inline constexpr char kWebKitPasswordEchoEnabled[] =
    "webkit.webprefs.password_echo_enabled";
#endif
inline constexpr char kWebKitForceDarkModeEnabled[] =
    "webkit.webprefs.force_dark_mode_enabled";

inline constexpr char kWebKitCommonScript[] = "Zyyy";
inline constexpr char kWebKitStandardFontFamily[] =
    "webkit.webprefs.fonts.standard.Zyyy";
inline constexpr char kWebKitFixedFontFamily[] =
    "webkit.webprefs.fonts.fixed.Zyyy";
inline constexpr char kWebKitSerifFontFamily[] =
    "webkit.webprefs.fonts.serif.Zyyy";
inline constexpr char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.fonts.sansserif.Zyyy";
inline constexpr char kWebKitCursiveFontFamily[] =
    "webkit.webprefs.fonts.cursive.Zyyy";
inline constexpr char kWebKitFantasyFontFamily[] =
    "webkit.webprefs.fonts.fantasy.Zyyy";
inline constexpr char kWebKitMathFontFamily[] =
    "webkit.webprefs.fonts.math.Zyyy";
inline constexpr char kWebKitDefaultFontSize[] =
    "webkit.webprefs.default_font_size";
inline constexpr char kWebKitDefaultFixedFontSize[] =
    "webkit.webprefs.default_fixed_font_size";
inline constexpr char kWebKitMinimumFontSize[] =
    "webkit.webprefs.minimum_font_size";
inline constexpr char kWebKitMinimumLogicalFontSize[] =
    "webkit.webprefs.minimum_logical_font_size";
inline constexpr char kWebKitJavascriptEnabled[] =
    "webkit.webprefs.javascript_enabled";
inline constexpr char kWebKitLoadsImagesAutomatically[] =
    "webkit.webprefs.loads_images_automatically";
inline constexpr char kWebKitPluginsEnabled[] =
    "webkit.webprefs.plugins_enabled";

// Boolean that is true when the SSL interstitial should allow users to
// proceed anyway. Otherwise, proceeding is not possible.
inline constexpr char kSSLErrorOverrideAllowed[] = "ssl.error_override_allowed";

// List of origins for which the SSL interstitial should allow users to proceed
// anyway. Ignored if kSSLErrorOverrideAllowed is false.
inline constexpr char kSSLErrorOverrideAllowedForOrigins[] =
    "ssl.error_override_allowed_for_origins";

// Boolean that is true when Suggest support is enabled.
inline constexpr char kSearchSuggestEnabled[] = "search.suggest_enabled";

#if BUILDFLAG(IS_ANDROID)
// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
inline constexpr char kContextualSearchEnabled[] =
    "search.contextual_search_enabled";
inline constexpr char kContextualSearchDisabledValue[] = "false";
inline constexpr char kContextualSearchEnabledValue[] = "true";

// A integer preference to store the number of times the Contextual Search promo
// card shown.
inline constexpr char kContextualSearchPromoCardShownCount[] =
    "search.contextual_search_promo_card_shown_count";

// Boolean that indicates whether the user chose to fully opt in for Contextual
// Search.
inline constexpr char kContextualSearchWasFullyPrivacyEnabled[] =
    "search.contextual_search_fully_opted_in";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Boolean pref recording whether cookie and data would be used only for
// essential purposes.
inline constexpr char kEssentialSearchEnabled[] = "essential_search_enabled";
// Boolean pref recording the last applied value for kEssentialSearchEnabled
// prefs.
inline constexpr char kLastEssentialSearchValue[] =
    "last_essential_search_value";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
// Boolean that indicates whether the browser should put up a confirmation
// window when the user is attempting to quit. Only on Mac.
inline constexpr char kConfirmToQuitEnabled[] = "browser.confirm_to_quit";

// Boolean that indicates whether the browser should show the toolbar when it's
// in fullscreen. Mac only.
inline constexpr char kShowFullscreenToolbar[] =
    "browser.show_fullscreen_toolbar";

// Boolean that indicates whether the browser should allow Javascript injection
// via Apple Events. Mac only.
inline constexpr char kAllowJavascriptAppleEvents[] =
    "browser.allow_javascript_apple_events";

#endif

// Boolean which specifies whether we should ask the user if we should download
// a file (true) or just download it automatically.
inline constexpr char kPromptForDownload[] = "download.prompt_for_download";

// Controls if the QUIC protocol is allowed.
inline constexpr char kQuicAllowed[] = "net.quic_allowed";

// Prefs for keeping whitespace for data URLs.
inline constexpr char kDataURLWhitespacePreservationEnabled[] =
    "net.keep_whitespace_data_urls";

// Prefs for persisting network qualities.
inline constexpr char kNetworkQualities[] = "net.network_qualities";

// Pref storing the user's network easter egg game high score.
inline constexpr char kNetworkEasterEggHighScore[] =
    "net.easter_egg_high_score";

// A preference of enum chrome_browser_net::NetworkPredictionOptions shows
// if prediction of network actions is allowed, depending on network type.
// Actions include DNS prefetching, TCP and SSL preconnection, prerendering
// of web pages, and resource prefetching.
// TODO(bnc): Implement this preference as per crbug.com/334602.
inline constexpr char kNetworkPredictionOptions[] =
    "net.network_prediction_options";

// An integer representing the state of the default apps installation process.
// This value is persisted in the profile's user preferences because the process
// is async, and the user may have stopped chrome in the middle.  The next time
// the profile is opened, the process will continue from where it left off.
//
// See possible values in external_provider_impl.cc.
inline constexpr char kPreinstalledAppsInstallState[] =
    "default_apps_install_state";

#if BUILDFLAG(IS_CHROMEOS)
// The list of extensions allowed to use the platformKeys API for remote
// attestation.
inline constexpr char kAttestationExtensionAllowlist[] =
    "attestation.extension_allowlist";

// A boolean specifying whether the Desk API is enabled for third party web
// applications. If set to true, the Desk API bridge component extension will be
// installed.
inline constexpr char kDeskAPIThirdPartyAccessEnabled[] =
    "desk_api.third_party_access_enabled";

inline constexpr char kDeskAPIDeskSaveAndShareEnabled[] =
    "desk_api.desk_save_and_share_enabled";

// A list of third party web application domains allowed to use the Desk API.
inline constexpr char kDeskAPIThirdPartyAllowlist[] =
    "desk_api.third_party_allowlist";

// The list of extensions allowed to skip print job confirmation dialog when
// they use the chrome.printing.submitJob() function. Note that this used to be
// `kPrintingAPIExtensionsWhitelist`, hence the difference between the variable
// name and the string value.
inline constexpr char kPrintingAPIExtensionsAllowlist[] =
    "printing.printing_api_extensions_whitelist";

// The list of extensions allowed to skip discovery and scan confirmation
// dialogs when using the chrome.documentScan API.
inline constexpr char kDocumentScanAPITrustedExtensions[] =
    "document_scan.document_scan_api_trusted_extensions";

// A boolean specifying whether the insights extension is enabled. If set to
// true, the CCaaS Chrome component extension will be installed.
inline constexpr char kInsightsExtensionEnabled[] =
    "insights_extension_enabled";

// Boolean controlling whether showing Sync Consent during sign-in is enabled.
// Controlled by policy.
inline constexpr char kEnableSyncConsent[] = "sync_consent.enabled";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)

// A boolean pref set to true if time should be displayed in 24-hour clock.
inline constexpr char kUse24HourClock[] = "settings.clock.use_24hour_clock";

// A string pref containing Timezone ID for this user.
inline constexpr char kUserTimezone[] = "settings.timezone";

// This setting controls what information is sent to the server to get
// device location to resolve time zone in user session. Values must
// match TimeZoneResolverManager::TimeZoneResolveMethod enum.
inline constexpr char kResolveTimezoneByGeolocationMethod[] =
    "settings.resolve_timezone_by_geolocation_method";

// This setting is true when kResolveTimezoneByGeolocation value
// has been migrated to kResolveTimezoneByGeolocationMethod.
inline constexpr char kResolveTimezoneByGeolocationMigratedToMethod[] =
    "settings.resolve_timezone_by_geolocation_migrated_to_method";

// A string pref set to the current input method.
// TODO: b/308389509 - Remove this constant to complete migration.
inline constexpr char kLanguageCurrentInputMethod[] =
    "settings.language.current_input_method";

// A string pref set to the previous input method.
inline constexpr char kLanguagePreviousInputMethod[] =
    "settings.language.previous_input_method";

// A list pref set to the allowed input methods (see policy
// "AllowedInputMethods").
inline constexpr char kLanguageAllowedInputMethods[] =
    "settings.language.allowed_input_methods";

// A string pref (comma-separated list) set to the preloaded (active) input
// method IDs (ex. "pinyin,mozc").
// TODO: b/308389509 - Remove this constant to complete migration.
inline constexpr char kLanguagePreloadEngines[] =
    "settings.language.preload_engines";
inline constexpr char kLanguagePreloadEnginesSyncable[] =
    "settings.language.preload_engines_syncable";

// A string pref (comma-separated list) set to the extension and ARC IMEs to be
// enabled.
inline constexpr char kLanguageEnabledImes[] =
    "settings.language.enabled_extension_imes";
inline constexpr char kLanguageEnabledImesSyncable[] =
    "settings.language.enabled_extension_imes_syncable";

// A boolean pref set to true if the IME menu is activated.
inline constexpr char kLanguageImeMenuActivated[] =
    "settings.language.ime_menu_activated";

// A dictionary of input method IDs and their settings. Each value is itself a
// dictionary of key / value string pairs, with each pair representing a setting
// and its value.
inline constexpr char kLanguageInputMethodSpecificSettings[] =
    "settings.language.input_method_specific_settings";

// A boolean pref to indicate whether we still need to add the globally synced
// input methods. False after the initial post-OOBE sync.
inline constexpr char kLanguageShouldMergeInputMethods[] =
    "settings.language.merge_input_methods";

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
inline constexpr char kLabsAdvancedFilesystemEnabled[] =
    "settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
inline constexpr char kLabsMediaplayerEnabled[] = "settings.labs.mediaplayer";

// A boolean pref of whether to show mobile data first-use warning notification.
// Note: 3g in the name is for legacy reasons. The pref was added while only 3G
// mobile data was supported.
inline constexpr char kShowMobileDataNotification[] =
    "settings.internet.mobile.show_3g_promo_notification";

// A string pref that contains version where "What's new" promo was shown.
inline constexpr char kChromeOSReleaseNotesVersion[] =
    "settings.release_notes.version";

// A string pref that contains either a Chrome app ID (see
// extensions::ExtensionId) or an Android package name (using Java package
// naming conventions) of the preferred note-taking app. An empty value
// indicates that the user hasn't selected an app yet.
inline constexpr char kNoteTakingAppId[] = "settings.note_taking_app_id";

// A boolean pref indicating whether preferred note-taking app (see
// |kNoteTakingAppId|) is allowed to handle note taking actions on the lock
// screen.
inline constexpr char kNoteTakingAppEnabledOnLockScreen[] =
    "settings.note_taking_app_enabled_on_lock_screen";

// List of note taking aps that can be enabled to run on the lock screen.
// The intended usage is to allow the set of apps that the user can enable
// to run on lock screen, not to actually enable the apps to run on lock screen.
// Note that this used to be `kNoteTakingAppsLockScreenWhitelist`, hence the
// difference between the variable name and the string value.
inline constexpr char kNoteTakingAppsLockScreenAllowlist[] =
    "settings.note_taking_apps_lock_screen_whitelist";

// Dictionary pref that maps lock screen app ID to a boolean indicating whether
// the toast dialog has been show and dismissed as the app was being launched
// on the lock screen.
inline constexpr char kNoteTakingAppsLockScreenToastShown[] =
    "settings.note_taking_apps_lock_screen_toast_shown";

// Whether the preferred note taking app should be requested to restore the last
// note created on lock screen when launched on lock screen.
inline constexpr char kRestoreLastLockScreenNote[] =
    "settings.restore_last_lock_screen_note";

// Automatically open online re-authentication window on the lock screen.
inline constexpr char kLockScreenAutoStartOnlineReauth[] =
    "lock_screen_auto_start_online_reauth";

// A boolean pref indicating whether user activity has been observed in the
// current session already. The pref is used to restore information about user
// activity after browser crashes.
inline constexpr char kSessionUserActivitySeen[] = "session.user_activity_seen";

// A preference to keep track of the session start time. If the session length
// limit is configured to start running after initial user activity has been
// observed, the pref is set after the first user activity in a session.
// Otherwise, it is set immediately after session start. The pref is used to
// restore the session start time after browser crashes. The time is expressed
// as the serialization obtained from base::Time::ToInternalValue().
inline constexpr char kSessionStartTime[] = "session.start_time";

// Holds the maximum session time in milliseconds. If this pref is set, the
// user is logged out when the maximum session time is reached. The user is
// informed about the remaining time by a countdown timer shown in the ash
// system tray.
inline constexpr char kSessionLengthLimit[] = "session.length_limit";

// Whether the session length limit should start running only after the first
// user activity has been observed in a session.
inline constexpr char kSessionWaitForInitialUserActivity[] =
    "session.wait_for_initial_user_activity";

// A preference of the last user session type. It is used with the
// kLastSessionLength pref below to store the last user session info
// on shutdown so that it could be reported on the next run.
inline constexpr char kLastSessionType[] = "session.last_session_type";

// A preference of the last user session length.
inline constexpr char kLastSessionLength[] = "session.last_session_length";

// The URL from which the Terms of Service can be downloaded. The value is only
// honored for public accounts.
inline constexpr char kTermsOfServiceURL[] = "terms_of_service.url";

// A boolean preference indicating whether user has seen first-run tutorial
// already.
inline constexpr char kFirstRunTutorialShown[] =
    "settings.first_run_tutorial_shown";

// List of mounted file systems via the File System Provider API. Used to
// restore them after a reboot.
inline constexpr char kFileSystemProviderMounted[] =
    "file_system_provider.mounted";

// A boolean pref set to true if the virtual keyboard should be enabled.
inline constexpr char kTouchVirtualKeyboardEnabled[] =
    "ui.touch_virtual_keyboard_enabled";

// A boolean pref to enable virtual keyboard smart visibility.
inline constexpr char kVirtualKeyboardSmartVisibilityEnabled[] =
    "ui.virtual_keyboard_smart_visibility_enabled";

// A dictionary pref mapping public keys that identify platform keys to its
// properties like whether it's meant for corporate usage.
inline constexpr char kPlatformKeys[] = "platform_keys";

// A boolean preference that will be registered in local_state prefs to track
// migration of permissions on device-wide key pairs and will be registered in
// Profile prefs to track migration of permissions on user-owned key pairs.
inline constexpr char kKeyPermissionsOneTimeMigrationDone[] =
    "key_permissions_one_time_migration_done";

// A boolean preference that is registered in user prefs to tracks that at least
// one PKCS#12 certificate+key pair was dual written into NSS software-backed
// slot and Chaps. This is a part of the experiment to import PKCS#12 files into
// Chaps user slot instead of NSS and if the copy from Chaps will not work this
// preference will be used to decide when a clean up is needed to delete
// non-working certificates+keys.
inline constexpr char kNssChapsDualWrittenCertsExist[] =
    "nss_chaps_dual_written_certs_exist";

// A boolean pref. If set to true, the Unified Desktop feature is made
// available and turned on by default, which allows applications to span
// multiple screens. Users may turn the feature off and on in the settings
// while this is set to true.
inline constexpr char kUnifiedDesktopEnabledByDefault[] =
    "settings.display.unified_desktop_enabled_by_default";

// A boolean pref. If set to true, the Exclude Display in Mirror Mode feature
// is made available to the user, which allows a display to be excluded in
// mirror mode. Users may turn the feature off and on in the settings while
// this is set to true.
inline constexpr char kAllowExcludeDisplayInMirrorMode[] =
    "settings.display.allow_exclude_display_in_mirror_mode";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Bluetooth revamp experience survey.
inline constexpr char kHatsBluetoothRevampCycleEndTs[] =
    "hats_bluetooth_revamp_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Bluetooth
// revamp experience survey.
inline constexpr char kHatsBluetoothRevampIsSelected[] =
    "hats_bluetooth_revamp_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Battery life experience survey.
inline constexpr char kHatsBatteryLifeCycleEndTs[] =
    "hats_battery_life_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Battery
// life experience survey.
inline constexpr char kHatsBatteryLifeIsSelected[] =
    "hats_battery_life_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Peripherals experience survey.
inline constexpr char kHatsPeripheralsCycleEndTs[] =
    "hats_peripherals_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Peripherals
// experience survey.
inline constexpr char kHatsPeripheralsIsSelected[] =
    "hats_peripherals_is_selected";

// An int64 pref. This is a timestamp, microseconds after epoch, of the most
// recent time the profile took or dismissed HaTS (happiness-tracking) survey.
inline constexpr char kHatsLastInteractionTimestamp[] =
    "hats_last_interaction_timestamp";

// An int64 pref. This is a timestamp, microseconds after epoch, of the most
// recent time the profile took or dismissed prioritized HaTS survey.
inline constexpr char kHatsPrioritizedLastInteractionTimestamp[] =
    "hats_prioritized_last_interaction_timestamp";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent survey cycle (general survey).
inline constexpr char kHatsSurveyCycleEndTimestamp[] =
    "hats_survey_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for HaTS in the current
// survey cycle (general survey).
inline constexpr char kHatsDeviceIsSelected[] = "hats_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the ENT survey
inline constexpr char kHatsEntSurveyCycleEndTs[] =
    "hats_ent_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS ENT
// survey
inline constexpr char kHatsEntDeviceIsSelected[] =
    "hats_ent_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Stability survey
inline constexpr char kHatsStabilitySurveyCycleEndTs[] =
    "hats_stability_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Stability
// survey
inline constexpr char kHatsStabilityDeviceIsSelected[] =
    "hats_stability_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the HaTS Performance survey
inline constexpr char kHatsPerformanceSurveyCycleEndTs[] =
    "hats_performance_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Performance
// survey
inline constexpr char kHatsPerformanceDeviceIsSelected[] =
    "hats_performance_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Onboarding Experience survey
inline constexpr char kHatsOnboardingSurveyCycleEndTs[] =
    "hats_onboarding_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Onboarding
// Experience survey
inline constexpr char kHatsOnboardingDeviceIsSelected[] =
    "hats_onboarding_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent ARC Games survey cycle.
inline constexpr char kHatsArcGamesSurveyCycleEndTs[] =
    "hats_arc_games_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the ARC Games survey
inline constexpr char kHatsArcGamesDeviceIsSelected[] =
    "hats_arc_games_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Audio survey cycle.
inline constexpr char kHatsAudioSurveyCycleEndTs[] =
    "hats_audio_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Audio survey
inline constexpr char kHatsAudioDeviceIsSelected[] =
    "hats_audio_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Audio Output Processing survey cycle.
inline constexpr char kHatsAudioOutputProcSurveyCycleEndTs[] =
    "hats_audio_output_proc_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Audio Output
// Processing survey
inline constexpr char kHatsAudioOutputProcDeviceIsSelected[] =
    "hats_audio_output_proc_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Bluetooth Audio survey cycle.
inline constexpr char kHatsBluetoothAudioSurveyCycleEndTs[] =
    "hats_bluetooth_audio_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Bluetooth Audio
// survey
inline constexpr char kHatsBluetoothAudioDeviceIsSelected[] =
    "hats_bluetooth_audio_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Avatar survey cycle.
inline constexpr char kHatsPersonalizationAvatarSurveyCycleEndTs[] =
    "hats_personalization_avatar_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Avatar survey.
inline constexpr char kHatsPersonalizationAvatarSurveyIsSelected[] =
    "hats_personalization_avatar_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Screensaver survey
// cycle.
inline constexpr char kHatsPersonalizationScreensaverSurveyCycleEndTs[] =
    "hats_personalization_screensaver_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Screensaver survey.
inline constexpr char kHatsPersonalizationScreensaverSurveyIsSelected[] =
    "hats_personalization_screensaver_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Wallpaper survey cycle.
inline constexpr char kHatsPersonalizationWallpaperSurveyCycleEndTs[] =
    "hats_personalization_wallpaper_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Wallpaper survey.
inline constexpr char kHatsPersonalizationWallpaperSurveyIsSelected[] =
    "hats_personalization_wallpaper_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Media App PDF survey cycle.
inline constexpr char kHatsMediaAppPdfCycleEndTs[] =
    "hats_media_app_pdf_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Media App PDF
// survey.
inline constexpr char kHatsMediaAppPdfIsSelected[] =
    "hats_media_app_pdf_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Camera App survey cycle.
inline constexpr char kHatsCameraAppSurveyCycleEndTs[] =
    "hats_camera_app_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Camera App
// survey.
inline constexpr char kHatsCameraAppDeviceIsSelected[] =
    "hats_camera_app_device_is_selected";

// indicates the end of the most recent Photos Experience survey cycle.
inline constexpr char kHatsPhotosExperienceCycleEndTs[] =
    "hats_photos_experience_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Photos Experience
// survey.
inline constexpr char kHatsPhotosExperienceIsSelected[] =
    "hats_photos_experience_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent general camera survey cycle.
inline constexpr char kHatsGeneralCameraSurveyCycleEndTs[] =
    "hats_general_camera_cycle_end_timestamp";

// A boolean pref. Indicated if the device is selected for the general camera
// survey.
inline constexpr char kHatsGeneralCameraIsSelected[] =
    "hats_general_camera_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent prioritized general camera survey cycle.
inline constexpr char kHatsGeneralCameraPrioritizedSurveyCycleEndTs[] =
    "hats_general_camera_prioritized_cycle_end_timestamp";

// A boolean pref. Indicated if the device is selected for the prioritized
// general camera survey.
inline constexpr char kHatsGeneralCameraPrioritizedIsSelected[] =
    "hats_general_camera_prioritized_is_selected";

// An base::Time pref. This is the timestamp that indicates the end of the
// most recent prioritized general camera survey.
inline constexpr char kHatsGeneralCameraPrioritizedLastInteractionTimestamp[] =
    "hats_general_camera_prioritized_last_interaction_timestamp";

// A boolean pref. Indicated if the device is selected for the Privacy Hub
// post launch survey.
inline constexpr char kHatsPrivacyHubPostLaunchIsSelected[] =
    "hats_privacy_hub_postlaunch_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent Privacy Hub post launch cycle.
inline constexpr char kHatsPrivacyHubPostLaunchCycleEndTs[] =
    "hats_privacy_hub_postlaunch_end_timestamp";

// A boolean pref. Indicated if the device is selected for the Borealis games
// survey.
inline constexpr char kHatsBorealisGamesSurveyIsSelected[] =
    "hats_borealis_games_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent Borealis games survey cycle.
inline constexpr char kHatsBorealisGamesSurveyCycleEndTs[] =
    "hats_borealis_games_end_timestamp";

// An base::Time pref. This is the timestamp that indicates the end of the
// most recent Borealis games survey interaction.
inline constexpr char kHatsBorealisGamesLastInteractionTimestamp[] =
    "hats_borealis_games_last_interaction_timestamp";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the OS Launcher Apps satisfaction survey cycle.
inline constexpr char kHatsLauncherAppsSurveyCycleEndTs[] =
    "hats_launcher_apps_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the OS Launcher
// Apps satisfaction survey.
inline constexpr char kHatsLauncherAppsSurveyIsSelected[] =
    "hats_launcher_apps_is_selected";

// A boolean pref. Indicated if the device is selected for the Office
// integration survey.
inline constexpr char kHatsOfficeSurveyIsSelected[] = "hats_office_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent Office integration survey cycle.
inline constexpr char kHatsOfficeSurveyCycleEndTs[] =
    "hats_office_end_timestamp";

// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the quick unlock feature.
inline constexpr char kPinUnlockFeatureNotificationShown[] =
    "pin_unlock_feature_notification_shown";
// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the fingerprint unlock feature.
inline constexpr char kFingerprintUnlockFeatureNotificationShown[] =
    "fingerprint_unlock_feature_notification_shown";

// Deprecated (crbug/998983) in favor of kEndOfLifeDate.
// An integer pref. Holds one of several values:
// 0: Supported. Device is in supported state.
// 1: Security Only. Device is in Security-Only update (after initial 5 years).
// 2: EOL. Device is End of Life(No more updates expected).
// This value needs to be consistent with EndOfLifeStatus enum.
inline constexpr char kEolStatus[] = "eol_status";

// A Time pref.  Holds the last used Eol Date and is compared to the latest Eol
// Date received to make changes to Eol notifications accordingly.
inline constexpr char kEndOfLifeDate[] = "eol_date";

// Boolean pref indicating that the first warning End Of Life month and year
// notification was dismissed by the user.
inline constexpr char kFirstEolWarningDismissed[] =
    "first_eol_warning_dismissed";

// Boolean pref indicating that the second warning End Of Life month and year
// notification was dismissed by the user.
inline constexpr char kSecondEolWarningDismissed[] =
    "second_eol_warning_dismissed";

// Boolean pref indicating that the End Of Life final update notification was
// dismissed by the user.
inline constexpr char kEolNotificationDismissed[] =
    "eol_notification_dismissed";

inline constexpr char kEolApproachingIncentiveNotificationDismissed[] =
    "approaching_eol_incentive_dismissed";
inline constexpr char kEolPassedFinalIncentiveDismissed[] =
    "passed_eol_incentive_dismissed";

// A boolean pref that controls whether the PIN autosubmit feature is enabled.
// This feature, when enabled, exposes the user's PIN length by showing how many
// digits are necessary to unlock the device. Can be recommended.
inline constexpr char kPinUnlockAutosubmitEnabled[] =
    "pin_unlock_autosubmit_enabled";

// Boolean pref indicating whether someone can cast to the device.
inline constexpr char kCastReceiverEnabled[] = "cast_receiver.enabled";

// String pref indicating what is the minimum version of Chrome required to
// allow user sign in. If the string is empty or blank no restrictions will
// be applied. See base::Version for exact string format.
inline constexpr char kMinimumAllowedChromeVersion[] = "minimum_req.version";

// Boolean preference that triggers chrome://settings/androidApps/details to be
// opened on user session start.
inline constexpr char kShowArcSettingsOnSessionStart[] =
    "start_arc_settings_on_session_start";

// Boolean preference that triggers chrome://settings/syncSetup to be opened
// on user session start.
inline constexpr char kShowSyncSettingsOnSessionStart[] =
    "start_sync_settings_on_session_start";

// Dictionary preference that maps language to default voice name preferences
// for the users's text-to-speech settings. For example, this might map
// 'en-US' to 'Chrome OS US English'.
inline constexpr char kTextToSpeechLangToVoiceName[] =
    "settings.tts.lang_to_voice_name";

// Double preference that controls the default text-to-speech voice rate,
// where 1.0 is an unchanged rate, and for example, 0.5 is half as fast,
// and 2.0 is twice as fast.
inline constexpr char kTextToSpeechRate[] = "settings.tts.speech_rate";

// Double preference that controls the default text-to-speech voice pitch,
// where 1.0 is unchanged, and for example 0.5 is lower, and 2.0 is
// higher-pitched.
inline constexpr char kTextToSpeechPitch[] = "settings.tts.speech_pitch";

// Double preference that controls the default text-to-speech voice volume
// relative to the system volume, where lower than 1.0 is quieter than the
// system volume, and higher than 1.0 is louder.
inline constexpr char kTextToSpeechVolume[] = "settings.tts.speech_volume";

// A dictionary containing the latest Time Limits override authorized by parent
// access code.
inline constexpr char kTimeLimitLocalOverride[] = "screen_time.local_override";

// A dictionary preference holding the usage time limit definitions for a user.
inline constexpr char kUsageTimeLimit[] = "screen_time.limit";

// Last state of the screen time limit.
inline constexpr char kScreenTimeLastState[] = "screen_time.last_state";

// Boolean pref indicating whether a user is allowed to use the Network File
// Shares for Chrome OS feature.
inline constexpr char kNetworkFileSharesAllowed[] =
    "network_file_shares.allowed";

// Boolean pref indicating whether the message displayed on the login screen for
// the managed guest session should be the full warning or not.
// True means the full warning should be displayed.
// False means the normal warning should be displayed.
// It's true by default, unless it's ensured that all extensions are "safe".
inline constexpr char kManagedSessionUseFullLoginWarning[] =
    "managed_session.use_full_warning";

// Boolean pref indicating whether the user has previously dismissed the
// one-time notification indicating the need for a cleanup powerwash after TPM
// firmware update that didn't flush the TPM SRK.
inline constexpr char kTPMFirmwareUpdateCleanupDismissed[] =
    "tpm_firmware_update.cleanup_dismissed";

// Int64 pref indicating the time in microseconds since Windows epoch
// (1601-01-01 00:00:00 UTC) when the notification informing the user about a
// planned TPM update that will clear all user data was shown. If the
// notification was not yet shown the pref holds the value Time::Min().
inline constexpr char kTPMUpdatePlannedNotificationShownTime[] =
    "tpm_auto_update.planned_notification_shown_time";

// Boolean pref indicating whether the notification informing the user that an
// auto-update that will clear all the user data at next reboot was shown.
inline constexpr char kTPMUpdateOnNextRebootNotificationShown[] =
    "tpm_auto_update.update_on_reboot_notification_shown";

// Boolean pref indicating whether the NetBios Name Query Request Protocol is
// used for discovering shares on the user's network by the Network File
// Shares for Chrome OS feature.
inline constexpr char kNetBiosShareDiscoveryEnabled[] =
    "network_file_shares.netbios_discovery.enabled";

// Amount of screen time that a child user has used in the current day.
inline constexpr char kChildScreenTimeMilliseconds[] = "child_screen_time";

// Last time the kChildScreenTimeMilliseconds was saved.
inline constexpr char kLastChildScreenTimeSaved[] =
    "last_child_screen_time_saved";

// Last time that the kChildScreenTime pref was reset.
inline constexpr char kLastChildScreenTimeReset[] =
    "last_child_screen_time_reset";

// Last milestone on which a Help App notification was shown.
inline constexpr char kHelpAppNotificationLastShownMilestone[] =
    "help_app_notification_last_shown_milestone";

// Amount of times the release notes suggestion chip should be
// shown before it disappears.
inline constexpr char kReleaseNotesSuggestionChipTimesLeftToShow[] =
    "times_left_to_show_release_notes_suggestion_chip";

// Boolean pref indicating whether the NTLM authentication protocol should be
// enabled when mounting an SMB share with a user credential by the Network File
// Shares for Chrome OS feature.
inline constexpr char kNTLMShareAuthenticationEnabled[] =
    "network_file_shares.ntlm_share_authentication.enabled";

// Dictionary pref containing configuration used to verify Parent Access Code.
// Controlled by ParentAccessCodeConfig policy.
inline constexpr char kParentAccessCodeConfig[] =
    "child_user.parent_access_code.config";

// List pref containing app activity and state for each application.
inline constexpr char kPerAppTimeLimitsAppActivities[] =
    "child_user.per_app_time_limits.app_activities";

// Int64 to specify the last timestamp the AppActivityRegistry was reset.
inline constexpr char kPerAppTimeLimitsLastResetTime[] =
    "child_user.per_app_time_limits.last_reset_time";

// Int64 to specify the last timestamp the app activity has been successfully
// reported.
inline constexpr char kPerAppTimeLimitsLastSuccessfulReportTime[] =
    "child_user.per_app_time_limits.last_successful_report_time";

// Int64 to specify the latest AppLimit update timestamp from.
inline constexpr char kPerAppTimeLimitsLatestLimitUpdateTime[] =
    "child_user.per_app_time_limits.latest_limit_update_time";

// Dictionary pref containing the per-app time limits configuration for
// child user. Controlled by PerAppTimeLimits policy.
inline constexpr char kPerAppTimeLimitsPolicy[] =
    "child_user.per_app_time_limits.policy";

// Dictionary pref containing the allowed urls, schemes and applications
// that would not be blocked by per app time limits.
inline constexpr char kPerAppTimeLimitsAllowlistPolicy[] =
    "child_user.per_app_time_limits.allowlist";

// Integer pref to record the day id (number of days since origin of time) when
// family user metrics were last recorded.
inline constexpr char kFamilyUserMetricsDayId[] = "family_user.metrics.day_id";

// TimeDelta pref to record the accumulated user session duration for family
// user metrics.
inline constexpr char kFamilyUserMetricsSessionEngagementDuration[] =
    "family_user.metrics.session_engagement_duration";

// TimeDelta pref to record the accumulated Chrome browser app usage for family
// user metrics.
inline constexpr char kFamilyUserMetricsChromeBrowserEngagementDuration[] =
    "family_user.metrics.chrome_browser_engagement_duration";

// List of preconfigured network file shares.
inline constexpr char kNetworkFileSharesPreconfiguredShares[] =
    "network_file_shares.preconfigured_shares";

// URL path string of the most recently used SMB NetworkFileShare path.
inline constexpr char kMostRecentlyUsedNetworkFileShareURL[] =
    "network_file_shares.most_recently_used_url";

// List of network files shares added by the user.
inline constexpr char kNetworkFileSharesSavedShares[] =
    "network_file_shares.saved_shares";

// A string pref storing the path of device wallpaper image file.
inline constexpr char kDeviceWallpaperImageFilePath[] =
    "policy.device_wallpaper_image_file_path";

// Boolean whether Kerberos daemon supports remembering passwords.
// Tied to KerberosRememberPasswordEnabled policy.
inline constexpr char kKerberosRememberPasswordEnabled[] =
    "kerberos.remember_password_enabled";
// Boolean whether users may add new Kerberos accounts.
// Tied to KerberosAddAccountsAllowed policy.
inline constexpr char kKerberosAddAccountsAllowed[] =
    "kerberos.add_accounts_allowed";
// Dictionary specifying a pre-set list of Kerberos accounts.
// Tied to KerberosAccounts policy.
inline constexpr char kKerberosAccounts[] = "kerberos.accounts";
// Used by KerberosCredentialsManager to remember which account is currently
// active (empty if none) and to determine whether to wake up the Kerberos
// daemon on session startup.
inline constexpr char kKerberosActivePrincipalName[] =
    "kerberos.active_principal_name";
// Used by KerberosAccountsHandler to prefill kerberos domain in
// username field of "Add a ticket" UI window.
// Tied to KerberosDomainAutocomplete policy.
inline constexpr char kKerberosDomainAutocomplete[] =
    "kerberos.domain_autocomplete";
// Used by KerberosAccountsHandler to decide if the custom default configuration
// should be prefilled.
// Tied to KerberosUseCustomPrefilledConfig policy.
inline constexpr char kKerberosUseCustomPrefilledConfig[] =
    "kerberos.use_custom_prefilled_config";
// Used by KerberosAccountsHandler to prefill kerberos krb5 config for
// manually creating new tickets.
// Tied to KerberosCustomPrefilledConfig policy.
inline constexpr char kKerberosCustomPrefilledConfig[] =
    "kerberos.custom_prefilled_config";

// A boolean pref for enabling/disabling App reinstall recommendations in Zero
// State Launcher by policy.
inline constexpr char kAppReinstallRecommendationEnabled[] =
    "zero_state_app_install_recommendation.enabled";

// A boolean pref that when set to true, prevents the browser window from
// launching at the start of the session.
inline constexpr char kStartupBrowserWindowLaunchSuppressed[] =
    "startup_browser_window_launch_suppressed";

// A string pref stored in local state. Set and read by extensions using the
// chrome.login API.
inline constexpr char kLoginExtensionApiDataForNextLoginAttempt[] =
    "extensions_api.login.data_for_next_login_attempt";

// String containing last RSU lookup key uploaded. Empty until first upload.
inline constexpr char kLastRsuDeviceIdUploaded[] =
    "rsu.last_rsu_device_id_uploaded";

// A string pref stored in local state containing the name of the device.
inline constexpr char kDeviceName[] = "device_name";

// Int64 pref indicating the time in microseconds since Windows epoch when the
// timer for update required which will block user session was started. If the
// timer is not started the pref holds the default value base::Time().
inline constexpr char kUpdateRequiredTimerStartTime[] =
    "update_required_timer_start_time";

// Int64 pref indicating the waiting time in microseconds after which the update
// required timer will expire and block user session. If the timer is not
// started the pref holds the default value base::TimeDelta().
inline constexpr char kUpdateRequiredWarningPeriod[] =
    "update_required_warning_period";

// String user profile pref that contains the host and port of the local
// proxy which tunnels user traffic, in the format <address>:<proxy>. Only set
// when System-proxy and ARC++ are enabled by policy.
inline constexpr char kSystemProxyUserTrafficHostAndPort[] =
    "system_proxy.user_traffic_host_and_port";

// Boolean pref indicating whether the supervised user has migrated EDU
// secondary account to ARC++.
inline constexpr char kEduCoexistenceArcMigrationCompleted[] =
    "account_manager.edu_coexistence_arc_migration_completed";

// Dictionary pref for shared extension storage for device pin.
inline constexpr char kSharedStorage[] = "shared_storage";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent OS Settings Search survey cycle.
inline constexpr char kHatsOsSettingsSearchSurveyCycleEndTs[] =
    "hats_os_settings_search_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the OS Settings
// Search survey.
inline constexpr char kHatsOsSettingsSearchSurveyIsSelected[] =
    "hats_os_settings_search_is_selected";

// A dictionary storing the string representation of
// chromeos::settings::mojom::Setting IDs for the unique OS Settings changed.
// Implicitly stores the total count of the unique OS Settings changed by each
// user per device.
// Key:string = the int equivalent of the Settings enum
//      chromeos::settings::mojom::Setting casted to string. Need to cast to
//      string since the keys in a dictionary can only be strings.
// Value:int = constant number 1. It signifies whether that particular Settings
//      has been used by the user during the device's lifetime.
inline constexpr char kTotalUniqueOsSettingsChanged[] =
    "settings.total_unique_os_settings_changed";

// A boolean representing whether the user has changed a unique Setting after at
// least 7 days have passed since the user completed OOBE.
inline constexpr char kHasResetFirst7DaysSettingsUsedCount[] =
    "settings.has_reset_first_seven_days_settings_used_count";

// A boolean representing whether the user has revoked their consent
// for UMA at least one time in the lifetime of the device.
//
// If the value is true, the user has revoked consent for recording their
// metrics at least once in the device's lifetime AND has made a change to
// Settings when the consent was revoked. This is the final value of this pref,
// ie. once the pref is set to true, the value will never change again. Even if
// the user grants consent again, we will not record their metric in the
// histogram
// "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.{Time}".
const char kHasEverRevokedMetricsConsent[] =
    "settings.has_ever_revoked_metrics_consent";

// A boolean to store that an admin user accessed the host device remotely when
// no user was present at the device. This boolean enables the device to display
// a notification to the local user when the session was terminated.
inline constexpr char kRemoteAdminWasPresent[] = "remote_admin_was_present";

// Pref that contains the value of the default location/volume that the user
// should see in the Files App. Normally this is MyFiles. If
// LocalUserFilesAllowed is False, this might be Google Drive or OneDrive,
// depending on the value of the DownloadDirectory policy.
inline constexpr char kFilesAppDefaultLocation[] =
    "filebrowser.default_location";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// List pref containing blocked domains of cookies that will not be moved when a
// user switches between ChromeOS devices, when the Floating SSO Service is
// enabled.
inline constexpr char kFloatingSsoDomainBlocklist[] =
    "floating_sso_domain_blocklist";

// List pref containing blocklist excepted domains of cookies to be moved when a
// user switches between ChromeOS devices, when the Floating SSO Service is
// enabled.
inline constexpr char kFloatingSsoDomainBlocklistExceptions[] =
    "floating_sso_domain_blocklist_exceptions";

// Boolean pref specifying if the the Floating SSO Service is enabled. The
// service restores the user's web service authentication state by moving
// cookies from the previous device onto another, on ChromeOS.
inline constexpr char kFloatingSsoEnabled[] = "floating_sso_enabled";

// This boolean controls whether the first window shown on first run should be
// unconditionally maximized, overriding the heuristic that normally chooses the
// window size.
inline constexpr char kForceMaximizeOnFirstRun[] =
    "ui.force_maximize_on_first_run";

// A list of extensions ids that have to be allowed to run in Incognito by the
// user in order to use Incognito mode.
inline constexpr char kMandatoryExtensionsForIncognitoNavigation[] =
    "mandatory_extensions_for_incognito_navigation";

// Counter for reporting daily OOM kills count.
inline constexpr char kOOMKillsDailyCount[] = "oom_kills.daily_count";

// Integer pref used by the metrics::DailyEvent owned by
// memory::OOMKillsMonitor.
inline constexpr char kOOMKillsDailySample[] = "oomkills.daily_sample";

// List pref containing extension IDs that are exempt from the restricted
// managed guest session clean-up procedure.
inline constexpr char
    kRestrictedManagedGuestSessionExtensionCleanupExemptList[] =
        "restricted_managed_guest_session_extension_cleanup_exempt_list";

// This pref is used in two contexts:
// In Profile prefs, it is a bool pref which encodes whether the Profile has
// used a policy-provided trusted CA certificate. This is used to display the
// "enterprise icon" security indicator in the URL bar.
//
// Legacy usage: In Local State prefs, it is a list of usernames encoding the
// same thing for the Profile associated with the user name.
//
// There is code migrating from the legacy Local State pref to the Profile pref
// in policy_cert_service_factory_ash.cc::MigrateLocalPrefIntoProfilePref .
inline constexpr char kUsedPolicyCertificates[] =
    "policy.used_policy_certificates";
#endif  // BUILDFLAG(IS_CHROMEOS)

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
inline constexpr char kShowHomeButton[] = "browser.show_home_button";

// A boolean pref set to true if the Forward button should be visible on the
// toolbar.
inline constexpr char kShowForwardButton[] = "browser.show_forward_button";

// Comma separated list of domain names (e.g. "google.com,school.edu").
// When this pref is set, the user will be able to access Google Apps
// only using an account that belongs to one of the domains from this pref.
inline constexpr char kAllowedDomainsForApps[] =
    "settings.allowed_domains_for_apps";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// A boolean pref that controls whether proxy settings from Ash-Chrome are
// applied or ignored. Always true for the primary profile.
inline constexpr char kUseAshProxy[] = "lacros.proxy.use_ash_proxy";
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Linux specific preference on whether we should match the system theme.
inline constexpr char kSystemTheme[] = "extensions.theme.system_theme";
#endif
inline constexpr char kCurrentThemePackFilename[] = "extensions.theme.pack";
inline constexpr char kCurrentThemeID[] = "extensions.theme.id";
inline constexpr char kAutogeneratedThemeColor[] = "autogenerated.theme.color";
// Policy-controlled SkColor used to generate the browser's theme. The value
// SK_ColorTRANSPARENT means the policy has not been set.
inline constexpr char kPolicyThemeColor[] = "autogenerated.theme.policy.color";

// Stores the local theme as a serialized ThemeSpecifics when signing in. This
// is used to restore the local theme upon signout.
inline constexpr char kSavedLocalTheme[] = "browser.theme.saved_local_theme";

// Flag denoting whether or not migration from syncing theme prefs to
// non-syncing counter-parts is done. See crbug.com/356148174.
inline constexpr char kSyncingThemePrefsMigratedToNonSyncing[] =
    "syncing_theme_prefs_migrated_to_non_syncing";

// A one-off flag (set to true by default) marking whether or not the incoming
// syncing theme prefs should be read. The syncing theme prefs will be read only
// once per client to honor the previously set theme in the account. See
// crbug.com/356148174.
inline constexpr char kShouldReadIncomingSyncingThemePrefs[] =
    "should_read_incoming_syncing_theme_prefs";

// Enum tracking the color scheme preference for the browser.
// Note: In the process of migration. Please use `GetThemePrefNameInMigration()`
// instead. See crbug.com/356148174.
inline constexpr char kBrowserColorSchemeDoNotUse[] =
    "browser.theme.color_scheme";
inline constexpr char kNonSyncingBrowserColorSchemeDoNotUse[] =
    "browser.theme.color_scheme2";

// SkColor used to theme the browser for Chrome Refresh. The value
// SK_ColorTRANSPARENT means the user color has not been set.
// Note: In the process of migration. Please use `GetThemePrefNameInMigration()`
// instead. See crbug.com/356148174.
inline constexpr char kUserColorDoNotUse[] = "browser.theme.user_color";
inline constexpr char kNonSyncingUserColorDoNotUse[] =
    "browser.theme.user_color2";

// Enum tracking the color variant preference for the browser.
// Note: In the process of migration. Please use `GetThemePrefNameInMigration()`
// instead. See crbug.com/356148174.
extern inline constexpr char kBrowserColorVariantDoNotUse[] =
    "browser.theme.color_variant";
inline constexpr char kNonSyncingBrowserColorVariantDoNotUse[] =
    "browser.theme.color_variant2";

// Boolean pref tracking whether chrome follows the system's color theme.
extern inline constexpr char kBrowserFollowsSystemThemeColors[] =
    "browser.theme.follows_system_colors";

// Boolean pref tracking whether the grayscale theme has been enabled.
// Note: In the process of migration. Please use `GetThemePrefNameInMigration()`
// instead. See crbug.com/356148174.
inline constexpr char kGrayscaleThemeEnabledDoNotUse[] =
    "browser.theme.is_grayscale";
inline constexpr char kNonSyncingGrayscaleThemeEnabledDoNotUse[] =
    "browser.theme.is_grayscale2";

// Boolean pref which persists whether the extensions_ui is in developer mode
// (showing developer packing tools and extensions details)
inline constexpr char kExtensionsUIDeveloperMode[] =
    "extensions.ui.developer_mode";

// Dictionary pref that tracks which command belongs to which
// extension + named command pair.
inline constexpr char kExtensionCommands[] = "extensions.commands";

// Whether Chrome should use its internal PDF viewer or not.
inline constexpr char kPluginsAlwaysOpenPdfExternally[] =
    "plugins.always_open_pdf_externally";

// Int64 containing the internal value of the time at which the default browser
// infobar was last dismissed by the user.
inline constexpr char kDefaultBrowserLastDeclined[] =
    "browser.default_browser_infobar_last_declined";

// base::Time containing time at which the default browser infobar was last
// dismissed by the user.
inline constexpr char kDefaultBrowserLastDeclinedTime[] =
    "browser.default_browser_infobar_last_declined_time";

// Int representing the number of times the user has dismissed the infobar.
inline constexpr char kDefaultBrowserDeclinedCount[] =
    "browser.default_browser_infobar_declined_count";

// base::Time containing first time the default browser app menu chip was shown.
inline constexpr char kDefaultBrowserFirstShownTime[] =
    "browser.default_browser_app_menu_first_shown_time";

// Policy setting whether default browser check should be disabled and default
// browser registration should take place.
inline constexpr char kDefaultBrowserSettingEnabled[] =
    "browser.default_browser_setting_enabled";

// String that indicates which API chrome://accessibility should show on the
// accessibility tree viewer.
inline constexpr char kShownAccessibilityApiType[] =
    "accessibility.shown_api_type";

// Whether the "Get Image Descriptions from Google" feature is enabled.
// Only shown to screen reader users.
inline constexpr char kAccessibilityImageLabelsEnabled[] =
    "settings.a11y.enable_accessibility_image_labels";

// Whether the opt-in dialog for image labels has been accepted yet. The opt-in
// need not be shown every time if it has already been accepted once.
inline constexpr char kAccessibilityImageLabelsOptInAccepted[] =
    "settings.a11y.enable_accessibility_image_labels_opt_in_accepted";

#if BUILDFLAG(IS_ANDROID)
// Whether the "Get Image Descriptions from Google" feature is enabled on
// Android. We expose this only to mobile Android.
inline constexpr char kAccessibilityImageLabelsEnabledAndroid[] =
    "settings.a11y.enable_accessibility_image_labels_android";

// Whether the "Get Image Descriptions from Google" feature is enabled only
// while on Wi-Fi, or if it can use mobile data. Exposed only to mobile Android.
inline constexpr char kAccessibilityImageLabelsOnlyOnWifi[] =
    "settings.a11y.enable_accessibility_image_labels_only_on_wifi";
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// A boolean pref which determines whether focus highlighting is enabled.
inline constexpr char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
#endif

#if defined(USE_AURA)
// Whether horizontal overscroll will trigger history navigation.
inline constexpr char kOverscrollHistoryNavigationEnabled[] =
    "settings.a11y.overscroll_history_navigation";
#endif

// Whether main node annotations are enabled.
inline constexpr char kAccessibilityMainNodeAnnotationsEnabled[] =
    "settings.a11y.enable_main_node_annotations";

// Pref indicating the page colors option the user wants. Page colors is an
// accessibility feature that simulates forced colors mode at the browser level.
inline constexpr char kPageColors[] = "settings.a11y.page_colors";

// Boolean Pref that indicates whether the user wants to enable page colors only
// when the OS is in an Increased Contrast mode such as High Contrast on Windows
// or Increased Contrast on Mac.
inline constexpr char kApplyPageColorsOnlyOnIncreasedContrast[] =
    "settings.a11y.apply_page_colors_only_on_increased_contrast";

#if BUILDFLAG(IS_WIN)
// Boolean that indicates what the default page colors state should be. When
// true, page colors will be 'High Contrast' when OS High Contrast is turned on,
// otherwise page colors will remain 'Off'.
inline constexpr char kIsDefaultPageColorsOnHighContrast[] =
    "settings.a11y.is_default_page_colors_on_high_contrast";
#endif  // BUILDFLAG(IS_WIN)

// List pref containing site urls where forced colors should not be applied.
inline constexpr char kPageColorsBlockList[] =
    "settings.a11y.page_colors_block_list";

// Boolean that indicates whether a user prefers to have default scrollbar
// styles.
inline constexpr char kPrefersDefaultScrollbarStyles[] =
    "settings.a11y.prefers_default_scrollbar_styles";

#if BUILDFLAG(IS_MAC)
// Boolean that indicates whether the application should show the info bar
// asking the user to set up automatic updates when Keystone promotion is
// required.
inline constexpr char kShowUpdatePromotionInfoBar[] =
    "browser.show_update_promotion_info_bar";
#endif

#if BUILDFLAG(IS_LINUX)
// Boolean that is false if we should show window manager decorations.  If
// true, we draw a custom chrome frame (thicker title bar and blue border).
inline constexpr char kUseCustomChromeFrame[] = "browser.custom_chrome_frame";
#endif

// Double that indicates the default zoom level.
inline constexpr char kPartitionDefaultZoomLevel[] =
#if !BUILDFLAG(IS_ANDROID)
    "partition.default_zoom_level";
#else
    "partition.default_zoom_level.android";
#endif

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
inline constexpr char kPartitionPerHostZoomLevels[] =
#if !BUILDFLAG(IS_ANDROID)
    "partition.per_host_zoom_levels";
#else
    "partition.per_host_zoom_levels.android";
#endif

#if !BUILDFLAG(IS_ANDROID)
inline constexpr char kPinnedTabs[] = "pinned_tabs";
#endif  // !BUILDFLAG(IS_ANDROID)

// Preference to disable 3D APIs (WebGL, Pepper 3D).
inline constexpr char kDisable3DAPIs[] = "disable_3d_apis";

// Whether to enable hyperlink auditing ("<a ping>").
inline constexpr char kEnableHyperlinkAuditing[] = "enable_a_ping";

// Whether to enable sending referrers.
inline constexpr char kEnableReferrers[] = "enable_referrers";

// Whether to allow the use of Encrypted Media Extensions (EME), except for the
// use of Clear Key key sytems, which is always allowed as required by the spec.
// TODO(crbug.com/40549758): This pref was used as a WebPreference which is why
// the string is prefixed with "webkit.webprefs". Now this is used in
// blink::RendererPreferences and we should migrate the pref to use a new
// non-webkit-prefixed string.
inline constexpr char kEnableEncryptedMedia[] =
    "webkit.webprefs.encrypted_media_enabled";

// Whether the deprecated PrefixedFullscreenVideo API is enabled or not.
inline constexpr char kPrefixedVideoFullscreenApiAvailability[] =
    "media.prefixed_fullscreen_video_api_availability";

// Boolean that specifies whether to import the form data for autofill from the
// default browser on first run.
inline constexpr char kImportAutofillFormData[] = "import_autofill_form_data";

// Boolean that specifies whether to import bookmarks from the default browser
// on first run.
inline constexpr char kImportBookmarks[] = "import_bookmarks";

// Boolean that specifies whether to import the browsing history from the
// default browser on first run.
inline constexpr char kImportHistory[] = "import_history";

// Boolean that specifies whether to import the homepage from the default
// browser on first run.
inline constexpr char kImportHomepage[] = "import_home_page";

// Boolean that specifies whether to import the saved passwords from the default
// browser on first run.
inline constexpr char kImportSavedPasswords[] = "import_saved_passwords";

// Boolean that specifies whether to import the search engine from the default
// browser on first run.
inline constexpr char kImportSearchEngine[] = "import_search_engine";

// Prefs used to remember selections in the "Import data" dialog on the settings
// page (chrome://settings/importData).
inline constexpr char kImportDialogAutofillFormData[] =
    "import_dialog_autofill_form_data";
inline constexpr char kImportDialogBookmarks[] = "import_dialog_bookmarks";
inline constexpr char kImportDialogHistory[] = "import_dialog_history";
inline constexpr char kImportDialogSavedPasswords[] =
    "import_dialog_saved_passwords";
inline constexpr char kImportDialogSearchEngine[] =
    "import_dialog_search_engine";

#if BUILDFLAG(IS_CHROMEOS)
// Boolean controlling whether native client is force allowed by policy.
inline constexpr char kNativeClientForceAllowed[] =
    "native_client_force_allowed";
#endif

// Profile avatar and name
inline constexpr char kProfileAvatarIndex[] = "profile.avatar_index";
inline constexpr char kProfileName[] = "profile.name";
// Whether a profile is using a default avatar name (eg. Pickles or Person 1)
// because it was randomly assigned at profile creation time.
inline constexpr char kProfileUsingDefaultName[] = "profile.using_default_name";
// Whether a profile is using an avatar without having explicitly chosen it
// (i.e. was assigned by default by legacy profile creation).
inline constexpr char kProfileUsingDefaultAvatar[] =
    "profile.using_default_avatar";
inline constexpr char kProfileUsingGAIAAvatar[] = "profile.using_gaia_avatar";

// Indicates if we've already shown a notification that high contrast
// mode is on, recommending high-contrast extensions and themes.
inline constexpr char kInvertNotificationShown[] =
    "invert_notification_version_2_shown";

// A pref holding the list of printer types to be disabled.
inline constexpr char kPrinterTypeDenyList[] =
    "printing.printer_type_deny_list";

// The allowed/default value for the 'Headers and footers' checkbox, in Print
// Preview.
inline constexpr char kPrintHeaderFooter[] = "printing.print_header_footer";

// A pref holding the allowed background graphics printing modes.
inline constexpr char kPrintingAllowedBackgroundGraphicsModes[] =
    "printing.allowed_background_graphics_modes";

// A pref holding the default background graphics mode.
inline constexpr char kPrintingBackgroundGraphicsDefault[] =
    "printing.background_graphics_default";

// A pref holding the default paper size.
inline constexpr char kPrintingPaperSizeDefault[] =
    "printing.paper_size_default";

#if BUILDFLAG(ENABLE_PRINTING)
// Boolean controlling whether printing is enabled.
inline constexpr char kPrintingEnabled[] = "printing.enabled";
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Boolean controlling whether making platform printing calls from a
// PrintBackend service instead of from the browser process is allowed by
// policy.
inline constexpr char kOopPrintDriversAllowedByPolicy[] =
    "printing.oop_print_drivers_allowed_by_policy";
#endif

// Boolean controlling whether print preview is disabled.
inline constexpr char kPrintPreviewDisabled[] =
    "printing.print_preview_disabled";

// A pref holding the value of the policy used to control default destination
// selection in the Print Preview. See DefaultPrinterSelection policy.
inline constexpr char kPrintPreviewDefaultDestinationSelectionRules[] =
    "printing.default_destination_selection_rules";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Boolean controlling whether the "Print as image" option should be available
// in Print Preview when printing a PDF.
inline constexpr char kPrintPdfAsImageAvailability[] =
    "printing.print_pdf_as_image_availability";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// An integer resolution to use for DPI when rasterizing PDFs with "Print to
// image".
inline constexpr char kPrintRasterizePdfDpi[] = "printing.rasterize_pdf_dpi";

// Boolean controlling whether the "Print as image" option should default to set
// in Print Preview when printing a PDF.
inline constexpr char kPrintPdfAsImageDefault[] =
    "printing.print_pdf_as_image_default";
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINTING)
// An integer pref that holds the PostScript mode to use when printing.
inline constexpr char kPrintPostScriptMode[] = "printing.postscript_mode";

// An integer pref that holds the rasterization mode to use when printing.
inline constexpr char kPrintRasterizationMode[] = "printing.rasterization_mode";
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// A pref that sets the default destination in Print Preview to always be the
// OS default printer instead of the most recently used destination.
inline constexpr char kPrintPreviewUseSystemDefaultPrinter[] =
    "printing.use_system_default_printer";

// A prefs that limits how many snapshots of the user's data directory there can
// be on the disk at any time. Following each major version update, Chrome will
// create a snapshot of certain portions of the user's browsing data for use in
// case of a later emergency version rollback.
inline constexpr char kUserDataSnapshotRetentionLimit[] =
    "downgrade.snapshot_retention_limit";
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// List of print servers ids that are allowed in the user policy. List of
// strings. Note that this used to be `kExternalPrintServersWhitelist`, hence
// the difference between the variable name and the string value.
inline constexpr char kExternalPrintServersAllowlist[] =
    "native_printing.external_print_servers_whitelist";

// List of print servers ids that are allowed in the device policy. List of
// strings.
inline constexpr char kDeviceExternalPrintServersAllowlist[] =
    "native_printing.device_external_print_servers_allowlist";

// List of printers configured by policy.
inline constexpr char kRecommendedPrinters[] =
    "native_printing.recommended_printers";

// Enum designating the type of restrictions bulk printers are using.
inline constexpr char kRecommendedPrintersAccessMode[] =
    "native_printing.recommended_printers_access_mode";

// List of printer ids which are explicitly disallowed.  List of strings. Note
// that this used to be `kRecommendedPrintersBlacklist`, hence the difference
// between the variable name and the string value.
inline constexpr char kRecommendedPrintersBlocklist[] =
    "native_printing.recommended_printers_blacklist";

// List of printer ids that are allowed.  List of strings. Note that this
// used to be `kRecommendedNativePrintersWhitelist`, hence the difference
// between the variable name and the string value.
inline constexpr char kRecommendedPrintersAllowlist[] =
    "native_printing.recommended_printers_whitelist";

// A Boolean flag which represents whether or not users are allowed to configure
// and use their own printers.
inline constexpr char kUserPrintersAllowed[] =
    "native_printing.user_native_printers_allowed";

// A pref holding the list of allowed printing color mode as a bitmask composed
// of |printing::ColorModeRestriction| values. 0 is no restriction.
inline constexpr char kPrintingAllowedColorModes[] =
    "printing.allowed_color_modes";

// A pref holding the list of allowed printing duplex mode as a bitmask composed
// of |printing::DuplexModeRestriction| values. 0 is no restriction.
inline constexpr char kPrintingAllowedDuplexModes[] =
    "printing.allowed_duplex_modes";

// A pref holding the allowed PIN printing modes.
inline constexpr char kPrintingAllowedPinModes[] = "printing.allowed_pin_modes";

// A pref holding the default color mode.
inline constexpr char kPrintingColorDefault[] = "printing.color_default";

// A pref holding the default duplex mode.
inline constexpr char kPrintingDuplexDefault[] = "printing.duplex_default";

// A pref holding the default PIN mode.
inline constexpr char kPrintingPinDefault[] = "printing.pin_default";

// Boolean flag which represents whether username and filename should be sent
// to print server.
inline constexpr char kPrintingSendUsernameAndFilenameEnabled[] =
    "printing.send_username_and_filename_enabled";

// Indicates how many sheets is allowed to use for a single print job.
inline constexpr char kPrintingMaxSheetsAllowed[] =
    "printing.max_sheets_allowed";

// Indicates how long print jobs metadata is stored on the device, in days.
inline constexpr char kPrintJobHistoryExpirationPeriod[] =
    "printing.print_job_history_expiration_period";

// Boolean flag which represents whether the user's print job history can be
// deleted.
inline constexpr char kDeletePrintJobHistoryAllowed[] =
    "printing.delete_print_job_history_allowed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// List pref containing the users supervised by this user.
inline constexpr char kSupervisedUsers[] = "profile.managed_users";

// List pref containing the extension ids which are not allowed to send
// notifications to the message center.
inline constexpr char kMessageCenterDisabledExtensionIds[] =
    "message_center.disabled_extension_ids";

// Boolean pref that determines whether the user can enter fullscreen mode.
// Disabling fullscreen mode also makes kiosk mode unavailable on desktop
// platforms.
inline constexpr char kFullscreenAllowed[] = "fullscreen.allowed";

#if BUILDFLAG(IS_ANDROID)
// The user requested font weight adjustment from OS-level settings.
// Exposed only to mobile Android.
inline constexpr char kAccessibilityFontWeightAdjustment[] =
    "settings.a11y.font_weight_adjustment";

inline constexpr char kAccessibilityTextSizeContrastFactor[] =
    "settings.a11y.text_size_contrast_factor";

// Boolean pref indicating whether notification permissions were migrated to
// notification channels (on Android O+ we use channels to store notification
// permission, so any existing permissions must be migrated).
inline constexpr char kMigratedToSiteNotificationChannels[] =
    "notifications.migrated_to_channels";

// Boolean pref indicating whether blocked site notification channels underwent
// a one-time reset yet for https://crbug.com/835232.
// TODO(crbug.com/40573963): Remove this after a few releases (M69?).
inline constexpr char kClearedBlockedSiteNotificationChannels[] =
    "notifications.cleared_blocked_channels";

// Usage stats reporting opt-in.
inline constexpr char kUsageStatsEnabled[] = "usage_stats_reporting.enabled";

#endif  // BUILDFLAG(IS_ANDROID)

// Maps from app ids to origin + Service Worker registration ID.
inline constexpr char kPushMessagingAppIdentifierMap[] =
    "gcm.push_messaging_application_id_map";

// A string like "com.chrome.macosx" that should be used as the GCM category
// when an app_id is sent as a subtype instead of as a category.
inline constexpr char kGCMProductCategoryForSubtypes[] =
    "gcm.product_category_for_subtypes";

// Whether a user is allowed to use Easy Unlock.
inline constexpr char kEasyUnlockAllowed[] = "easy_unlock.allowed";

// Preference storing Easy Unlock pairing data.
inline constexpr char kEasyUnlockPairing[] = "easy_unlock.pairing";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Used to indicate whether or not the toolbar redesign bubble has been shown
// and acknowledged, and the last time the bubble was shown.
inline constexpr char kToolbarIconSurfacingBubbleAcknowledged[] =
    "toolbar_icon_surfacing_bubble_acknowledged";
inline constexpr char kToolbarIconSurfacingBubbleLastShowTime[] =
    "toolbar_icon_surfacing_bubble_show_time";
#endif

// Define the IP handling policy override that WebRTC should follow. When not
// set, it defaults to "default".
inline constexpr char kWebRTCIPHandlingPolicy[] = "webrtc.ip_handling_policy";
// Define range of UDP ports allowed to be used by WebRTC PeerConnections.
inline constexpr char kWebRTCUDPPortRange[] = "webrtc.udp_port_range";
// Whether WebRTC event log collection by Google domains is allowed.
inline constexpr char kWebRtcEventLogCollectionAllowed[] =
    "webrtc.event_logs_collection";
// Holds URL patterns that specify URLs for which local IP addresses are exposed
// in ICE candidates.
inline constexpr char kWebRtcLocalIpsAllowedUrls[] =
    "webrtc.local_ips_allowed_urls";
// Whether WebRTC text log collection by Google domains is allowed.
inline constexpr char kWebRtcTextLogCollectionAllowed[] =
    "webrtc.text_log_collection_allowed";

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
// Boolean that indicates that the first run experience has been finished (or
// skipped by some policy) for this browser install.
inline constexpr char kFirstRunFinished[] = "browser.first_run_finished";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Whether or not this profile has been shown the Welcome page.
inline constexpr char kHasSeenWelcomePage[] = "browser.has_seen_welcome_page";

// The restriction imposed on managed accounts.
inline constexpr char kManagedAccountsSigninRestriction[] =
    "profile.managed_accounts.restriction.value";

// Whether or not the restriction is applied on all managed accounts of the
// machine. If this is set to True, the restriction set in
// `profile.managed_accounts.restriction.value` will be applied on all managed
// accounts on the machine, otherwhise only the account where the policy is set
// will have the restriction applied.
inline constexpr char kManagedAccountsSigninRestrictionScopeMachine[] =
    "profile.managed_accounts.restriction.all_managed_accounts";
#if !BUILDFLAG(IS_CHROMEOS)
// Whether or not the option to keep existing browsing data is checked by
// default.
inline constexpr char kEnterpriseProfileCreationKeepBrowsingData[] =
    "profile.enterprise_profile_creation.keep_existing_data_by_default";
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif

#if BUILDFLAG(IS_WIN)
// Put the user into an onboarding group that's decided when they go through
// the first run onboarding experience. Only users in a group will have their
// finch group pinged to keep track of them for the experiment.
inline constexpr char kNaviOnboardGroup[] = "browser.navi_onboard_group";
#endif  // BUILDFLAG(IS_WIN)

// Boolean indicating whether, as part of the adaptive activation quiet UI dry
// run experiment, the user has accumulated three notification permission
// request denies in a row.
inline constexpr char kHadThreeConsecutiveNotificationPermissionDenies[] =
    "profile.content_settings.had_three_consecutive_denies.notifications";

// Boolean indicating whether to show a promo for the quiet notification
// permission UI.
inline constexpr char kQuietNotificationPermissionShouldShowPromo[] =
    "profile.content_settings.quiet_permission_ui_promo.should_show."
    "notifications";

// Boolean indicating whether the promo was shown for the quiet notification
// permission UI.
inline constexpr char kQuietNotificationPermissionPromoWasShown[] =
    "profile.content_settings.quiet_permission_ui_promo.was_shown."
    "notifications";

// Boolean indicating whether support for Data URLs in SVGUseElement should be
// removed.
inline constexpr char kDataUrlInSvgUseEnabled[] =
    "profile.content_settings.data_url_in_svg_use_enabled";

// Boolean indicating if JS dialogs triggered from a different origin iframe
// should be blocked. Has no effect if
// "SuppressDifferentOriginSubframeJSDialogs" feature is disabled.
inline constexpr char kSuppressDifferentOriginSubframeJSDialogs[] =
    "suppress_different_origin_subframe_js_dialogs";

// Enum indicating if the user agent reduction feature should be forced enabled
// or disabled. Defaults to blink::features::kReduceUserAgent field trial.
inline constexpr char kUserAgentReduction[] = "user_agent_reduction";

#if !BUILDFLAG(IS_ANDROID)
// Boolean determining the side the side panel will be appear on (left / right).
// True when the side panel is aligned to the right.
inline constexpr char kSidePanelHorizontalAlignment[] =
    "side_panel.is_right_aligned";
// Boolean determining whether the companion side panel should be pinned to have
// a button in the toolbar.
inline constexpr char kSidePanelCompanionEntryPinnedToToolbar[] =
    "side_panel.companion_pinned_to_toolbar";
// Stores the mapping of side panel IDs to their widths.
inline constexpr char kSidePanelIdToWidth[] = "side_panel.id_to_width";
// Corresponds to the enterprise policy.
inline constexpr char kGoogleSearchSidePanelEnabled[] =
    "side_panel.google_search_side_panel_enabled";
// Boolean determining the side the tab search will be appear on (left / right).
// True when the tab search button is on the right side of the tab strip even in
// RTL.
inline constexpr char kTabSearchRightAligned[] = "tab_search.is_right_aligned";
#endif  // !BUILDFLAG(IS_ANDROID)

inline constexpr char kManagedPrivateNetworkAccessRestrictionsEnabled[] =
    "managed_private_network_access_restrictions_enabled";

#if BUILDFLAG(ENABLE_COMPOSE)
// Boolean indicating whether or not the Compose FRE has been completed.
inline constexpr char kPrefHasCompletedComposeFRE[] =
    "compose_has_completed_fre";

// Boolean that is true when the writing help proactive nudge UI is globally
// enabled. When false, the UI will never be shown.
inline constexpr char kEnableProactiveNudge[] =
    "compose.proactive_nudge_enabled";

// Dictionary of domains mapped to the time that they are added. A domain can be
// added through the proactive nudge UI, and can be removed through the "Offer
// writing help" settings page. When a domain is on the disabled list, the
// proactive nudge is prevented from being shown on all pages under that domain.
// The recorded time tracks when the domain was added to the disabled list and
// is used for integrating with the Chrome settings "Clear browsing data"
// feature.
// TODO(b/339524210): Refactor the stored dictionary value to track a second
// timestamp, `last_visit`, that can be used for re-surfacing the nudge after an
// elapsed time.
inline constexpr char kProactiveNudgeDisabledSitesWithTime[] =
    "compose.proactive_nudge_disabled_sites_with_time";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Integer value controlling the data region to store covered data from Chrome.
// By default, no preference is selected.
// - 0: No preference
// - 1: United States
// - 2: Europe
inline constexpr char kChromeDataRegionSetting[] = "chrome_data_region_setting";
#endif  // !BUILDFLAG(IS_ANDROID)

// Network annotations that are expected to be disabled based on policy values.
// Stored as a dict with annotation hash codes as keys.
inline constexpr char kNetworkAnnotationBlocklist[] =
    "network_annotation_blocklist";

// A dictionary pref that can contain a list of configured endpoints for
// reports to be sent to.
inline constexpr char kReportingEndpoints[] =
    "enterprise_reporting.reporting_endpoints";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The state of the SkyVault migration of local files to the cloud.
inline constexpr char kSkyVaultMigrationState[] = "skyvault.migration_state";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

#if !BUILDFLAG(IS_ANDROID)
// Used to store the value of the SerialAllowAllPortsForUrls policy.
inline constexpr char kManagedSerialAllowAllPortsForUrls[] =
    "managed.serial_allow_all_ports_for_urls";

// Used to store the value of the SerialAllowUsbDevicesForUrls policy.
inline constexpr char kManagedSerialAllowUsbDevicesForUrls[] =
    "managed.serial_allow_usb_devices_for_urls";

// Used to store the value of the WebHidAllowAllDevicesForUrls policy.
inline constexpr char kManagedWebHidAllowAllDevicesForUrls[] =
    "managed.web_hid_allow_all_devices_for_urls";

// Used to store the value of the WebHidAllowDevicesForUrls policy.
inline constexpr char kManagedWebHidAllowDevicesForUrls[] =
    "managed.web_hid_allow_devices_for_urls";

// Used to store the value of the DeviceLoginScreenWebHidAllowDevicesForUrls
// policy.
inline constexpr char kManagedWebHidAllowDevicesForUrlsOnLoginScreen[] =
    "managed.web_hid_allow_devices_for_urls_on_login_screen";

// Used to store the value of the WebHidAllowAllDevicesWithHidUsagesForUrls
// policy.
inline constexpr char kManagedWebHidAllowDevicesWithHidUsagesForUrls[] =
    "managed.web_hid_allow_devices_with_hid_usages_for_urls";
#endif  // !BUILDFLAG(IS_ANDROID)

// Directory of the last profile used.
inline constexpr char kProfileLastUsed[] = "profile.last_used";

// List of directories of the profiles last active in browser windows. It does
// not include profiles active in app windows. When a browser window is opened,
// if it's the only browser window open in the profile, its profile is added to
// this list. When a browser window is closed, and there are no other browser
// windows open in the profile, its profile is removed from this list. When
// Chrome is launched with --session-restore, each of the profiles in this list
// have their sessions restored.
inline constexpr char kProfilesLastActive[] = "profile.last_active_profiles";

// Total number of profiles created for this Chrome build. Used to tag profile
// directories.
inline constexpr char kProfilesNumCreated[] = "profile.profiles_created";

// String containing the version of Chrome that the profile was created by.
// If profile was created before this feature was added, this pref will default
// to "1.0.0.0".
inline constexpr char kProfileCreatedByVersion[] = "profile.created_by_version";

// A map of profile data directory to profile attributes. These attributes can
// be used to display information about profiles without actually having to load
// them.
inline constexpr char kProfileAttributes[] = "profile.info_cache";

// A list of the profiles that is ordered based on the user preferences. It is
// stored using the storage key of each profile which is unique. The order can
// be seen and modified on the profile picker using the drag and drop
// functionality.
inline constexpr char kProfilesOrder[] = "profile.profiles_order";

// A list of profile paths that should be deleted on shutdown. The deletion does
// not happen if the browser crashes, so we remove the profile on next start.
inline constexpr char kProfilesDeleted[] = "profiles.profiles_deleted";

// On Chrome OS, total number of non-Chrome user process crashes
// since the last report.
inline constexpr char kStabilityOtherUserCrashCount[] =
    "user_experience_metrics.stability.other_user_crash_count";

// On Chrome OS, total number of kernel crashes since the last report.
inline constexpr char kStabilityKernelCrashCount[] =
    "user_experience_metrics.stability.kernel_crash_count";

// On Chrome OS, total number of unclean system shutdowns since the
// last report.
inline constexpr char kStabilitySystemUncleanShutdownCount[] =
    "user_experience_metrics.stability.system_unclean_shutdowns";

// String containing the version of Chrome for which Chrome will not prompt the
// user about setting Chrome as the default browser.
inline constexpr char kBrowserSuppressDefaultBrowserPrompt[] =
    "browser.suppress_default_browser_prompt_for_version";

// String that refers to the study group in which this install was enrolled.
// Used to implement the sticky experiment tracking.
inline constexpr char kDefaultBrowserPromptRefreshStudyGroup[] =
    "browser.default_browser_prompt_refresh_study_group";

// A collection of position, size, and other data relating to the browser
// window to restore on startup.
inline constexpr char kBrowserWindowPlacement[] = "browser.window_placement";

// Browser window placement for popup windows.
inline constexpr char kBrowserWindowPlacementPopup[] =
    "browser.window_placement_popup";

// A collection of position, size, and other data relating to the task
// manager window to restore on startup.
inline constexpr char kTaskManagerWindowPlacement[] =
    "task_manager.window_placement";

// The most recent stored column visibility of the task manager table to be
// restored on startup.
inline constexpr char kTaskManagerColumnVisibility[] =
    "task_manager.column_visibility";

// A boolean indicating if ending processes are enabled or disabled by policy.
inline constexpr char kTaskManagerEndProcessEnabled[] =
    "task_manager.end_process_enabled";

// A collection of position, size, and other data relating to app windows to
// restore on startup.
inline constexpr char kAppWindowPlacement[] = "browser.app_window_placement";

// String which specifies where to download files to by default.
inline constexpr char kDownloadDefaultDirectory[] =
    "download.default_directory";

// Boolean that records if the download directory was changed by an
// upgrade a unsafe location to a safe location.
inline constexpr char kDownloadDirUpgraded[] = "download.directory_upgrade";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
inline constexpr char kOpenPdfDownloadInSystemReader[] =
    "download.open_pdf_in_system_reader";
#endif

#if BUILDFLAG(IS_ANDROID)
// A boolean specifying whether pdf files triggered by external apps are
// auto opened after download completion.
inline constexpr char kAutoOpenPdfEnabled[] = "download.auto_open_pdf_enabled";

// Int (as defined by DownloadPromptStatus) which specifies whether we should
// ask the user where they want to download the file (only for Android).
inline constexpr char kPromptForDownloadAndroid[] =
    "download.prompt_for_download_android";

// Boolean which specifies whether we should display the missing SD card error.
// This is only applicable for Android.
inline constexpr char kShowMissingSdCardErrorAndroid[] =
    "download.show_missing_sd_card_error_android";

// Boolean which specifies whether the user has turned on incognito
// reauthentication setting for Android.
inline constexpr char kIncognitoReauthenticationForAndroid[] =
    "incognito.incognito_reauthentication";
#endif

// String which specifies where to save html files to by default.
inline constexpr char kSaveFileDefaultDirectory[] =
    "savefile.default_directory";

// The type used to save the page. See the enum SavePackage::SavePackageType in
// the chrome/browser/download/save_package.h for the possible values.
inline constexpr char kSaveFileType[] = "savefile.type";

// String which specifies the last directory that was chosen for uploading
// or opening a file.
inline constexpr char kSelectFileLastDirectory[] = "selectfile.last_directory";

// Boolean that specifies if file selection dialogs are shown.
inline constexpr char kAllowFileSelectionDialogs[] =
    "select_file_dialogs.allowed";

// Map of default tasks, associated by MIME type.
inline constexpr char kDefaultTasksByMimeType[] =
    "filebrowser.tasks.default_by_mime_type";

// Map of default tasks, associated by file suffix.
inline constexpr char kDefaultTasksBySuffix[] =
    "filebrowser.tasks.default_by_suffix";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Maps file extensions to handlers according to the
// DefaultHandlersForFileExtensions policy.
inline constexpr char kDefaultHandlersForFileExtensions[] =
    "filebrowser.default_handlers_for_file_extensions";

// Whether we should always move office files to Google Drive without prompting
// the user first.
inline constexpr char kOfficeFilesAlwaysMoveToDrive[] =
    "filebrowser.office.always_move_to_drive";

// Whether we should always move office files to OneDrive without prompting the
// user first.
inline constexpr char kOfficeFilesAlwaysMoveToOneDrive[] =
    "filebrowser.office.always_move_to_onedrive";

// Whether the move confirmation dialog has been shown before for Google Drive.
inline constexpr char kOfficeMoveConfirmationShownForDrive[] =
    "filebrowser.office.move_confirmation_shown_for_drive";

// Whether the move confirmation dialog has been shown before for OneDrive.
inline constexpr char kOfficeMoveConfirmationShownForOneDrive[] =
    "filebrowser.office.move_confirmation_shown_for_onedrive";

// Whether the move confirmation dialog has been shown before for uploading
// local files to Drive.
inline constexpr char kOfficeMoveConfirmationShownForLocalToDrive[] =
    "filebrowser.office.move_confirmation_shown_for_local_to_drive";

// Whether the move confirmation dialog has been shown before for uploading
// local files to OneDrive.
inline constexpr char kOfficeMoveConfirmationShownForLocalToOneDrive[] =
    "filebrowser.office.move_confirmation_shown_for_local_to_onedrive";

// Whether the move confirmation dialog has been shown before for uploading
// cloud files to Drive.
inline constexpr char kOfficeMoveConfirmationShownForCloudToDrive[] =
    "filebrowser.office.move_confirmation_shown_for_cloud_to_drive";

// Whether the move confirmation dialog has been shown before for uploading
// cloud files to OneDrive.
inline constexpr char kOfficeMoveConfirmationShownForCloudToOneDrive[] =
    "filebrowser.office.move_confirmation_shown_for_cloud_to_onedrive";

// The timestamp of the latest office file automatically moved to OneDrive.
inline constexpr char kOfficeFileMovedToOneDrive[] =
    "filebrowser.office.file_moved_one_drive";

// The timestamp of the latest office file automatically moved to Google Drive.
inline constexpr char kOfficeFileMovedToGoogleDrive[] =
    "filebrowser.office.file_moved_google_drive";

// Pref that contains the value of the LocalUserFilesAllowed policy.
inline constexpr char kLocalUserFilesAllowed[] =
    "filebrowser.local_user_files_allowed";

// Pref that contains the value of the LocalUserFilesMigrationDestination
// policy.
inline constexpr char kLocalUserFilesMigrationDestination[] =
    "filebrowser.local_user_files_migration_destination";

// Whether the user can remove OneDrive.
inline constexpr char kAllowUserToRemoveODFS[] = "allow_user_to_remove_odfs";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Pref that contains the value of the MicrosoftOneDriveMount policy.
inline constexpr char kMicrosoftOneDriveMount[] =
    "filebrowser.office.microsoft_one_drive_mount";

// Pref that contains the value of the MicrosoftOneDriveAccountRestrictions
// policy.
inline constexpr char kMicrosoftOneDriveAccountRestrictions[] =
    "filebrowser.office.microsoft_one_drive_account_restrictions";

// Pref that contains the value of the MicrosoftOfficeCloudUpload policy.
inline constexpr char kMicrosoftOfficeCloudUpload[] =
    "filebrowser.office.microsoft_office_cloud_upload";

// Pref that contains the value of the GoogleWorkspaceCloudUpload policy.
inline constexpr char kGoogleWorkspaceCloudUpload[] =
    "filebrowser.office.google_workspace_cloud_upload";
#endif

// Extensions which should be opened upon completion.
inline constexpr char kDownloadExtensionsToOpen[] =
    "download.extensions_to_open";

// Extensions which should be opened upon completion, set by policy.
inline constexpr char kDownloadExtensionsToOpenByPolicy[] =
    "download.extensions_to_open_by_policy";

inline constexpr char kDownloadAllowedURLsForOpenByPolicy[] =
    "download.allowed_urls_for_open_by_policy";

// Dictionary of origins that have permission to launch at least one protocol
// without first prompting the user. Each origin is a nested dictionary.
// Within an origin dictionary, if a protocol is present with value |true|,
// that protocol may be launched by that origin without first prompting
// the user.
inline constexpr char kProtocolHandlerPerOriginAllowedProtocols[] =
    "protocol_handler.allowed_origin_protocol_pairs";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
inline constexpr char kLastKnownIntranetRedirectOrigin[] =
    "browser.last_redirect_origin";

// Boolean specifying that the intranet redirect detector should be enabled.
// Defaults to true.
// See also kIntranetRedirectBehavior in the omnibox component's prefs, which
// also impacts the redirect detector.
inline constexpr char kDNSInterceptionChecksEnabled[] =
    "browser.dns_interception_checks_enabled";

// Whether to restart the current Chrome session automatically as the last thing
// before shutting everything down.
inline constexpr char kRestartLastSessionOnShutdown[] =
    "restart.last.session.on.shutdown";

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean that specifies whether or not to show security warnings for some
// potentially bad command-line flags. True by default. Controlled by the
// CommandLineFlagSecurityWarningsEnabled policy setting.
inline constexpr char kCommandLineFlagSecurityWarningsEnabled[] =
    "browser.command_line_flag_security_warnings_enabled";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Pref name for controlling presentation of promotions, including full-tab
// promotional and/or educational content.
// This preference replaces browser.promotional_tabs_enabled.
inline constexpr char kPromotionsEnabled[] = "browser.promotions_enabled";

// Boolean that specifies whether or not showing the unsupported OS warning is
// suppressed. False by default. Controlled by the SuppressUnsupportedOSWarning
// policy setting.
inline constexpr char kSuppressUnsupportedOSWarning[] =
    "browser.suppress_unsupported_os_warning";

// Set before autorestarting Chrome, cleared on clean exit.
inline constexpr char kWasRestarted[] = "was.restarted";
#endif  // !BUILDFLAG(IS_ANDROID)

// Whether Extensions are enabled.
inline constexpr char kDisableExtensions[] = "extensions.disabled";

// Keeps track of which sessions are collapsed in the Other Devices menu.
inline constexpr char kNtpCollapsedForeignSessions[] =
    "ntp.collapsed_foreign_sessions";

#if BUILDFLAG(IS_ANDROID)
// Keeps track of recently closed tabs collapsed state in the Other Devices
// menu.
inline constexpr char kNtpCollapsedRecentlyClosedTabs[] =
    "ntp.collapsed_recently_closed_tabs";

// Keeps track of snapshot documents collapsed state in the Other Devices menu.
inline constexpr char kNtpCollapsedSnapshotDocument[] =
    "ntp.collapsed_snapshot_document";

// Keeps track of sync promo collapsed state in the Other Devices menu.
inline constexpr char kNtpCollapsedSyncPromo[] = "ntp.collapsed_sync_promo";
#else
// Holds info for New Tab Page custom background
// Note: In the process of migration. Please use `GetThemePrefNameInMigration()`
// instead. See crbug.com/356148174.
inline constexpr char kNtpCustomBackgroundDictDoNotUse[] =
    "ntp.custom_background_dict";
inline constexpr char kNonSyncingNtpCustomBackgroundDictDoNotUse[] =
    "ntp.custom_background_dict2";
inline constexpr char kNtpCustomBackgroundLocalToDevice[] =
    "ntp.custom_background_local_to_device";
inline constexpr char kNtpCustomBackgroundLocalToDeviceId[] =
    "ntp.custom_background_local_to_device_id";
inline constexpr char kNtpCustomBackgroundInspiration[] =
    "ntp.custom_background_inspiration";
// Number of times the user has opened the side panel with the customize chrome
// button.
inline constexpr char kNtpCustomizeChromeButtonOpenCount[] =
    "NewTabPage.CustomizeChromeButtonOpenCount";
// List keeping track of disabled NTP modules.
inline constexpr char kNtpDisabledModules[] = "NewTabPage.DisabledModules";
// List keeping track of NTP modules order.
inline constexpr char kNtpModulesOrder[] = "NewTabPage.ModulesOrder";
// Whether NTP modules are visible.
inline constexpr char kNtpModulesVisible[] = "NewTabPage.ModulesVisible";
// Dictionary of number of times a module has loaded.
inline constexpr char kNtpModulesLoadedCountDict[] =
    "NewTabPage.ModulesLoadedCountDict";
// Dictionary of number of times the user has interacted with a module.
inline constexpr char kNtpModulesInteractedCountDict[] =
    "NewTabPage.ModulesInteractedCountDict";
// List of promos that the user has dismissed while on the NTP.
inline constexpr char kNtpPromoBlocklist[] = "ntp.promo_blocklist";
// Whether the promo is visible.
inline constexpr char kNtpPromoVisible[] = "ntp.promo_visible";
// Number of times NTP wallpaper search button animation has been visible.
inline constexpr char kNtpWallpaperSearchButtonShownCount[] =
    "NewTabPage.WallpaperSearchButtonShownCount";
// List of ids for past wallpaper search themes.
inline constexpr char kNtpWallpaperSearchHistory[] =
    "ntp.wallpaper_search_history";
// Number of times the seed color has been changed via the Customize Chrome
// panel across NTP tabs. Incremented at most once per NTP tab.
inline constexpr char kSeedColorChangeCount[] =
    "colorpicker.SeedColorChangeCount";
#endif  // BUILDFLAG(IS_ANDROID)

// A private RSA key for ADB handshake.
inline constexpr char kDevToolsAdbKey[] = "devtools.adb_key";

// Defines administrator-set availability of developer tools.
inline constexpr char kDevToolsAvailability[] = "devtools.availability";

// This is a timestamp, milliseconds after epoch, of when devtools was last
// opened.
inline constexpr char kDevToolsLastOpenTimestamp[] =
    "devtools.last_open_timestamp";

// Defines administrator-set availability of developer tools remote debugging.
inline constexpr char kDevToolsRemoteDebuggingAllowed[] =
    "devtools.remote_debugging.allowed";

// Dictionary from background service to recording expiration time.
inline constexpr char kDevToolsBackgroundServicesExpirationDict[] =
    "devtools.backgroundserviceexpiration";

// Determines whether devtools should be discovering usb devices for
// remote debugging at chrome://inspect.
inline constexpr char kDevToolsDiscoverUsbDevicesEnabled[] =
    "devtools.discover_usb_devices";

// Maps of files edited locally using DevTools.
inline constexpr char kDevToolsEditedFiles[] = "devtools.edited_files";

// List of file system paths added in DevTools.
inline constexpr char kDevToolsFileSystemPaths[] = "devtools.file_system_paths";

// A boolean specifying whether port forwarding should be enabled.
inline constexpr char kDevToolsPortForwardingEnabled[] =
    "devtools.port_forwarding_enabled";

// A boolean specifying whether default port forwarding configuration has been
// set.
inline constexpr char kDevToolsPortForwardingDefaultSet[] =
    "devtools.port_forwarding_default_set";

// A dictionary of port->location pairs for port forwarding.
inline constexpr char kDevToolsPortForwardingConfig[] =
    "devtools.port_forwarding_config";

// A boolean specifying whether or not Chrome will scan for available remote
// debugging targets.
inline constexpr char kDevToolsDiscoverTCPTargetsEnabled[] =
    "devtools.discover_tcp_targets";

// A list of strings representing devtools target discovery servers.
inline constexpr char kDevToolsTCPDiscoveryConfig[] =
    "devtools.tcp_discovery_config";

// A dictionary with all unsynced DevTools settings.
inline constexpr char kDevToolsPreferences[] = "devtools.preferences";

// A boolean specifying whether the "syncable" subset of DevTools preferences
// should be synced or not.
inline constexpr char kDevToolsSyncPreferences[] = "devtools.sync_preferences";

// Dictionaries with all synced DevTools settings. Depending on the state of the
// kDevToolsSyncPreferences toggle, one or the other dictionary will be used.
// The "Enabled" dictionary is synced via Chrome Sync with the rest of Chrome
// settings, while the "Disabled" dictionary won't be synced. This allows
// DevTools to opt-in of syncing DevTools settings independently from syncing
// Chrome settings.
inline constexpr char kDevToolsSyncedPreferencesSyncEnabled[] =
    "devtools.synced_preferences_sync_enabled";
inline constexpr char kDevToolsSyncedPreferencesSyncDisabled[] =
    "devtools.synced_preferences_sync_disabled";

inline constexpr char kDevToolsGenAiSettings[] = "devtools.gen_ai_settings";

#if !BUILDFLAG(IS_ANDROID)
// Tracks the number of times the dice signin promo has been shown in the user
// menu.
inline constexpr char kDiceSigninUserMenuPromoCount[] =
    "sync_promo.user_menu_show_count";
#endif

// Create web application shortcut dialog preferences.
inline constexpr char kWebAppCreateOnDesktop[] =
    "browser.web_app.create_on_desktop";
inline constexpr char kWebAppCreateInAppsMenu[] =
    "browser.web_app.create_in_apps_menu";
inline constexpr char kWebAppCreateInQuickLaunchBar[] =
    "browser.web_app.create_in_quick_launch_bar";

// A list of dictionaries for force-installed Web Apps. Each dictionary contains
// two strings: the URL of the Web App and "tab" or "window" for where the app
// will be launched.
inline constexpr char kWebAppInstallForceList[] =
    "profile.web_app.install.forcelist";

// A list of dictionaries for managing Web Apps.
inline constexpr char kWebAppSettings[] = "profile.web_app.policy_settings";

// A map of App ID to install URLs to keep track of preinstalled web apps
// after they have been deleted.
inline constexpr char kUserUninstalledPreinstalledWebAppPref[] =
    "web_app.app_id.install_url";

// A list of dictionaries for managed configurations. Each dictionary
// contains 3 strings -- origin to be configured, link to the configuration,
// and the hashed value to that configuration.
inline constexpr char kManagedConfigurationPerOrigin[] =
    "profile.managed_configuration.list";

// Dictionary that maps the hash of the last downloaded managed configuration
// for a particular origin.
inline constexpr char kLastManagedConfigurationHashForOrigin[] =
    "profile.managed_configuration.last_hash";

// Dictionary that maps web app ids to installation metrics used by UMA.
inline constexpr char kWebAppInstallMetrics[] = "web_app_install_metrics";

// Dictionary that maps web app start URLs to temporary metric info to be
// emitted once the date changes.
inline constexpr char kWebAppsDailyMetrics[] = "web_apps.daily_metrics";

// Time representing the date for which |kWebAppsDailyMetrics| is stored.
inline constexpr char kWebAppsDailyMetricsDate[] =
    "web_apps.daily_metrics_date";

// Dictionary that stores IPH state not scoped to a particular app.
inline constexpr char kWebAppsAppAgnosticIphState[] =
    "web_apps.app_agnostic_iph_state";

// Dictionary that stores ML state not scoped to a particular app.
inline constexpr char kWebAppsAppAgnosticMlState[] =
    "web_apps.app_agnostic_ml_state";

// Dictionary that stores IPH state for link capturing not scoped to a
// particular app
inline constexpr char kWebAppsAppAgnosticIPHLinkCapturingState[] =
    "web_apps.app_agnostic_iph_link_capturing_state";

// A string representing the last version of Chrome preinstalled web apps were
// synchronised for.
inline constexpr char kWebAppsLastPreinstallSynchronizeVersion[] =
    "web_apps.last_preinstall_synchronize_version";

// A list of migrated features for migrating default chrome apps.
inline constexpr char kWebAppsDidMigrateDefaultChromeApps[] =
    "web_apps.did_migrate_default_chrome_apps";

// A list of default chrome apps that were uninstalled by the user.
inline constexpr char kWebAppsUninstalledDefaultChromeApps[] =
    "web_apps.uninstalled_default_chrome_apps";

// Dictionary that maps web app ID to a dictionary of various preferences.
// Used only in the new web applications system to store app preferences which
// outlive the app installation and uninstallation.
inline constexpr char kWebAppsPreferences[] = "web_apps.web_app_ids";

#if BUILDFLAG(IS_MAC)
// A boolean that indicates whether ad-hoc code signing should be used for
// PWA app shims. This is managed by enterprise policy.
inline constexpr char kWebAppsUseAdHocCodeSigningForAppShims[] =
    "web_apps.use_adhoc_code_signing_for_app_shims";
#endif  // BUILDFLAG(IS_MAC)

// The default audio capture device used by the Media content setting.
// TODO(crbug.com/311205211): Remove this once users have been migrated to
// `kAudioInputUserPreferenceRanking`.
inline constexpr char kDefaultAudioCaptureDeviceDeprecated[] =
    "media.default_audio_capture_device";

// The default video capture device used by the Media content setting.
// TODO(crbug.com/311205211): Remove this once users have been migrated to
// `kVideoInputUserPreferenceRanking`.
inline constexpr char kDefaultVideoCaptureDeviceDeprecated[] =
    "media.default_video_capture_Device";

// The salt used for creating Storage IDs. The Storage ID is used by encrypted
// media to bind persistent licenses to the device which is authorized to play
// the content.
inline constexpr char kMediaStorageIdSalt[] = "media.storage_id_salt";

#if BUILDFLAG(IS_WIN)
// Mapping of origin to their origin id (UnguessableToken). Origin IDs are only
// stored for origins using MediaFoundation-based CDMs.
inline constexpr char kMediaCdmOriginData[] = "media.cdm.origin_data";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// A boolean pref to determine whether or not the network service is running
// sandboxed.
inline constexpr char kNetworkServiceSandboxEnabled[] =
    "net.network_service_sandbox";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
// Records whether the user has seen an HTTP auth "negotiate" header.
inline constexpr char kReceivedHttpAuthNegotiateHeader[] =
    "net.received_http_auth_negotiate_headers";
#endif  // BUILDFLAG(IS_LINUX)

// The last used printer and its settings.
inline constexpr char kPrintPreviewStickySettings[] =
    "printing.print_preview_sticky_settings";

// The list of BackgroundContents that should be loaded when the browser
// launches.
inline constexpr char kRegisteredBackgroundContents[] =
    "background_contents.registered";

// Integer that specifies the total memory usage, in mb, that chrome will
// attempt to stay under. Can be specified via policy in addition to the default
// memory pressure rules applied per platform.
inline constexpr char kTotalMemoryLimitMb[] = "total_memory_limit_mb";

// String that lists supported HTTP authentication schemes.
inline constexpr char kAuthSchemes[] = "auth.schemes";

// List of origin schemes that allow the supported HTTP authentication schemes
// from "auth.schemes".
inline constexpr char kAllHttpAuthSchemesAllowedForOrigins[] =
    "auth.http_auth_allowed_for_origins";

// Boolean that specifies whether to disable CNAME lookups when generating
// Kerberos SPN.
inline constexpr char kDisableAuthNegotiateCnameLookup[] =
    "auth.disable_negotiate_cname_lookup";

// Boolean that specifies whether to include the port in a generated Kerberos
// SPN.
inline constexpr char kEnableAuthNegotiatePort[] = "auth.enable_negotiate_port";

// Allowlist containing servers for which Integrated Authentication is enabled.
// This pref should match |android_webview::prefs::kAuthServerAllowlist|.
inline constexpr char kAuthServerAllowlist[] = "auth.server_allowlist";

// Allowlist containing servers Chrome is allowed to do Kerberos delegation
// with. Note that this used to be `kAuthNegotiateDelegateWhitelist`, hence the
// difference between the variable name and the string value.
inline constexpr char kAuthNegotiateDelegateAllowlist[] =
    "auth.negotiate_delegate_whitelist";

// String that specifies the name of a custom GSSAPI library to load.
inline constexpr char kGSSAPILibraryName[] = "auth.gssapi_library_name";

// String that specifies the Android account type to use for Negotiate
// authentication.
inline constexpr char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Boolean that specifies whether to allow basic auth prompting on cross-
// domain sub-content requests.
inline constexpr char kAllowCrossOriginAuthPrompt[] =
    "auth.allow_cross_origin_prompt";

// Boolean that specifies whether cached (server) auth credentials are separated
// by NetworkAnonymizationKey.
inline constexpr char kGloballyScopeHTTPAuthCacheEnabled[] =
    "auth.globally_scoped_http_auth_cache_enabled";

// Integer specifying the cases where ambient authentication is enabled.
// 0 - Only allow ambient authentication in regular sessions
// 1 - Only allow ambient authentication in regular and incognito sessions
// 2 - Only allow ambient authentication in regular and guest sessions
// 3 - Allow ambient authentication in regular, incognito and guest sessions
inline constexpr char kAmbientAuthenticationInPrivateModesEnabled[] =
    "auth.ambient_auth_in_private_modes";

// Boolean that specifies whether HTTP Basic authentication is allowed for HTTP
// requests.
inline constexpr char kBasicAuthOverHttpEnabled[] =
    "auth.basic_over_http_enabled";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
// Boolean that specifies whether OK-AS-DELEGATE flag from KDC is respected
// along with kAuthNegotiateDelegateAllowlist.
inline constexpr char kAuthNegotiateDelegateByKdcPolicy[] =
    "auth.negotiate_delegate_by_kdc_policy";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
// Boolean that specifies whether NTLMv2 is enabled.
inline constexpr char kNtlmV2Enabled[] = "auth.ntlm_v2_enabled";
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_CHROMEOS)
// Boolean whether Kerberos functionality is enabled.
inline constexpr char kKerberosEnabled[] = "kerberos.enabled";

// A list of dictionaries for force-installed Isolated Web Apps. Each dictionary
// contains two strings: the update manifest URL and Web Bundle ID of the
// Isolated Web App,
inline constexpr char kIsolatedWebAppInstallForceList[] =
    "profile.isolated_web_app.install.forcelist";

// An integer pref that remembers how many force install initializations are
// pending. If more than `kIsolatedWebAppForceInstallMaxRetryTreshold`
// initializations are pending, the initialization is delayed for
// `kIsolatedWebAppForceInstallEmergencyDelay` time (More details in
// go/iwa-install-emergency-mechanism).
inline constexpr char kIsolatedWebAppPendingInitializationCount[] =
    "profile.isolated_web_app.install.pending_initialization_count";

// Holds URL patterns that specify origins that will be allowed to call
// `subApps.{add|remove|list}())` without prior user gesture and that will skip
// the user dialog authorization.
inline constexpr char
    kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins[] =
        "profile.isolated_web_app.sub_apps_allowed_without_user_gesture_and_"
        "authorization";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// The integer value of the CloudAPAuthEnabled policy.
inline constexpr char kCloudApAuthEnabled[] = "auth.cloud_ap_auth.enabled";
#endif  // BUILDFLAG(IS_WIN)

// Boolean that specifies whether to enable revocation checking (best effort)
// by default.
inline constexpr char kCertRevocationCheckingEnabled[] =
    "ssl.rev_checking.enabled";

// Boolean that specifies whether to require a successful revocation check if
// a certificate path ends in a locally-trusted (as opposed to publicly
// trusted) trust anchor.
inline constexpr char kCertRevocationCheckingRequiredLocalAnchors[] =
    "ssl.rev_checking.required_for_local_anchors";

// String specifying the minimum TLS version to negotiate. Supported values
// are "tls1.2", "tls1.3".
inline constexpr char kSSLVersionMin[] = "ssl.version_min";

// String specifying the maximum TLS version to negotiate. Supported values
// are "tls1.2", "tls1.3"
inline constexpr char kSSLVersionMax[] = "ssl.version_max";

// String specifying the TLS ciphersuites to disable. Ciphersuites are
// specified as a comma-separated list of 16-bit hexadecimal values, with
// the values being the ciphersuites assigned by the IANA registry (e.g.
// "0x0004,0x0005").
inline constexpr char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";

// List of strings specifying which hosts are allowed to have H2 connections
// coalesced when client certs are also used. This follows rules similar to
// the URLBlocklist format for hostnames: a pattern with a leading dot (e.g.
// ".example.net") matches exactly the hostname following the dot (i.e. only
// "example.net"), and a pattern with no leading dot (e.g. "example.com")
// matches that hostname and all subdomains.
inline constexpr char kH2ClientCertCoalescingHosts[] =
    "ssl.client_certs.h2_coalescing_hosts";

// List of single-label hostnames that will skip the check to possibly upgrade
// from http to https.
inline constexpr char kHSTSPolicyBypassList[] =
    "hsts.policy.upgrade_bypass_list";

// If false, disable post-quantum key agreement in TLS connections.
inline constexpr char kPostQuantumKeyAgreementEnabled[] =
    "ssl.post_quantum_enabled";
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kDevicePostQuantumKeyAgreementEnabled[] =
    "ssl.device_post_quantum_enabled";
#endif

// If false, disable Encrypted ClientHello (ECH) in TLS connections.
inline constexpr char kEncryptedClientHelloEnabled[] = "ssl.ech_enabled";

// Boolean that specifies whether the built-in asynchronous DNS client is used.
inline constexpr char kBuiltInDnsClientEnabled[] = "async_dns.enabled";

// String specifying the secure DNS mode to use. Any string other than
// "secure" or "automatic" will be mapped to the default "off" mode.
inline constexpr char kDnsOverHttpsMode[] = "dns_over_https.mode";

// String containing a space-separated list of DNS over HTTPS templates to use
// in secure mode or automatic mode. If no templates are specified in automatic
// mode, we will attempt discovery of DoH servers associated with the configured
// insecure resolvers.
inline constexpr char kDnsOverHttpsTemplates[] = "dns_over_https.templates";

#if BUILDFLAG(IS_CHROMEOS)
// String containing a space-separated list of DNS over HTTPS URI templates,
// with placeholders for user and device identifiers, to use in secure mode or
// automatic mode. If no templates are specified in automatic mode, we will
// attempt discovery of DoH servers associated with the configured insecure
// resolvers. This is very similar to kDnsOverHttpsTemplates except that on
// ChromeOS it supports additional placeholder variables which are used to
// transport identity information to the DNS provider. This is ignored on all
// other platforms than ChromeOS. On ChromeOS if it exists it will override
// kDnsOverHttpsTemplates, otherwise kDnsOverHttpsTemplates will be used. This
// pref is controlled by an enterprise policy.
inline constexpr char kDnsOverHttpsTemplatesWithIdentifiers[] =
    "dns_over_https.templates_with_identifiers";
// String containing a salt value. This is used together with
// kDnsOverHttpsTemplatesWithIdentifiers, only. The value will be used as a salt
// to a hash applied to the various identity variables to prevent dictionary
// attacks. This pref is controlled by an enterprise policy.
inline constexpr char kDnsOverHttpsSalt[] = "dns_over_https.salt";
// String containing a space-separated list of effective DNS over HTTPS URI
// templates. If `kDnsOverHttpsTemplatesWithIdentifiers` is set, this string is
// the result of evaluating `kDnsOverHttpsTemplatesWithIdentifiers` against real
// user and device data; the identity placeholders are replaced with the
// hex-encoded hashed value of the user and device identifier. When
// `kDnsOverHttpsTemplatesWithIdentifiers` is empty or not set,
// `kDnsOverHttpsEffectiveTemplates` is equal to `kDnsOverHttpsTemplates`.
// This pref is set at runtime by ash::SecureDnsManager.
inline constexpr char kDnsOverHttpsEffectiveTemplatesChromeOS[] =
    "dns_over_https.effective_templates_with_identifiers";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Boolean that specifies whether additional DNS query types (e.g. HTTPS) may be
// queried alongside the traditional A and AAAA queries.
inline constexpr char kAdditionalDnsQueryTypesEnabled[] =
    "async_dns.additional_dns_query_types_enabled";

// A pref holding the value of the policy used to explicitly allow or deny
// access to audio capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to audio capture devices
// is not allowed and no prompt will be shown.
// See also kAudioCaptureAllowedUrls.
inline constexpr char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to audio
// capture devices without prompt.
inline constexpr char kAudioCaptureAllowedUrls[] =
    "hardware.audio_capture_allowed_urls";

// A pref holding the value of the policy used to explicitly allow or deny
// access to video capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to video capture devices
// is not allowed and no prompt will be shown.
inline constexpr char kVideoCaptureAllowed[] = "hardware.video_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to video
// capture devices without prompt.
inline constexpr char kVideoCaptureAllowedUrls[] =
    "hardware.video_capture_allowed_urls";

// A pref holding the value of the policy used to explicitly allow or deny
// access to screen capture.  This includes all APIs that allow capturing
// the desktop, a window or a tab. When disabled, access to screen capture
// is not allowed and API calls will fail with an error, unless overriden by one
// of the "allowed" lists below.
inline constexpr char kScreenCaptureAllowed[] =
    "hardware.screen_capture_enabled";

// The Origin Pattern lists below serve as an "override" to the standard screen
// capture allowed pref. A given origin will be restricted to only capture the
// most restricted list that it appears in. If an origin matches a pattern from
// these lists, that origin will ignore any value set in kScreenCaptureAllowed.
// These lists are listed from least restrictive to most restrictive.
// e.g. If an origin would match patterns in both |kTabCaptureAllowedByOrigins|
// and |kWindowCaptureAllowedByOrigins|, the site would only be allowed to
// capture tabs, but would still be allowed to capture tabs if
// |kScreenCaptureAllowed| was false.

// Sites matching the Origin patterns in this list will be permitted to capture
// the desktop, windows, and tabs.
inline constexpr char kScreenCaptureAllowedByOrigins[] =
    "hardware.screen_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// windows and tabs.
inline constexpr char kWindowCaptureAllowedByOrigins[] =
    "hardware.window_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// tabs. Note that this will also allow capturing Windowed Chrome Apps.
inline constexpr char kTabCaptureAllowedByOrigins[] =
    "hardware.tab_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// tabs that have the same origin as themselves. Note that this will also allow
// capturing Windowed Chrome Apps with the same origin as the site.
inline constexpr char kSameOriginTabCaptureAllowedByOrigins[] =
    "hardware.same_origin_tab_capture_allowed_by_origins";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Dictionary for transient storage of settings that should go into device
// settings storage before owner has been assigned.
inline constexpr char kDeviceSettingsCache[] = "signed_settings_cache";

// The hardware keyboard layout of the device. This should look like
// "xkb:us::eng".
inline constexpr char kHardwareKeyboardLayout[] = "intl.hardware_keyboard";

// A boolean pref of the auto-enrollment decision. Its value is only valid if
// it's not the default value; otherwise, no auto-enrollment decision has been
// made yet.
inline constexpr char kShouldAutoEnroll[] = "ShouldAutoEnroll";

// A boolean pref of the private-set-membership decision. Its value is only
// valid if it's not the default value; otherwise, no private-set-membership
// decision has been made yet.
inline constexpr char kShouldRetrieveDeviceState[] =
    "ShouldRetrieveDeviceState";

// An integer pref. Its valid values are defined in
// enterprise_management::DeviceRegisterRequest::PsmExecutionResult enum which
// indicates all possible PSM execution results in the Chrome OS enrollment
// flow.
inline constexpr char kEnrollmentPsmResult[] = "EnrollmentPsmResult";

// An int64 pref to record the timestamp of PSM retrieving the device's
// determination successfully in the Chrome OS enrollment flow.
inline constexpr char kEnrollmentPsmDeterminationTime[] =
    "EnrollmentPsmDeterminationTime";

// An integer pref with the maximum number of bits used by the client in a
// previous auto-enrollment request. If the client goes through an auto update
// during OOBE and reboots into a version of the OS with a larger maximum
// modulus, then it will retry auto-enrollment using the updated value.
inline constexpr char kAutoEnrollmentPowerLimit[] = "AutoEnrollmentPowerLimit";

// The local state pref that stores device activity times before reporting
// them to the policy server.
inline constexpr char kDeviceActivityTimes[] = "device_status.activity_times";

// A pref that stores user app activity times before reporting them to the
// policy server.
inline constexpr char kAppActivityTimes[] = "device_status.app_activity_times";

// A pref that stores user activity times before reporting them to the policy
// server.
inline constexpr char kUserActivityTimes[] =
    "consumer_device_status.activity_times";

// The length of device uptime after which an automatic reboot is scheduled,
// expressed in seconds.
inline constexpr char kUptimeLimit[] = "automatic_reboot.uptime_limit";

// Whether an automatic reboot should be scheduled when an update has been
// applied and a reboot is required to complete the update process.
inline constexpr char kRebootAfterUpdate[] =
    "automatic_reboot.reboot_after_update";

// An any-api scoped refresh token for enterprise-enrolled devices.  Allows
// for connection to Google APIs when the user isn't logged in.  Currently used
// for for getting a cloudprint scoped token to allow printing in Guest mode,
// Public Accounts and kiosks. The versions are used to distinguish different
// token formats.
inline constexpr char kDeviceRobotAnyApiRefreshTokenV1[] =
    "device_robot_refresh_token.any-api";
inline constexpr char kDeviceRobotAnyApiRefreshTokenV2[] =
    "device_robot_refresh_token_v2.any-api";

// Device requisition for enterprise enrollment.
inline constexpr char kDeviceEnrollmentRequisition[] =
    "enrollment.device_requisition";

// Sub organization for enterprise enrollment.
inline constexpr char kDeviceEnrollmentSubOrganization[] =
    "enrollment.sub_organization";

// Whether to automatically start the enterprise enrollment step during OOBE.
inline constexpr char kDeviceEnrollmentAutoStart[] = "enrollment.auto_start";

// Whether the user may exit enrollment.
inline constexpr char kDeviceEnrollmentCanExit[] = "enrollment.can_exit";

// A string pref with initial locale set in VPD or manifest.
inline constexpr char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the device registered flag (second part after first login).
inline constexpr char kDeviceRegistered[] = "DeviceRegistered";

// Boolean pref to signal corrupted enrollment to force the device through
// enrollment recovery flow upon next boot.
inline constexpr char kEnrollmentRecoveryRequired[] =
    "EnrollmentRecoveryRequired";

// String pref with the data about the OS version and browser version at the
// time of enrollment. The format is established by release management team.
// The Chrome OS version format is
// [Milestone.]TIP_BUILD.BRANCH_BUILD.BRANCH_BRANCH_BUILD.
// Example: 15711.0.0
// For browser version the format is MAJOR.MINOR.BRANCH.BUILD.
// Example: 122.0.6252.0
inline constexpr char kEnrollmentVersionOS[] = "EnrollmentVersionOS";
inline constexpr char kEnrollmentVersionBrowser[] = "EnrollmentVersionBrowser";

// Pref name for whether we should show the Getting Started module in the Help
// app.
inline constexpr char kHelpAppShouldShowGetStarted[] =
    "help_app.should_show_get_started";

// Pref name for whether we should show the Parental Control module in the Help
// app.
inline constexpr char kHelpAppShouldShowParentalControl[] =
    "help_app.should_show_parental_control";

// Pref name for whether the device was in tablet mode when going through
// the OOBE.
inline constexpr char kHelpAppTabletModeDuringOobe[] =
    "help_app.tablet_mode_during_oobe";

// A dictionary containing server-provided device state pulled form the cloud
// after recovery.
inline constexpr char kServerBackedDeviceState[] = "server_backed_device_state";

// Customized wallpaper URL, which is already downloaded and scaled.
// The URL from this preference must never be fetched. It is compared to the
// URL from customization document to check if wallpaper URL has changed
// since wallpaper was cached.
inline constexpr char kCustomizationDefaultWallpaperURL[] =
    "customization.default_wallpaper_url";

// System uptime, when last logout started.
// This is saved to file and cleared after chrome process starts.
inline constexpr char kLogoutStartedLast[] = "chromeos.logout-started";

// A boolean preference controlling Android status reporting.
inline constexpr char kReportArcStatusEnabled[] =
    "arc.status_reporting_enabled";

// A string preference indicating the name of the OS level task scheduler
// configuration to use.
inline constexpr char kSchedulerConfiguration[] =
    "chromeos.scheduler_configuration";

// Dictionary indicating current network bandwidth throttling settings.
// Contains a boolean (is throttling enabled) and two integers (upload rate
// and download rate in kbits/s to throttle to)
inline constexpr char kNetworkThrottlingEnabled[] = "net.throttling_enabled";

// Integer pref used by the metrics::DailyEvent owned by
// ash::PowerMetricsReporter.
inline constexpr char kPowerMetricsDailySample[] = "power.metrics.daily_sample";

// Key for list of users that should be reported.
inline constexpr char kReportingUsers[] = "reporting_users";

// Whether to log events for Android app installs.
inline constexpr char kArcAppInstallEventLoggingEnabled[] =
    "arc.app_install_event_logging_enabled";

// Whether we received the remove users remote command, and hence should proceed
// with removing the users while at the login screen.
inline constexpr char kRemoveUsersRemoteCommand[] =
    "remove_users_remote_command";

// Integer pref used by the metrics::DailyEvent owned by
// ash::power::auto_screen_brightness::MetricsReporter.
inline constexpr char kAutoScreenBrightnessMetricsDailySample[] =
    "auto_screen_brightness.metrics.daily_sample";

// Integer prefs used to back event counts reported by
// ash::power::auto_screen_brightness::MetricsReporter.
inline constexpr char kAutoScreenBrightnessMetricsAtlasUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.atlas_user_adjustment_count";
inline constexpr char kAutoScreenBrightnessMetricsEveUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.eve_user_adjustment_count";
inline constexpr char
    kAutoScreenBrightnessMetricsNocturneUserAdjustmentCount[] =
        "auto_screen_brightness.metrics.nocturne_user_adjustment_count";
inline constexpr char kAutoScreenBrightnessMetricsKohakuUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.kohaku_user_adjustment_count";
inline constexpr char kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.no_als_user_adjustment_count";
inline constexpr char
    kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount[] =
        "auto_screen_brightness.metrics.supported_als_user_adjustment_count";
inline constexpr char
    kAutoScreenBrightnessMetricsUnsupportedAlsUserAdjustmentCount[] =
        "auto_screen_brightness.metrics.unsupported_als_user_adjustment_count";

// Dictionary pref containing the configuration used to verify Parent Access
// Code. The data is sent through the ParentAccessCodeConfig policy, which is
// set for child users only, and kept on the known user storage.
inline constexpr char kKnownUserParentAccessCodeConfig[] =
    "child_user.parent_access_code.config";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// String which specifies where to store the disk cache.
inline constexpr char kDiskCacheDir[] = "browser.disk_cache_dir";
// Pref name for the policy specifying the maximal cache size.
inline constexpr char kDiskCacheSize[] = "browser.disk_cache_size";

inline constexpr char kPerformanceTracingEnabled[] =
    "feedback.performance_tracing_enabled";

// Indicates that factory reset was requested from options page or reset screen.
inline constexpr char kFactoryResetRequested[] = "FactoryResetRequested";

// Indicates that when a factory reset is requested by setting
// |kFactoryResetRequested|, the user should only have the option to powerwash
// and cannot cancel the dialog otherwise.
inline constexpr char kForceFactoryReset[] = "ForceFactoryReset";

// Presence of this value indicates that a TPM firmware update has been
// requested. The value indicates the requested update mode.
inline constexpr char kFactoryResetTPMFirmwareUpdateMode[] =
    "FactoryResetTPMFirmwareUpdateMode";

// Indicates that debugging features were requested from oobe screen.
inline constexpr char kDebuggingFeaturesRequested[] =
    "DebuggingFeaturesRequested";

// Indicates that the user has requested that ARC APK Sideloading be enabled.
inline constexpr char kEnableAdbSideloadingRequested[] =
    "EnableAdbSideloadingRequested";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This setting controls initial device timezone that is used before user
// session started. It is controlled by device owner.
inline constexpr char kSigninScreenTimezone[] =
    "settings.signin_screen_timezone";

// This setting controls what information is sent to the server to get
// device location to resolve time zone outside of user session. Values must
// match TimeZoneResolverManager::TimeZoneResolveMethod enum.
inline constexpr char kResolveDeviceTimezoneByGeolocationMethod[] =
    "settings.resolve_device_timezone_by_geolocation_method";

// This is policy-controlled preference.
// It has values defined in policy enum
// SystemTimezoneAutomaticDetectionProto_AutomaticTimezoneDetectionType;
inline constexpr char kSystemTimezoneAutomaticDetectionPolicy[] =
    "settings.resolve_device_timezone_by_geolocation_policy";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Pref name for the policy controlling whether to enable Media Router.
inline constexpr char kEnableMediaRouter[] = "media_router.enable_media_router";
#if !BUILDFLAG(IS_ANDROID)
// Pref name for the policy controlling whether to force the Cast icon to be
// shown in the toolbar/overflow menu.
inline constexpr char kShowCastIconInToolbar[] =
    "media_router.show_cast_icon_in_toolbar";
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Pref name for the policy controlling the way in which users are notified of
// the need to relaunch the browser for a pending update.
inline constexpr char kRelaunchNotification[] = "browser.relaunch_notification";
// Pref name for the policy controlling the time period over which users are
// notified of the need to relaunch the browser for a pending update. Values
// are in milliseconds.
inline constexpr char kRelaunchNotificationPeriod[] =
    "browser.relaunch_notification_period";
// Pref name for the policy controlling the time interval within which the
// relaunch should take place.
inline constexpr char kRelaunchWindow[] = "browser.relaunch_window";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Pref name for the policy controlling the time period between the first user
// notification about need to relaunch and the end of the
// RelaunchNotificationPeriod. Values are in milliseconds.
inline constexpr char kRelaunchHeadsUpPeriod[] =
    "browser.relaunch_heads_up_period";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
// Counts how many times prominent call-to-actions have occurred as part of the
// Mac restore permissions experiment. https://crbug.com/1211052
inline constexpr char kMacRestoreLocationPermissionsExperimentCount[] =
    "mac_restore_location_permissions_experiment_count";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// A list of base::Time value indicating the timestamps when hardware secure
// decryption was disabled due to errors or crashes. The implementation
// maintains a max size of the list (e.g. 2).
inline constexpr char kGlobalHardwareSecureDecryptionDisabledTimes[] =
    "media.hardware_secure_decryption.disabled_times";
inline constexpr char kHardwareSecureDecryptionDisabledTimes[] =
    "hardware_secure_decryption.disabled_times";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// A dictionary containing kiosk metrics latest session related information.
// For example, kiosk session start times, number of network drops.
// This setting resides in local state.
inline constexpr char kKioskMetrics[] = "kiosk-metrics";

// A boolean pref which determines whether kiosk troubleshooting tools are
// enabled.
inline constexpr char kKioskTroubleshootingToolsEnabled[] =
    "kiosk_troubleshooting_tools_enabled";

// Pref name for providing additional urls which can access browser permissions
// already available to the kiosk web apps.
inline constexpr char kKioskBrowserPermissionsAllowedForOrigins[] =
    "policy.kiosk_browser_permissions_allowed_for_origins";

// Pref name to toggle the network prompt at web app kiosk launch when the
// device is offline and the web app is not offline enabled.
inline constexpr char kKioskWebAppOfflineEnabled[] =
    "policy.kiosk_web_app_offline_enabled";

// A boolean pref to change the kiosk active WiFi credentials scope from in
// session level to the device level.
inline constexpr char kKioskActiveWiFiCredentialsScopeChangeEnabled[] =
    "kiosk_active_wifi_credentials_scope_change_enabled";

// A boolean pref which determines whether a Web Kiosk can open more than one
// browser window.
inline constexpr char kNewWindowsInKioskAllowed[] =
    "new_windows_in_kiosk_allowed";

// A boolean pref which determines whether a remote admin can start a CRD
// connection through the 'start crd session' remote command.
inline constexpr char
    kRemoteAccessHostAllowEnterpriseRemoteSupportConnections[] =
        "enterprise_remote_support_connections_allowed";

// A boolean pref which determines whether a remote admin can start a CRD
// connection through the 'start crd session' remote command when no local user
// is present at the device.
inline constexpr char kDeviceAllowEnterpriseRemoteAccessConnections[] =
    "device_allow_enterprise_remote_access_connections";

// A dictionary containing weekly time intervals to automatically sleep and wake
// up the device.
inline constexpr char kDeviceWeeklyScheduledSuspend[] =
    "device_weekly_scheduled_suspend";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
// Defines administrator-set availability of Chrome for Testing.
inline constexpr char kChromeForTestingAllowed[] = "chrome_for_testing.allowed";
#endif

#if BUILDFLAG(IS_WIN)
inline constexpr char kUiAutomationProviderEnabled[] =
    "accessibility.ui_automation_provider_enabled";
#endif

// A boolean pref which determines whether the QR Code generator feature is
// enabled. Controlled by QRCodeGeneratorEnabled policy.
inline constexpr char kQRCodeGeneratorEnabled[] = "qr_code_generator_enabled";

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr char kOsUpdateHandlerEnabled[] = "os_update_handler_enabled";

// A boolean pref that determines whether Chrome shows system notifications
// about its features.
inline constexpr char kFeatureNotificationsEnabled[] =
    "feature_notifications_enabled";
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

// *************** SERVICE PREFS ***************
// These are attached to the service process.

inline constexpr char kCloudPrintRoot[] = "cloud_print";
inline constexpr char kCloudPrintProxyEnabled[] = "cloud_print.enabled";
// The unique id for this instance of the cloud print proxy.
inline constexpr char kCloudPrintProxyId[] = "cloud_print.proxy_id";
// The GAIA auth token for Cloud Print
inline constexpr char kCloudPrintAuthToken[] = "cloud_print.auth_token";
// The email address of the account used to authenticate with the Cloud Print
// server.
inline constexpr char kCloudPrintEmail[] = "cloud_print.email";
// Settings specific to underlying print system.
inline constexpr char kCloudPrintPrintSystemSettings[] =
    "cloud_print.print_system_settings";
// A boolean indicating whether we should poll for print jobs when don't have
// an XMPP connection (false by default).
inline constexpr char kCloudPrintEnableJobPoll[] =
    "cloud_print.enable_job_poll";
inline constexpr char kCloudPrintRobotRefreshToken[] =
    "cloud_print.robot_refresh_token";
inline constexpr char kCloudPrintRobotEmail[] = "cloud_print.robot_email";
// A boolean indicating whether we should connect to cloud print new printers.
inline constexpr char kCloudPrintConnectNewPrinters[] =
    "cloud_print.user_settings.connectNewPrinters";
// A boolean indicating whether we should ping XMPP connection.
inline constexpr char kCloudPrintXmppPingEnabled[] =
    "cloud_print.xmpp_ping_enabled";
// An int value indicating the average timeout between xmpp pings.
inline constexpr char kCloudPrintXmppPingTimeout[] =
    "cloud_print.xmpp_ping_timeout_sec";
// Dictionary with settings stored by connector setup page.
inline constexpr char kCloudPrintUserSettings[] = "cloud_print.user_settings";
// List of printers settings.
inline constexpr char kCloudPrintPrinters[] =
    "cloud_print.user_settings.printers";

// Preference to store proxy settings.
inline constexpr char kMaxConnectionsPerProxy[] =
    "net.max_connections_per_proxy";

#if BUILDFLAG(IS_MAC)
// A boolean that tracks whether to show a notification when trying to quit
// while there are apps running.
inline constexpr char kNotifyWhenAppsKeepChromeAlive[] =
    "apps.notify-when-apps-keep-chrome-alive";
#endif

// Set to true if background mode is enabled on this browser.
inline constexpr char kBackgroundModeEnabled[] = "background_mode.enabled";

// Set to true if hardware acceleration mode is enabled on this browser.
inline constexpr char kHardwareAccelerationModeEnabled[] =
    "hardware_acceleration_mode.enabled";

// Hardware acceleration mode from previous browser launch.
inline constexpr char kHardwareAccelerationModePrevious[] =
    "hardware_acceleration_mode_previous";

// Integer that specifies the policy refresh rate for device-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
inline constexpr char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";

#if !BUILDFLAG(IS_ANDROID)
// A boolean where true means that the browser has previously attempted to
// enable autoupdate and failed, so the next out-of-date browser start should
// not prompt the user to enable autoupdate, it should offer to reinstall Chrome
// instead.
inline constexpr char kAttemptedToEnableAutoupdate[] =
    "browser.attempted_to_enable_autoupdate";

// The next media gallery ID to assign.
inline constexpr char kMediaGalleriesUniqueId[] = "media_galleries.gallery_id";

// A list of dictionaries, where each dictionary represents a known media
// gallery.
inline constexpr char kMediaGalleriesRememberedGalleries[] =
    "media_galleries.remembered_galleries";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kPolicyPinnedLauncherApps[] =
    "policy_pinned_launcher_apps";
// Keeps names of rolled default pin layouts for shelf in order not to apply
// this twice. Names are separated by comma.
inline constexpr char kShelfDefaultPinLayoutRolls[] =
    "shelf_default_pin_layout_rolls";
// Same as kShelfDefaultPinLayoutRolls, but for tablet form factor devices.
inline constexpr char kShelfDefaultPinLayoutRollsForTabletFormFactor[] =
    "shelf_default_pin_layout_rolls_for_tablet_form_factor";
// Keeps track of whether a container app was pinned to shelf as a default app,
// to prevent applying the default pin twice (after the user unpins the app).
inline constexpr char kShelfContainerAppPinRolls[] =
    "shelf_container_app_pin_layout_rolls";
// Keeps track of whether the Mall app was pinned to shelf as a default app,
// to prevent applying the default pin twice (after the user unpins the app).
inline constexpr char kShelfMallAppPinRolls[] =
    "shelf_mall_app_pin_layout_rolls";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// Counts how many more times the 'profile on a network share' warning should be
// shown to the user before the next silence period.
inline constexpr char kNetworkProfileWarningsLeft[] =
    "network_profile.warnings_left";
// Tracks the time of the last shown warning. Used to reset
// |network_profile.warnings_left| after a silence period.
inline constexpr char kNetworkProfileLastWarningTime[] =
    "network_profile.last_warning_time";

// The last Chrome version at which
// shell_integration::win::MigrateTaskbarPins() completed.
inline constexpr char kShortcutMigrationVersion[] =
    "browser.shortcut_migration_version";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The RLZ brand code, if enabled.
inline constexpr char kRLZBrand[] = "rlz.brand";
// Whether RLZ pings are disabled.
inline constexpr char kRLZDisabled[] = "rlz.disabled";
// Keeps local state of app list while sync service is not available.
inline constexpr char kAppListLocalState[] = "app_list.local_state";
inline constexpr char kAppListPreferredOrder[] = "app_list.preferred_order";
#endif

// An integer that is incremented whenever changes are made to app shortcuts.
// Increasing this causes all app shortcuts to be recreated.
inline constexpr char kAppShortcutsVersion[] = "apps.shortcuts_version";

// A string indicating the architecture in which app shortcuts have been
// created. If this changes (e.g, due to migrating one's home directory
// from an Intel mac to an ARM mac), then this will cause all shortcuts to be
// re-created.
inline constexpr char kAppShortcutsArch[] = "apps.shortcuts_arch";

// This references a default content setting value which we expose through the
// preferences extensions API and also used for migration of the old
// |kEnableDRM| preference.
inline constexpr char kProtectedContentDefault[] =
    "profile.default_content_setting_values.protected_media_identifier";

// An integer per-profile pref that signals if the watchdog extension is
// installed and active. We need to know if the watchdog extension active for
// ActivityLog initialization before the extension system is initialized.
inline constexpr char kWatchdogExtensionActive[] =
    "profile.extensions.activity_log.num_consumers_active";

#if BUILDFLAG(IS_ANDROID)
// A list of partner bookmark rename/remove mappings.
// Each list item is a dictionary containing a "url", a "provider_title" and
// "mapped_title" entries, detailing the bookmark target URL (if any), the title
// given by the PartnerBookmarksProvider and either the user-visible renamed
// title or an empty string if the bookmark node was removed.
inline constexpr char kPartnerBookmarkMappings[] = "partnerbookmarks.mappings";
#endif  // BUILDFLAG(IS_ANDROID)

// Whether DNS Quick Check is disabled in proxy resolution.
//
// This is a performance optimization for WPAD (Web Proxy
// Auto-Discovery) which places a 1 second timeout on resolving the
// DNS for PAC script URLs.
//
// It is on by default, but can be disabled via the Policy option
// "WPADQuickCheckEnbled". There is no other UI for changing this
// preference.
//
// For instance, if the DNS resolution for 'wpad' takes longer than 1
// second, auto-detection will give up and fallback to the next proxy
// configuration (which could be manually configured proxy server
// rules, or an implicit fallback to DIRECT connections).
inline constexpr char kQuickCheckEnabled[] = "proxy.quick_check_enabled";

// Whether Guest Mode is enabled within the browser.
inline constexpr char kBrowserGuestModeEnabled[] =
    "profile.browser_guest_enabled";

// Whether Guest Mode is enforced within the browser.
inline constexpr char kBrowserGuestModeEnforced[] =
    "profile.browser_guest_enforced";

// Whether Adding a new Person is enabled within the user manager.
inline constexpr char kBrowserAddPersonEnabled[] = "profile.add_person_enabled";

// Whether profile can be used before sign in.
inline constexpr char kForceBrowserSignin[] = "profile.force_browser_signin";

// Whether profile picker is enabled, disabled or forced on startup.
inline constexpr char kBrowserProfilePickerAvailabilityOnStartup[] =
    "profile.picker_availability_on_startup";

// Whether the profile picker has been shown at least once.
inline constexpr char kBrowserProfilePickerShown[] = "profile.picker_shown";

// Whether to show the profile picker on startup or not.
inline constexpr char kBrowserShowProfilePickerOnStartup[] =
    "profile.show_picker_on_startup";

// Boolean which indicate if signin interception is enabled.
inline constexpr char kSigninInterceptionEnabled[] =
    "signin.interception_enabled";

#if BUILDFLAG(IS_CHROMEOS)
// A dictionary pref of the echo offer check flag. It sets offer info when
// an offer is checked.
inline constexpr char kEchoCheckedOffers[] = "EchoCheckedOffers";

// Boolean pref indicating whether the user is allowed to create secondary
// profiles in Lacros browser. This is set by a policy, and the default value
// for managed users is false.
inline constexpr char kLacrosSecondaryProfilesAllowed[] =
    "lacros_secondary_profiles_allowed";
// String pref indicating what to do when Lacros is disabled and we go back
// to using Ash.
inline constexpr char kLacrosDataBackwardMigrationMode[] =
    "lacros_data_backward_migration_mode";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Device identifier used by CryptAuth stored in local state. This ID is
// combined with a user ID before being registered with the CryptAuth server,
// so it can't correlate users on the same device.
// Note: This constant was previously specific to EasyUnlock, so the string
//       constant contains "easy_unlock".
inline constexpr char kCryptAuthDeviceId[] = "easy_unlock.device_id";

// The most recently retrieved Instance ID and Instance ID token for the app ID,
// "com.google.chrome.cryptauth", used by the CryptAuth client. These prefs are
// used to track how often (if ever) the Instance ID and Instance ID token
// rotate because CryptAuth assumes the Instance ID is static.
inline constexpr char kCryptAuthInstanceId[] = "cryptauth.instance_id";
inline constexpr char kCryptAuthInstanceIdToken[] =
    "cryptauth.instance_id_token";

// Boolean that indicates whether elevation is needed to recover Chrome upgrade.
inline constexpr char kRecoveryComponentNeedsElevation[] =
    "recovery_component.needs_elevation";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Boolean that indicates whether Chrome enterprise extension request is enabled
// or not.
inline constexpr char kCloudExtensionRequestEnabled[] =
    "enterprise_reporting.extension_request.enabled";

// A list of extension ids represents pending extension request. The ids are
// stored once user sent the request until the request is canceled, approved or
// denied.
inline constexpr char kCloudExtensionRequestIds[] =
    "enterprise_reporting.extension_request.ids";

// Policy that indicates how to handle animated images.
inline constexpr char kAnimationPolicy[] = "settings.a11y.animation_policy";

// A list of URLs (for U2F) or domains (for webauthn) that automatically permit
// direct attestation of a Security Key.
inline constexpr char kSecurityKeyPermitAttestation[] =
    "securitykey.permit_attestation";

#if BUILDFLAG(IS_MAC)
// Whether to create platform WebAuthn credentials in iCloud Keychain rather
// than the Chrome profile.
inline constexpr char kCreatePasskeysInICloudKeychain[] =
    "webauthn.create_in_icloud_keychain";
#endif

// Records the last time the CWS Info Service downloaded information about
// currently installed extensions from the Chrome Web Store, successfully
// compared it with the information stored in extension_prefs and updated the
// latter if necessary. The timestamp therefore represents the "freshness" of
// the CWS information saved.
inline constexpr char kCWSInfoTimestamp[] = "extensions.cws_info_timestamp";
inline constexpr char kCWSInfoFetchErrorTimestamp[] =
    "extensions.cws_info_fetch_error_timestamp";

// A bool value for running GarbageCollectStoragePartitionCommand.
inline constexpr char kShouldGarbageCollectStoragePartitions[] =
    "storage_partitions.should_garbage_collect";
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// In Lacros, these prefs store the expected value of the equivalent ash pref
// used by extensions. The values are sent to ash.

// A boolean pref which determines whether focus highlighting is enabled.
inline constexpr char kLacrosAccessibilityFocusHighlightEnabled[] =
    "lacros.settings.a11y.focus_highlight";

// A boolean pref storing the enabled status of the Docked Magnifier feature.
inline constexpr char kLacrosDockedMagnifierEnabled[] =
    "lacros.docked_magnifier.enabled";

// A boolean pref which determines whether autoclick is enabled.
inline constexpr char kLacrosAccessibilityAutoclickEnabled[] =
    "lacros.settings.a11y.autoclick";

// A boolean pref which determines whether caret highlighting is enabled.
inline constexpr char kLacrosAccessibilityCaretHighlightEnabled[] =
    "lacros.settings.a11y.caret_highlight";

// A boolean pref which determines whether custom cursor color is enabled.
inline constexpr char kLacrosAccessibilityCursorColorEnabled[] =
    "lacros.settings.a11y.cursor_color_enabled";

// A boolean pref which determines whether cursor highlighting is enabled.
inline constexpr char kLacrosAccessibilityCursorHighlightEnabled[] =
    "lacros.settings.a11y.cursor_highlight";

// A boolean pref which determines whether dictation is enabled.
inline constexpr char kLacrosAccessibilityDictationEnabled[] =
    "lacros.settings.a11y.dictation";

// A boolean pref which determines whether high contrast is enabled.
inline constexpr char kLacrosAccessibilityHighContrastEnabled[] =
    "lacros.settings.a11y.high_contrast_enabled";

// A boolean pref which determines whether the large cursor feature is enabled.
inline constexpr char kLacrosAccessibilityLargeCursorEnabled[] =
    "lacros.settings.a11y.large_cursor_enabled";

// A boolean pref which determines whether screen magnifier is enabled.
// NOTE: We previously had prefs named settings.a11y.screen_magnifier_type and
// settings.a11y.screen_magnifier_type2, but we only shipped one type (full).
// See http://crbug.com/170850 for history.
inline constexpr char kLacrosAccessibilityScreenMagnifierEnabled[] =
    "lacros.settings.a11y.screen_magnifier";

// A boolean pref which determines whether select-to-speak is enabled.
inline constexpr char kLacrosAccessibilitySelectToSpeakEnabled[] =
    "lacros.settings.a11y.select_to_speak";

// A boolean pref which determines whether spoken feedback is enabled.
inline constexpr char kLacrosAccessibilitySpokenFeedbackEnabled[] =
    "lacros.settings.accessibility";

// A boolean pref which determines whether the sticky keys feature is enabled.
inline constexpr char kLacrosAccessibilityStickyKeysEnabled[] =
    "lacros.settings.a11y.sticky_keys_enabled";

// A boolean pref which determines whether Switch Access is enabled.
inline constexpr char kLacrosAccessibilitySwitchAccessEnabled[] =
    "lacros.settings.a11y.switch_access.enabled";

// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
inline constexpr char kLacrosAccessibilityVirtualKeyboardEnabled[] =
    "lacros.settings.a11y.virtual_keyboard";
#endif

inline constexpr char kAllowDinosaurEasterEgg[] = "allow_dinosaur_easter_egg";

#if BUILDFLAG(IS_ANDROID)
// The latest version of Chrome available when the user clicked on the update
// menu item.
inline constexpr char kLatestVersionWhenClickedUpdateMenuItem[] =
    "omaha.latest_version_when_clicked_upate_menu_item";
#endif

#if BUILDFLAG(IS_ANDROID)
// The serialized timestamps of latest shown merchant viewer messages.
inline constexpr char kCommerceMerchantViewerMessagesShownTime[] =
    "commerce_merchant_viewer_messages_shown_time";
#endif

// A dictionary which stores whether location access is enabled for the current
// default search engine. Deprecated for kDSEPermissionsSetting.
inline constexpr char kDSEGeolocationSettingDeprecated[] =
    "dse_geolocation_setting";

// A dictionary which stores the geolocation and notifications content settings
// for the default search engine before it became the default search engine so
// that they can be restored if the DSE is ever changed.
inline constexpr char kDSEPermissionsSettings[] = "dse_permissions_settings";

// A boolean indicating whether the DSE was previously disabled by enterprise
// policy.
inline constexpr char kDSEWasDisabledByPolicy[] = "dse_was_disabled_by_policy";

// A dictionary of manifest URLs of Web Share Targets to a dictionary containing
// attributes of its share_target field found in its manifest. Each key in the
// dictionary is the name of the attribute, and the value is the corresponding
// value.
inline constexpr char kWebShareVisitedTargets[] =
    "profile.web_share.visited_targets";

#if BUILDFLAG(IS_WIN)
// Acts as a cache to remember incompatible applications through restarts. Used
// for the Incompatible Applications Warning feature.
inline constexpr char kIncompatibleApplications[] = "incompatible_applications";

// Contains the MD5 digest of the current module blacklist cache. Used to detect
// external tampering.
inline constexpr char kModuleBlocklistCacheMD5Digest[] =
    "module_blocklist_cache_md5_digest";

// A boolean value, controlling whether third party software is allowed to
// inject into Chrome's processes.
inline constexpr char kThirdPartyBlockingEnabled[] =
    "third_party_blocking_enabled";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
// A boolean value, controlling whether Chrome renderer processes have the CIG
// mitigation enabled.
inline constexpr char kRendererCodeIntegrityEnabled[] =
    "renderer_code_integrity_enabled";

// A boolean value, controlling whether Chrome renderer processes should have
// Renderer App Container enabled or not. If this pref is set to false then
// Renderer App Container is disabled, otherwise Renderer App Container is
// controlled by the `RendererAppContainer` feature owned by sandbox/policy.
inline constexpr char kRendererAppContainerEnabled[] =
    "renderer_app_container_enabled";

// A boolean that controls whether the Browser process has
// ProcessExtensionPointDisablePolicy enabled.
inline constexpr char kBlockBrowserLegacyExtensionPoints[] =
    "block_browser_legacy_extension_points";

// An integer enum that controls the policy-managed dynamic code settings. This
// is linked via a PolicyToPreferenceMapEntry to the underlying policy.
inline constexpr char kDynamicCodeSettings[] = "dynamic_code_settings";

// A boolean that controls whether the Browser process has Application Bound
// (App-Bound) Encryption enabled.
inline constexpr char kApplicationBoundEncryptionEnabled[] =
    "application_bound_encryption_enabled";

// A boolean that controls whether or not the Printing LPAC Sandbox is enabled
// or not. This is linked via a PolicyToPreferenceMapEntry to the underlying
// policy PrintingLPACSandboxEnabled.
inline constexpr char kPrintingLPACSandboxEnabled[] =
    "printing_lpac_sandbox_enabled";

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Timestamp of the clipboard's last modified time, stored in base::Time's
// internal format (int64) in local store.  (I.e., this is not a per-profile
// pref.)
inline constexpr char kClipboardLastModifiedTime[] =
    "ui.clipboard.last_modified_time";
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)

// The following set of Prefs is used by OfflineMetricsCollectorImpl to
// backup the current Chrome usage tracking state and accumulated counters
// of days with specific Chrome usage.

// The boolean flags indicating whether the specific activity was observed
// in Chrome during the day that started at |kOfflineUsageTrackingDay|. These
// are used to track usage of Chrome is used while offline and how various
// offline features affect that.
inline constexpr char kOfflineUsageStartObserved[] =
    "offline_pages.start_observed";
inline constexpr char kOfflineUsageOnlineObserved[] =
    "offline_pages.online_observed";
inline constexpr char kOfflineUsageOfflineObserved[] =
    "offline_pages.offline_observed";
// Boolean flags indicating state of a prefetch subsystem during a day.
inline constexpr char kPrefetchUsageEnabledObserved[] =
    "offline_pages.prefetch_enabled_observed";
inline constexpr char kPrefetchUsageFetchObserved[] =
    "offline_pages.prefetch_fetch_observed";
inline constexpr char kPrefetchUsageOpenObserved[] =
    "offline_pages.prefetch_open_observed";
// A time corresponding to a midnight that starts the day for which
// OfflineMetricsCollector tracks the Chrome usage. Once current time passes
// 24hrs from this point, the further tracking is attributed to the next day.
inline constexpr char kOfflineUsageTrackingDay[] = "offline_pages.tracking_day";
// Accumulated counters of days with specified Chrome usage. When there is
// likely a network connection, these counters are reported via UMA and reset.
inline constexpr char kOfflineUsageUnusedCount[] = "offline_pages.unused_count";
inline constexpr char kOfflineUsageStartedCount[] =
    "offline_pages.started_count";
inline constexpr char kOfflineUsageOfflineCount[] =
    "offline_pages.offline_count";
inline constexpr char kOfflineUsageOnlineCount[] = "offline_pages.online_count";
inline constexpr char kOfflineUsageMixedCount[] = "offline_pages.mixed_count";
// Accumulated counters of days with specified Prefetch usage. When there is
// likely a network connection, these counters are reported via UMA and reset.
inline constexpr char kPrefetchUsageEnabledCount[] =
    "offline_pages.prefetch_enabled_count";
inline constexpr char kPrefetchUsageFetchedCount[] =
    "offline_pages.prefetch_fetched_count";
inline constexpr char kPrefetchUsageOpenedCount[] =
    "offline_pages.prefetch_opened_count";
inline constexpr char kPrefetchUsageMixedCount[] =
    "offline_pages.prefetch_mixed_count";

#endif

// Stores the Media Engagement Index schema version. If the stored value
// is lower than the value in MediaEngagementService then the MEI data
// will be wiped.
inline constexpr char kMediaEngagementSchemaVersion[] =
    "media.engagement.schema_version";

// Maximum number of tabs that has been opened since the last time it has been
// reported.
inline constexpr char kTabStatsTotalTabCountMax[] =
    "tab_stats.total_tab_count_max";

// Maximum number of tabs that has been opened in a single window since the last
// time it has been reported.
inline constexpr char kTabStatsMaxTabsPerWindow[] =
    "tab_stats.max_tabs_per_window";

// Maximum number of windows that has been opened since the last time it has
// been reported.
inline constexpr char kTabStatsWindowCountMax[] = "tab_stats.window_count_max";

//  Timestamp of the last time the tab stats daily metrics have been reported.
inline constexpr char kTabStatsDailySample[] = "tab_stats.last_daily_sample";

// Discards/Reloads since last daily report.
inline constexpr char kTabStatsDiscardsExternal[] =
    "tab_stats.discards_external";
inline constexpr char kTabStatsDiscardsUrgent[] = "tab_stats.discards_urgent";
inline constexpr char kTabStatsDiscardsProactive[] =
    "tab_stats.discards_proactive";
inline constexpr char kTabStatsDiscardsSuggested[] =
    "tab_stats.discards_suggested";
inline constexpr char kTabStatsReloadsExternal[] = "tab_stats.reloads_external";
inline constexpr char kTabStatsReloadsUrgent[] = "tab_stats.reloads_urgent";
inline constexpr char kTabStatsReloadsProactive[] =
    "tab_stats.reloads_proactive";
inline constexpr char kTabStatsReloadsSuggested[] =
    "tab_stats.reloads_suggested";

// A list of origins (URLs) to treat as "secure origins" for debugging purposes.
inline constexpr char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely_treat_insecure_origin_as_secure";

// A list of origins (URLs) that specifies opting into --isolate-origins=...
// (selective Site Isolation).
inline constexpr char kIsolateOrigins[] = "site_isolation.isolate_origins";

// Boolean that specifies opting into --site-per-process (full Site Isolation).
inline constexpr char kSitePerProcess[] = "site_isolation.site_per_process";

#if !BUILDFLAG(IS_ANDROID)
// Boolean to allow SharedArrayBuffer in non-crossOriginIsolated contexts.
// TODO(crbug.com/40155376) Remove when migration to COOP+COEP is complete.
inline constexpr char kSharedArrayBufferUnrestrictedAccessAllowed[] =
    "profile.shared_array_buffer_unrestricted_access_allowed";

// Boolean that specifies whether media (audio/video) autoplay is allowed.
inline constexpr char kAutoplayAllowed[] = "media.autoplay_allowed";

// Holds URL patterns that specify URLs that will be allowed to autoplay.
inline constexpr char kAutoplayAllowlist[] = "media.autoplay_whitelist";

// Boolean that specifies whether autoplay blocking is enabled.
inline constexpr char kBlockAutoplayEnabled[] = "media.block_autoplay";

// Holds URL patterns that specify origins that will be allowed to call
// `getDisplayMedia()` without prior user gesture.
inline constexpr char kScreenCaptureWithoutGestureAllowedForOrigins[] =
    "media.screen_capture_without_gesture_allowed_for_origins";

// Holds URL patterns that specify origins that will be allowed to call
// `show{OpenFile|SaveFile|Directory}Picker()` without prior user gesture.
inline constexpr char kFileOrDirectoryPickerWithoutGestureAllowedForOrigins[] =
    "file_system.file_or_directory_picker_without_allowed_for_origins";
#endif  // !BUILDFLAG(IS_ANDROID)

// Boolean allowing Chrome to block external protocol navigation in sandboxed
// iframes.
inline constexpr char kSandboxExternalProtocolBlocked[] =
    "profile.sandbox_external_protocol_blocked";

#if BUILDFLAG(IS_LINUX)
// Boolean that indicates if system notifications are allowed to be used in
// place of Chrome notifications.
inline constexpr char kAllowSystemNotifications[] =
    "system_notifications.allowed";
#endif  // BUILDFLAG(IS_LINUX)

// Integer that holds the value of the next persistent notification ID to be
// used.
inline constexpr char kNotificationNextPersistentId[] =
    "persistent_notifications.next_id";

// Time that holds the value of the next notification trigger timestamp.
inline constexpr char kNotificationNextTriggerTime[] =
    "persistent_notifications.next_trigger";

// Preference for controlling whether tab freezing is enabled.
inline constexpr char kTabFreezingEnabled[] = "tab_freezing_enabled";

// Boolean that enables the Enterprise Hardware Platform Extension API for
// extensions installed by enterprise policy.
inline constexpr char kEnterpriseHardwarePlatformAPIEnabled[] =
    "enterprise_hardware_platform_api.enabled";

// Boolean that specifies whether Signed HTTP Exchange (SXG) loading is enabled.
inline constexpr char kSignedHTTPExchangeEnabled[] =
    "web_package.signed_exchange.enabled";

#if BUILDFLAG(IS_CHROMEOS)
// Enum that specifies client certificate management permissions for user. It
// can have one of the following values.
// 0: Users can manage all certificates.
// 1: Users can manage user certificates, but not device certificates.
// 2: Disallow users from managing certificates
// Controlled by ClientCertificateManagementAllowed policy.
inline constexpr char kClientCertificateManagementAllowed[] =
    "client_certificate_management_allowed";

// Enum that specifies CA certificate management permissions for user. It
// can have one of the following values.
// 0: Users can manage all certificates.
// 1: Users can manage user certificates, but not built-in certificates.
// 2: Disallow users from managing certificates
// Controlled by CACertificateManagementAllowed policy.
inline constexpr char kCACertificateManagementAllowed[] =
    "ca_certificate_management_allowed";
#endif

// Dictionary that contains all of the Hats Survey Metadata for desktop surveys.
inline constexpr char kHatsSurveyMetadata[] = "hats.survey_metadata";

inline constexpr char kExternalProtocolDialogShowAlwaysOpenCheckbox[] =
    "external_protocol_dialog.show_always_open_checkbox";

// List of dictionaries. For each dictionary, key "protocol" is a protocol
// (as a string) that is permitted by policy to launch an external application
// without prompting the user. Key "allowed_origins" is a nested list of origin
// patterns that defines the scope of applicability of that protocol. If the
// "allow" list is empty, that protocol rule will never apply.
inline constexpr char kAutoLaunchProtocolsFromOrigins[] =
    "protocol_handler.policy.auto_launch_protocols_from_origins";

// This pref enables the ScrollToTextFragment feature.
inline constexpr char kScrollToTextFragmentEnabled[] =
    "scroll_to_text_fragment_enabled";

#if BUILDFLAG(IS_ANDROID)
// Last time the known interception disclosure message was dismissed. Used to
// ensure a cooldown period passes before the disclosure message is displayed
// again.
inline constexpr char kKnownInterceptionDisclosureInfobarLastShown[] =
    "known_interception_disclosure_infobar_last_shown";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kRequiredClientCertificateForUser[] =
    "required_client_certificate_for_user";
inline constexpr char kRequiredClientCertificateForDevice[] =
    "required_client_certificate_for_device";
inline constexpr char kCertificateProvisioningStateForUser[] =
    "cert_provisioning_user_state";
inline constexpr char kCertificateProvisioningStateForDevice[] =
    "cert_provisioning_device_state";
#endif
// A boolean pref that enables certificate prompts when multiple certificates
// match the auto-selection policy. This pref is controlled exclusively by
// policies (PromptOnMultipleMatchingCertificates or, in the sign-in profile,
// DeviceLoginScreenPromptOnMultipleMatchingCertificates).
inline constexpr char kPromptOnMultipleMatchingCertificates[] =
    "prompt_on_multiple_matching_certificates";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref indicating whether the notification informing the user that
// adb sideloading had been disabled by their admin was shown.
inline constexpr char kAdbSideloadingDisallowedNotificationShown[] =
    "adb_sideloading_disallowed_notification_shown";
// Int64 pref indicating the time in microseconds since Windows epoch
// (1601-01-01 00:00:00 UTC) when the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
// If the notification was not yet shown the pref holds the value Time::Min().
inline constexpr char kAdbSideloadingPowerwashPlannedNotificationShownTime[] =
    "adb_sideloading_powerwash_planned_notification_shown_time";
// Boolean pref indicating whether the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
inline constexpr char kAdbSideloadingPowerwashOnNextRebootNotificationShown[] =
    "adb_sideloading_powerwash_on_next_reboot_notification_shown";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Boolean pref that indicates whether caret browsing is currently enabled.
inline constexpr char kCaretBrowsingEnabled[] =
    "settings.a11y.caretbrowsing.enabled";

// Boolean pref for whether the user is shown a dialog to confirm that caret
// browsing should be enabled/disabled when the keyboard shortcut is pressed.
// If set to false, no intervening dialog is displayed and caret browsing mode
// is toggled silently by the keyboard shortcut.
inline constexpr char kShowCaretBrowsingDialog[] =
    "settings.a11y.caretbrowsing.show_dialog";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enum pref indicating how to launch the Lacros browser. It is managed by
// LacrosAvailability policy and can have one of the following values:
// 0: User choice (default value).
// 1: Lacros is disallowed.
// 4: Lacros is the only available browser.
// Values 2 and 3 were removed and should not be reused.
inline constexpr char kLacrosLaunchSwitch[] = "lacros_launch_switch";

// Enum pref indicating which Lacros browser to launch: rootfs or stateful. It
// is managed by LacrosSelection policy and can have one of the following
// values:
// 0: User choice (default value).
// 1: Always load rootfs Lacros.
// 2: Always load stateful Lacros.
inline constexpr char kLacrosSelection[] = "lacros_selection";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// String enum pref determining what should happen when a user who authenticates
// via a security token is removing this token. "IGNORE" - nothing happens
// (default). "LOGOUT" - The user is logged out. "LOCK" - The session is locked.
inline constexpr char kSecurityTokenSessionBehavior[] =
    "security_token_session_behavior";
// When the above pref is set to "LOGOUT" or "LOCK", this integer pref
// determines the duration of a notification that appears when the smart card is
// removed. The action will only happen after the notification timed out. If
// this pref is set to 0, the action happens immediately.
inline constexpr char kSecurityTokenSessionNotificationSeconds[] =
    "security_token_session_notification_seconds";
// This string pref is set when the notification after the action mentioned
// above is about to be displayed. It contains the domain that manages the user
// who was logged out, to be used as part of the notification message.
inline constexpr char kSecurityTokenSessionNotificationScheduledDomain[] =
    "security_token_session_notification_scheduled";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Boolean pref indicating whether user has hidden the cart module on NTP.
inline constexpr char kCartModuleHidden[] = "cart_module_hidden";
// An integer that keeps track of how many times welcome surface has shown in
// cart module.
inline constexpr char kCartModuleWelcomeSurfaceShownTimes[] =
    "cart_module_welcome_surface_shown_times";
// Boolean pref indicating whether user has reacted to the consent for
// rule-based discount in cart module.
inline constexpr char kCartDiscountAcknowledged[] =
    "cart_discount_acknowledged";
// Boolean pref indicating whether user has enabled rule-based discount in cart
// module.
inline constexpr char kCartDiscountEnabled[] = "cart_discount_enabled";
// Map pref recording the discounts used by users.
inline constexpr char kCartUsedDiscounts[] = "cart_used_discounts";
// A time pref indicating the timestamp of when last cart discount fetch
// happened.
inline constexpr char kCartDiscountLastFetchedTime[] =
    "cart_discount_last_fetched_time";
// Boolean pref indicating whether the consent for discount has ever shown or
// not.
inline constexpr char kCartDiscountConsentShown[] =
    "cart_discount_consent_shown";
// Integer pref indicating in which variation the user has made their decision,
// accept or reject the consent.
inline constexpr char kDiscountConsentDecisionMadeIn[] =
    "discount_consent_decision_made_in";
// Integer pref indicating in which variation the user has dismissed the
// consent. Only the Inline and Dialog variation applies.
inline constexpr char kDiscountConsentDismissedIn[] =
    "discount_consent_dismissed_in";
// A time pref indicating the timestamp of when user last explicitly dismissed
// the discount consent.
inline constexpr char kDiscountConsentLastDimissedTime[] =
    "discount_consent_last_dimissed_time";
// Integer pref indicating the last consent was shown in which variation.
inline constexpr char kDiscountConsentLastShownInVariation[] =
    "discount_consent_last_shown_in";
// An integer pref that keeps track of how many times user has explicitly
// dismissed the disount consent.
inline constexpr char kDiscountConsentPastDismissedCount[] =
    "discount_consent_dismissed_count";
// Boolean pref indicating whether the user has shown interest in the consent,
// e.g. if the use has clicked the 'continue' button.
inline constexpr char kDiscountConsentShowInterest[] =
    "discount_consent_show_interest";
// Integer pref indicating in which variation the user has shown interest to the
// consent, they has clicked the 'continue' button.
inline constexpr char kDiscountConsentShowInterestIn[] =
    "discount_consent_show_interest_in";
#endif

#if BUILDFLAG(IS_ANDROID)
// Boolean pref controlling whether immersive AR sessions are enabled
// in WebXR Device API.
inline constexpr char kWebXRImmersiveArEnabled[] = "webxr.immersive_ar_enabled";
#endif

#if !BUILDFLAG(IS_ANDROID)
// The duration for keepalive requests on browser shutdown.
inline constexpr char kFetchKeepaliveDurationOnShutdown[] =
    "fetch_keepalive_duration_on_shutdown";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref to control whether to enable annotation mode in the PDF viewer
// or not.
inline constexpr char kPdfAnnotationsEnabled[] = "pdf.enable_annotations";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref to control whether to enable Lens integration with media app
inline constexpr char kMediaAppLensEnabled[] = "media_app.enable_lens";
#endif

// A comma-separated list of ports on which outgoing connections will be
// permitted even if they would otherwise be blocked.
inline constexpr char kExplicitlyAllowedNetworkPorts[] =
    "net.explicitly_allowed_network_ports";

#if !BUILDFLAG(IS_ANDROID)
// Pref name for whether force-installed web apps (origins) are able to query
// device attributes.
inline constexpr char kDeviceAttributesAllowedForOrigins[] =
    "policy.device_attributes_allowed_for_origins";
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// A boolean indicating whether the desktop sharing hub is enabled by enterprise
// policy.
inline constexpr char kDesktopSharingHubEnabled[] =
    "sharing_hub.desktop_sharing_hub_enabled";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Pref name for the last major version where the What's New page was
// successfully shown.
inline constexpr char kLastWhatsNewVersion[] = "browser.last_whats_new_version";
// A boolean indicating whether the Lens Region search feature should be enabled
// if supported.
inline constexpr char kLensRegionSearchEnabled[] =
    "policy.lens_region_search_enabled";
// A boolean indicating whether the Lens NTP searchbox feature should be enabled
// if supported.
inline constexpr char kLensDesktopNTPSearchEnabled[] =
    "policy.lens_desktop_ntp_search_enabled";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// A dict mapping the edition name with the major version it was shown.
inline constexpr char kWhatsNewEditionUsed[] = "browser.whats_new.edition_used";
// A list containing the features of each module in order of when they
// were first enabled.
inline constexpr char kWhatsNewFirstEnabledOrder[] =
    "browser.whats_new.enabled_order";
#endif

// An integer indicating the number of times the Lens Overlay was started.
inline constexpr char kLensOverlayStartCount[] =
    "lens.lens_overlay_start_count";

// A boolean indicating whether the Privacy guide feature has been viewed. This
// is set to true if the user has done any of the following: (1) opened the
// privacy guide, (2) dismissed the privacy guide promo, (3) seen the privacy
// guide promo a certain number of times.
inline constexpr char kPrivacyGuideViewed[] = "privacy_guide.viewed";

// A boolean indicating support of "CORS non-wildcard request header name".
// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
inline constexpr char kCorsNonWildcardRequestHeadersSupport[] =
    "cors_non_wildcard_request_headers_support";

// A boolean indicating whether documents are allowed to be assigned to
// origin-keyed agent clusters by default (i.e., when the Origin-Agent-Cluster
// header is absent). When true, Chromium may enable this behavior based on
// feature settings. When false, site-keyed agent clusters will continue to be
// used by default.
inline constexpr char kOriginAgentClusterDefaultEnabled[] =
    "origin_agent_cluster_default_enabled";

// An integer count of how many SCT Auditing hashdance reports have ever been
// sent by this client, across all profiles.
inline constexpr char kSCTAuditingHashdanceReportCount[] =
    "sct_auditing.hashdance_report_count";

#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kConsumerAutoUpdateToggle[] =
    "settings.consumer_auto_update_toggle";
#endif

#if !BUILDFLAG(IS_ANDROID)
// An integer count of how many times the user has seen the memory saver mode
// page action chip in the expanded size. While the feature was renamed to
// "Memory Saver" the pref cannot be changed without migration.
inline constexpr char kMemorySaverChipExpandedCount[] =
    "high_efficiency.chip_expanded_count";

// Stores the timestamp of the last time the memory saver chip was shown
// expanded to highlight memory savings. While the feature was renamed to
// "Memory Saver" the pref cannot be changed without migration.
inline constexpr char kLastMemorySaverChipExpandedTimestamp[] =
    "high_efficiency.last_chip_expanded_timestamp";

inline constexpr char kPerformanceInterventionBackgroundCpuMessageCount[] =
    "performance_intervention.background_cpu_message_count";

inline constexpr char kPerformanceInterventionBackgroundCpuRateLimitedCount[] =
    "performance_intervention.background_cpu_rate_limited_count";

inline constexpr char kPerformanceInterventionDailySample[] =
    "performance_intervention.last_daily_sample";

// A boolean indicating whether the price track first user experience bubble
// should show. This is set to false if the user has clicked the "Price track"
// button in the FUE bubble once.
inline constexpr char kShouldShowPriceTrackFUEBubble[] =
    "should_show_price_track_fue_bubble_fue";
#endif

inline constexpr char kStrictMimetypeCheckForWorkerScriptsEnabled[] =
    "strict_mime_type_check_for_worker_scripts_enabled";

#if BUILDFLAG(IS_ANDROID)
// If true, the virtual keyboard will resize the layout viewport by default.
// Has no effect otherwise.
inline constexpr char kVirtualKeyboardResizesLayoutByDefault[] =
    "virtual_keyboard_resizes_layout_by_default";
#endif  // BUILDFLAG(IS_ANDROID)

// A boolean indicating whether Access-Control-Allow-Methods matching in CORS
// preflights is fixed according to the spec. https://crbug.com/1228178
inline constexpr char
    kAccessControlAllowMethodsInCORSPreflightSpecConformant[] =
        "access_control_allow_methods_in_cors_preflight_spec_conformant";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A dictionary that keeps client_ids assigned by Authorization Servers indexed
// by URLs of these servers. It does not contain empty strings.
inline constexpr char kPrintingOAuth2AuthorizationServers[] =
    "printing.oauth2_authorization_servers";
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
// If this exists and is true, Chrome may run system DNS resolution out of the
// network process. If false, Chrome will run system DNS resolution in the
// network process. If non-existent, Chrome will decide where to run system DNS
// resolution (in the network process, out of the network process, or partially
// inside the network process and partially out) based on system configuration
// and feature flags.
//
// Only necessary on Android and Linux, where it is difficult to sandbox the
// network process with system DNS resolution running inside it.
inline constexpr char kOutOfProcessSystemDnsResolutionEnabled[] =
    "net.out_of_process_system_dns_resolution_enabled";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

// A list of hostnames to disable HTTPS Upgrades / HTTPS-First Mode warnings on.
inline constexpr char kHttpAllowlist[] = "https_upgrades.policy.http_allowlist";

// Whether the HTTPS Upgrades feature is enabled or disabled by the
// `HttpsUpgradesEnabled` enterprise policy.
inline constexpr char kHttpsUpgradesEnabled[] =
    "https_upgrades.policy.upgrades_enabled";

// Whether the hovercard image previews is enabled
inline constexpr char kHoverCardImagesEnabled[] =
    "browser.hovercard.image_previews_enabled";

// Whether hovercard memory usage is enabled
inline constexpr char kHoverCardMemoryUsageEnabled[] =
    "browser.hovercard.memory_usage_enabled";

// Boolean that specifies whether Compression Dictionary Transport is enabled.
inline constexpr char kCompressionDictionaryTransportEnabled[] =
    "net.compression_dictionary_transport_enabled";

// Boolean that specifies whether Zstd Content-Encoding is enabled.
inline constexpr char kZstdContentEncodingEnabled[] =
    "net.zstd_content_encoding_enabled";

// Boolean that specifies whether IPv6 reachability check override is enabled.
inline constexpr char kIPv6ReachabilityOverrideEnabled[] =
    "net.ipv6_reachability_override_enabled";

#if BUILDFLAG(IS_WIN)
// Whether native hosts executables launch directly is enabled or
// disabled.
inline constexpr char kNativeHostsExecutablesLaunchDirectly[] =
    "native_hosts_executables_launch_directly";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Dictionary mapping language to Read Aloud voice. Keys are language names like
// "en" and values are voice ID strings.
inline constexpr char kReadAloudVoiceSettings[] = "readaloud.voices";

// Double indicating Read Aloud playback speed. Default is 1.0, double speed
// is 2.0, etc.
inline constexpr char kReadAloudSpeed[] = "readaloud.speed";

// Boolean that specifies whether Read Aloud highlights words on the page during
// playback and scrolls the page to match the playback position.
inline constexpr char kReadAloudHighlightingEnabled[] =
    "readaloud.highlighting_enabled";

// Boolean that specifies whether the ListenToThisPageEnabled policy is true or
// not.
inline constexpr char kListenToThisPageEnabled[] =
    "readaloud.listen_to_this_page_enabled";

// Dictionary storing details about past synthetic trials. Key is (feature name,
// synthetic trial suffix) and value is a field trial name. sessions.
inline constexpr char kReadAloudSyntheticTrials[] =
    "readaloud.synthetic_trials";
#endif  // BUILDFLAG(IS_ANDROID)

// A list of base64 encoded certificates that are to be trusted as root certs.
// Only specifiable as an enterprise policy.
inline constexpr char kCACertificates[] = "certificates.ca_certificates";

// A list of objects. Each object contains a base64 encoded certificates that
// are to be trusted as root certs, but with constraints specified outside of
// the certificate in the object.
// Only specifiable as an enterprise policy.
inline constexpr char kCACertificatesWithConstraints[] =
    "certificates.ca_certificates_with_constraints";

// A list of base64 encoded certificates containing SPKIs that are not to be
// trusted.
// Only specifiable as an enterprise policy.
inline constexpr char kCADistrustedCertificates[] =
    "certificates.ca_distrusted_certificates";

// A list of base64 certificates that are to be used as hints for path
// building. Only specifiable as an enterprise policy.
inline constexpr char kCAHintCertificates[] =
    "certificates.ca_hint_certificates";

#if !BUILDFLAG(IS_CHROMEOS)
// Boolean that specifies whether to use user-added certificates that are in the
// platform trust stores.
inline constexpr char kCAPlatformIntegrationEnabled[] =
    "certificates.ca_platform_integration_enabled";
#endif

// Integer value controlling whether to show any enterprise badging on a managed
// profile.
// - 0: Hide all badging
// - 1: Show badging for managed profiles on unmanaged devices
// - 2: Show badging for managed profiles on all devices
// - 3: Show badging for managed profiles on managed devices
inline constexpr char kEnterpriseBadgingTemporarySetting[] =
    "temporary_setting.enterpise_badging";

// Url to an image representing the enterprise logo.
inline constexpr char kEnterpriseLogoUrl[] = "enterprise_logo.url";

// Url to an image representing the enterprise logo for ta profile.
// This is used for cloud user policies only.
inline constexpr char kEnterpriseLogoUrlForProfile[] =
    "enterprise_logo.url.for_profile";

// String value of the custom label for the entity managing the profile.
inline constexpr char kEnterpriseCustomLabel[] =
    "enterprise_label.custom_value";

// String value of the enterprise label for the entity managing the profile
// only.
// This is used for cloud user policies only.
inline constexpr char kEnterpriseCustomLabelForProfile[] =
    "enterprise_label.custom_value.for_profile";

// IntegerValue of the custom label preset of a managed profile.
inline constexpr char kEnterpriseProfileBadgeToolbarSettings[] =
    "enterprise.profile_badging.toolbar_settings";

#if BUILDFLAG(IS_ANDROID)
// An integer count of how many account-level breached credentials were detected
// by GMSCore.
inline constexpr char kBreachedCredentialsCount[] =
    "profile.safety_hub_breached_credentials_count";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// The integer value of the ExtensibleEnterpriseSSOEnabled policy.
inline constexpr char kExtensibleEnterpriseSSOEnabled[] =
    "extensible_enterprise_sso.enabled";
#endif  //  BUILDFLAG(IS_MAC)
}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
