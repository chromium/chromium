// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

#include <iterator>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// A bool pref that keeps whether the child status for this profile was already
// successfully checked via ChildAccountService.
const char kChildAccountStatusKnown[] = "child_account_status_known";

// A string property indicating whether default apps should be installed
// in this profile.  Use the value "install" to enable defaults apps, or
// "noinstall" to disable them.  This property is usually set in the
// master_preferences and copied into the profile preferences on first run.
// Defaults apps are installed only when creating a new profile.
const char kPreinstalledApps[] = "default_apps";

// Disable SafeBrowsing checks for files coming from trusted URLs when false.
const char kSafeBrowsingForTrustedSourcesEnabled[] =
    "safebrowsing_for_trusted_sources_enabled";

// Disables screenshot accelerators and extension APIs.
// This setting resides both in profile prefs and local state. Accelerator
// handling code reads local state, while extension APIs use profile pref.
const char kDisableScreenshots[] = "disable_screenshots";

// Prevents certain types of downloads based on integer value, which corresponds
// to DownloadPrefs::DownloadRestriction.
// 0 - No special restrictions (default)
// 1 - Block dangerous downloads
// 2 - Block potentially dangerous downloads
// 3 - Block all downloads
// 4 - Block malicious downloads
const char kDownloadRestrictions[] = "download_restrictions";

// A boolean specifying whether the new download bubble UI is enabled. If it is
// set to false, the old download shelf UI will be shown instead.
const char kDownloadBubbleEnabled[] = "download_bubble_enabled";

// If set to true profiles are created in ephemeral mode and do not store their
// data in the profile folder on disk but only in memory.
const char kForceEphemeralProfiles[] = "profile.ephemeral_mode";

// A boolean specifying whether the New Tab page is the home page or not.
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const char kHomePage[] = "homepage";

// A boolean specifying whether HTTPS-Only Mode is enabled.
const char kHttpsOnlyModeEnabled[] = "https_only_mode_enabled";

// Stores information about the important sites dialog, including the time and
// frequency it has been ignored.
const char kImportantSitesDialogHistory[] = "important_sites_dialog";

// This is the profile creation time.
const char kProfileCreationTime[] = "profile.creation_time";

#if BUILDFLAG(IS_WIN)
// This is a timestamp of the last time this profile was reset by a third party
// tool. On Windows, a third party tool may set a registry value that will be
// compared to this value and if different will result in a profile reset
// prompt. See triggered_profile_resetter.h for more information.
const char kLastProfileResetTimestamp[] = "profile.last_reset_timestamp";

// A boolean indicating if settings should be reset for this profile once a
// run of the Chrome Cleanup Tool has completed.
const char kChromeCleanerResetPending[] = "chrome_cleaner.reset_pending";

// The last time the Chrome cleaner scan completed without finding anything,
// while Chrome was opened.
const char kChromeCleanerScanCompletionTime[] =
    "chrome_cleaner.scan_completion_time";
#endif

// The URL to open the new tab page to. Only set by Group Policy.
const char kNewTabPageLocationOverride[] = "newtab_page_location_override";

// An integer that keeps track of the profile icon version. This allows us to
// determine the state of the profile icon for icon format changes.
const char kProfileIconVersion[] = "profile.icon_version";

// A string pref whose values is one of the values defined by
// |ProfileImpl::kPrefExitTypeXXX|. Set to |kPrefExitTypeCrashed| on startup and
// one of |kPrefExitTypeNormal| or |kPrefExitTypeSessionEnded| during
// shutdown. Used to determine the exit type the last time the profile was open.
const char kSessionExitType[] = "profile.exit_type";

// An integer pref. Holds one of several values:
// 0: unused, previously indicated to open the homepage on startup
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
// 5: open the New Tab Page on startup.
const char kRestoreOnStartup[] = "session.restore_on_startup";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
const char kURLsToRestoreOnStartup[] = "session.startup_urls";

// Boolean that is true when user feedback to Google is allowed.
const char kUserFeedbackAllowed[] = "feedback_allowed";

#if !BUILDFLAG(IS_ANDROID)
// Replaced by kManagedSerialAllowAllPortsForUrls in M-93.
const char kManagedProfileSerialAllowAllPortsForUrlsDeprecated[] =
    "profile.managed.serial_allow_all_ports_for_urls";
// Replaced by kManagedSerialAllowUsbDevicesForUrls in M-93.
const char kManagedProfileSerialAllowUsbDevicesForUrlsDeprecated[] =
    "profile.managed.serial_allow_usb_devices_for_urls";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
// DictionaryValue that maps extension ids to the approved version of this
// extension for a supervised user. Missing extensions are not approved.
const char kSupervisedUserApprovedExtensions[] =
    "profile.managed.approved_extensions";
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
// Integer pref to record the day id (number of days since origin of time) when
// supervised user metrics were last recorded.
const char kSupervisedUserMetricsDayId[] = "supervised_user.metrics.day_id";
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(ENABLE_RLZ)
// Integer. RLZ ping delay in seconds.
const char kRlzPingDelaySeconds[] = "rlz_ping_delay";
#endif  // BUILDFLAG(ENABLE_RLZ)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Locale preference of device' owner.  ChromeOS device appears in this locale
// after startup/wakeup/signout.
const char kOwnerLocale[] = "intl.owner_locale";
// Locale accepted by user.  Non-syncable.
// Used to determine whether we need to show Locale Change notification.
const char kApplicationLocaleAccepted[] = "intl.app_locale_accepted";
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
const char kApplicationLocaleBackup[] = "intl.app_locale_backup";

// List of locales the UI is allowed to be displayed in by policy. The list is
// empty if no restriction is being enforced.
const char kAllowedLanguages[] = "intl.allowed_languages";
#endif

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
const char kDefaultCharset[] = "intl.charset_default";

// If these change, the corresponding enums in the extension API
// experimental.fontSettings.json must also change.
const char* const kWebKitScriptsForFontFamilyMaps[] = {
#define EXPAND_SCRIPT_FONT(x, script_name) script_name,
#include "chrome/common/pref_font_script_names-inl.h"
    ALL_FONT_SCRIPTS("unused param")
#undef EXPAND_SCRIPT_FONT
};

const size_t kWebKitScriptsForFontFamilyMapsLength =
    std::size(kWebKitScriptsForFontFamilyMaps);

// Strings for WebKit font family preferences. If these change, the pref prefix
// in pref_names_util.cc and the pref format in font_settings_api.cc must also
// change.
const char kWebKitStandardFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_STANDARD;
const char kWebKitFixedFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_FIXED;
const char kWebKitSerifFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_SERIF;
const char kWebKitSansSerifFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_SANSERIF;
const char kWebKitCursiveFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_CURSIVE;
const char kWebKitFantasyFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_FANTASY;
const char kWebKitMathFontFamilyMap[] = WEBKIT_WEBPREFS_FONTS_MATH;
const char kWebKitStandardFontFamilyArabic[] =
    "webkit.webprefs.fonts.standard.Arab";
#if BUILDFLAG(IS_WIN)
const char kWebKitFixedFontFamilyArabic[] = "webkit.webprefs.fonts.fixed.Arab";
#endif
const char kWebKitSerifFontFamilyArabic[] = "webkit.webprefs.fonts.serif.Arab";
const char kWebKitSansSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.sansserif.Arab";
#if BUILDFLAG(IS_WIN)
const char kWebKitStandardFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.standard.Cyrl";
const char kWebKitFixedFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.fixed.Cyrl";
const char kWebKitSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.serif.Cyrl";
const char kWebKitSansSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.sansserif.Cyrl";
const char kWebKitStandardFontFamilyGreek[] =
    "webkit.webprefs.fonts.standard.Grek";
const char kWebKitFixedFontFamilyGreek[] = "webkit.webprefs.fonts.fixed.Grek";
const char kWebKitSerifFontFamilyGreek[] = "webkit.webprefs.fonts.serif.Grek";
const char kWebKitSansSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.sansserif.Grek";
#endif
const char kWebKitStandardFontFamilyJapanese[] =
    "webkit.webprefs.fonts.standard.Jpan";
const char kWebKitFixedFontFamilyJapanese[] =
    "webkit.webprefs.fonts.fixed.Jpan";
const char kWebKitSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.serif.Jpan";
const char kWebKitSansSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.sansserif.Jpan";
const char kWebKitStandardFontFamilyKorean[] =
    "webkit.webprefs.fonts.standard.Hang";
const char kWebKitFixedFontFamilyKorean[] = "webkit.webprefs.fonts.fixed.Hang";
const char kWebKitSerifFontFamilyKorean[] = "webkit.webprefs.fonts.serif.Hang";
const char kWebKitSansSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.sansserif.Hang";
#if BUILDFLAG(IS_WIN)
const char kWebKitCursiveFontFamilyKorean[] =
    "webkit.webprefs.fonts.cursive.Hang";
#endif
const char kWebKitStandardFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.standard.Hans";
const char kWebKitFixedFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.fixed.Hans";
const char kWebKitSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.serif.Hans";
const char kWebKitSansSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.sansserif.Hans";
const char kWebKitStandardFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.standard.Hant";
const char kWebKitFixedFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.fixed.Hant";
const char kWebKitSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.serif.Hant";
const char kWebKitSansSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.sansserif.Hant";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
const char kWebKitCursiveFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.cursive.Hans";
const char kWebKitCursiveFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.cursive.Hant";
#endif

// WebKit preferences.
const char kWebKitWebSecurityEnabled[] = "webkit.webprefs.web_security_enabled";
const char kWebKitDomPasteEnabled[] = "webkit.webprefs.dom_paste_enabled";
const char kWebKitTextAreasAreResizable[] =
    "webkit.webprefs.text_areas_are_resizable";
const char kWebKitJavascriptCanAccessClipboard[] =
    "webkit.webprefs.javascript_can_access_clipboard";
const char kWebkitTabsToLinks[] = "webkit.webprefs.tabs_to_links";
const char kWebKitAllowRunningInsecureContent[] =
    "webkit.webprefs.allow_running_insecure_content";
#if BUILDFLAG(IS_ANDROID)
const char kWebKitPasswordEchoEnabled[] =
    "webkit.webprefs.password_echo_enabled";
#endif
const char kWebKitForceDarkModeEnabled[] =
    "webkit.webprefs.force_dark_mode_enabled";

const char kWebKitCommonScript[] = "Zyyy";
const char kWebKitStandardFontFamily[] = "webkit.webprefs.fonts.standard.Zyyy";
const char kWebKitFixedFontFamily[] = "webkit.webprefs.fonts.fixed.Zyyy";
const char kWebKitSerifFontFamily[] = "webkit.webprefs.fonts.serif.Zyyy";
const char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.fonts.sansserif.Zyyy";
const char kWebKitCursiveFontFamily[] = "webkit.webprefs.fonts.cursive.Zyyy";
const char kWebKitFantasyFontFamily[] = "webkit.webprefs.fonts.fantasy.Zyyy";
const char kWebKitMathFontFamily[] = "webkit.webprefs.fonts.math.Zyyy";
const char kWebKitDefaultFontSize[] = "webkit.webprefs.default_font_size";
const char kWebKitDefaultFixedFontSize[] =
    "webkit.webprefs.default_fixed_font_size";
const char kWebKitMinimumFontSize[] = "webkit.webprefs.minimum_font_size";
const char kWebKitMinimumLogicalFontSize[] =
    "webkit.webprefs.minimum_logical_font_size";
const char kWebKitJavascriptEnabled[] = "webkit.webprefs.javascript_enabled";
const char kWebKitLoadsImagesAutomatically[] =
    "webkit.webprefs.loads_images_automatically";
const char kWebKitPluginsEnabled[] = "webkit.webprefs.plugins_enabled";

// Boolean that is true when the SSL interstitial should allow users to
// proceed anyway. Otherwise, proceeding is not possible.
const char kSSLErrorOverrideAllowed[] = "ssl.error_override_allowed";

// List of origins for which the SSL interstitial should allow users to proceed
// anyway. Ignored if kSSLErrorOverrideAllowed is false.
const char kSSLErrorOverrideAllowedForOrigins[] =
    "ssl.error_override_allowed_for_origins";

// Enum that specifies whether Incognito mode is:
// 0 - Enabled. Default behaviour. Default mode is available on demand.
// 1 - Disabled. User cannot browse pages in Incognito mode.
// 2 - Forced. All pages/sessions are forced into Incognito.
const char kIncognitoModeAvailability[] = "incognito.mode_availability";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

#if BUILDFLAG(IS_ANDROID)
// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";
const char kContextualSearchDisabledValue[] = "false";
const char kContextualSearchEnabledValue[] = "true";

// A integer preference to store the number of times the Contextual Search promo
// card shown.
const char kContextualSearchPromoCardShownCount[] =
    "search.contextual_search_promo_card_shown_count";

// Boolean that indicates whether the user chose to fully opt in for Contextual
// Search.
const char kContextualSearchWasFullyPrivacyEnabled[] =
    "search.contextual_search_fully_opted_in";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Boolean that indicates whether the browser should put up a confirmation
// window when the user is attempting to quit. Only on Mac.
const char kConfirmToQuitEnabled[] = "browser.confirm_to_quit";

// Boolean that indicates whether the browser should show the toolbar when it's
// in fullscreen. Mac only.
const char kShowFullscreenToolbar[] = "browser.show_fullscreen_toolbar";

// Boolean that indicates whether the browser should allow Javascript injection
// via Apple Events. Mac only.
const char kAllowJavascriptAppleEvents[] =
    "browser.allow_javascript_apple_events";

#endif

// Boolean which specifies whether we should ask the user if we should download
// a file (true) or just download it automatically.
const char kPromptForDownload[] = "download.prompt_for_download";

// Controls if the QUIC protocol is allowed.
const char kQuicAllowed[] = "net.quic_allowed";

// Prefs for persisting network qualities.
const char kNetworkQualities[] = "net.network_qualities";

// Pref storing the user's network easter egg game high score.
const char kNetworkEasterEggHighScore[] = "net.easter_egg_high_score";

// A preference of enum chrome_browser_net::NetworkPredictionOptions shows
// if prediction of network actions is allowed, depending on network type.
// Actions include DNS prefetching, TCP and SSL preconnection, prerendering
// of web pages, and resource prefetching.
// TODO(bnc): Implement this preference as per crbug.com/334602.
const char kNetworkPredictionOptions[] = "net.network_prediction_options";

// An integer representing the state of the default apps installation process.
// This value is persisted in the profile's user preferences because the process
// is async, and the user may have stopped chrome in the middle.  The next time
// the profile is opened, the process will continue from where it left off.
//
// See possible values in external_provider_impl.cc.
const char kPreinstalledAppsInstallState[] = "default_apps_install_state";

// A boolean pref set to true if the Chrome Web Store icons should be hidden
// from the New Tab Page and app launcher.
const char kHideWebStoreIcon[] = "hide_web_store_icon";

#if BUILDFLAG(IS_CHROMEOS)
// The list of extensions allowed to use the platformKeys API for remote
// attestation.
const char kAttestationExtensionAllowlist[] = "attestation.extension_allowlist";

// A boolean specifying whether the Desk API is enabled for third party web
// applications. If set to true, the Desk API bridge component extension will be
// installed.
const char kDeskAPIThirdPartyAccessEnabled[] =
    "desk_api.third_party_access_enabled";

// A list of third party web application domains allowed to use the Desk API.
const char kDeskAPIThirdPartyAllowlist[] = "desk_api.third_party_allowlist";

// The list of extensions allowed to skip print job confirmation dialog when
// they use the chrome.printing.submitJob() function. Note that this used to be
// `kPrintingAPIExtensionsWhitelist`, hence the difference between the variable
// name and the string value.
const char kPrintingAPIExtensionsAllowlist[] =
    "printing.printing_api_extensions_whitelist";

// A boolean specifying whether the insights extension is enabled. If set to
// true, the CCaaS Chrome component extension will be installed.
const char kInsightsExtensionEnabled[] = "insights_extension_enabled";

// Boolean controlling whether showing Sync Consent during sign-in is enabled.
// Controlled by policy.
const char kEnableSyncConsent[] = "sync_consent.enabled";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A boolean pref set to true if touchpad tap-to-click is enabled.
const char kTapToClickEnabled[] = "settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if touchpad three-finger-click is enabled.
const char kEnableTouchpadThreeFingerClick[] =
    "settings.touchpad.enable_three_finger_click";

// A boolean pref set to true if primary mouse button is the left button.
const char kPrimaryMouseButtonRight[] = "settings.mouse.primary_right";

// A boolean pref set to true if primary pointing stick button is the left
// button.
const char kPrimaryPointingStickButtonRight[] =
    "settings.pointing_stick.primary_right";

// Copy of the primary pointing stick buttons option to use on login screen.
const char kOwnerPrimaryPointingStickButtonRight[] =
    "owner.pointing_stick.primary_right";

// A boolean pref set to true if mouse acceleration is enabled. When disabled
// only simple linear scaling is applied based on sensitivity.
const char kMouseAcceleration[] = "settings.mouse.acceleration";

// A boolean pref set to true if mouse scroll acceleration is enabled. When
// disabled, only simple linear scaling is applied based on sensitivity.
const char kMouseScrollAcceleration[] = "settings.mouse.scroll_acceleration";

// A boolean pref set to true if pointing stick acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
const char kPointingStickAcceleration[] =
    "settings.pointing_stick.acceleration";

// A boolean pref set to true if touchpad acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
const char kTouchpadAcceleration[] = "settings.touchpad.acceleration";

// A boolean pref set to true if touchpad scroll acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
const char kTouchpadScrollAcceleration[] =
    "settings.touchpad.scroll_acceleration";

// A boolean pref set to true if touchpad haptic feedback is enabled.
const char kTouchpadHapticFeedback[] = "settings.touchpad.haptic_feedback";

// A integer pref for the touchpad haptic click sensitivity ranging from Soft
// feedback to Firm feedback [1, 3, 5].
const char kTouchpadHapticClickSensitivity[] =
    "settings.touchpad.haptic_click_sensitivity";

// A integer pref for the touchpad sensitivity.
const char kMouseSensitivity[] = "settings.mouse.sensitivity2";

// A integer pref for the touchpad scroll sensitivity, in the range
// [PointerSensitivity::kLowest, PointerSensitivity::kHighest].
const char kMouseScrollSensitivity[] = "settings.mouse.scroll_sensitivity";

// A integer pref for the touchpad sensitivity.
const char kTouchpadSensitivity[] = "settings.touchpad.sensitivity2";

// A integer pref for the touchpad scroll sensitivity, in the range
// [PointerSensitivity::kLowest, PointerSensitivity::kHighest].
const char kTouchpadScrollSensitivity[] =
    "settings.touchpad.scroll_sensitivity";

// A integer pref for pointing stick sensitivity.
const char kPointingStickSensitivity[] = "settings.pointing_stick.sensitivity";

// A boolean pref set to true if time should be displayed in 24-hour clock.
const char kUse24HourClock[] = "settings.clock.use_24hour_clock";

// A string pref containing Timezone ID for this user.
const char kUserTimezone[] = "settings.timezone";

// This setting controls what information is sent to the server to get
// device location to resolve time zone in user session. Values must
// match TimeZoneResolverManager::TimeZoneResolveMethod enum.
const char kResolveTimezoneByGeolocationMethod[] =
    "settings.resolve_timezone_by_geolocation_method";

// This setting is true when kResolveTimezoneByGeolocation value
// has been migrated to kResolveTimezoneByGeolocationMethod.
const char kResolveTimezoneByGeolocationMigratedToMethod[] =
    "settings.resolve_timezone_by_geolocation_migrated_to_method";

// A string pref set to the current input method.
const char kLanguageCurrentInputMethod[] =
    "settings.language.current_input_method";

// A string pref set to the previous input method.
const char kLanguagePreviousInputMethod[] =
    "settings.language.previous_input_method";

// A list pref set to the allowed input methods (see policy
// "AllowedInputMethods").
const char kLanguageAllowedInputMethods[] =
    "settings.language.allowed_input_methods";

// A string pref (comma-separated list) set to the preloaded (active) input
// method IDs (ex. "pinyin,mozc").
const char kLanguagePreloadEngines[] = "settings.language.preload_engines";
const char kLanguagePreloadEnginesSyncable[] =
    "settings.language.preload_engines_syncable";

// A string pref (comma-separated list) set to the extension and ARC IMEs to be
// enabled.
const char kLanguageEnabledImes[] = "settings.language.enabled_extension_imes";
const char kLanguageEnabledImesSyncable[] =
    "settings.language.enabled_extension_imes_syncable";

// A boolean pref set to true if the IME menu is activated.
const char kLanguageImeMenuActivated[] = "settings.language.ime_menu_activated";

// A dictionary of input method IDs and their settings. Each value is itself a
// dictionary of key / value string pairs, with each pair representing a setting
// and its value.
const char kLanguageInputMethodSpecificSettings[] =
    "settings.language.input_method_specific_settings";

// A boolean pref to indicate whether we still need to add the globally synced
// input methods. False after the initial post-OOBE sync.
const char kLanguageShouldMergeInputMethods[] =
    "settings.language.merge_input_methods";

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
const char kLabsAdvancedFilesystemEnabled[] =
    "settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
const char kLabsMediaplayerEnabled[] = "settings.labs.mediaplayer";

// A boolean pref of whether to show mobile data first-use warning notification.
// Note: 3g in the name is for legacy reasons. The pref was added while only 3G
// mobile data was supported.
const char kShowMobileDataNotification[] =
    "settings.internet.mobile.show_3g_promo_notification";

// A string pref that contains version where "What's new" promo was shown.
const char kChromeOSReleaseNotesVersion[] = "settings.release_notes.version";

// A string pref that contains either a Chrome app ID (see
// extensions::ExtensionId) or an Android package name (using Java package
// naming conventions) of the preferred note-taking app. An empty value
// indicates that the user hasn't selected an app yet.
const char kNoteTakingAppId[] = "settings.note_taking_app_id";

// A boolean pref indicating whether preferred note-taking app (see
// |kNoteTakingAppId|) is allowed to handle note taking actions on the lock
// screen.
const char kNoteTakingAppEnabledOnLockScreen[] =
    "settings.note_taking_app_enabled_on_lock_screen";

// List of note taking aps that can be enabled to run on the lock screen.
// The intended usage is to allow the set of apps that the user can enable
// to run on lock screen, not to actually enable the apps to run on lock screen.
// Note that this used to be `kNoteTakingAppsLockScreenWhitelist`, hence the
// difference between the variable name and the string value.
const char kNoteTakingAppsLockScreenAllowlist[] =
    "settings.note_taking_apps_lock_screen_whitelist";

// Dictionary pref that maps lock screen app ID to a boolean indicating whether
// the toast dialog has been show and dismissed as the app was being launched
// on the lock screen.
const char kNoteTakingAppsLockScreenToastShown[] =
    "settings.note_taking_apps_lock_screen_toast_shown";

// Whether the preferred note taking app should be requested to restore the last
// note created on lock screen when launched on lock screen.
const char kRestoreLastLockScreenNote[] =
    "settings.restore_last_lock_screen_note";

// A boolean pref indicating whether user activity has been observed in the
// current session already. The pref is used to restore information about user
// activity after browser crashes.
const char kSessionUserActivitySeen[] = "session.user_activity_seen";

// A preference to keep track of the session start time. If the session length
// limit is configured to start running after initial user activity has been
// observed, the pref is set after the first user activity in a session.
// Otherwise, it is set immediately after session start. The pref is used to
// restore the session start time after browser crashes. The time is expressed
// as the serialization obtained from base::Time::ToInternalValue().
const char kSessionStartTime[] = "session.start_time";

// Holds the maximum session time in milliseconds. If this pref is set, the
// user is logged out when the maximum session time is reached. The user is
// informed about the remaining time by a countdown timer shown in the ash
// system tray.
const char kSessionLengthLimit[] = "session.length_limit";

// Whether the session length limit should start running only after the first
// user activity has been observed in a session.
const char kSessionWaitForInitialUserActivity[] =
    "session.wait_for_initial_user_activity";

// A preference of the last user session type. It is used with the
// kLastSessionLength pref below to store the last user session info
// on shutdown so that it could be reported on the next run.
const char kLastSessionType[] = "session.last_session_type";

// A preference of the last user session length.
const char kLastSessionLength[] = "session.last_session_length";

// The URL from which the Terms of Service can be downloaded. The value is only
// honored for public accounts.
const char kTermsOfServiceURL[] = "terms_of_service.url";

// Indicates whether the remote attestation is enabled for the user.
const char kAttestationEnabled[] = "attestation.enabled";

// A boolean pref recording whether user has dismissed the multiprofile
// introduction dialog show.
const char kMultiProfileNeverShowIntro[] =
    "settings.multi_profile_never_show_intro";

// A boolean pref recording whether user has dismissed the multiprofile
// teleport warning dialog show.
const char kMultiProfileWarningShowDismissed[] =
    "settings.multi_profile_warning_show_dismissed";

// A string pref that holds string enum values of how the user should behave
// in a multiprofile session. See ChromeOsMultiProfileUserBehavior policy
// for more details of the valid values.
const char kMultiProfileUserBehavior[] = "settings.multiprofile_user_behavior";

// A boolean preference indicating whether user has seen first-run tutorial
// already.
const char kFirstRunTutorialShown[] = "settings.first_run_tutorial_shown";

// List of mounted file systems via the File System Provider API. Used to
// restore them after a reboot.
const char kFileSystemProviderMounted[] = "file_system_provider.mounted";

// A boolean pref set to true if the virtual keyboard should be enabled.
const char kTouchVirtualKeyboardEnabled[] = "ui.touch_virtual_keyboard_enabled";

// This is the policy CaptivePortalAuthenticationIgnoresProxy that allows to
// open captive portal authentication pages in a separate window under
// a temporary incognito profile ("signin profile" is used for this purpose),
// which allows to bypass the user's proxy for captive portal authentication.
const char kCaptivePortalAuthenticationIgnoresProxy[] =
    "proxy.captive_portal_ignores_proxy";

// A dictionary pref mapping public keys that identify platform keys to its
// properties like whether it's meant for corporate usage.
const char kPlatformKeys[] = "platform_keys";

// A boolean preference that will be registered in local_state prefs to track
// migration of permissions on device-wide key pairs and will be registered in
// Profile prefs to track migration of permissions on user-owned key pairs.
const char kKeyPermissionsOneTimeMigrationDone[] =
    "key_permissions_one_time_migration_done";

// A boolean pref. If set to true, the Unified Desktop feature is made
// available and turned on by default, which allows applications to span
// multiple screens. Users may turn the feature off and on in the settings
// while this is set to true.
const char kUnifiedDesktopEnabledByDefault[] =
    "settings.display.unified_desktop_enabled_by_default";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Bluetooth revamp experience survey.
const char kHatsBluetoothRevampCycleEndTs[] =
    "hats_bluetooth_revamp_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Bluetooth
// revamp experience survey.
const char kHatsBluetoothRevampIsSelected[] =
    "hats_bluetooth_revamp_is_selected";

// An int64 pref. This is a timestamp, microseconds after epoch, of the most
// recent time the profile took or dismissed HaTS (happiness-tracking) survey.
const char kHatsLastInteractionTimestamp[] = "hats_last_interaction_timestamp";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent survey cycle (general survey).
const char kHatsSurveyCycleEndTimestamp[] = "hats_survey_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for HaTS in the current
// survey cycle (general survey).
const char kHatsDeviceIsSelected[] = "hats_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the ENT survey
const char kHatsEntSurveyCycleEndTs[] = "hats_ent_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS ENT
// survey
const char kHatsEntDeviceIsSelected[] = "hats_ent_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Stability survey
const char kHatsStabilitySurveyCycleEndTs[] =
    "hats_stability_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Stability
// survey
const char kHatsStabilityDeviceIsSelected[] =
    "hats_stability_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the HaTS Performance survey
const char kHatsPerformanceSurveyCycleEndTs[] =
    "hats_performance_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Performance
// survey
const char kHatsPerformanceDeviceIsSelected[] =
    "hats_performance_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the Onboarding Experience survey
const char kHatsOnboardingSurveyCycleEndTs[] =
    "hats_onboarding_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Onboarding
// Experience survey
const char kHatsOnboardingDeviceIsSelected[] =
    "hats_onboarding_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Unlock Experience survey cycle.
const char kHatsUnlockSurveyCycleEndTs[] = "hats_unlock_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Unlock
// Experience survey
const char kHatsUnlockDeviceIsSelected[] = "hats_unlock_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Smart Lock Experience survey cycle.
const char kHatsSmartLockSurveyCycleEndTs[] =
    "hats_smartlock_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the HaTS Smart Lock
// Experience survey
const char kHatsSmartLockDeviceIsSelected[] =
    "hats_smartlock_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent ARC Games survey cycle.
const char kHatsArcGamesSurveyCycleEndTs[] =
    "hats_arc_games_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the ARC Games survey
const char kHatsArcGamesDeviceIsSelected[] =
    "hats_arc_games_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Audio survey cycle.
const char kHatsAudioSurveyCycleEndTs[] = "hats_audio_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Audio survey
const char kHatsAudioDeviceIsSelected[] = "hats_audio_device_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Avatar survey cycle.
const char kHatsPersonalizationAvatarSurveyCycleEndTs[] =
    "hats_personalization_avatar_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Avatar survey.
const char kHatsPersonalizationAvatarSurveyIsSelected[] =
    "hats_personalization_avatar_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Screensaver survey
// cycle.
const char kHatsPersonalizationScreensaverSurveyCycleEndTs[] =
    "hats_personalization_screensaver_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Screensaver survey.
const char kHatsPersonalizationScreensaverSurveyIsSelected[] =
    "hats_personalization_screensaver_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Personalization Wallpaper survey cycle.
const char kHatsPersonalizationWallpaperSurveyCycleEndTs[] =
    "hats_personalization_wallpaper_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Personalization
// Wallpaper survey.
const char kHatsPersonalizationWallpaperSurveyIsSelected[] =
    "hats_personalization_wallpaper_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Media App PDF survey cycle.
const char kHatsMediaAppPdfCycleEndTs[] =
    "hats_media_app_pdf_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Media App PDF
// survey.
const char kHatsMediaAppPdfIsSelected[] = "hats_media_app_pdf_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicates the end of the most recent Camera App survey cycle.
const char kHatsCameraAppSurveyCycleEndTs[] =
    "hats_camera_app_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Camera App
// survey.
const char kHatsCameraAppDeviceIsSelected[] =
    "hats_camera_app_device_is_selected";

// indicates the end of the most recent Photos Experience survey cycle.
const char kHatsPhotosExperienceCycleEndTs[] =
    "hats_photos_experience_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for the Photos Experience
// survey.
const char kHatsPhotosExperienceIsSelected[] =
    "hats_photos_experience_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent general camera survey cycle.
const char kHatsGeneralCameraSurveyCycleEndTs[] =
    "hats_general_camera_cycle_end_timestamp";

// A boolean pref. Indicated if the device is selected for the general camera
// survey.
const char kHatsGeneralCameraIsSelected[] = "hats_general_camera_is_selected";

// A boolean pref. Indicated if the device is selected for the Privacy Hub
// baseline survey.
const char kHatsPrivacyHubBaselineIsSelected[] =
    "hats_privacy_hub_baseline_is_selected";

// An int64 pref. This is the timestamp, microseconds after epoch, that
// indicated the end of the most recent Privacy Hub baseline cycle.
const char kHatsPrivacyHubBaselineCycleEndTs[] =
    "hats_privacy_hub_baseline_end_timestamp";

// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the quick unlock feature.
const char kPinUnlockFeatureNotificationShown[] =
    "pin_unlock_feature_notification_shown";
// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the fingerprint unlock feature.
const char kFingerprintUnlockFeatureNotificationShown[] =
    "fingerprint_unlock_feature_notification_shown";

// Deprecated (crbug/998983) in favor of kEndOfLifeDate.
// An integer pref. Holds one of several values:
// 0: Supported. Device is in supported state.
// 1: Security Only. Device is in Security-Only update (after initial 5 years).
// 2: EOL. Device is End of Life(No more updates expected).
// This value needs to be consistent with EndOfLifeStatus enum.
const char kEolStatus[] = "eol_status";

// A Time pref.  Holds the last used Eol Date and is compared to the latest Eol
// Date received to make changes to Eol notifications accordingly.
const char kEndOfLifeDate[] = "eol_date";

// Boolean pref indicating that the first warning End Of Life month and year
// notification was dismissed by the user.
const char kFirstEolWarningDismissed[] = "first_eol_warning_dismissed";

// Boolean pref indicating that the second warning End Of Life month and year
// notification was dismissed by the user.
const char kSecondEolWarningDismissed[] = "second_eol_warning_dismissed";

// Boolean pref indicating that the End Of Life final update notification was
// dismissed by the user.
const char kEolNotificationDismissed[] = "eol_notification_dismissed";

// A boolean pref that controls whether the PIN autosubmit feature is enabled.
// This feature, when enabled, exposes the user's PIN length by showing how many
// digits are necessary to unlock the device. Can be recommended.
const char kPinUnlockAutosubmitEnabled[] = "pin_unlock_autosubmit_enabled";

// Boolean pref indicating whether someone can cast to the device.
const char kCastReceiverEnabled[] = "cast_receiver.enabled";

// String pref indicating what is the minimum version of Chrome required to
// allow user sign in. If the string is empty or blank no restrictions will
// be applied. See base::Version for exact string format.
const char kMinimumAllowedChromeVersion[] = "minimum_req.version";

// Boolean preference that triggers chrome://settings/androidApps/details to be
// opened on user session start.
const char kShowArcSettingsOnSessionStart[] =
    "start_arc_settings_on_session_start";

// Boolean preference that triggers chrome://settings/syncSetup to be opened
// on user session start.
const char kShowSyncSettingsOnSessionStart[] =
    "start_sync_settings_on_session_start";

// Dictionary preference that maps language to default voice name preferences
// for the users's text-to-speech settings. For example, this might map
// 'en-US' to 'Chrome OS US English'.
const char kTextToSpeechLangToVoiceName[] = "settings.tts.lang_to_voice_name";

// Double preference that controls the default text-to-speech voice rate,
// where 1.0 is an unchanged rate, and for example, 0.5 is half as fast,
// and 2.0 is twice as fast.
const char kTextToSpeechRate[] = "settings.tts.speech_rate";

// Double preference that controls the default text-to-speech voice pitch,
// where 1.0 is unchanged, and for example 0.5 is lower, and 2.0 is
// higher-pitched.
const char kTextToSpeechPitch[] = "settings.tts.speech_pitch";

// Double preference that controls the default text-to-speech voice volume
// relative to the system volume, where lower than 1.0 is quieter than the
// system volume, and higher than 1.0 is louder.
const char kTextToSpeechVolume[] = "settings.tts.speech_volume";

// A dictionary containing the latest Time Limits override authorized by parent
// access code.
const char kTimeLimitLocalOverride[] = "screen_time.local_override";

// A dictionary preference holding the usage time limit definitions for a user.
const char kUsageTimeLimit[] = "screen_time.limit";

// Last state of the screen time limit.
const char kScreenTimeLastState[] = "screen_time.last_state";

// Boolean pref indicating whether a user is allowed to use the Network File
// Shares for Chrome OS feature.
const char kNetworkFileSharesAllowed[] = "network_file_shares.allowed";

// Boolean pref indicating whether the message displayed on the login screen for
// the managed guest session should be the full warning or not.
// True means the full warning should be displayed.
// False means the normal warning should be displayed.
// It's true by default, unless it's ensured that all extensions are "safe".
const char kManagedSessionUseFullLoginWarning[] =
    "managed_session.use_full_warning";

// Boolean pref indicating whether the user has previously dismissed the
// one-time notification indicating the need for a cleanup powerwash after TPM
// firmware update that didn't flush the TPM SRK.
const char kTPMFirmwareUpdateCleanupDismissed[] =
    "tpm_firmware_update.cleanup_dismissed";

// Int64 pref indicating the time in microseconds since Windows epoch
// (1601-01-01 00:00:00 UTC) when the notification informing the user about a
// planned TPM update that will clear all user data was shown. If the
// notification was not yet shown the pref holds the value Time::Min().
const char kTPMUpdatePlannedNotificationShownTime[] =
    "tpm_auto_update.planned_notification_shown_time";

// Boolean pref indicating whether the notification informing the user that an
// auto-update that will clear all the user data at next reboot was shown.
const char kTPMUpdateOnNextRebootNotificationShown[] =
    "tpm_auto_update.update_on_reboot_notification_shown";

// Boolean pref indicating whether the NetBios Name Query Request Protocol is
// used for discovering shares on the user's network by the Network File
// Shares for Chrome OS feature.
const char kNetBiosShareDiscoveryEnabled[] =
    "network_file_shares.netbios_discovery.enabled";

// Amount of screen time that a child user has used in the current day.
const char kChildScreenTimeMilliseconds[] = "child_screen_time";

// Last time the kChildScreenTimeMilliseconds was saved.
const char kLastChildScreenTimeSaved[] = "last_child_screen_time_saved";

// Last time that the kChildScreenTime pref was reset.
const char kLastChildScreenTimeReset[] = "last_child_screen_time_reset";

// Last milestone on which a Help App notification was shown.
const char kHelpAppNotificationLastShownMilestone[] =
    "help_app_notification_last_shown_milestone";

// Amount of times the release notes suggestion chip should be
// shown before it disappears.
const char kReleaseNotesSuggestionChipTimesLeftToShow[] =
    "times_left_to_show_release_notes_suggestion_chip";

// Amount of times the discover tab suggestion chip should be shown before it
// disappears.
const char kDiscoverTabSuggestionChipTimesLeftToShow[] =
    "times_left_to_show_discover_tab_suggestion_chip";

// Boolean pref indicating whether the NTLM authentication protocol should be
// enabled when mounting an SMB share with a user credential by the Network File
// Shares for Chrome OS feature.
const char kNTLMShareAuthenticationEnabled[] =
    "network_file_shares.ntlm_share_authentication.enabled";

// Dictionary pref containing configuration used to verify Parent Access Code.
// Controlled by ParentAccessCodeConfig policy.
const char kParentAccessCodeConfig[] = "child_user.parent_access_code.config";

// List pref containing app activity and state for each application.
const char kPerAppTimeLimitsAppActivities[] =
    "child_user.per_app_time_limits.app_activities";

// Int64 to specify the last timestamp the AppActivityRegistry was reset.
const char kPerAppTimeLimitsLastResetTime[] =
    "child_user.per_app_time_limits.last_reset_time";

// Int64 to specify the last timestamp the app activity has been successfully
// reported.
const char kPerAppTimeLimitsLastSuccessfulReportTime[] =
    "child_user.per_app_time_limits.last_successful_report_time";

// Int64 to specify the latest AppLimit update timestamp from.
const char kPerAppTimeLimitsLatestLimitUpdateTime[] =
    "child_user.per_app_time_limits.latest_limit_update_time";

// Dictionary pref containing the per-app time limits configuration for
// child user. Controlled by PerAppTimeLimits policy.
const char kPerAppTimeLimitsPolicy[] = "child_user.per_app_time_limits.policy";

// Dictionary pref containing the allowed urls, schemes and applications
// that would not be blocked by per app time limits.
// Note that this used to be `kPerAppTimeLimitsWhitelistPolicy`, hence the
// difference between the variable name and the string value.
const char kPerAppTimeLimitsAllowlistPolicy[] =
    "child_user.per_app_time_limits.whitelist";

// Integer pref to record the day id (number of days since origin of time) when
// family user metrics were last recorded.
const char kFamilyUserMetricsDayId[] = "family_user.metrics.day_id";

// TimeDelta pref to record the accumulated user session duration for family
// user metrics.
const char kFamilyUserMetricsSessionEngagementDuration[] =
    "family_user.metrics.session_engagement_duration";

// TimeDelta pref to record the accumulated Chrome browser app usage for family
// user metrics.
const char kFamilyUserMetricsChromeBrowserEngagementDuration[] =
    "family_user.metrics.chrome_browser_engagement_duration";

// List of preconfigured network file shares.
const char kNetworkFileSharesPreconfiguredShares[] =
    "network_file_shares.preconfigured_shares";

// URL path string of the most recently used SMB NetworkFileShare path.
const char kMostRecentlyUsedNetworkFileShareURL[] =
    "network_file_shares.most_recently_used_url";

// List of network files shares added by the user.
const char kNetworkFileSharesSavedShares[] = "network_file_shares.saved_shares";

// A string pref storing the path of device wallpaper image file.
const char kDeviceWallpaperImageFilePath[] =
    "policy.device_wallpaper_image_file_path";

// Boolean whether Kerberos daemon supports remembering passwords.
// Tied to KerberosRememberPasswordEnabled policy.
const char kKerberosRememberPasswordEnabled[] =
    "kerberos.remember_password_enabled";
// Boolean whether users may add new Kerberos accounts.
// Tied to KerberosAddAccountsAllowed policy.
const char kKerberosAddAccountsAllowed[] = "kerberos.add_accounts_allowed";
// Dictionary specifying a pre-set list of Kerberos accounts.
// Tied to KerberosAccounts policy.
const char kKerberosAccounts[] = "kerberos.accounts";
// Used by KerberosCredentialsManager to remember which account is currently
// active (empty if none) and to determine whether to wake up the Kerberos
// daemon on session startup.
const char kKerberosActivePrincipalName[] = "kerberos.active_principal_name";
// Used by KerberosAccountsHandler to prefill kerberos domain in
// username field of "Add a ticket" UI window.
// Tied to KerberosDomainAutocomplete policy.
const char kKerberosDomainAutocomplete[] = "kerberos.domain_autocomplete";
// Used by KerberosAccountsHandler to decide if the custom default configuration
// should be prefilled.
// Tied to KerberosUseCustomPrefilledConfig policy.
const char kKerberosUseCustomPrefilledConfig[] =
    "kerberos.use_custom_prefilled_config";
// Used by KerberosAccountsHandler to prefill kerberos krb5 config for
// manually creating new tickets.
// Tied to KerberosCustomPrefilledConfig policy.
const char kKerberosCustomPrefilledConfig[] =
    "kerberos.custom_prefilled_config";

// A boolean pref for enabling/disabling App reinstall recommendations in Zero
// State Launcher by policy.
const char kAppReinstallRecommendationEnabled[] =
    "zero_state_app_install_recommendation.enabled";

// A boolean pref that when set to true, prevents the browser window from
// launching at the start of the session.
const char kStartupBrowserWindowLaunchSuppressed[] =
    "startup_browser_window_launch_suppressed";

// A string pref stored in local state. Set and read by extensions using the
// chrome.login API.
const char kLoginExtensionApiDataForNextLoginAttempt[] =
    "extensions_api.login.data_for_next_login_attempt";

// A boolean user profile pref which indicates that the current Managed Guest
// Session is lockable. Set by the chrome.login extension API and read by
// `UserManager`.
const char kLoginExtensionApiCanLockManagedGuestSession[] =
    "extensions_api.login.can_lock_managed_guest_session";

// String containing last RSU lookup key uploaded. Empty until first upload.
const char kLastRsuDeviceIdUploaded[] = "rsu.last_rsu_device_id_uploaded";

// A string pref stored in local state containing the name of the device.
const char kDeviceName[] = "device_name";

// Int64 pref indicating the time in microseconds since Windows epoch when the
// timer for update required which will block user session was started. If the
// timer is not started the pref holds the default value base::Time().
const char kUpdateRequiredTimerStartTime[] = "update_required_timer_start_time";

// Int64 pref indicating the waiting time in microseconds after which the update
// required timer will expire and block user session. If the timer is not
// started the pref holds the default value base::TimeDelta().
const char kUpdateRequiredWarningPeriod[] = "update_required_warning_period";

// String user profile pref that contains the host and port of the local
// proxy which tunnels user traffic, in the format <address>:<proxy>. Only set
// when System-proxy and ARC++ are enabled by policy.
const char kSystemProxyUserTrafficHostAndPort[] =
    "system_proxy.user_traffic_host_and_port";

// Boolean pref indicating whether the supervised user has migrated EDU
// secondary account to ARC++.
const char kEduCoexistenceArcMigrationCompleted[] =
    "account_manager.edu_coexistence_arc_migration_completed";

// Dictionary pref for shared extension storage for device pin.
const char kSharedStorage[] = "shared_storage";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// This boolean controls whether the first window shown on first run should be
// unconditionally maximized, overriding the heuristic that normally chooses the
// window size.
const char kForceMaximizeOnFirstRun[] = "ui.force_maximize_on_first_run";

// Counter for reporting daily OOM kills count.
const char kOOMKillsDailyCount[] = "oom_kills.daily_count";

// Integer pref used by the metrics::DailyEvent owned by
// memory::OOMKillsMonitor.
const char kOOMKillsDailySample[] = "oomkills.daily_sample";

// List pref containing extension IDs that are exempt from the restricted
// managed guest session clean-up procedure.
const char kRestrictedManagedGuestSessionExtensionCleanupExemptList[] =
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
const char kUsedPolicyCertificates[] = "policy.used_policy_certificates";
#endif  // BUILDFLAG(IS_CHROMEOS)

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
const char kShowHomeButton[] = "browser.show_home_button";

// Boolean pref to define the default setting for "block offensive words".
// The old key value is kept to avoid unnecessary migration code.
const char kSpeechRecognitionFilterProfanities[] =
    "browser.speechinput_censor_results";

// Boolean controlling whether deleting browsing and download history is
// permitted.
const char kAllowDeletingBrowserHistory[] = "history.deleting_enabled";

// Boolean controlling whether SafeSearch is mandatory for Google Web Searches.
const char kForceGoogleSafeSearch[] = "settings.force_google_safesearch";

// Integer controlling whether Restrict Mode (moderate/strict) is mandatory on
// YouTube. See |safe_search_util::YouTubeRestrictMode| for possible values.
const char kForceYouTubeRestrict[] = "settings.force_youtube_restrict";

// Comma separated list of domain names (e.g. "google.com,school.edu").
// When this pref is set, the user will be able to access Google Apps
// only using an account that belongs to one of the domains from this pref.
const char kAllowedDomainsForApps[] = "settings.allowed_domains_for_apps";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// A boolean pref that controls whether proxy settings from Ash-Chrome are
// applied or ignored. Always true for the primary profile.
const char kUseAshProxy[] = "lacros.proxy.use_ash_proxy";
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Linux specific preference on whether we should match the system theme.
const char kUsesSystemThemeDeprecated[] = "extensions.theme.use_system";
const char kSystemTheme[] = "extensions.theme.system_theme";
#endif
const char kCurrentThemePackFilename[] = "extensions.theme.pack";
const char kCurrentThemeID[] = "extensions.theme.id";
const char kAutogeneratedThemeColor[] = "autogenerated.theme.color";
// Policy-controlled SkColor used to generate the browser's theme. The value
// SK_ColorTRANSPARENT means the policy has not been set.
const char kPolicyThemeColor[] = "autogenerated.theme.policy.color";

// Boolean pref which persists whether the extensions_ui is in developer mode
// (showing developer packing tools and extensions details)
const char kExtensionsUIDeveloperMode[] = "extensions.ui.developer_mode";

// Dictionary pref that tracks which command belongs to which
// extension + named command pair.
const char kExtensionCommands[] = "extensions.commands";

// Pref containing the directory for internal plugins as written to the plugins
// list (below).
const char kPluginsLastInternalDirectory[] = "plugins.last_internal_directory";

// List pref containing information (dictionaries) on plugins.
const char kPluginsPluginsList[] = "plugins.plugins_list";

// Whether Chrome should use its internal PDF viewer or not.
const char kPluginsAlwaysOpenPdfExternally[] =
    "plugins.always_open_pdf_externally";

#if BUILDFLAG(ENABLE_PLUGINS)
// Whether about:plugins is shown in the details mode or not.
const char kPluginsShowDetails[] = "plugins.show_details";
#endif

// Boolean that indicates whether outdated plugins are allowed or not.
const char kPluginsAllowOutdated[] = "plugins.allow_outdated";

// Int64 containing the internal value of the time at which the default browser
// infobar was last dismissed by the user.
const char kDefaultBrowserLastDeclined[] =
    "browser.default_browser_infobar_last_declined";
// Boolean that indicates whether the kDefaultBrowserLastDeclined preference
// should be reset on start-up.
const char kResetCheckDefaultBrowser[] =
    "browser.should_reset_check_default_browser";

// Policy setting whether default browser check should be disabled and default
// browser registration should take place.
const char kDefaultBrowserSettingEnabled[] =
    "browser.default_browser_setting_enabled";

// Boolean that indicates whether chrome://accessibility should show the
// internal accessibility tree.
const char kShowInternalAccessibilityTree[] =
    "accessibility.show_internal_accessibility_tree";

// Whether the "Get Image Descriptions from Google" feature is enabled.
// Only shown to screen reader users.
const char kAccessibilityImageLabelsEnabled[] =
    "settings.a11y.enable_accessibility_image_labels";

// Whether the opt-in dialog for image labels has been accepted yet. The opt-in
// need not be shown every time if it has already been accepted once.
const char kAccessibilityImageLabelsOptInAccepted[] =
    "settings.a11y.enable_accessibility_image_labels_opt_in_accepted";

#if BUILDFLAG(IS_ANDROID)
// Whether the "Get Image Descriptions from Google" feature is enabled on
// Android. We expose this only to mobile Android.
const char kAccessibilityImageLabelsEnabledAndroid[] =
    "settings.a11y.enable_accessibility_image_labels_android";

// Whether the "Get Image Descriptions from Google" feature is enabled only
// while on Wi-Fi, or if it can use mobile data. Exposed only to mobile Android.
const char kAccessibilityImageLabelsOnlyOnWifi[] =
    "settings.a11y.enable_accessibility_image_labels_only_on_wifi";
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// A boolean pref which determines whether focus highlighting is enabled.
const char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
#endif

// Whether the PDF OCR feature is set to be always active. The PDF OCR feature
// is exposed to only screen reader users.
const char kAccessibilityPdfOcrAlwaysActive[] =
    "settings.a11y.pdf_ocr_always_active";

// Pref indicating the page colors option the user wants. Page colors is an
// accessibility feature that simulates forced colors mode at the browser level.
const char kPageColors[] = "settings.a11y.page_colors";

// Boolean Pref that indicates whether the user wants to enable page colors only
// when the OS is in an Increased Contrast mode such as High Contrast on Windows
// or Increased Contrast on Mac.
const char kApplyPageColorsOnlyOnIncreasedContrast[] =
    "settings.a11y.apply_page_colors_only_on_increased_contrast";

#if BUILDFLAG(IS_WIN)
// Boolean that indicates what the default page colors state should be. When
// true, page colors will be 'High Contrast' when OS High Contrast is turned on,
// otherwise page colors will remain 'Off'.
const char kIsDefaultPageColorsOnHighContrast[] =
    "settings.a11y.is_default_page_colors_on_high_contrast";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
// Boolean that indicates whether the application should show the info bar
// asking the user to set up automatic updates when Keystone promotion is
// required.
const char kShowUpdatePromotionInfoBar[] =
    "browser.show_update_promotion_info_bar";
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Boolean that is false if we should show window manager decorations.  If
// true, we draw a custom chrome frame (thicker title bar and blue border).
const char kUseCustomChromeFrame[] = "browser.custom_chrome_frame";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Which plugins have been allowed manually by the user.
// Note that this used to be `kContentSettingsPluginWhitelist`, hence the
// difference between the variable name and the string value.
const char kContentSettingsPluginAllowlist[] =
    "profile.content_settings.plugin_whitelist";
#endif

// Double that indicates the default zoom level.
const char kPartitionDefaultZoomLevel[] = "partition.default_zoom_level";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const char kPartitionPerHostZoomLevels[] = "partition.per_host_zoom_levels";

#if !BUILDFLAG(IS_ANDROID)
const char kPinnedTabs[] = "pinned_tabs";
#endif  // !BUILDFLAG(IS_ANDROID)

// Preference to disable 3D APIs (WebGL, Pepper 3D).
const char kDisable3DAPIs[] = "disable_3d_apis";

// Whether to enable hyperlink auditing ("<a ping>").
const char kEnableHyperlinkAuditing[] = "enable_a_ping";

// Whether to enable sending referrers.
const char kEnableReferrers[] = "enable_referrers";

// Whether to send the DNT header.
const char kEnableDoNotTrack[] = "enable_do_not_track";

// Whether to allow the use of Encrypted Media Extensions (EME), except for the
// use of Clear Key key sytems, which is always allowed as required by the spec.
// TODO(crbug.com/784675): This pref was used as a WebPreference which is why
// the string is prefixed with "webkit.webprefs". Now this is used in
// blink::RendererPreferences and we should migrate the pref to use a new
// non-webkit-prefixed string.
const char kEnableEncryptedMedia[] = "webkit.webprefs.encrypted_media_enabled";

// Boolean that specifies whether to import the form data for autofill from the
// default browser on first run.
const char kImportAutofillFormData[] = "import_autofill_form_data";

// Boolean that specifies whether to import bookmarks from the default browser
// on first run.
const char kImportBookmarks[] = "import_bookmarks";

// Boolean that specifies whether to import the browsing history from the
// default browser on first run.
const char kImportHistory[] = "import_history";

// Boolean that specifies whether to import the homepage from the default
// browser on first run.
const char kImportHomepage[] = "import_home_page";

// Boolean that specifies whether to import the saved passwords from the default
// browser on first run.
const char kImportSavedPasswords[] = "import_saved_passwords";

// Boolean that specifies whether to import the search engine from the default
// browser on first run.
const char kImportSearchEngine[] = "import_search_engine";

// Prefs used to remember selections in the "Import data" dialog on the settings
// page (chrome://settings/importData).
const char kImportDialogAutofillFormData[] = "import_dialog_autofill_form_data";
const char kImportDialogBookmarks[] = "import_dialog_bookmarks";
const char kImportDialogHistory[] = "import_dialog_history";
const char kImportDialogSavedPasswords[] = "import_dialog_saved_passwords";
const char kImportDialogSearchEngine[] = "import_dialog_search_engine";

// Profile avatar and name
const char kProfileAvatarIndex[] = "profile.avatar_index";
const char kProfileName[] = "profile.name";
// Whether a profile is using a default avatar name (eg. Pickles or Person 1)
// because it was randomly assigned at profile creation time.
const char kProfileUsingDefaultName[] = "profile.using_default_name";
// Whether a profile is using an avatar without having explicitly chosen it
// (i.e. was assigned by default by legacy profile creation).
const char kProfileUsingDefaultAvatar[] = "profile.using_default_avatar";
const char kProfileUsingGAIAAvatar[] = "profile.using_gaia_avatar";

// The supervised user ID.
const char kSupervisedUserId[] = "profile.managed_user_id";

// Indicates if we've already shown a notification that high contrast
// mode is on, recommending high-contrast extensions and themes.
const char kInvertNotificationShown[] = "invert_notification_version_2_shown";

// A pref holding the list of printer types to be disabled.
const char kPrinterTypeDenyList[] = "printing.printer_type_deny_list";

// The allowed/default value for the 'Headers and footers' checkbox, in Print
// Preview.
const char kPrintHeaderFooter[] = "printing.print_header_footer";

// A pref holding the allowed background graphics printing modes.
const char kPrintingAllowedBackgroundGraphicsModes[] =
    "printing.allowed_background_graphics_modes";

// A pref holding the default background graphics mode.
const char kPrintingBackgroundGraphicsDefault[] =
    "printing.background_graphics_default";

// A pref holding the default paper size.
const char kPrintingPaperSizeDefault[] = "printing.paper_size_default";

#if BUILDFLAG(ENABLE_PRINTING)
// Boolean controlling whether printing is enabled.
const char kPrintingEnabled[] = "printing.enabled";
#endif  // BUILDFLAG(ENABLE_PRINTING)

// Boolean controlling whether print preview is disabled.
const char kPrintPreviewDisabled[] = "printing.print_preview_disabled";

// A pref holding the value of the policy used to control default destination
// selection in the Print Preview. See DefaultPrinterSelection policy.
const char kPrintPreviewDefaultDestinationSelectionRules[] =
    "printing.default_destination_selection_rules";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Boolean controlling whether the "Print as image" option should be available
// in Print Preview when printing a PDF.
const char kPrintPdfAsImageAvailability[] =
    "printing.print_pdf_as_image_availability";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// An integer resolution to use for DPI when rasterizing PDFs with "Print to
// image".
const char kPrintRasterizePdfDpi[] = "printing.rasterize_pdf_dpi";

// Boolean controlling whether the "Print as image" option should default to set
// in Print Preview when printing a PDF.
const char kPrintPdfAsImageDefault[] = "printing.print_pdf_as_image_default";
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINTING)
// An integer pref that holds the PostScript mode to use when printing.
const char kPrintPostScriptMode[] = "printing.postscript_mode";

// An integer pref that holds the rasterization mode to use when printing.
const char kPrintRasterizationMode[] = "printing.rasterization_mode";
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// A pref that sets the default destination in Print Preview to always be the
// OS default printer instead of the most recently used destination.
const char kPrintPreviewUseSystemDefaultPrinter[] =
    "printing.use_system_default_printer";

// A prefs that limits how many snapshots of the user's data directory there can
// be on the disk at any time. Following each major version update, Chrome will
// create a snapshot of certain portions of the user's browsing data for use in
// case of a later emergency version rollback.
const char kUserDataSnapshotRetentionLimit[] =
    "downgrade.snapshot_retention_limit";
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// List of print servers ids that are allowed in the user policy. List of
// strings. Note that this used to be `kExternalPrintServersWhitelist`, hence
// the difference between the variable name and the string value.
const char kExternalPrintServersAllowlist[] =
    "native_printing.external_print_servers_whitelist";

// List of print servers ids that are allowed in the device policy. List of
// strings.
const char kDeviceExternalPrintServersAllowlist[] =
    "native_printing.device_external_print_servers_allowlist";

// List of printers configured by policy.
const char kRecommendedPrinters[] = "native_printing.recommended_printers";

// Enum designating the type of restrictions bulk printers are using.
const char kRecommendedPrintersAccessMode[] =
    "native_printing.recommended_printers_access_mode";

// List of printer ids which are explicitly disallowed.  List of strings. Note
// that this used to be `kRecommendedPrintersBlacklist`, hence the difference
// between the variable name and the string value.
const char kRecommendedPrintersBlocklist[] =
    "native_printing.recommended_printers_blacklist";

// List of printer ids that are allowed.  List of strings. Note that this
// used to be `kRecommendedNativePrintersWhitelist`, hence the difference
// between the variable name and the string value.
const char kRecommendedPrintersAllowlist[] =
    "native_printing.recommended_printers_whitelist";

// A Boolean flag which represents whether or not users are allowed to configure
// and use their own printers.
const char kUserPrintersAllowed[] =
    "native_printing.user_native_printers_allowed";

// A pref holding the list of allowed printing color mode as a bitmask composed
// of |printing::ColorModeRestriction| values. 0 is no restriction.
const char kPrintingAllowedColorModes[] = "printing.allowed_color_modes";

// A pref holding the list of allowed printing duplex mode as a bitmask composed
// of |printing::DuplexModeRestriction| values. 0 is no restriction.
const char kPrintingAllowedDuplexModes[] = "printing.allowed_duplex_modes";

// A pref holding the allowed PIN printing modes.
const char kPrintingAllowedPinModes[] = "printing.allowed_pin_modes";

// A pref holding the default color mode.
const char kPrintingColorDefault[] = "printing.color_default";

// A pref holding the default duplex mode.
const char kPrintingDuplexDefault[] = "printing.duplex_default";

// A pref holding the default PIN mode.
const char kPrintingPinDefault[] = "printing.pin_default";

// Boolean flag which represents whether username and filename should be sent
// to print server.
const char kPrintingSendUsernameAndFilenameEnabled[] =
    "printing.send_username_and_filename_enabled";

// Indicates how many sheets is allowed to use for a single print job.
const char kPrintingMaxSheetsAllowed[] = "printing.max_sheets_allowed";

// Indicates how long print jobs metadata is stored on the device, in days.
const char kPrintJobHistoryExpirationPeriod[] =
    "printing.print_job_history_expiration_period";

// Boolean flag which represents whether the user's print job history can be
// deleted.
const char kDeletePrintJobHistoryAllowed[] =
    "printing.delete_print_job_history_allowed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// An integer pref specifying the fallback behavior for sites outside of content
// packs. One of:
// 0: Allow (does nothing)
// 1: Warn. [Deprecated]
// 2: Block.
const char kDefaultSupervisedUserFilteringBehavior[] =
    "profile.managed.default_filtering_behavior";

// List pref containing the users supervised by this user.
const char kSupervisedUsers[] = "profile.managed_users";

// List pref containing the extension ids which are not allowed to send
// notifications to the message center.
const char kMessageCenterDisabledExtensionIds[] =
    "message_center.disabled_extension_ids";

// Boolean pref that determines whether the user can enter fullscreen mode.
// Disabling fullscreen mode also makes kiosk mode unavailable on desktop
// platforms.
const char kFullscreenAllowed[] = "fullscreen.allowed";

#if BUILDFLAG(IS_ANDROID)
// Boolean pref indicating whether notification permissions were migrated to
// notification channels (on Android O+ we use channels to store notification
// permission, so any existing permissions must be migrated).
const char kMigratedToSiteNotificationChannels[] =
    "notifications.migrated_to_channels";

// Boolean pref indicating whether blocked site notification channels underwent
// a one-time reset yet for https://crbug.com/835232.
// TODO(https://crbug.com/837614): Remove this after a few releases (M69?).
const char kClearedBlockedSiteNotificationChannels[] =
    "notifications.cleared_blocked_channels";

// Usage stats reporting opt-in.
const char kUsageStatsEnabled[] = "usage_stats_reporting.enabled";

#endif  // BUILDFLAG(IS_ANDROID)

// Maps from app ids to origin + Service Worker registration ID.
const char kPushMessagingAppIdentifierMap[] =
    "gcm.push_messaging_application_id_map";

// A string like "com.chrome.macosx" that should be used as the GCM category
// when an app_id is sent as a subtype instead of as a category.
const char kGCMProductCategoryForSubtypes[] =
    "gcm.product_category_for_subtypes";

// Whether a user is allowed to use Easy Unlock.
const char kEasyUnlockAllowed[] = "easy_unlock.allowed";

// Preference storing Easy Unlock pairing data.
const char kEasyUnlockPairing[] = "easy_unlock.pairing";

const char kHasSeenSmartLockSignInRemovedNotification[] =
    "easy_unlock.has_seen_smart_lock_sign_in_removed_notification";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Used to indicate whether or not the toolbar redesign bubble has been shown
// and acknowledged, and the last time the bubble was shown.
const char kToolbarIconSurfacingBubbleAcknowledged[] =
    "toolbar_icon_surfacing_bubble_acknowledged";
const char kToolbarIconSurfacingBubbleLastShowTime[] =
    "toolbar_icon_surfacing_bubble_show_time";
#endif

// Define the IP handling policy override that WebRTC should follow. When not
// set, it defaults to "default".
const char kWebRTCIPHandlingPolicy[] = "webrtc.ip_handling_policy";
// Define range of UDP ports allowed to be used by WebRTC PeerConnections.
const char kWebRTCUDPPortRange[] = "webrtc.udp_port_range";
// Whether WebRTC event log collection by Google domains is allowed.
const char kWebRtcEventLogCollectionAllowed[] = "webrtc.event_logs_collection";
// Holds URL patterns that specify URLs for which local IP addresses are exposed
// in ICE candidates.
const char kWebRtcLocalIpsAllowedUrls[] = "webrtc.local_ips_allowed_urls";
// Whether WebRTC PeerConnections are allowed to use legacy versions of the TLS
// and DTLS protocols.
const char kWebRTCAllowLegacyTLSProtocols[] =
    "webrtc.allow_legacy_tls_protocols";

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
// Boolean that indicates that the first run experience has been finished (or
// skipped by some policy) for this browser install.
const char kFirstRunFinished[] = "browser.first_run_finished";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Whether or not this profile has been shown the Welcome page.
const char kHasSeenWelcomePage[] = "browser.has_seen_welcome_page";

// The restriction imposed on managed accounts.
const char kManagedAccountsSigninRestriction[] =
    "profile.managed_accounts.restriction.value";

// Whether or not the restriction is applied on all managed accounts of the
// machine. If this is set to True, the restriction set in
// `profile.managed_accounts.restriction.value` will be applied on all managed
// accounts on the machine, otherwhise only the account where the policy is set
// will have the restriction applied.
const char kManagedAccountsSigninRestrictionScopeMachine[] =
    "profile.managed_accounts.restriction.all_managed_accounts";
#if !BUILDFLAG(IS_CHROMEOS)
// Whether or not the option to keep existing browsing data is checked by
// default.
extern const char kEnterpriseProfileCreationKeepBrowsingData[] =
    "profile.enterprise_profile_creation.keep_existing_data_by_default";
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif

#if BUILDFLAG(IS_WIN)
// Put the user into an onboarding group that's decided when they go through
// the first run onboarding experience. Only users in a group will have their
// finch group pinged to keep track of them for the experiment.
const char kNaviOnboardGroup[] = "browser.navi_onboard_group";
#endif  // BUILDFLAG(IS_WIN)

// Boolean indicating whether, as part of the adaptive activation quiet UI dry
// run experiment, the user has accumulated three notification permission
// request denies in a row.
const char kHadThreeConsecutiveNotificationPermissionDenies[] =
    "profile.content_settings.had_three_consecutive_denies.notifications";

// Boolean indicating whether to show a promo for the quiet notification
// permission UI.
const char kQuietNotificationPermissionShouldShowPromo[] =
    "profile.content_settings.quiet_permission_ui_promo.should_show."
    "notifications";

// Boolean indicating whether the promo was shown for the quiet notification
// permission UI.
const char kQuietNotificationPermissionPromoWasShown[] =
    "profile.content_settings.quiet_permission_ui_promo.was_shown."
    "notifications";

// Boolean indicating if JS dialogs triggered from a different origin iframe
// should be blocked. Has no effect if
// "SuppressDifferentOriginSubframeJSDialogs" feature is disabled.
const char kSuppressDifferentOriginSubframeJSDialogs[] =
    "suppress_different_origin_subframe_js_dialogs";

// Enum indicating if the user agent reduction feature should be forced enabled
// or disabled. Defaults to blink::features::kReduceUserAgent field trial.
const char kUserAgentReduction[] = "user_agent_reduction";

// Enum indicating if the user agent string should freeze the major version
// at 99 and report the browser's major version in the minor position.
const char kForceMajorVersionToMinorPositionInUserAgent[] =
    "force_major_version_to_minor_position_in_user_agent";

#if (!BUILDFLAG(IS_ANDROID))
// Boolean determining the side the side panel will be appear on (left / right).
// True when the side panel is aligned to the right.
const char kSidePanelHorizontalAlignment[] = "side_panel.is_right_aligned";
#endif

// Number of minutes of inactivity before running actions from
// kIdleTimeoutActions. Controlled via the IdleTimeout policy.
const char kIdleTimeout[] = "idle_timeout";

// Actions to run when the idle timeout is reached. Controller via the
// IdleTimeoutActions policy.
const char kIdleTimeoutActions[] = "idle_timeout_actions";

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

#if !BUILDFLAG(IS_ANDROID)
// Used to store the value of the SerialAllowAllPortsForUrls policy.
const char kManagedSerialAllowAllPortsForUrls[] =
    "managed.serial_allow_all_ports_for_urls";

// Used to store the value of the SerialAllowUsbDevicesForUrls policy.
const char kManagedSerialAllowUsbDevicesForUrls[] =
    "managed.serial_allow_usb_devices_for_urls";

// Used to store the value of the WebHidAllowAllDevicesForUrls policy.
const char kManagedWebHidAllowAllDevicesForUrls[] =
    "managed.web_hid_allow_all_devices_for_urls";

// Used to store the value of the WebHidAllowDevicesForUrls policy.
const char kManagedWebHidAllowDevicesForUrls[] =
    "managed.web_hid_allow_devices_for_urls";

// Used to store the value of the WebHidAllowAllDevicesWithHidUsagesForUrls
// policy.
const char kManagedWebHidAllowDevicesWithHidUsagesForUrls[] =
    "managed.web_hid_allow_devices_with_hid_usages_for_urls";
#endif  // !BUILDFLAG(IS_ANDROID)

// Directory of the last profile used.
const char kProfileLastUsed[] = "profile.last_used";

// List of directories of the profiles last active in browser windows. It does
// not include profiles active in app windows. When a browser window is opened,
// if it's the only browser window open in the profile, its profile is added to
// this list. When a browser window is closed, and there are no other browser
// windows open in the profile, its profile is removed from this list. When
// Chrome is launched with --session-restore, each of the profiles in this list
// have their sessions restored.
const char kProfilesLastActive[] = "profile.last_active_profiles";

// Total number of profiles created for this Chrome build. Used to tag profile
// directories.
const char kProfilesNumCreated[] = "profile.profiles_created";

// String containing the version of Chrome that the profile was created by.
// If profile was created before this feature was added, this pref will default
// to "1.0.0.0".
const char kProfileCreatedByVersion[] = "profile.created_by_version";

// A map of profile data directory to profile attributes. These attributes can
// be used to display information about profiles without actually having to load
// them.
const char kProfileAttributes[] = "profile.info_cache";

// A list of profile paths that should be deleted on shutdown. The deletion does
// not happen if the browser crashes, so we remove the profile on next start.
const char kProfilesDeleted[] = "profiles.profiles_deleted";

// On Chrome OS, total number of non-Chrome user process crashes
// since the last report.
const char kStabilityOtherUserCrashCount[] =
    "user_experience_metrics.stability.other_user_crash_count";

// On Chrome OS, total number of kernel crashes since the last report.
const char kStabilityKernelCrashCount[] =
    "user_experience_metrics.stability.kernel_crash_count";

// On Chrome OS, total number of unclean system shutdowns since the
// last report.
const char kStabilitySystemUncleanShutdownCount[] =
    "user_experience_metrics.stability.system_unclean_shutdowns";

// String containing the version of Chrome for which Chrome will not prompt the
// user about setting Chrome as the default browser.
const char kBrowserSuppressDefaultBrowserPrompt[] =
    "browser.suppress_default_browser_prompt_for_version";

// A collection of position, size, and other data relating to the browser
// window to restore on startup.
const char kBrowserWindowPlacement[] = "browser.window_placement";

// Browser window placement for popup windows.
const char kBrowserWindowPlacementPopup[] = "browser.window_placement_popup";

// A collection of position, size, and other data relating to the task
// manager window to restore on startup.
const char kTaskManagerWindowPlacement[] = "task_manager.window_placement";

// The most recent stored column visibility of the task manager table to be
// restored on startup.
const char kTaskManagerColumnVisibility[] = "task_manager.column_visibility";

// A boolean indicating if ending processes are enabled or disabled by policy.
const char kTaskManagerEndProcessEnabled[] = "task_manager.end_process_enabled";

// A collection of position, size, and other data relating to app windows to
// restore on startup.
const char kAppWindowPlacement[] = "browser.app_window_placement";

// String which specifies where to download files to by default.
const char kDownloadDefaultDirectory[] = "download.default_directory";

// Boolean that records if the download directory was changed by an
// upgrade a unsafe location to a safe location.
const char kDownloadDirUpgraded[] = "download.directory_upgrade";

// base::Time value indicating the last timestamp when a download is completed.
const char kDownloadLastCompleteTime[] = "download.last_complete_time";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
const char kOpenPdfDownloadInSystemReader[] =
    "download.open_pdf_in_system_reader";
#endif

#if BUILDFLAG(IS_ANDROID)
// Int (as defined by DownloadPromptStatus) which specifies whether we should
// ask the user where they want to download the file (only for Android).
const char kPromptForDownloadAndroid[] = "download.prompt_for_download_android";

// Boolean which specifies whether we should display the missing SD card error.
// This is only applicable for Android.
const char kShowMissingSdCardErrorAndroid[] =
    "download.show_missing_sd_card_error_android";

// Boolean which specifies whether the user has turned on incognito
// reauthentication setting for Android.
const char kIncognitoReauthenticationForAndroid[] =
    "incognito.incognito_reauthentication";
#endif

// String which specifies where to save html files to by default.
const char kSaveFileDefaultDirectory[] = "savefile.default_directory";

// The type used to save the page. See the enum SavePackage::SavePackageType in
// the chrome/browser/download/save_package.h for the possible values.
const char kSaveFileType[] = "savefile.type";

// String which specifies the last directory that was chosen for uploading
// or opening a file.
const char kSelectFileLastDirectory[] = "selectfile.last_directory";

// Boolean that specifies if file selection dialogs are shown.
const char kAllowFileSelectionDialogs[] = "select_file_dialogs.allowed";

// Map of default tasks, associated by MIME type.
const char kDefaultTasksByMimeType[] = "filebrowser.tasks.default_by_mime_type";

// Map of default tasks, associated by file suffix.
const char kDefaultTasksBySuffix[] = "filebrowser.tasks.default_by_suffix";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Maps file extensions to handlers according to the
// DefaultHandlersForFileExtensions policy.
const char kDefaultHandlersForFileExtensions[] =
    "filebrowser.default_handlers_for_file_extensions";

// Whether the office files setup flow has ever been completed by the user.
const char kOfficeSetupComplete[] = "filebrowser.office.setup_complete";

// Whether we should always move office files without prompting the user first.
const char kOfficeFilesAlwaysMove[] = "filebrowser.office.always_move";
#endif

// A flag to enable/disable the Shared Clipboard feature which enables users to
// send text across devices.
const char kSharedClipboardEnabled[] = "browser.shared_clipboard_enabled";

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
// A flag to enable/disable the Click to Call feature which enables users to
// send phone numbers from desktop to Android phones.
const char kClickToCallEnabled[] = "browser.click_to_call_enabled";
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

// Extensions which should be opened upon completion.
const char kDownloadExtensionsToOpen[] = "download.extensions_to_open";

// Extensions which should be opened upon completion, set by policy.
const char kDownloadExtensionsToOpenByPolicy[] =
    "download.extensions_to_open_by_policy";

const char kDownloadAllowedURLsForOpenByPolicy[] =
    "download.allowed_urls_for_open_by_policy";

// Dictionary of origins that have permission to launch at least one protocol
// without first prompting the user. Each origin is a nested dictionary.
// Within an origin dictionary, if a protocol is present with value |true|,
// that protocol may be launched by that origin without first prompting
// the user.
const char kProtocolHandlerPerOriginAllowedProtocols[] =
    "protocol_handler.allowed_origin_protocol_pairs";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
const char kLastKnownIntranetRedirectOrigin[] = "browser.last_redirect_origin";

// Boolean specifying that the intranet redirect detector should be enabled.
// Defaults to true.
// See also kIntranetRedirectBehavior in the omnibox component's prefs, which
// also impacts the redirect detector.
const char kDNSInterceptionChecksEnabled[] =
    "browser.dns_interception_checks_enabled";

// An enum value of how the browser was shut down (see browser_shutdown.h).
const char kShutdownType[] = "shutdown.type";
// Number of processes that were open when the user shut down.
const char kShutdownNumProcesses[] = "shutdown.num_processes";
// Number of processes that were shut down using the slow path.
const char kShutdownNumProcessesSlow[] = "shutdown.num_processes_slow";

// Whether to restart the current Chrome session automatically as the last thing
// before shutting everything down.
const char kRestartLastSessionOnShutdown[] = "restart.last.session.on.shutdown";

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean that specifies whether or not to show security warnings for some
// potentially bad command-line flags. True by default. Controlled by the
// CommandLineFlagSecurityWarningsEnabled policy setting.
const char kCommandLineFlagSecurityWarningsEnabled[] =
    "browser.command_line_flag_security_warnings_enabled";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Pref name for the policy controlling presentation of full-tab promotional
// and/or educational content.
const char kPromotionalTabsEnabled[] = "browser.promotional_tabs_enabled";

// Boolean that specifies whether or not showing the unsupported OS warning is
// suppressed. False by default. Controlled by the SuppressUnsupportedOSWarning
// policy setting.
const char kSuppressUnsupportedOSWarning[] =
    "browser.suppress_unsupported_os_warning";

// Set before autorestarting Chrome, cleared on clean exit.
const char kWasRestarted[] = "was.restarted";
#endif  // !BUILDFLAG(IS_ANDROID)

// Whether Extensions are enabled.
const char kDisableExtensions[] = "extensions.disabled";

// Customized app page names that appear on the New Tab Page.
const char kNtpAppPageNames[] = "ntp.app_page_names";

// Keeps track of which sessions are collapsed in the Other Devices menu.
const char kNtpCollapsedForeignSessions[] = "ntp.collapsed_foreign_sessions";

#if BUILDFLAG(IS_ANDROID)
// Keeps track of recently closed tabs collapsed state in the Other Devices
// menu.
const char kNtpCollapsedRecentlyClosedTabs[] =
    "ntp.collapsed_recently_closed_tabs";

// Keeps track of snapshot documents collapsed state in the Other Devices menu.
const char kNtpCollapsedSnapshotDocument[] = "ntp.collapsed_snapshot_document";

// Keeps track of sync promo collapsed state in the Other Devices menu.
const char kNtpCollapsedSyncPromo[] = "ntp.collapsed_sync_promo";
#else
// Holds info for New Tab Page custom background
const char kNtpCustomBackgroundDict[] = "ntp.custom_background_dict";
const char kNtpCustomBackgroundLocalToDevice[] =
    "ntp.custom_background_local_to_device";
// Number of times the user has opened the side panel with the customize chrome
// button.
const char kNtpCustomizeChromeButtonOpenCount[] =
    "NewTabPage.CustomizeChromeButtonOpenCount";
// List keeping track of disabled NTP modules.
const char kNtpDisabledModules[] = "NewTabPage.DisabledModules";
// List keeping track of NTP modules order.
const char kNtpModulesOrder[] = "NewTabPage.ModulesOrder";
// Whether NTP modules are visible.
const char kNtpModulesVisible[] = "NewTabPage.ModulesVisible";
// Number of times user has seen an NTP module.
const char kNtpModulesShownCount[] = "NewTabPage.ModulesShownCount";
// Time modules were first shown to user.
const char kNtpModulesFirstShownTime[] = "NewTabPage.ModulesFirstShownTime";
// Whether Modular NTP Desktop v1 First Run Experience is visible.
const char kNtpModulesFreVisible[] = "NewTabPage.ModulesFreVisible";
// List of promos that the user has dismissed while on the NTP.
const char kNtpPromoBlocklist[] = "ntp.promo_blocklist";
// Whether the promo is visible.
const char kNtpPromoVisible[] = "ntp.promo_visible";
#endif  // BUILDFLAG(IS_ANDROID)

// Which page should be visible on the new tab page v4
const char kNtpShownPage[] = "ntp.shown_page";

// A private RSA key for ADB handshake.
const char kDevToolsAdbKey[] = "devtools.adb_key";

// Defines administrator-set availability of developer tools.
const char kDevToolsAvailability[] = "devtools.availability";

// Defines administrator-set availability of developer tools remote debugging.
const char kDevToolsRemoteDebuggingAllowed[] =
    "devtools.remote_debugging.allowed";

// Dictionary from background service to recording expiration time.
const char kDevToolsBackgroundServicesExpirationDict[] =
    "devtools.backgroundserviceexpiration";

// Determines whether devtools should be discovering usb devices for
// remote debugging at chrome://inspect.
const char kDevToolsDiscoverUsbDevicesEnabled[] =
    "devtools.discover_usb_devices";

// Maps of files edited locally using DevTools.
const char kDevToolsEditedFiles[] = "devtools.edited_files";

// List of file system paths added in DevTools.
const char kDevToolsFileSystemPaths[] = "devtools.file_system_paths";

// A boolean specifying whether port forwarding should be enabled.
const char kDevToolsPortForwardingEnabled[] =
    "devtools.port_forwarding_enabled";

// A boolean specifying whether default port forwarding configuration has been
// set.
const char kDevToolsPortForwardingDefaultSet[] =
    "devtools.port_forwarding_default_set";

// A dictionary of port->location pairs for port forwarding.
const char kDevToolsPortForwardingConfig[] = "devtools.port_forwarding_config";

// A boolean specifying whether or not Chrome will scan for available remote
// debugging targets.
const char kDevToolsDiscoverTCPTargetsEnabled[] =
    "devtools.discover_tcp_targets";

// A list of strings representing devtools target discovery servers.
const char kDevToolsTCPDiscoveryConfig[] = "devtools.tcp_discovery_config";

// A dictionary with all unsynced DevTools settings.
const char kDevToolsPreferences[] = "devtools.preferences";

// A boolean specifying whether the "syncable" subset of DevTools preferences
// should be synced or not.
const char kDevToolsSyncPreferences[] = "devtools.sync_preferences";

// Dictionaries with all synced DevTools settings. Depending on the state of the
// kDevToolsSyncPreferences toggle, one or the other dictionary will be used.
// The "Enabled" dictionary is synced via Chrome Sync with the rest of Chrome
// settings, while the "Disabled" dictionary won't be synced. This allows
// DevTools to opt-in of syncing DevTools settings independently from syncing
// Chrome settings.
const char kDevToolsSyncedPreferencesSyncEnabled[] =
    "devtools.synced_preferences_sync_enabled";
const char kDevToolsSyncedPreferencesSyncDisabled[] =
    "devtools.synced_preferences_sync_disabled";

#if !BUILDFLAG(IS_ANDROID)
// Tracks the number of times the dice signin promo has been shown in the user
// menu.
const char kDiceSigninUserMenuPromoCount[] = "sync_promo.user_menu_show_count";
#endif

// Create web application shortcut dialog preferences.
const char kWebAppCreateOnDesktop[] = "browser.web_app.create_on_desktop";
const char kWebAppCreateInAppsMenu[] = "browser.web_app.create_in_apps_menu";
const char kWebAppCreateInQuickLaunchBar[] =
    "browser.web_app.create_in_quick_launch_bar";

// A list of dictionaries for force-installed Web Apps. Each dictionary contains
// two strings: the URL of the Web App and "tab" or "window" for where the app
// will be launched.
const char kWebAppInstallForceList[] = "profile.web_app.install.forcelist";

// A list of dictionaries for managing Web Apps.
const char kWebAppSettings[] = "profile.web_app.policy_settings";

// A map of App ID to install URLs to keep track of preinstalled web apps
// after they have been deleted.
const char kUserUninstalledPreinstalledWebAppPref[] =
    "web_app.app_id.install_url";

// A list of dictionaries for managed configurations. Each dictionary
// contains 3 strings -- origin to be configured, link to the configuration,
// and the hashed value to that configuration.
const char kManagedConfigurationPerOrigin[] =
    "profile.managed_configuration.list";

// Dictionary that maps the hash of the last downloaded managed configuration
// for a particular origin.
const char kLastManagedConfigurationHashForOrigin[] =
    "profile.managed_configuration.last_hash";

// Dictionary that maps web app ids to installation metrics used by UMA.
const char kWebAppInstallMetrics[] = "web_app_install_metrics";

// Dictionary that maps web app start URLs to temporary metric info to be
// emitted once the date changes.
const char kWebAppsDailyMetrics[] = "web_apps.daily_metrics";

// Time representing the date for which |kWebAppsDailyMetrics| is stored.
const char kWebAppsDailyMetricsDate[] = "web_apps.daily_metrics_date";

// Dictionary that maps web app URLs to Chrome extension IDs.
const char kWebAppsExtensionIDs[] = "web_apps.extension_ids";

// Dictionary that stores IPH state not scoped to a particular app.
const char kWebAppsAppAgnosticIphState[] = "web_apps.app_agnostic_iph_state";

// A string representing the last version of Chrome preinstalled web apps were
// synchronised for.
const char kWebAppsLastPreinstallSynchronizeVersion[] =
    "web_apps.last_preinstall_synchronize_version";

// A list of migrated features for migrating default chrome apps.
const char kWebAppsDidMigrateDefaultChromeApps[] =
    "web_apps.did_migrate_default_chrome_apps";

// A list of default chrome apps that were uninstalled by the user.
const char kWebAppsUninstalledDefaultChromeApps[] =
    "web_apps.uninstalled_default_chrome_apps";

// Dictionary that maps web app ID to a dictionary of various preferences.
// Used only in the new web applications system to store app preferences which
// outlive the app installation and uninstallation.
const char kWebAppsPreferences[] = "web_apps.web_app_ids";

// Dictionary that maps the origin of a web app to other preferences related to
// its isolation requirements.
const char kWebAppsIsolationState[] = "web_apps.isolation_state";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
// Dictionary that maps origins to web apps that can act as URL handlers.
const char kWebAppsUrlHandlerInfo[] = "web_apps.url_handler_info";
#endif

// The default audio capture device used by the Media content setting.
const char kDefaultAudioCaptureDevice[] = "media.default_audio_capture_device";

// The default video capture device used by the Media content setting.
const char kDefaultVideoCaptureDevice[] = "media.default_video_capture_Device";

// The salt used for creating random MediaSource IDs.
const char kMediaDeviceIdSalt[] = "media.device_id_salt";

// The salt used for creating Storage IDs. The Storage ID is used by encrypted
// media to bind persistent licenses to the device which is authorized to play
// the content.
const char kMediaStorageIdSalt[] = "media.storage_id_salt";

#if BUILDFLAG(IS_WIN)
// Mapping of origin to their origin id (UnguessableToken). Origin IDs are only
// stored for origins using MediaFoundation-based CDMs.
const char kMediaCdmOriginData[] = "media.cdm.origin_data";

// A boolean pref to determine whether or not the network service is running
// sandboxed.
const char kNetworkServiceSandboxEnabled[] = "net.network_service_sandbox";

#endif  // BUILDFLAG(IS_WIN)

// The last used printer and its settings.
const char kPrintPreviewStickySettings[] =
    "printing.print_preview_sticky_settings";

// The list of BackgroundContents that should be loaded when the browser
// launches.
const char kRegisteredBackgroundContents[] = "background_contents.registered";

// Integer that specifies the total memory usage, in mb, that chrome will
// attempt to stay under. Can be specified via policy in addition to the default
// memory pressure rules applied per platform.
const char kTotalMemoryLimitMb[] = "total_memory_limit_mb";

// String that lists supported HTTP authentication schemes.
const char kAuthSchemes[] = "auth.schemes";

// List of origin schemes that allow the supported HTTP authentication schemes
// from "auth.schemes".
const char kAllHttpAuthSchemesAllowedForOrigins[] =
    "auth.http_auth_allowed_for_origins";

// Boolean that specifies whether to disable CNAME lookups when generating
// Kerberos SPN.
const char kDisableAuthNegotiateCnameLookup[] =
    "auth.disable_negotiate_cname_lookup";

// Boolean that specifies whether to include the port in a generated Kerberos
// SPN.
const char kEnableAuthNegotiatePort[] = "auth.enable_negotiate_port";

// Allowlist containing servers for which Integrated Authentication is enabled.
// This pref should match |android_webview::prefs::kAuthServerAllowlist|.
const char kAuthServerAllowlist[] = "auth.server_allowlist";

// Allowlist containing servers Chrome is allowed to do Kerberos delegation
// with. Note that this used to be `kAuthNegotiateDelegateWhitelist`, hence the
// difference between the variable name and the string value.
const char kAuthNegotiateDelegateAllowlist[] =
    "auth.negotiate_delegate_whitelist";

// String that specifies the name of a custom GSSAPI library to load.
const char kGSSAPILibraryName[] = "auth.gssapi_library_name";

// String that specifies the Android account type to use for Negotiate
// authentication.
const char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Boolean that specifies whether to allow basic auth prompting on cross-
// domain sub-content requests.
const char kAllowCrossOriginAuthPrompt[] = "auth.allow_cross_origin_prompt";

// Boolean that specifies whether cached (server) auth credentials are separated
// by NetworkAnonymizationKey.
const char kGloballyScopeHTTPAuthCacheEnabled[] =
    "auth.globally_scoped_http_auth_cache_enabled";

// Integer specifying the cases where ambient authentication is enabled.
// 0 - Only allow ambient authentication in regular sessions
// 1 - Only allow ambient authentication in regular and incognito sessions
// 2 - Only allow ambient authentication in regular and guest sessions
// 3 - Allow ambient authentication in regular, incognito and guest sessions
const char kAmbientAuthenticationInPrivateModesEnabled[] =
    "auth.ambient_auth_in_private_modes";

// Boolean that specifies whether HTTP Basic authentication is allowed for HTTP
// requests.
const char kBasicAuthOverHttpEnabled[] = "auth.basic_over_http_enabled";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
// Boolean that specifies whether OK-AS-DELEGATE flag from KDC is respected
// along with kAuthNegotiateDelegateAllowlist.
const char kAuthNegotiateDelegateByKdcPolicy[] =
    "auth.negotiate_delegate_by_kdc_policy";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// Boolean that specifies whether NTLMv2 is enabled.
const char kNtlmV2Enabled[] = "auth.ntlm_v2_enabled";
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
// Boolean whether Kerberos functionality is enabled.
const char kKerberosEnabled[] = "kerberos.enabled";

// A list of dictionaries for force-installed Isolated Web Apps. Each dictionary
// contains two strings: the update manifest URL and Web Bundle ID of the
// Isolated Web App,
const char kIsolatedWebAppInstallForceList[] =
    "profile.isolated_web_app.install.forcelist";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// The integer value of the CloudAPAuthEnabled policy.
const char kCloudApAuthEnabled[] = "auth.cloud_ap_auth.enabled";
#endif  // BUILDFLAG(IS_WIN)

// Boolean that specifies whether to enable revocation checking (best effort)
// by default.
const char kCertRevocationCheckingEnabled[] = "ssl.rev_checking.enabled";

// Boolean that specifies whether to require a successful revocation check if
// a certificate path ends in a locally-trusted (as opposed to publicly
// trusted) trust anchor.
const char kCertRevocationCheckingRequiredLocalAnchors[] =
    "ssl.rev_checking.required_for_local_anchors";

// String specifying the minimum TLS version to negotiate. Supported values
// are "tls1.2", "tls1.3".
const char kSSLVersionMin[] = "ssl.version_min";

// String specifying the maximum TLS version to negotiate. Supported values
// are "tls1.2", "tls1.3"
const char kSSLVersionMax[] = "ssl.version_max";

// String specifying the TLS ciphersuites to disable. Ciphersuites are
// specified as a comma-separated list of 16-bit hexadecimal values, with
// the values being the ciphersuites assigned by the IANA registry (e.g.
// "0x0004,0x0005").
const char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";

// List of strings specifying which hosts are allowed to have H2 connections
// coalesced when client certs are also used. This follows rules similar to
// the URLBlocklist format for hostnames: a pattern with a leading dot (e.g.
// ".example.net") matches exactly the hostname following the dot (i.e. only
// "example.net"), and a pattern with no leading dot (e.g. "example.com")
// matches that hostname and all subdomains.
const char kH2ClientCertCoalescingHosts[] =
    "ssl.client_certs.h2_coalescing_hosts";

// List of single-label hostnames that will skip the check to possibly upgrade
// from http to https.
const char kHSTSPolicyBypassList[] = "hsts.policy.upgrade_bypass_list";

// If false, disable post-quantum key agreement in TLS connections.
const char kCECPQ2Enabled[] = "ssl.cecpq2_enabled";

// If false, disable Encrypted ClientHello (ECH) in TLS connections.
const char kEncryptedClientHelloEnabled[] = "ssl.ech_enabled";

// Boolean that specifies whether the built-in asynchronous DNS client is used.
const char kBuiltInDnsClientEnabled[] = "async_dns.enabled";

// String specifying the secure DNS mode to use. Any string other than
// "secure" or "automatic" will be mapped to the default "off" mode.
const char kDnsOverHttpsMode[] = "dns_over_https.mode";

// String containing a space-separated list of DNS over HTTPS templates to use
// in secure mode or automatic mode. If no templates are specified in automatic
// mode, we will attempt discovery of DoH servers associated with the configured
// insecure resolvers.
const char kDnsOverHttpsTemplates[] = "dns_over_https.templates";

#if BUILDFLAG(IS_CHROMEOS)
// String containing a space-separated list of DNS over HTTPS templates to use
// in secure mode or automatic mode. If no templates are specified in automatic
// mode, we will attempt discovery of DoH servers associated with the configured
// insecure resolvers.
// This is very similar to kDnsOverHttpsTemplates except that on ChromeOS it
// supports additional variables which are used to transport identity
// information to the DNS provider. This is ignored on all other platforms than
// ChromeOS. On ChromeOS if it exists it will override kDnsOverHttpsTemplates,
// otherwise kDnsOverHttpsTemplates will be used. This pref is only evaluated if
// kDnsOverHttpsSalt is set.
const char kDnsOverHttpsTemplatesWithIdentifiers[] =
    "dns_over_https.templates_with_identifiers";
// String containing a salt value. This is used together with
// kDnsOverHttpsTemplatesWithIdentifiers, only. The value will be used as a salt
// to a hash applied to the various identity variables to prevent dictionary
// attacks.
const char kDnsOverHttpsSalt[] = "dns_over_https.salt";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Boolean that specifies whether additional DNS query types (e.g. HTTPS) may be
// queried alongside the traditional A and AAAA queries.
const char kAdditionalDnsQueryTypesEnabled[] =
    "async_dns.additional_dns_query_types_enabled";

// A pref holding the value of the policy used to explicitly allow or deny
// access to audio capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to audio capture devices
// is not allowed and no prompt will be shown.
// See also kAudioCaptureAllowedUrls.
const char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to audio
// capture devices without prompt.
const char kAudioCaptureAllowedUrls[] = "hardware.audio_capture_allowed_urls";

// A pref holding the value of the policy used to explicitly allow or deny
// access to video capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to video capture devices
// is not allowed and no prompt will be shown.
const char kVideoCaptureAllowed[] = "hardware.video_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to video
// capture devices without prompt.
const char kVideoCaptureAllowedUrls[] = "hardware.video_capture_allowed_urls";

// A pref holding the value of the policy used to explicitly allow or deny
// access to screen capture.  This includes all APIs that allow capturing
// the desktop, a window or a tab. When disabled, access to screen capture
// is not allowed and API calls will fail with an error, unless overriden by one
// of the "allowed" lists below.
const char kScreenCaptureAllowed[] = "hardware.screen_capture_enabled";

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
const char kScreenCaptureAllowedByOrigins[] =
    "hardware.screen_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// windows and tabs.
const char kWindowCaptureAllowedByOrigins[] =
    "hardware.window_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// tabs. Note that this will also allow capturing Windowed Chrome Apps.
const char kTabCaptureAllowedByOrigins[] =
    "hardware.tab_capture_allowed_by_origins";
// Sites matching the Origin patterns in this list will be permitted to capture
// tabs that have the same origin as themselves. Note that this will also allow
// capturing Windowed Chrome Apps with the same origin as the site.
const char kSameOriginTabCaptureAllowedByOrigins[] =
    "hardware.same_origin_tab_capture_allowed_by_origins";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// An integer pref that holds enum value of current demo mode configuration.
// Values are defined by DemoSession::DemoModeConfig enum.
const char kDemoModeConfig[] = "demo_mode.config";

// A string pref holding the value of the current country for demo sessions.
const char kDemoModeCountry[] = "demo_mode.country";

// A string pref holding the value of the retailer name input for demo sessions.
// This is now mostly called "retailer_name" in code other than in this pref and
// in Omaha request attributes
const char kDemoModeRetailerId[] = "demo_mode.retailer_id";

// A string pref holding the value of the store number input for demo sessions.
// This is now mostly called "store_number" in code other than in this pref and
// in Omaha request attributes
const char kDemoModeStoreId[] = "demo_mode.store_id";

// A string pref holding the value of the default locale for demo sessions.
const char kDemoModeDefaultLocale[] = "demo_mode.default_locale";

// Dictionary for transient storage of settings that should go into device
// settings storage before owner has been assigned.
const char kDeviceSettingsCache[] = "signed_settings_cache";

// The hardware keyboard layout of the device. This should look like
// "xkb:us::eng".
const char kHardwareKeyboardLayout[] = "intl.hardware_keyboard";

// A boolean pref of the auto-enrollment decision. Its value is only valid if
// it's not the default value; otherwise, no auto-enrollment decision has been
// made yet.
const char kShouldAutoEnroll[] = "ShouldAutoEnroll";

// A boolean pref of the private-set-membership decision. Its value is only
// valid if it's not the default value; otherwise, no private-set-membership
// decision has been made yet.
const char kShouldRetrieveDeviceState[] = "ShouldRetrieveDeviceState";

// An integer pref. Its valid values are defined in
// enterprise_management::DeviceRegisterRequest::PsmExecutionResult enum which
// indicates all possible PSM execution results in the Chrome OS enrollment
// flow.
const char kEnrollmentPsmResult[] = "EnrollmentPsmResult";

// An int64 pref to record the timestamp of PSM retrieving the device's
// determination successfully in the Chrome OS enrollment flow.
const char kEnrollmentPsmDeterminationTime[] = "EnrollmentPsmDeterminationTime";

// An integer pref with the maximum number of bits used by the client in a
// previous auto-enrollment request. If the client goes through an auto update
// during OOBE and reboots into a version of the OS with a larger maximum
// modulus, then it will retry auto-enrollment using the updated value.
const char kAutoEnrollmentPowerLimit[] = "AutoEnrollmentPowerLimit";

// The local state pref that stores device activity times before reporting
// them to the policy server.
const char kDeviceActivityTimes[] = "device_status.activity_times";

// A pref that stores user app activity times before reporting them to the
// policy server.
const char kAppActivityTimes[] = "device_status.app_activity_times";

// A pref that stores user activity times before reporting them to the policy
// server.
const char kUserActivityTimes[] = "consumer_device_status.activity_times";

// Copy of owner swap mouse buttons option to use on login screen.
const char kOwnerPrimaryMouseButtonRight[] = "owner.mouse.primary_right";

// Copy of owner tap-to-click option to use on login screen.
const char kOwnerTapToClickEnabled[] = "owner.touchpad.enable_tap_to_click";

// The length of device uptime after which an automatic reboot is scheduled,
// expressed in seconds.
const char kUptimeLimit[] = "automatic_reboot.uptime_limit";

// Whether an automatic reboot should be scheduled when an update has been
// applied and a reboot is required to complete the update process.
const char kRebootAfterUpdate[] = "automatic_reboot.reboot_after_update";

// An any-api scoped refresh token for enterprise-enrolled devices.  Allows
// for connection to Google APIs when the user isn't logged in.  Currently used
// for for getting a cloudprint scoped token to allow printing in Guest mode,
// Public Accounts and kiosks.
const char kDeviceRobotAnyApiRefreshToken[] =
    "device_robot_refresh_token.any-api";

// Device requisition for enterprise enrollment.
const char kDeviceEnrollmentRequisition[] = "enrollment.device_requisition";

// Sub organization for enterprise enrollment.
const char kDeviceEnrollmentSubOrganization[] = "enrollment.sub_organization";

// Whether to automatically start the enterprise enrollment step during OOBE.
const char kDeviceEnrollmentAutoStart[] = "enrollment.auto_start";

// Whether the user may exit enrollment.
const char kDeviceEnrollmentCanExit[] = "enrollment.can_exit";

// DM token fetched from the DM server during enrollment. Stored for Active
// Directory devices only.
const char kDeviceDMToken[] = "device_dm_token";

// Key name of a dictionary in local state to store cached multiprofle user
// behavior policy value.
const char kCachedMultiProfileUserBehavior[] = "CachedMultiProfileUserBehavior";

// A string pref with initial locale set in VPD or manifest.
const char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the device registered flag (second part after first login).
const char kDeviceRegistered[] = "DeviceRegistered";

// Boolean pref to signal corrupted enrollment to force the device through
// enrollment recovery flow upon next boot.
const char kEnrollmentRecoveryRequired[] = "EnrollmentRecoveryRequired";

// Pref name for whether we should show the Getting Started module in the Help
// app.
const char kHelpAppShouldShowGetStarted[] = "help_app.should_show_get_started";

// Pref name for whether we should show the Parental Control module in the Help
// app.
const char kHelpAppShouldShowParentalControl[] =
    "help_app.should_show_parental_control";

// Pref name for whether the device was in tablet mode when going through
// the OOBE.
const char kHelpAppTabletModeDuringOobe[] = "help_app.tablet_mode_during_oobe";

// A dictionary containing server-provided device state pulled form the cloud
// after recovery.
const char kServerBackedDeviceState[] = "server_backed_device_state";

// Customized wallpaper URL, which is already downloaded and scaled.
// The URL from this preference must never be fetched. It is compared to the
// URL from customization document to check if wallpaper URL has changed
// since wallpaper was cached.
const char kCustomizationDefaultWallpaperURL[] =
    "customization.default_wallpaper_url";

// System uptime, when last logout started.
// This is saved to file and cleared after chrome process starts.
const char kLogoutStartedLast[] = "chromeos.logout-started";

// A boolean preference controlling Android status reporting.
const char kReportArcStatusEnabled[] = "arc.status_reporting_enabled";

// A string preference indicating the name of the OS level task scheduler
// configuration to use.
const char kSchedulerConfiguration[] = "chromeos.scheduler_configuration";

// Dictionary indicating current network bandwidth throttling settings.
// Contains a boolean (is throttling enabled) and two integers (upload rate
// and download rate in kbits/s to throttle to)
const char kNetworkThrottlingEnabled[] = "net.throttling_enabled";

// Integer pref used by the metrics::DailyEvent owned by
// ash::PowerMetricsReporter.
const char kPowerMetricsDailySample[] = "power.metrics.daily_sample";

// Integer prefs used to back event counts reported by
// ash::PowerMetricsReporter.
const char kPowerMetricsIdleScreenDimCount[] =
    "power.metrics.idle_screen_dim_count";
const char kPowerMetricsIdleScreenOffCount[] =
    "power.metrics.idle_screen_off_count";
const char kPowerMetricsIdleSuspendCount[] = "power.metrics.idle_suspend_count";
const char kPowerMetricsLidClosedSuspendCount[] =
    "power.metrics.lid_closed_suspend_count";

// Key for list of users that should be reported.
const char kReportingUsers[] = "reporting_users";

// Whether to log events for Android app installs.
const char kArcAppInstallEventLoggingEnabled[] =
    "arc.app_install_event_logging_enabled";

// Whether we received the remove users remote command, and hence should proceed
// with removing the users while at the login screen.
const char kRemoveUsersRemoteCommand[] = "remove_users_remote_command";

// Integer pref used by the metrics::DailyEvent owned by
// ash::power::auto_screen_brightness::MetricsReporter.
const char kAutoScreenBrightnessMetricsDailySample[] =
    "auto_screen_brightness.metrics.daily_sample";

// Integer prefs used to back event counts reported by
// ash::power::auto_screen_brightness::MetricsReporter.
const char kAutoScreenBrightnessMetricsAtlasUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.atlas_user_adjustment_count";
const char kAutoScreenBrightnessMetricsEveUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.eve_user_adjustment_count";
const char kAutoScreenBrightnessMetricsNocturneUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.nocturne_user_adjustment_count";
const char kAutoScreenBrightnessMetricsKohakuUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.kohaku_user_adjustment_count";
const char kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.no_als_user_adjustment_count";
const char kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.supported_als_user_adjustment_count";
const char kAutoScreenBrightnessMetricsUnsupportedAlsUserAdjustmentCount[] =
    "auto_screen_brightness.metrics.unsupported_als_user_adjustment_count";

// Dictionary pref containing the configuration used to verify Parent Access
// Code. The data is sent through the ParentAccessCodeConfig policy, which is
// set for child users only, and kept on the known user storage.
const char kKnownUserParentAccessCodeConfig[] =
    "child_user.parent_access_code.config";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Whether there is a Flash version installed that supports clearing LSO data.
const char kClearPluginLSODataEnabled[] = "browser.clear_lso_data_enabled";

// Whether we should show Pepper Flash-specific settings.
const char kPepperFlashSettingsEnabled[] =
    "browser.pepper_flash_settings_enabled";

// String which specifies where to store the disk cache.
const char kDiskCacheDir[] = "browser.disk_cache_dir";
// Pref name for the policy specifying the maximal cache size.
const char kDiskCacheSize[] = "browser.disk_cache_size";

// Specifies the release channel that the device should be locked to.
// Possible values: "stable-channel", "beta-channel", "dev-channel", or an
// empty string, in which case the value will be ignored.
// TODO(dubroy): This preference may not be necessary once
// http://crosbug.com/17015 is implemented and the update engine can just
// fetch the correct value from the policy.
const char kChromeOsReleaseChannel[] = "cros.system.releaseChannel";

const char kPerformanceTracingEnabled[] =
    "feedback.performance_tracing_enabled";

// Indicates that factory reset was requested from options page or reset screen.
const char kFactoryResetRequested[] = "FactoryResetRequested";

// Indicates that when a factory reset is requested by setting
// |kFactoryResetRequested|, the user should only have the option to powerwash
// and cannot cancel the dialog otherwise.
const char kForceFactoryReset[] = "ForceFactoryReset";

// Presence of this value indicates that a TPM firmware update has been
// requested. The value indicates the requested update mode.
const char kFactoryResetTPMFirmwareUpdateMode[] =
    "FactoryResetTPMFirmwareUpdateMode";

// Indicates that debugging features were requested from oobe screen.
const char kDebuggingFeaturesRequested[] = "DebuggingFeaturesRequested";

// Indicates that the user has requested that ARC APK Sideloading be enabled.
const char kEnableAdbSideloadingRequested[] = "EnableAdbSideloadingRequested";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This setting controls initial device timezone that is used before user
// session started. It is controlled by device owner.
const char kSigninScreenTimezone[] = "settings.signin_screen_timezone";

// This setting controls what information is sent to the server to get
// device location to resolve time zone outside of user session. Values must
// match TimeZoneResolverManager::TimeZoneResolveMethod enum.
const char kResolveDeviceTimezoneByGeolocationMethod[] =
    "settings.resolve_device_timezone_by_geolocation_method";

// This is policy-controlled preference.
// It has values defined in policy enum
// SystemTimezoneAutomaticDetectionProto_AutomaticTimezoneDetectionType;
const char kSystemTimezoneAutomaticDetectionPolicy[] =
    "settings.resolve_device_timezone_by_geolocation_policy";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Pref name for the policy controlling whether to enable Media Router.
const char kEnableMediaRouter[] = "media_router.enable_media_router";
#if !BUILDFLAG(IS_ANDROID)
// Pref name for the policy controlling whether to force the Cast icon to be
// shown in the toolbar/overflow menu.
const char kShowCastIconInToolbar[] = "media_router.show_cast_icon_in_toolbar";
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Pref name for the policy controlling the way in which users are notified of
// the need to relaunch the browser for a pending update.
const char kRelaunchNotification[] = "browser.relaunch_notification";
// Pref name for the policy controlling the time period over which users are
// notified of the need to relaunch the browser for a pending update. Values
// are in milliseconds.
const char kRelaunchNotificationPeriod[] =
    "browser.relaunch_notification_period";
// Pref name for the policy controlling the time interval within which the
// relaunch should take place.
const char kRelaunchWindow[] = "browser.relaunch_window";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Pref name for the policy controlling the time period between the first user
// notification about need to relaunch and the end of the
// RelaunchNotificationPeriod. Values are in milliseconds.
const char kRelaunchHeadsUpPeriod[] = "browser.relaunch_heads_up_period";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
// Counts how many times prominent call-to-actions have occurred as part of the
// Mac restore permissions experiment. https://crbug.com/1211052
const char kMacRestoreLocationPermissionsExperimentCount[] =
    "mac_restore_location_permissions_experiment_count";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean indicating whether the Enrollment ID (EID) has already been uploaded
// to DM Server. Only used on Chromad devices. If this pref is true, the device
// is ready for the remote migration to cloud management.
const char kEnrollmentIdUploadedOnChromad[] = "chromad.enrollment_id_uploaded";

// base::Time value indicating the last timestamp when the
// ActiveDirectoryMigrationManager tried to trigger the migration. This device
// migration from AD management into cloud management starts with a powerwash.
// The goal of this pref is to avoid a loop of failed powerwash requests, by
// adding a backoff time of 1 day between retries.
const char kLastChromadMigrationAttemptTime[] =
    "chromad.last_migration_attempt_time";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// A list of base::Time value indicating the timestamps when hardware secure
// decryption was disabled due to errors or crashes. The implementation
// maintains a max size of the list (e.g. 2).
const char kHardwareSecureDecryptionDisabledTimes[] =
    "media.hardware_secure_decryption.disabled_times";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// A dictionary containing kiosk metrics latest session related information.
// For example, kiosk session start times, number of network drops.
// This setting resides in local state.
const char kKioskMetrics[] = "kiosk-metrics";

// A boolean pref which determines whether kiosk troubleshooting tools are
// enabled.
const char kKioskTroubleshootingToolsEnabled[] =
    "kiosk_troubleshooting_tools_enabled";

// A boolean pref which determines whether a Web Kiosk can open more than one
// browser window.
const char kNewWindowsInKioskAllowed[] = "new_windows_in_kiosk_allowed";
#endif  // BUILDFLAG(IS_CHROMEOS)

// *************** SERVICE PREFS ***************
// These are attached to the service process.

const char kCloudPrintRoot[] = "cloud_print";
const char kCloudPrintProxyEnabled[] = "cloud_print.enabled";
// The unique id for this instance of the cloud print proxy.
const char kCloudPrintProxyId[] = "cloud_print.proxy_id";
// The GAIA auth token for Cloud Print
const char kCloudPrintAuthToken[] = "cloud_print.auth_token";
// The email address of the account used to authenticate with the Cloud Print
// server.
const char kCloudPrintEmail[] = "cloud_print.email";
// Settings specific to underlying print system.
const char kCloudPrintPrintSystemSettings[] =
    "cloud_print.print_system_settings";
// A boolean indicating whether we should poll for print jobs when don't have
// an XMPP connection (false by default).
const char kCloudPrintEnableJobPoll[] = "cloud_print.enable_job_poll";
const char kCloudPrintRobotRefreshToken[] = "cloud_print.robot_refresh_token";
const char kCloudPrintRobotEmail[] = "cloud_print.robot_email";
// A boolean indicating whether we should connect to cloud print new printers.
const char kCloudPrintConnectNewPrinters[] =
    "cloud_print.user_settings.connectNewPrinters";
// A boolean indicating whether we should ping XMPP connection.
const char kCloudPrintXmppPingEnabled[] = "cloud_print.xmpp_ping_enabled";
// An int value indicating the average timeout between xmpp pings.
const char kCloudPrintXmppPingTimeout[] = "cloud_print.xmpp_ping_timeout_sec";
// Dictionary with settings stored by connector setup page.
const char kCloudPrintUserSettings[] = "cloud_print.user_settings";
// List of printers settings.
const char kCloudPrintPrinters[] = "cloud_print.user_settings.printers";
// A boolean indicating whether submitting jobs to Google Cloud Print is
// blocked by policy.
const char kCloudPrintSubmitEnabled[] = "cloud_print.submit_enabled";

// Preference to store proxy settings.
const char kMaxConnectionsPerProxy[] = "net.max_connections_per_proxy";

#if BUILDFLAG(IS_MAC)
// A boolean that tracks whether to show a notification when trying to quit
// while there are apps running.
const char kNotifyWhenAppsKeepChromeAlive[] =
    "apps.notify-when-apps-keep-chrome-alive";
#endif

// Set to true if background mode is enabled on this browser.
const char kBackgroundModeEnabled[] = "background_mode.enabled";

// Set to true if hardware acceleration mode is enabled on this browser.
const char kHardwareAccelerationModeEnabled[] =
    "hardware_acceleration_mode.enabled";

// Hardware acceleration mode from previous browser launch.
const char kHardwareAccelerationModePrevious[] =
    "hardware_acceleration_mode_previous";

// Integer that specifies the policy refresh rate for device-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
const char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";

#if !BUILDFLAG(IS_ANDROID)
// A boolean where true means that the browser has previously attempted to
// enable autoupdate and failed, so the next out-of-date browser start should
// not prompt the user to enable autoupdate, it should offer to reinstall Chrome
// instead.
const char kAttemptedToEnableAutoupdate[] =
    "browser.attempted_to_enable_autoupdate";

// The next media gallery ID to assign.
const char kMediaGalleriesUniqueId[] = "media_galleries.gallery_id";

// A list of dictionaries, where each dictionary represents a known media
// gallery.
const char kMediaGalleriesRememberedGalleries[] =
    "media_galleries.remembered_galleries";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kPolicyPinnedLauncherApps[] = "policy_pinned_launcher_apps";
// Keeps names of rolled default pin layouts for shelf in order not to apply
// this twice. Names are separated by comma.
const char kShelfDefaultPinLayoutRolls[] = "shelf_default_pin_layout_rolls";
// Same as kShelfDefaultPinLayoutRolls, but for tablet form factor devices.
const char kShelfDefaultPinLayoutRollsForTabletFormFactor[] =
    "shelf_default_pin_layout_rolls_for_tablet_form_factor";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// Counts how many more times the 'profile on a network share' warning should be
// shown to the user before the next silence period.
const char kNetworkProfileWarningsLeft[] = "network_profile.warnings_left";
// Tracks the time of the last shown warning. Used to reset
// |network_profile.warnings_left| after a silence period.
const char kNetworkProfileLastWarningTime[] =
    "network_profile.last_warning_time";

// The last Chrome version at which
// shell_integration::win::MigrateTaskbarPins() completed.
const char kShortcutMigrationVersion[] = "browser.shortcut_migration_version";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The RLZ brand code, if enabled.
const char kRLZBrand[] = "rlz.brand";
// Whether RLZ pings are disabled.
const char kRLZDisabled[] = "rlz.disabled";
// Keeps local state of app list while sync service is not available.
const char kAppListLocalState[] = "app_list.local_state";
const char kAppListPreferredOrder[] = "app_list.preferred_order";
#endif

// An integer that is incremented whenever changes are made to app shortcuts.
// Increasing this causes all app shortcuts to be recreated.
const char kAppShortcutsVersion[] = "apps.shortcuts_version";

// A string indicating the architecture in which app shortcuts have been
// created. If this changes (e.g, due to migrating one's home directory
// from an Intel mac to an ARM mac), then this will cause all shortcuts to be
// re-created.
const char kAppShortcutsArch[] = "apps.shortcuts_arch";

// This references a default content setting value which we expose through the
// preferences extensions API and also used for migration of the old
// |kEnableDRM| preference.
const char kProtectedContentDefault[] =
    "profile.default_content_setting_values.protected_media_identifier";

// An integer per-profile pref that signals if the watchdog extension is
// installed and active. We need to know if the watchdog extension active for
// ActivityLog initialization before the extension system is initialized.
const char kWatchdogExtensionActive[] =
    "profile.extensions.activity_log.num_consumers_active";

#if BUILDFLAG(IS_ANDROID)
// A list of partner bookmark rename/remove mappings.
// Each list item is a dictionary containing a "url", a "provider_title" and
// "mapped_title" entries, detailing the bookmark target URL (if any), the title
// given by the PartnerBookmarksProvider and either the user-visible renamed
// title or an empty string if the bookmark node was removed.
const char kPartnerBookmarkMappings[] = "partnerbookmarks.mappings";
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
const char kQuickCheckEnabled[] = "proxy.quick_check_enabled";

// Whether Guest Mode is enabled within the browser.
const char kBrowserGuestModeEnabled[] = "profile.browser_guest_enabled";

// Whether Guest Mode is enforced within the browser.
const char kBrowserGuestModeEnforced[] = "profile.browser_guest_enforced";

// Whether Adding a new Person is enabled within the user manager.
const char kBrowserAddPersonEnabled[] = "profile.add_person_enabled";

// Whether profile can be used before sign in.
const char kForceBrowserSignin[] = "profile.force_browser_signin";

// Whether profile picker is enabled, disabled or forced on startup.
const char kBrowserProfilePickerAvailabilityOnStartup[] =
    "profile.picker_availability_on_startup";

// Whether the profile picker has been shown at least once.
const char kBrowserProfilePickerShown[] = "profile.picker_shown";

// Whether to show the profile picker on startup or not.
const char kBrowserShowProfilePickerOnStartup[] =
    "profile.show_picker_on_startup";

// Boolean which indicates if the user is allowed to sign into Chrome on the
// next startup.
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";

// Boolean which indicate if signin interception is enabled.
const char kSigninInterceptionEnabled[] = "signin.interception_enabled";

#if BUILDFLAG(IS_CHROMEOS)
// A dictionary pref of the echo offer check flag. It sets offer info when
// an offer is checked.
const char kEchoCheckedOffers[] = "EchoCheckedOffers";

// Boolean pref indicating whether the user is allowed to create secondary
// profiles in Lacros browser. This is set by a policy, and the default value
// for managed users is false.
const char kLacrosSecondaryProfilesAllowed[] =
    "lacros_secondary_profiles_allowed";
// String pref indicating what to do when Lacros is disabled and we go back
// to using Ash.
const char kLacrosDataBackwardMigrationMode[] =
    "lacros_data_backward_migration_mode";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Device identifier used by CryptAuth stored in local state. This ID is
// combined with a user ID before being registered with the CryptAuth server,
// so it can't correlate users on the same device.
// Note: This constant was previously specific to EasyUnlock, so the string
//       constant contains "easy_unlock".
const char kCryptAuthDeviceId[] = "easy_unlock.device_id";

// The most recently retrieved Instance ID and Instance ID token for the app ID,
// "com.google.chrome.cryptauth", used by the CryptAuth client. These prefs are
// used to track how often (if ever) the Instance ID and Instance ID token
// rotate because CryptAuth assumes the Instance ID is static.
const char kCryptAuthInstanceId[] = "cryptauth.instance_id";
const char kCryptAuthInstanceIdToken[] = "cryptauth.instance_id_token";

// A dictionary that maps user id to hardlock state.
const char kEasyUnlockHardlockState[] = "easy_unlock.hardlock_state";

// A dictionary that maps user id to public part of RSA key pair used by
// Easy Sign-in for the user.
const char kEasyUnlockLocalStateTpmKeys[] = "easy_unlock.public_tpm_keys";

// A dictionary in local state containing each user's Easy Unlock profile
// preferences, so they can be accessed outside of the user's profile. The value
// is a dictionary containing an entry for each user. Each user's entry mirrors
// their profile's Easy Unlock preferences.
const char kEasyUnlockLocalStateUserPrefs[] = "easy_unlock.user_prefs";

// Boolean that indicates whether elevation is needed to recover Chrome upgrade.
const char kRecoveryComponentNeedsElevation[] =
    "recovery_component.needs_elevation";

#if !BUILDFLAG(IS_ANDROID)
// Boolean that indicates whether Chrome enterprise extension request is enabled
// or not.
const char kCloudExtensionRequestEnabled[] =
    "enterprise_reporting.extension_request.enabled";

// A list of extension ids represents pending extension request. The ids are
// stored once user sent the request until the request is canceled, approved or
// denied.
const char kCloudExtensionRequestIds[] =
    "enterprise_reporting.extension_request.ids";
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Policy that indicates how to handle animated images.
const char kAnimationPolicy[] = "settings.a11y.animation_policy";

// A list of URLs (for U2F) or domains (for webauthn) that automatically permit
// direct attestation of a Security Key.
const char kSecurityKeyPermitAttestation[] = "securitykey.permit_attestation";
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// In Lacros, these prefs store the expected value of the equivalent ash pref
// used by extensions. The values are sent to ash.

// A boolean pref which determines whether focus highlighting is enabled.
const char kLacrosAccessibilityFocusHighlightEnabled[] =
    "lacros.settings.a11y.focus_highlight";

// A boolean pref storing the enabled status of the Docked Magnifier feature.
const char kLacrosDockedMagnifierEnabled[] = "lacros.docked_magnifier.enabled";

// A boolean pref which determines whether autoclick is enabled.
const char kLacrosAccessibilityAutoclickEnabled[] =
    "lacros.settings.a11y.autoclick";

// A boolean pref which determines whether caret highlighting is enabled.
const char kLacrosAccessibilityCaretHighlightEnabled[] =
    "lacros.settings.a11y.caret_highlight";

// A boolean pref which determines whether custom cursor color is enabled.
const char kLacrosAccessibilityCursorColorEnabled[] =
    "lacros.settings.a11y.cursor_color_enabled";

// A boolean pref which determines whether cursor highlighting is enabled.
const char kLacrosAccessibilityCursorHighlightEnabled[] =
    "lacros.settings.a11y.cursor_highlight";

// A boolean pref which determines whether dictation is enabled.
const char kLacrosAccessibilityDictationEnabled[] =
    "lacros.settings.a11y.dictation";

// A boolean pref which determines whether high contrast is enabled.
const char kLacrosAccessibilityHighContrastEnabled[] =
    "lacros.settings.a11y.high_contrast_enabled";

// A boolean pref which determines whether the large cursor feature is enabled.
const char kLacrosAccessibilityLargeCursorEnabled[] =
    "lacros.settings.a11y.large_cursor_enabled";

// A boolean pref which determines whether screen magnifier is enabled.
// NOTE: We previously had prefs named settings.a11y.screen_magnifier_type and
// settings.a11y.screen_magnifier_type2, but we only shipped one type (full).
// See http://crbug.com/170850 for history.
const char kLacrosAccessibilityScreenMagnifierEnabled[] =
    "lacros.settings.a11y.screen_magnifier";

// A boolean pref which determines whether select-to-speak is enabled.
const char kLacrosAccessibilitySelectToSpeakEnabled[] =
    "lacros.settings.a11y.select_to_speak";

// A boolean pref which determines whether spoken feedback is enabled.
const char kLacrosAccessibilitySpokenFeedbackEnabled[] =
    "lacros.settings.accessibility";

// A boolean pref which determines whether the sticky keys feature is enabled.
const char kLacrosAccessibilityStickyKeysEnabled[] =
    "lacros.settings.a11y.sticky_keys_enabled";

// A boolean pref which determines whether Switch Access is enabled.
const char kLacrosAccessibilitySwitchAccessEnabled[] =
    "lacros.settings.a11y.switch_access.enabled";

// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
const char kLacrosAccessibilityVirtualKeyboardEnabled[] =
    "lacros.settings.a11y.virtual_keyboard";
#endif

const char kAllowDinosaurEasterEgg[] = "allow_dinosaur_easter_egg";

#if BUILDFLAG(IS_ANDROID)
// The latest version of Chrome available when the user clicked on the update
// menu item.
const char kLatestVersionWhenClickedUpdateMenuItem[] =
    "omaha.latest_version_when_clicked_upate_menu_item";
#endif

#if BUILDFLAG(IS_ANDROID)
// The serialized timestamps of latest shown merchant viewer messages.
const char kCommerceMerchantViewerMessagesShownTime[] =
    "commerce_merchant_viewer_messages_shown_time";
#endif

// A dictionary which stores whether location access is enabled for the current
// default search engine. Deprecated for kDSEPermissionsSetting.
const char kDSEGeolocationSettingDeprecated[] = "dse_geolocation_setting";

// A dictionary which stores the geolocation and notifications content settings
// for the default search engine before it became the default search engine so
// that they can be restored if the DSE is ever changed.
const char kDSEPermissionsSettings[] = "dse_permissions_settings";

// A boolean indicating whether the DSE was previously disabled by enterprise
// policy.
const char kDSEWasDisabledByPolicy[] = "dse_was_disabled_by_policy";

// A dictionary of manifest URLs of Web Share Targets to a dictionary containing
// attributes of its share_target field found in its manifest. Each key in the
// dictionary is the name of the attribute, and the value is the corresponding
// value.
const char kWebShareVisitedTargets[] = "profile.web_share.visited_targets";

#if BUILDFLAG(IS_WIN)
// Acts as a cache to remember incompatible applications through restarts. Used
// for the Incompatible Applications Warning feature.
const char kIncompatibleApplications[] = "incompatible_applications";

// Contains the MD5 digest of the current module blacklist cache. Used to detect
// external tampering.
const char kModuleBlocklistCacheMD5Digest[] =
    "module_blocklist_cache_md5_digest";

// A boolean value, controlling whether third party software is allowed to
// inject into Chrome's processes.
const char kThirdPartyBlockingEnabled[] = "third_party_blocking_enabled";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
// A boolean value, controlling whether Chrome renderer processes have the CIG
// mitigation enabled.
const char kRendererCodeIntegrityEnabled[] = "renderer_code_integrity_enabled";

// A boolean value, controlling whether Chrome renderer processes should have
// Renderer App Container enabled or not. If this pref is set to false then
// Renderer App Container is disabled, otherwise Renderer App Container is
// controlled by the `RendererAppContainer` feature owned by sandbox/policy.
const char kRendererAppContainerEnabled[] = "renderer_app_container_enabled";

// A boolean that controls whether the Browser process has
// ProcessExtensionPointDisablePolicy enabled.
const char kBlockBrowserLegacyExtensionPoints[] =
    "block_browser_legacy_extension_points";
#endif  // BUILDFLAG(IS_WIN)

// An integer that keeps track of prompt waves for the settings reset
// prompt. Users will be prompted to reset settings at most once per prompt wave
// for each setting that the prompt targets (default search, startup URLs and
// homepage). The value is obtained via a feature parameter. When the stored
// value is different from the feature parameter, a new prompt wave begins.
const char kSettingsResetPromptPromptWave[] =
    "settings_reset_prompt.prompt_wave";

// Timestamp of the last time the settings reset prompt was shown during the
// current prompt wave asking the user if they want to restore their search
// engine.
const char kSettingsResetPromptLastTriggeredForDefaultSearch[] =
    "settings_reset_prompt.last_triggered_for_default_search";

// Timestamp of the last time the settings reset prompt was shown during the
// current prompt wave asking the user if they want to restore their startup
// settings.
const char kSettingsResetPromptLastTriggeredForStartupUrls[] =
    "settings_reset_prompt.last_triggered_for_startup_urls";

// Timestamp of the last time the settings reset prompt was shown during the
// current prompt wave asking the user if they want to restore their homepage.
const char kSettingsResetPromptLastTriggeredForHomepage[] =
    "settings_reset_prompt.last_triggered_for_homepage";

#if BUILDFLAG(IS_ANDROID)
// Timestamp of the clipboard's last modified time, stored in base::Time's
// internal format (int64) in local store.  (I.e., this is not a per-profile
// pref.)
const char kClipboardLastModifiedTime[] = "ui.clipboard.last_modified_time";
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)

// The following set of Prefs is used by OfflineMetricsCollectorImpl to
// backup the current Chrome usage tracking state and accumulated counters
// of days with specific Chrome usage.

// The boolean flags indicating whether the specific activity was observed
// in Chrome during the day that started at |kOfflineUsageTrackingDay|. These
// are used to track usage of Chrome is used while offline and how various
// offline features affect that.
const char kOfflineUsageStartObserved[] = "offline_pages.start_observed";
const char kOfflineUsageOnlineObserved[] = "offline_pages.online_observed";
const char kOfflineUsageOfflineObserved[] = "offline_pages.offline_observed";
// Boolean flags indicating state of a prefetch subsystem during a day.
const char kPrefetchUsageEnabledObserved[] =
    "offline_pages.prefetch_enabled_observed";
const char kPrefetchUsageFetchObserved[] =
    "offline_pages.prefetch_fetch_observed";
const char kPrefetchUsageOpenObserved[] =
    "offline_pages.prefetch_open_observed";
// A time corresponding to a midnight that starts the day for which
// OfflineMetricsCollector tracks the Chrome usage. Once current time passes
// 24hrs from this point, the further tracking is attributed to the next day.
const char kOfflineUsageTrackingDay[] = "offline_pages.tracking_day";
// Accumulated counters of days with specified Chrome usage. When there is
// likely a network connection, these counters are reported via UMA and reset.
const char kOfflineUsageUnusedCount[] = "offline_pages.unused_count";
const char kOfflineUsageStartedCount[] = "offline_pages.started_count";
const char kOfflineUsageOfflineCount[] = "offline_pages.offline_count";
const char kOfflineUsageOnlineCount[] = "offline_pages.online_count";
const char kOfflineUsageMixedCount[] = "offline_pages.mixed_count";
// Accumulated counters of days with specified Prefetch usage. When there is
// likely a network connection, these counters are reported via UMA and reset.
const char kPrefetchUsageEnabledCount[] =
    "offline_pages.prefetch_enabled_count";
const char kPrefetchUsageFetchedCount[] =
    "offline_pages.prefetch_fetched_count";
const char kPrefetchUsageOpenedCount[] = "offline_pages.prefetch_opened_count";
const char kPrefetchUsageMixedCount[] = "offline_pages.prefetch_mixed_count";

#endif

// Stores the Media Engagement Index schema version. If the stored value
// is lower than the value in MediaEngagementService then the MEI data
// will be wiped.
const char kMediaEngagementSchemaVersion[] = "media.engagement.schema_version";

// Maximum number of tabs that has been opened since the last time it has been
// reported.
const char kTabStatsTotalTabCountMax[] = "tab_stats.total_tab_count_max";

// Maximum number of tabs that has been opened in a single window since the last
// time it has been reported.
const char kTabStatsMaxTabsPerWindow[] = "tab_stats.max_tabs_per_window";

// Maximum number of windows that has been opened since the last time it has
// been reported.
const char kTabStatsWindowCountMax[] = "tab_stats.window_count_max";

//  Timestamp of the last time the tab stats daily metrics have been reported.
const char kTabStatsDailySample[] = "tab_stats.last_daily_sample";

// Discards/Reloads since last daily report.
const char kTabStatsDiscardsExternal[] = "tab_stats.discards_external";
const char kTabStatsDiscardsUrgent[] = "tab_stats.discards_urgent";
const char kTabStatsDiscardsProactive[] = "tab_stats.discards_proactive";
const char kTabStatsReloadsExternal[] = "tab_stats.reloads_external";
const char kTabStatsReloadsUrgent[] = "tab_stats.reloads_urgent";
const char kTabStatsReloadsProactive[] = "tab_stats.reloads_proactive";

// A list of origins (URLs) to treat as "secure origins" for debugging purposes.
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely_treat_insecure_origin_as_secure";

// A list of origins (URLs) that specifies opting into --isolate-origins=...
// (selective Site Isolation).
const char kIsolateOrigins[] = "site_isolation.isolate_origins";

// Boolean that specifies opting into --site-per-process (full Site Isolation).
const char kSitePerProcess[] = "site_isolation.site_per_process";

#if !BUILDFLAG(IS_ANDROID)
// Boolean to allow SharedArrayBuffer in non-crossOriginIsolated contexts.
// TODO(crbug.com/1144104) Remove when migration to COOP+COEP is complete.
const char kSharedArrayBufferUnrestrictedAccessAllowed[] =
    "profile.shared_array_buffer_unrestricted_access_allowed";

// Boolean that specifies whether media (audio/video) autoplay is allowed.
const char kAutoplayAllowed[] = "media.autoplay_allowed";

// Holds URL patterns that specify URLs that will be allowed to autoplay.
const char kAutoplayAllowlist[] = "media.autoplay_whitelist";

// Boolean that specifies whether autoplay blocking is enabled.
const char kBlockAutoplayEnabled[] = "media.block_autoplay";
#endif  // !BUILDFLAG(IS_ANDROID)

// Boolean allowing Chrome to block external protocol navigation in sandboxed
// iframes.
const char kSandboxExternalProtocolBlocked[] =
    "profile.sandbox_external_protocol_blocked";

#if BUILDFLAG(IS_LINUX)
// Boolean that indicates if system notifications are allowed to be used in
// place of Chrome notifications.
const char kAllowSystemNotifications[] = "system_notifications.allowed";
#endif  // BUILDFLAG(IS_LINUX)

// Integer that holds the value of the next persistent notification ID to be
// used.
const char kNotificationNextPersistentId[] = "persistent_notifications.next_id";

// Time that holds the value of the next notification trigger timestamp.
const char kNotificationNextTriggerTime[] =
    "persistent_notifications.next_trigger";

// Preference for controlling whether tab freezing is enabled.
const char kTabFreezingEnabled[] = "tab_freezing_enabled";

// Boolean that enables the Enterprise Hardware Platform Extension API for
// extensions installed by enterprise policy.
const char kEnterpriseHardwarePlatformAPIEnabled[] =
    "enterprise_hardware_platform_api.enabled";

// Boolean that specifies whether Signed HTTP Exchange (SXG) loading is enabled.
const char kSignedHTTPExchangeEnabled[] = "web_package.signed_exchange.enabled";

#if BUILDFLAG(IS_CHROMEOS)
// Enum that specifies client certificate management permissions for user. It
// can have one of the following values.
// 0: Users can manage all certificates.
// 1: Users can manage user certificates, but not device certificates.
// 2: Disallow users from managing certificates
// Controlled by ClientCertificateManagementAllowed policy.
const char kClientCertificateManagementAllowed[] =
    "client_certificate_management_allowed";

// Enum that specifies CA certificate management permissions for user. It
// can have one of the following values.
// 0: Users can manage all certificates.
// 1: Users can manage user certificates, but not built-in certificates.
// 2: Disallow users from managing certificates
// Controlled by CACertificateManagementAllowed policy.
const char kCACertificateManagementAllowed[] =
    "ca_certificate_management_allowed";
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
// Boolean that specifies whether the Chrome Root Store and built-in
// certificate verifier should be used. If false, Chrome will not use the
// Chrome Root Store.
// If not set, Chrome will choose the root store based on experiments.
const char kChromeRootStoreEnabled[] = "chrome_root_store_enabled";
#endif

const char kSharingVapidKey[] = "sharing.vapid_key";
const char kSharingFCMRegistration[] = "sharing.fcm_registration";
const char kSharingLocalSharingInfo[] = "sharing.local_sharing_info";

#if !BUILDFLAG(IS_ANDROID)
// Dictionary that contains all of the Hats Survey Metadata.
const char kHatsSurveyMetadata[] = "hats.survey_metadata";
#endif  // !BUILDFLAG(IS_ANDROID)

const char kExternalProtocolDialogShowAlwaysOpenCheckbox[] =
    "external_protocol_dialog.show_always_open_checkbox";

// List of dictionaries. For each dictionary, key "protocol" is a protocol
// (as a string) that is permitted by policy to launch an external application
// without prompting the user. Key "allowed_origins" is a nested list of origin
// patterns that defines the scope of applicability of that protocol. If the
// "allow" list is empty, that protocol rule will never apply.
const char kAutoLaunchProtocolsFromOrigins[] =
    "protocol_handler.policy.auto_launch_protocols_from_origins";

// This pref enables the ScrollToTextFragment feature.
const char kScrollToTextFragmentEnabled[] = "scroll_to_text_fragment_enabled";

#if BUILDFLAG(IS_ANDROID)
// Last time the known interception disclosure message was dismissed. Used to
// ensure a cooldown period passes before the disclosure message is displayed
// again.
const char kKnownInterceptionDisclosureInfobarLastShown[] =
    "known_interception_disclosure_infobar_last_shown";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kRequiredClientCertificateForUser[] =
    "required_client_certificate_for_user";
const char kRequiredClientCertificateForDevice[] =
    "required_client_certificate_for_device";
const char kCertificateProvisioningStateForUser[] =
    "cert_provisioning_user_state";
const char kCertificateProvisioningStateForDevice[] =
    "cert_provisioning_device_state";
#endif
// A boolean pref that enables certificate prompts when multiple certificates
// match the auto-selection policy. This pref is controlled exclusively by
// policies (PromptOnMultipleMatchingCertificates or, in the sign-in profile,
// DeviceLoginScreenPromptOnMultipleMatchingCertificates).
const char kPromptOnMultipleMatchingCertificates[] =
    "prompt_on_multiple_matching_certificates";

// This pref enables periodically fetching new Media Feed items for top feeds.
const char kMediaFeedsBackgroundFetching[] =
    "media_feeds_background_fetching_enabled";

// This pref enables checking of Media Feed items against the Safe Search API.
const char kMediaFeedsSafeSearchEnabled[] = "media_feeds_safe_search_enabled";

// This pref enables automated selection of Media Feeds to fetch.
const char kMediaFeedsAutoSelectEnabled[] = "media_feeds_auto_select_enabled";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref indicating whether the notification informing the user that
// adb sideloading had been disabled by their admin was shown.
const char kAdbSideloadingDisallowedNotificationShown[] =
    "adb_sideloading_disallowed_notification_shown";
// Int64 pref indicating the time in microseconds since Windows epoch
// (1601-01-01 00:00:00 UTC) when the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
// If the notification was not yet shown the pref holds the value Time::Min().
const char kAdbSideloadingPowerwashPlannedNotificationShownTime[] =
    "adb_sideloading_powerwash_planned_notification_shown_time";
// Boolean pref indicating whether the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
const char kAdbSideloadingPowerwashOnNextRebootNotificationShown[] =
    "adb_sideloading_powerwash_on_next_reboot_notification_shown";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Boolean pref that indicates whether caret browsing is currently enabled.
const char kCaretBrowsingEnabled[] = "settings.a11y.caretbrowsing.enabled";

// Boolean pref for whether the user is shown a dialog to confirm that caret
// browsing should be enabled/disabled when the keyboard shortcut is pressed.
// If set to false, no intervening dialog is displayed and caret browsing mode
// is toggled silently by the keyboard shortcut.
const char kShowCaretBrowsingDialog[] =
    "settings.a11y.caretbrowsing.show_dialog";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enum pref indicating how to launch the Lacros browser. It is managed by
// LacrosAvailability policy and can have one of the following values:
// 0: User choice (default value).
// 1: Lacros is disallowed.
// 2: Lacros is enabled but not the pimary browser.
// 3: Lacros is enabled as the primary browser.
// 4: Lacros is the only available browser.
const char kLacrosLaunchSwitch[] = "lacros_launch_switch";

// Enum pref indicating which Lacros browser to launch: rootfs or stateful. It
// is managed by LacrosSelection policy and can have one of the following
// values:
// 0: User choice (default value).
// 1: Always load rootfs Lacros.
// 2: Always load stateful Lacros.
const char kLacrosSelection[] = "lacros_selection";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// String enum pref determining what should happen when a user who authenticates
// via a security token is removing this token. "IGNORE" - nothing happens
// (default). "LOGOUT" - The user is logged out. "LOCK" - The session is locked.
const char kSecurityTokenSessionBehavior[] = "security_token_session_behavior";
// When the above pref is set to "LOGOUT" or "LOCK", this integer pref
// determines the duration of a notification that appears when the smart card is
// removed. The action will only happen after the notification timed out. If
// this pref is set to 0, the action happens immediately.
const char kSecurityTokenSessionNotificationSeconds[] =
    "security_token_session_notification_seconds";
// This string pref is set when the notification after the action mentioned
// above is about to be displayed. It contains the domain that manages the user
// who was logged out, to be used as part of the notification message.
const char kSecurityTokenSessionNotificationScheduledDomain[] =
    "security_token_session_notification_scheduled";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Boolean pref indicating whether user has hidden the cart module on NTP.
const char kCartModuleHidden[] = "cart_module_hidden";
// An integer that keeps track of how many times welcome surface has shown in
// cart module.
const char kCartModuleWelcomeSurfaceShownTimes[] =
    "cart_module_welcome_surface_shown_times";
// Boolean pref indicating whether user has reacted to the consent for
// rule-based discount in cart module.
const char kCartDiscountAcknowledged[] = "cart_discount_acknowledged";
// Boolean pref indicating whether user has enabled rule-based discount in cart
// module.
const char kCartDiscountEnabled[] = "cart_discount_enabled";
// Map pref recording the discounts used by users.
const char kCartUsedDiscounts[] = "cart_used_discounts";
// A time pref indicating the timestamp of when last cart discount fetch
// happened.
const char kCartDiscountLastFetchedTime[] = "cart_discount_last_fetched_time";
// Boolean pref indicating whether the consent for discount has ever shown or
// not.
const char kCartDiscountConsentShown[] = "cart_discount_consent_shown";
// Integer pref indicating in which variation the user has made their decision,
// accept or reject the consent.
const char kDiscountConsentDecisionMadeIn[] =
    "discount_consent_decision_made_in";
// Integer pref indicating in which variation the user has dismissed the
// consent. Only the Inline and Dialog variation applies.
const char kDiscountConsentDismissedIn[] = "discount_consent_dismissed_in";
// A time pref indicating the timestamp of when user last explicitly dismissed
// the discount consent.
const char kDiscountConsentLastDimissedTime[] =
    "discount_consent_last_dimissed_time";
// Integer pref indicating the last consent was shown in which variation.
const char kDiscountConsentLastShownInVariation[] =
    "discount_consent_last_shown_in";
// An integer pref that keeps track of how many times user has explicitly
// dismissed the disount consent.
const char kDiscountConsentPastDismissedCount[] =
    "discount_consent_dismissed_count";
// Boolean pref indicating whether the user has shown interest in the consent,
// e.g. if the use has clicked the 'continue' button.
const char kDiscountConsentShowInterest[] = "discount_consent_show_interest";
// Integer pref indicating in which variation the user has shown interest to the
// consent, they has clicked the 'continue' button.
const char kDiscountConsentShowInterestIn[] =
    "discount_consent_show_interest_in";
#endif

#if BUILDFLAG(IS_ANDROID)
// Boolean pref controlling whether immersive AR sessions are enabled
// in WebXR Device API.
const char kWebXRImmersiveArEnabled[] = "webxr.immersive_ar_enabled";
#endif

#if !BUILDFLAG(IS_ANDROID)
// The duration for keepalive requests on browser shutdown.
const char kFetchKeepaliveDurationOnShutdown[] =
    "fetch_keepalive_duration_on_shutdown";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref to control whether to enable annotation mode in the PDF viewer
// or not.
const char kPdfAnnotationsEnabled[] = "pdf.enable_annotations";
#endif

// A comma-separated list of ports on which outgoing connections will be
// permitted even if they would otherwise be blocked.
const char kExplicitlyAllowedNetworkPorts[] =
    "net.explicitly_allowed_network_ports";

#if !BUILDFLAG(IS_ANDROID)
// Pref name for whether force-installed web apps (origins) are able to query
// device attributes.
const char kDeviceAttributesAllowedForOrigins[] =
    "policy.device_attributes_allowed_for_origins";
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// A boolean indicating whether the desktop sharing hub is enabled by enterprise
// policy.
const char kDesktopSharingHubEnabled[] =
    "sharing_hub.desktop_sharing_hub_enabled";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Pref name for the last major version where the What's New page was
// successfully shown.
const char kLastWhatsNewVersion[] = "browser.last_whats_new_version";
// A boolean indicating whether the Lens Region search feature should be enabled
// if supported.
const char kLensRegionSearchEnabled[] = "policy.lens_region_search_enabled";
// A boolean indicating whether the Lens NTP searchbox feature should be enabled
// if supported.
const char kLensDesktopNTPSearchEnabled[] =
    "policy.lens_desktop_ntp_search_enabled";
#endif

// A boolean indicating whether the Privacy guide feature has been viewed. This
// is set to true if the user has done any of the following: (1) opened the
// privacy guide, (2) dismissed the privacy guide promo, (3) seen the privacy
// guide promo a certain number of times.
const char kPrivacyGuideViewed[] = "privacy_guide.viewed";

// A boolean indicating support of "CORS non-wildcard request header name".
// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
const char kCorsNonWildcardRequestHeadersSupport[] =
    "cors_non_wildcard_request_headers_support";

// A boolean indicating whether documents are allowed to be assigned to
// origin-keyed agent clusters by default (i.e., when the Origin-Agent-Cluster
// header is absent). When true, Chromium may enable this behavior based on
// feature settings. When false, site-keyed agent clusters will continue to be
// used by default.
const char kOriginAgentClusterDefaultEnabled[] =
    "origin_agent_cluster_default_enabled";

// An integer count of how many SCT Auditing hashdance reports have ever been
// sent by this client, across all profiles.
const char kSCTAuditingHashdanceReportCount[] =
    "sct_auditing.hashdance_report_count";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kConsumerAutoUpdateToggle[] = "settings.consumer_auto_update_toggle";

// A boolean pref that controls whether or not Hindi Inscript keyboard layout
// is available.
// This is set by a user policy, but the user policy does not work to
// control the availability of the Hindi Inscript layout.
// TODO(jungshik): Deprecate it.
const char kHindiInscriptLayoutEnabled[] =
    "settings.input.hindi_inscript_layout_enabled";
// This is set by a device policy and does actually work.
const char kDeviceHindiInscriptLayoutEnabled[] =
    "settings.input.device_hindi_inscript_layout_enabled";
#endif

#if !BUILDFLAG(IS_ANDROID)
// An integer count of how many times the user has seen the high efficiency mode
// page action chip in the expanded size.
const char kHighEfficiencyChipExpandedCount[] =
    "high_efficiency.chip_expanded_count";

// A boolean indicating whether the price track first user experience bubble
// should show. This is set to false if the user has clicked the "Price track"
// button in the FUE bubble once.
const char kShouldShowPriceTrackFUEBubble[] =
    "should_show_price_track_fue_bubble_fue";

// A boolean indicating whether we should show the bookmark tab for the next
// side panel opening. Right now this is only used by Price Tracking feature
// to show the bookmark tab (which contains the price tracking list) after
// IPH.
const char kShouldShowSidePanelBookmarkTab[] =
    "should_show_side_panel_bookmark_tab";
#endif

const char kStrictMimetypeCheckForWorkerScriptsEnabled[] =
    "strict_mime_type_check_for_worker_scripts_enabled";

#if BUILDFLAG(IS_ANDROID)
// If true, the virtual keyboard will resize the layout viewport by default.
// Has no effect otherwise.
const char kVirtualKeyboardResizesLayoutByDefault[] =
    "virtual_keyboard_resizes_layout_by_default";
#endif  // BUILDFLAG(IS_ANDROID)

// A boolean indicating whether Access-Control-Allow-Methods matching in CORS
// preflights is fixed according to the spec. https://crbug.com/1228178
const char kAccessControlAllowMethodsInCORSPreflightSpecConformant[] =
    "access_control_allow_methods_in_cors_preflight_spec_conformant";

// A time preference keeping track of the last time the DIPS service performed
// DIPS-related repeated actions (logging metrics, clearing state, etc).
const char kDIPSTimerLastUpdate[] = "dips_timer_last_update";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A dictionary that keeps client_ids assigned by Authorization Servers indexed
// by URLs of these servers. It does not contain empty strings.
const char kPrintingOAuth2AuthorizationServers[] =
    "printing.oauth2_authorization_servers";
#endif

// If true, the feature ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes
// will be allowed, otherwise attempts to enable the feature will be
// disallowed.
const char kThrottleNonVisibleCrossOriginIframesAllowed[] =
    "throttle_non_visible_cross_origin_iframes_allowed";

// If true, the feature NewBaseUrlInheritanceBehavior will be allowed, otherwise
// attempts to enable the feature will be disallowed.
const char kNewBaseUrlInheritanceBehaviorAllowed[] =
    "new_base_url_inheritance_behavior_allowed";

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
const char kOutOfProcessSystemDnsResolutionEnabled[] =
    "net.out_of_process_system_dns_resolution_enabled";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

}  // namespace prefs
