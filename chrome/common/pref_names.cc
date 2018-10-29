// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// A bool pref that indicates whether interventions for abusive experiences
// should be enforced.
const char kAbusiveExperienceInterventionEnforce[] =
    "abusive_experience_intervention_enforce";

// A bool pref that keeps whether the child status for this profile was already
// successfully checked via ChildAccountService.
const char kChildAccountStatusKnown[] = "child_account_status_known";

// A string property indicating whether default apps should be installed
// in this profile.  Use the value "install" to enable defaults apps, or
// "noinstall" to disable them.  This property is usually set in the
// master_preferences and copied into the profile preferences on first run.
// Defaults apps are installed only when creating a new profile.
const char kDefaultApps[] = "default_apps";

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
const char kDownloadRestrictions[] = "download_restrictions";

// If set to true profiles are created in ephemeral mode and do not store their
// data in the profile folder on disk but only in memory.
const char kForceEphemeralProfiles[] = "profile.ephemeral_mode";

// A boolean specifying whether the New Tab page is the home page or not.
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const char kHomePage[] = "homepage";

// Stores information about the important sites dialog, including the time and
// frequency it has been ignored.
const char kImportantSitesDialogHistory[] = "important_sites_dialog";

#if defined(OS_WIN)
// This is a timestamp of the last time this profile was reset by a third party
// tool. On Windows, a third party tool may set a registry value that will be
// compared to this value and if different will result in a profile reset
// prompt. See triggered_profile_resetter.h for more information.
const char kLastProfileResetTimestamp[] = "profile.last_reset_timestamp";

// A boolean indicating if settings should be reset for this profile once a
// run of the Chrome Cleanup Tool has completed.
const char kChromeCleanerResetPending[] = "chrome_cleaner.reset_pending";
#endif

// The URL to open the new tab page to. Only set by Group Policy.
const char kNewTabPageLocationOverride[] = "newtab_page_location_override";

// An integer that keeps track of the profile icon version. This allows us to
// determine the state of the profile icon for icon format changes.
const char kProfileIconVersion[] = "profile.icon_version";

// Used to determine if the last session exited cleanly. Set to false when
// first opened, and to true when closing. On startup if the value is false,
// it means the profile didn't exit cleanly.
// DEPRECATED: this is replaced by kSessionExitType and exists for backwards
// compatibility.
const char kSessionExitedCleanly[] = "profile.exited_cleanly";

// A string pref whose values is one of the values defined by
// |ProfileImpl::kPrefExitTypeXXX|. Set to |kPrefExitTypeCrashed| on startup and
// one of |kPrefExitTypeNormal| or |kPrefExitTypeSessionEnded| during
// shutdown. Used to determine the exit type the last time the profile was open.
const char kSessionExitType[] = "profile.exit_type";

// Stores the total amount of observed active session time for the user while
// in-product help is active. Observed time is active session time in seconds.
const char kObservedSessionTime[] = "profile.observed_session_time";

// Stores counts and timestamps of SSL certificate errors that have occurred.
// When the same error recurs within some period of time, a message is added to
// the SSL interstitial.
const char kRecurrentSSLInterstitial[] = "profile.ssl_recurrent_interstitial";

// The last time that the site engagement service recorded an engagement event
// for this profile for any URL. Recorded only during shutdown. Used to prevent
// the service from decaying engagement when a user does not use Chrome at all
// for an extended period of time.
const char kSiteEngagementLastUpdateTime[] = "profile.last_engagement_time";

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

// Stores the email address associated with the google account of the custodian
// of the supervised user, set when the supervised user is created.
const char kSupervisedUserCustodianEmail[] = "profile.managed.custodian_email";

// Stores the display name associated with the google account of the custodian
// of the supervised user, updated (if possible) each time the supervised user
// starts a session.
const char kSupervisedUserCustodianName[] = "profile.managed.custodian_name";

// Stores the URL of the profile image associated with the google account of the
// custodian of the supervised user.
const char kSupervisedUserCustodianProfileImageURL[] =
    "profile.managed.custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// custodian of the supervised user.
const char kSupervisedUserCustodianProfileURL[] =
    "profile.managed.custodian_profile_url";

// Maps host names to whether the host is manually allowed or blocked.
const char kSupervisedUserManualHosts[] = "profile.managed.manual_hosts";

// Maps URLs to whether the URL is manually allowed or blocked.
const char kSupervisedUserManualURLs[] = "profile.managed.manual_urls";

// Maps extension ids to the approved version of this extension for a
// supervised user. Missing extensions are not approved.
const char kSupervisedUserApprovedExtensions[] =
    "profile.managed.approved_extensions";

// Stores whether the SafeSites filter is enabled.
const char kSupervisedUserSafeSites[] = "profile.managed.safe_sites";

// Stores the email address associated with the google account of the secondary
// custodian of the supervised user, set when the supervised user is created.
const char kSupervisedUserSecondCustodianEmail[] =
    "profile.managed.second_custodian_email";

// Stores the display name associated with the google account of the secondary
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
const char kSupervisedUserSecondCustodianName[] =
    "profile.managed.second_custodian_name";

// Stores the URL of the profile image associated with the google account of the
// secondary custodian of the supervised user.
const char kSupervisedUserSecondCustodianProfileImageURL[] =
    "profile.managed.second_custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// secondary custodian of the supervised user.
const char kSupervisedUserSecondCustodianProfileURL[] =
    "profile.managed.second_custodian_profile_url";

// Stores settings that can be modified both by a supervised user and their
// manager. See SupervisedUserSharedSettingsService for a description of
// the format.
const char kSupervisedUserSharedSettings[] = "profile.managed.shared_settings";

// A dictionary storing whitelists for a supervised user. The key is the CRX ID
// of the whitelist, the value a dictionary containing whitelist properties
// (currently the name).
const char kSupervisedUserWhitelists[] = "profile.managed.whitelists";

#if BUILDFLAG(ENABLE_RLZ)
// Integer. RLZ ping delay in seconds.
const char kRlzPingDelaySeconds[] = "rlz_ping_delay";
#endif  // BUILDFLAG(ENABLE_RLZ)

#if defined(OS_CHROMEOS)
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

// The value to use for Accept-Languages HTTP header when making an HTTP
// request.
const char kAcceptLanguages[] = "intl.accept_languages";

// If these change, the corresponding enums in the extension API
// experimental.fontSettings.json must also change.
const char* const kWebKitScriptsForFontFamilyMaps[] = {
#define EXPAND_SCRIPT_FONT(x, script_name) script_name ,
#include "chrome/common/pref_font_script_names-inl.h"
ALL_FONT_SCRIPTS("unused param")
#undef EXPAND_SCRIPT_FONT
};

const size_t kWebKitScriptsForFontFamilyMapsLength =
    arraysize(kWebKitScriptsForFontFamilyMaps);

// Strings for WebKit font family preferences. If these change, the pref prefix
// in pref_names_util.cc and the pref format in font_settings_api.cc must also
// change.
const char kWebKitStandardFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_STANDARD;
const char kWebKitFixedFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_FIXED;
const char kWebKitSerifFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_SERIF;
const char kWebKitSansSerifFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_SANSERIF;
const char kWebKitCursiveFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_CURSIVE;
const char kWebKitFantasyFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_FANTASY;
const char kWebKitPictographFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_PICTOGRAPH;
const char kWebKitStandardFontFamilyArabic[] =
    "webkit.webprefs.fonts.standard.Arab";
#if defined(OS_WIN)
const char kWebKitFixedFontFamilyArabic[] =
    "webkit.webprefs.fonts.fixed.Arab";
#endif
const char kWebKitSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.serif.Arab";
const char kWebKitSansSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.sansserif.Arab";
#if defined(OS_WIN)
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
const char kWebKitFixedFontFamilyGreek[] =
    "webkit.webprefs.fonts.fixed.Grek";
const char kWebKitSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.serif.Grek";
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
const char kWebKitFixedFontFamilyKorean[] =
    "webkit.webprefs.fonts.fixed.Hang";
const char kWebKitSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.serif.Hang";
const char kWebKitSansSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.sansserif.Hang";
#if defined(OS_WIN)
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
#if defined(OS_WIN) || defined(OS_MACOSX)
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
#if defined(OS_ANDROID)
const char kWebKitFontScaleFactor[] = "webkit.webprefs.font_scale_factor";
const char kWebKitForceEnableZoom[] = "webkit.webprefs.force_enable_zoom";
const char kWebKitPasswordEchoEnabled[] =
    "webkit.webprefs.password_echo_enabled";
#endif

const char kWebKitCommonScript[] = "Zyyy";
const char kWebKitStandardFontFamily[] = "webkit.webprefs.fonts.standard.Zyyy";
const char kWebKitFixedFontFamily[] = "webkit.webprefs.fonts.fixed.Zyyy";
const char kWebKitSerifFontFamily[] = "webkit.webprefs.fonts.serif.Zyyy";
const char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.fonts.sansserif.Zyyy";
const char kWebKitCursiveFontFamily[] = "webkit.webprefs.fonts.cursive.Zyyy";
const char kWebKitFantasyFontFamily[] = "webkit.webprefs.fonts.fantasy.Zyyy";
const char kWebKitPictographFontFamily[] =
    "webkit.webprefs.fonts.pictograph.Zyyy";
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

// Boolean that is true when Data Saver is enabled.
// TODO(bengr): Migrate the preference string to "data_saver.enabled"
// (crbug.com/564207).
const char kDataSaverEnabled[] = "spdy_proxy.enabled";

// Boolean that is true when the SSL interstitial should allow users to
// proceed anyway. Otherwise, proceeding is not possible.
const char kSSLErrorOverrideAllowed[] = "ssl.error_override_allowed";

// Enum that specifies whether Incognito mode is:
// 0 - Enabled. Default behaviour. Default mode is available on demand.
// 1 - Disabled. Used cannot browse pages in Incognito mode.
// 2 - Forced. All pages/sessions are forced into Incognito.
const char kIncognitoModeAvailability[] = "incognito.mode_availability";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

#if defined(OS_ANDROID)
// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";
const char kContextualSearchDisabledValue[] = "false";
const char kContextualSearchEnabledValue[] = "true";
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
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

// A boolean pref set to true if we're using Link Doctor error pages.
const char kAlternateErrorPagesEnabled[] = "alternate_error_pages.enabled";

// Controls if the QUIC protocol is allowed.
const char kQuicAllowed[] = "net.quic_allowed";

// Prefs for persisting network qualities.
const char kNetworkQualities[] = "net.network_qualities";

#if defined(OS_ANDROID)
// Last time that a check for cloud policy management was done. This time is
// recorded on Android so that retries aren't attempted on every startup.
// Instead the cloud policy registration is retried at least 1 or 3 days later.
const char kLastPolicyCheckTime[] = "policy.last_policy_check_time";
#endif

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
const char kDefaultAppsInstallState[] = "default_apps_install_state";

// A boolean pref set to true if the Chrome Web Store icons should be hidden
// from the New Tab Page and app launcher.
const char kHideWebStoreIcon[] = "hide_web_store_icon";

#if defined(OS_CHROMEOS)
// A boolean pref set to true if touchpad tap-to-click is enabled.
const char kTapToClickEnabled[] = "settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if touchpad three-finger-click is enabled.
const char kEnableTouchpadThreeFingerClick[] =
    "settings.touchpad.enable_three_finger_click";

// A boolean pref set to true if touchpad natural scrolling is enabled.
const char kNaturalScroll[] = "settings.touchpad.natural_scroll";

// A boolean pref set to true if primary mouse button is the left button.
const char kPrimaryMouseButtonRight[] = "settings.mouse.primary_right";

// A boolean pref set to true if turning the mouse wheel toward the user should
// result in scrolling up instead of the more common scrolling down.
const char kMouseReverseScroll[] = "settings.mouse.reverse_scroll";

// A integer pref for the touchpad sensitivity.
const char kMouseSensitivity[] = "settings.mouse.sensitivity2";

// A integer pref for the touchpad sensitivity.
const char kTouchpadSensitivity[] = "settings.touchpad.sensitivity2";

// A boolean pref set to true if time should be displayed in 24-hour clock.
const char kUse24HourClock[] = "settings.clock.use_24hour_clock";

// A string pref containing Timezone ID for this user.
const char kUserTimezone[] = "settings.timezone";

// This setting disables manual timezone selection and starts periodic timezone
// refresh.
// Deprecated. Replaced with kResolveTimezoneByGeolocationMethod.
// TODO(alemate): https://crbug.com/783367 Remove outdated prefs.
const char kResolveTimezoneByGeolocation[] =
    "settings.resolve_timezone_by_geolocation";

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

// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const char kLanguagePreferredLanguages[] =
    "settings.language.preferred_languages";
const char kLanguagePreferredLanguagesSyncable[] =
    "settings.language.preferred_languages_syncable";

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

// A boolean pref to indicate whether we still need to add the globally synced
// input methods. False after the initial post-OOBE sync.
const char kLanguageShouldMergeInputMethods[] =
    "settings.language.merge_input_methods";

// A boolean pref that causes top-row keys to be interpreted as function keys
// instead of as media keys.
const char kLanguageSendFunctionKeys[] =
    "settings.language.send_function_keys";

// A boolean pref which determines whether key repeat is enabled.
const char kLanguageXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";
// A integer pref which determines key repeat delay (in ms).
const char kLanguageXkbAutoRepeatDelay[] =
    "settings.language.xkb_auto_repeat_delay_r2";
// A integer pref which determines key repeat interval (in ms).
const char kLanguageXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes were added to the three prefs above when we changed the
// preferences to not be user-configurable or sync with the cloud. The prefs are
// now user-configurable and syncable again, but we don't want to overwrite the
// current values with the old synced values, so we continue to use this suffix.

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
const char kLabsAdvancedFilesystemEnabled[] =
    "settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
const char kLabsMediaplayerEnabled[] = "settings.labs.mediaplayer";

// A boolean pref of whether to show 3G promo notification.
const char kShow3gPromoNotification[] =
    "settings.internet.mobile.show_3g_promo_notification";

// An integer pref counting times Data Saver prompt has been shown.
const char kDataSaverPromptsShown[] =
    "settings.internet.mobile.datasaver_prompts_shown";

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
// The intended usage is to whitelist the set of apps that the user can enable
// to run on lock screen, not to actually enable the apps to run on lock screen.
const char kNoteTakingAppsLockScreenWhitelist[] =
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
// as the serialization obtained from base::TimeTicks::ToInternalValue().
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
// The list of extensions allowed to use the platformKeysPrivate API for
// remote attestation.
const char kAttestationExtensionWhitelist[] = "attestation.extension_whitelist";

// A boolean pref recording whether user has dismissed the multiprofile
// itroduction dialog show.
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

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again. The time is expressed in
// seconds. A value of -1 indicates no limit, allowing the user to use offline
// authentication indefinitely. The limit is in effect only if GAIA redirected
// the user to a SAML IdP during the last online authentication.
const char kSAMLOfflineSigninTimeLimit[] = "saml.offline_signin_time_limit";

// A preference to keep track of the last time the user authenticated against
// GAIA using SAML. The preference is updated whenever the user authenticates
// against GAIA: If GAIA redirects to a SAML IdP, the preference is set to the
// current time. If GAIA performs the authentication itself, the preference is
// cleared. The time is expressed as the serialization obtained from
// base::Time::ToInternalValue().
const char kSAMLLastGAIASignInTime[] = "saml.last_gaia_sign_in_time";

// The total number of seconds that the machine has spent sitting on the
// OOBE screen.
const char kTimeOnOobe[] = "settings.time_on_oobe";

// List of mounted file systems via the File System Provider API. Used to
// restore them after a reboot.
const char kFileSystemProviderMounted[] = "file_system_provider.mounted";

// A boolean pref set to true if the virtual keyboard should be enabled.
const char kTouchVirtualKeyboardEnabled[] = "ui.touch_virtual_keyboard_enabled";

// A boolean pref that controls whether the dark connect feature is enabled.
// The dark connect feature allows a Chrome OS device to periodically wake
// from suspend in a low-power state to maintain WiFi connectivity.
const char kWakeOnWifiDarkConnect[] =
    "settings.internet.wake_on_wifi_darkconnect";

// This is the policy CaptivePortalAuthenticationIgnoresProxy that allows to
// open captive portal authentication pages in a separate window under
// a temporary incognito profile ("signin profile" is used for this purpose),
// which allows to bypass the user's proxy for captive portal authentication.
const char kCaptivePortalAuthenticationIgnoresProxy[] =
    "proxy.captive_portal_ignores_proxy";

// This boolean controls whether the first window shown on first run should be
// unconditionally maximized, overriding the heuristic that normally chooses the
// window size.
const char kForceMaximizeOnFirstRun[] = "ui.force_maximize_on_first_run";

// A dictionary pref mapping public keys that identify platform keys to its
// properties like whether it's meant for corporate usage.
const char kPlatformKeys[] = "platform_keys";

// A boolean pref. If set to true, the Unified Desktop feature is made
// available and turned on by default, which allows applications to span
// multiple screens. Users may turn the feature off and on in the settings
// while this is set to true.
const char kUnifiedDesktopEnabledByDefault[] =
    "settings.display.unified_desktop_enabled_by_default";

// An int64 pref. This is a timestamp of the most recent time the profile took
// or dismissed HaTS (happiness-tracking) survey.
const char kHatsLastInteractionTimestamp[] = "hats_last_interaction_timestamp";

// An int64 pref. This is the timestamp that indicates the end of the most
// recent survey cycle.
const char kHatsSurveyCycleEndTimestamp[] = "hats_survey_cycle_end_timestamp";

// A boolean pref. Indicates if the device is selected for HaTS in the current
// survey cycle.
const char kHatsDeviceIsSelected[] = "hats_device_is_selected";

// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the quick unlock feature.
const char kPinUnlockFeatureNotificationShown[] =
    "pin_unlock_feature_notification_shown";
// A boolean pref. Indicates if we've already shown a notification to inform the
// current user about the fingerprint unlock feature.
const char kFingerprintUnlockFeatureNotificationShown[] =
    "fingerprint_unlock_feature_notification_shown";

// The hash for the pin quick unlock mechanism.
const char kQuickUnlockPinSecret[] = "quick_unlock.pin.secret";

// An integer pref. Indicates the number of fingerprint records registered.
const char kQuickUnlockFingerprintRecord[] = "quick_unlock.fingerprint.record";

// An integer pref. Holds one of several values:
// 0: Supported. Device is in supported state.
// 1: Security Only. Device is in Security-Only update (after initial 5 years).
// 2: EOL. Device is End of Life(No more updates expected).
// This value needs to be consistent with EndOfLifeStatus enum.
const char kEolStatus[] = "eol_status";

// Boolean pref indicating the End Of Life notification was dismissed by the
// user.
const char kEolNotificationDismissed[] = "eol_notification_dismissed";

// A list of allowed quick unlock modes. A quick unlock mode can only be used if
// its type is on this list, or if type all (all quick unlock modes enabled) is
// on this list.
const char kQuickUnlockModeWhitelist[] = "quick_unlock_mode_whitelist";
// Enum that specifies how often a user has to enter their password to continue
// using quick unlock. These values are the same as the ones in
// chromeos::QuickUnlockPasswordConfirmationFrequency.
// 0 - six hours. Users will have to enter their password every six hours.
// 1 - twelve hours. Users will have to enter their password every twelve hours.
// 2 - day. Users will have to enter their password every day.
// 3 - week. Users will have to enter their password every week.
const char kQuickUnlockTimeout[] = "quick_unlock_timeout";
// Integer prefs indicating the minimum and maximum lengths of the lock screen
// pin.
const char kPinUnlockMinimumLength[] = "pin_unlock_minimum_length";
const char kPinUnlockMaximumLength[] = "pin_unlock_maximum_length";
// Boolean pref indicating whether users are allowed to set easy pins.
const char kPinUnlockWeakPinsAllowed[] = "pin_unlock_weak_pins_allowed";

// Boolean pref indicating whether this device supports BLE advertising.
const char kInstantTetheringBleAdvertisingSupported[] =
    "tether.ble_advertising_supported";

// Boolean pref indicating whether someone can cast to the device.
const char kCastReceiverEnabled[] = "cast_receiver.enabled";

// String pref indicating what is the minimum version of Chrome required to
// allow user sign in. If the string is empty or blank no restrictions will
// be applied. See base::Version for exact string format.
const char kMinimumAllowedChromeVersion[] = "minimum_req.version";

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

// A dictionary preference holding the usage time limit definitions for a user.
const char kUsageTimeLimit[] = "screen_time.limit";

// Last state of the screen time limit.
const char kScreenTimeLastState[] = "screen_time.last_state";

// Boolean controlling whether showing Sync Consent during sign-in is enabled.
// Controlled by policy.
const char kEnableSyncConsent[] = "sync_consent.enabled";

// Boolean pref indicating whether a user is allowed to use the Network File
// Shares for Chrome OS feature.
const char kNetworkFileSharesAllowed[] = "network_file_shares.allowed";

// Boolean pref indicating whether the currently running public session runs in
// the old standard "public session" mode (false), or in the new "managed
// session" mode which has lifted restrictions (true).
const char kManagedSessionEnabled[] = "managed_session.enabled";

// Boolean pref indicating whether the user has previously dismissed the
// one-time notification indicating the need for a cleanup powerwash after TPM
// firmware update that didn't flush the TPM SRK.
const char kTPMFirmwareUpdateCleanupDismissed[] =
    "tpm_firmware_update.cleanup_dismissed";

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

// Boolean pref indicating whether the NTLM authentication protocol should be
// enabled when mounting an SMB share with a user credential by the Network File
// Shares for Chrome OS feature.
const char kNTLMShareAuthenticationEnabled[] =
    "network_file_shares.ntlm_share_authentication.enabled";

// List of preconfigured network file shares.
const char kNetworkFileSharesPreconfiguredShares[] =
    "network_file_shares.preconfigured_shares";

#endif  // defined(OS_CHROMEOS)

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
const char kShowHomeButton[] = "browser.show_home_button";

// Boolean pref to define the default setting for "block offensive words".
// The old key value is kept to avoid unnecessary migration code.
const char kSpeechRecognitionFilterProfanities[] =
    "browser.speechinput_censor_results";

// Boolean controlling whether history saving is disabled.
const char kSavingBrowserHistoryDisabled[] = "history.saving_disabled";

// Boolean controlling whether deleting browsing and download history is
// permitted.
const char kAllowDeletingBrowserHistory[] = "history.deleting_enabled";

#if !defined(OS_ANDROID)
// Whether the "Click here to clear your browsing data" tooltip promo has been
// shown on the Material Design History page.
const char kMdHistoryMenuPromoShown[] = "history.menu_promo_shown";
#endif

// Boolean controlling whether SafeSearch is mandatory for Google Web Searches.
const char kForceGoogleSafeSearch[] = "settings.force_google_safesearch";

// Integer controlling whether Restrict Mode (moderate/strict) is mandatory on
// YouTube. See |safe_search_util::YouTubeRestrictMode| for possible values.
const char kForceYouTubeRestrict[] = "settings.force_youtube_restrict";

// Boolean controlling whether history is recorded via Session Sync
// (for supervised users).
const char kForceSessionSync[] = "settings.history_recorded";

// Comma separated list of domain names (e.g. "google.com,school.edu").
// When this pref is set, the user will be able to access Google Apps
// only using an account that belongs to one of the domains from this pref.
const char kAllowedDomainsForApps[] = "settings.allowed_domains_for_apps";

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Linux specific preference on whether we should match the system theme.
const char kUsesSystemTheme[] = "extensions.theme.use_system";
#endif
const char kCurrentThemePackFilename[] = "extensions.theme.pack";
const char kCurrentThemeID[] = "extensions.theme.id";
const char kCurrentThemeImages[] = "extensions.theme.images";
const char kCurrentThemeColors[] = "extensions.theme.colors";
const char kCurrentThemeTints[] = "extensions.theme.tints";
const char kCurrentThemeDisplayProperties[] = "extensions.theme.properties";

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

// List pref containing names of plugins that are disabled by policy.
const char kPluginsDisabledPlugins[] = "plugins.plugins_disabled";

// List pref containing exceptions to the list of plugins disabled by policy.
const char kPluginsDisabledPluginsExceptions[] =
    "plugins.plugins_disabled_exceptions";

// List pref containing names of plugins that are enabled by policy.
const char kPluginsEnabledPlugins[] = "plugins.plugins_enabled";

// Whether Chrome should use its internal PDF viewer or not.
const char kPluginsAlwaysOpenPdfExternally[] =
    "plugins.always_open_pdf_externally";

#if BUILDFLAG(ENABLE_PLUGINS)
// Whether about:plugins is shown in the details mode or not.
const char kPluginsShowDetails[] = "plugins.show_details";
#endif

// Boolean that indicates whether outdated plugins are allowed or not.
const char kPluginsAllowOutdated[] = "plugins.allow_outdated";

// Boolean that indicates whether all Flash content (including cross-origin and
// small content) is allowed to run when it is explicitly allowed via content
// settings.
const char kRunAllFlashInAllowMode[] = "plugins.run_all_flash_in_allow_mode";

#if BUILDFLAG(ENABLE_PLUGINS)
// Dictionary holding plugins metadata.
const char kPluginsMetadata[] = "plugins.metadata";

// Last update time of plugins resource cache.
const char kPluginsResourceCacheUpdate[] = "plugins.resource_cache_update";
#endif

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

#if defined(OS_MACOSX)
// Boolean that indicates whether the application should show the info bar
// asking the user to set up automatic updates when Keystone promotion is
// required.
const char kShowUpdatePromotionInfoBar[] =
    "browser.show_update_promotion_info_bar";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Boolean that is false if we should show window manager decorations.  If
// true, we draw a custom chrome frame (thicker title bar and blue border).
const char kUseCustomChromeFrame[] = "browser.custom_chrome_frame";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Which plugins have been whitelisted manually by the user.
const char kContentSettingsPluginWhitelist[] =
    "profile.content_settings.plugin_whitelist";
#endif

#if !defined(OS_ANDROID)
// Double that indicates the default zoom level.
const char kPartitionDefaultZoomLevel[] = "partition.default_zoom_level";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const char kPartitionPerHostZoomLevels[] = "partition.per_host_zoom_levels";

const char kPinnedTabs[] = "pinned_tabs";
#endif  // !defined(OS_ANDROID)

// Preference to disable 3D APIs (WebGL, Pepper 3D).
const char kDisable3DAPIs[] = "disable_3d_apis";

const char kEnableDeprecatedWebPlatformFeatures[] =
    "enable_deprecated_web_platform_features";

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
// RendererPreferences and we should migrate the pref to use a new
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
const char kProfileLocalAvatarIndex[] = "profile.local_avatar_index";
const char kProfileName[] = "profile.name";
// Whether a profile is using a default avatar name (eg. Pickles or Person 1)
// because it was randomly assigned at profile creation time.
const char kProfileUsingDefaultName[] = "profile.using_default_name";
// Whether a profile is using an avatar without having explicitely chosen it
// (i.e. was assigned by default by legacy profile creation).
const char kProfileUsingDefaultAvatar[] = "profile.using_default_avatar";
const char kProfileUsingGAIAAvatar[] = "profile.using_gaia_avatar";

// The supervised user ID.
const char kSupervisedUserId[] = "profile.managed_user_id";

// 64-bit integer serialization of the base::Time when the user's GAIA info
// was last updated.
const char kProfileGAIAInfoUpdateTime[] = "profile.gaia_info_update_time";

// The URL from which the GAIA profile picture was downloaded. This is cached to
// prevent the same picture from being downloaded multiple times.
const char kProfileGAIAInfoPictureURL[] = "profile.gaia_info_picture_url";

// Integer that specifies the number of times that we have shown the upgrade
// tutorial card in the avatar menu bubble.
const char kProfileAvatarTutorialShown[] =
    "profile.avatar_bubble_tutorial_shown";

// Indicates if we've already shown a notification that high contrast
// mode is on, recommending high-contrast extensions and themes.
const char kInvertNotificationShown[] = "invert_notification_version_2_shown";

// Boolean controlling whether printing is enabled.
const char kPrintingEnabled[] = "printing.enabled";

// Boolean controlling whether print preview is disabled.
const char kPrintPreviewDisabled[] = "printing.print_preview_disabled";

// A pref holding the value of the policy used to control default destination
// selection in the Print Preview. See DefaultPrinterSelection policy.
const char kPrintPreviewDefaultDestinationSelectionRules[] =
    "printing.default_destination_selection_rules";

// The default value for the 'Headers and footers' checkbox, in Print Preview.
// Takes priority over kPrintPreviewStickySettings if set.
const char kPrintHeaderFooter[] = "printing.print_header_footer";

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// A pref that sets the default destination in Print Preview to always be the
// OS default printer instead of the most recently used destination.
const char kPrintPreviewUseSystemDefaultPrinter[] =
    "printing.use_system_default_printer";
#endif  // !OS_CHROMEOS && !OS_ANDROID

#if defined(OS_CHROMEOS)
// List of printers configured by policy.
const char kRecommendedNativePrinters[] =
    "native_printing.recommended_printers";

// Enum designating the type of restrictions bulk printers are using.
const char kRecommendedNativePrintersAccessMode[] =
    "native_printing.recommended_printers_access_mode";

// List of printer ids which are explicitly disallowed.  List of strings.
const char kRecommendedNativePrintersBlacklist[] =
    "native_printing.recommended_printers_blacklist";

// List of printer ids that are allowed.  List of strings.
const char kRecommendedNativePrintersWhitelist[] =
    "native_printing.recommended_printers_whitelist";

// A Boolean flag which represents whether or not users are allowed to configure
// and use their own native printers.
const char kUserNativePrintersAllowed[] =
    "native_printing.user_native_printers_allowed";

// A pref holding the list of allowed printing color mode as a bitmask composed
// of |printing::ColorModeRestriction| values. 0 is no restriction.
const char kPrintingAllowedColorModes[] = "printing.allowed_color_modes";

// A pref holding the list of allowed printing duplex mode as a bitmask composed
// of |printing::DuplexModeRestriction| values. 0 is no restriction.
const char kPrintingAllowedDuplexModes[] = "printing.allowed_duplex_modes";

// A pref holding the list of allowed printing duplex mode.
// Empty list is no restriction.
const char kPrintingAllowedPageSizes[] = "printing.allowed_page_sizes";

// A pref holding the default color mode.
const char kPrintingColorDefault[] = "printing.color_default";

// A pref holding the default duplex mode.
const char kPrintingDuplexDefault[] = "printing.duplex_default";

// A pref holding the default page size.
const char kPrintingSizeDefault[] = "printing.size_default";
#endif  // OS_CHROMEOS

// An integer pref specifying the fallback behavior for sites outside of content
// packs. One of:
// 0: Allow (does nothing)
// 1: Warn.
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

// Enable notifications for new devices on the local network that can be
// registered to the user's account, e.g. Google Cloud Print printers.
const char kLocalDiscoveryNotificationsEnabled[] =
    "local_discovery.notifications_enabled";

#if defined(OS_ANDROID)
// Enable vibration for web notifications.
const char kNotificationsVibrateEnabled[] = "notifications.vibrate_enabled";

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
#endif

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Used to indicate whether or not the toolbar redesign bubble has been shown
// and acknowledged, and the last time the bubble was shown.
const char kToolbarIconSurfacingBubbleAcknowledged[] =
    "toolbar_icon_surfacing_bubble_acknowledged";
const char kToolbarIconSurfacingBubbleLastShowTime[] =
    "toolbar_icon_surfacing_bubble_show_time";
#endif

// Whether WebRTC should bind to individual NICs to explore all possible routing
// options. Default is true. This has become obsoleted and replaced by
// kWebRTCIPHandlingPolicy. TODO(guoweis): Remove this at M50.
const char kWebRTCMultipleRoutesEnabled[] = "webrtc.multiple_routes_enabled";
// Whether WebRTC should use non-proxied UDP. If false, WebRTC will not send UDP
// unless it goes through a proxy (i.e RETURN when it's available).  If no UDP
// proxy is configured, it will not send UDP.  If true, WebRTC will send UDP
// regardless of whether or not a proxy is configured. TODO(guoweis): Remove
// this at M50.
const char kWebRTCNonProxiedUdpEnabled[] =
    "webrtc.nonproxied_udp_enabled";
// Define the IP handling policy override that WebRTC should follow. When not
// set, it defaults to "default".
const char kWebRTCIPHandlingPolicy[] = "webrtc.ip_handling_policy";
// Define range of UDP ports allowed to be used by WebRTC PeerConnections.
const char kWebRTCUDPPortRange[] = "webrtc.udp_port_range";
// Whether WebRTC event log collection by Google domains is allowed.
const char kWebRtcEventLogCollectionAllowed[] = "webrtc.event_logs_collection";

#if !defined(OS_ANDROID)
// Whether or not this profile has been shown the Welcome page.
const char kHasSeenWelcomePage[] = "browser.has_seen_welcome_page";
#endif

#if defined(OS_WIN)
// Whether or not this profile has been shown the Win10 promo page.
const char kHasSeenWin10PromoPage[] = "browser.has_seen_win10_promo_page";

#if defined(GOOGLE_CHROME_BUILD)
// Whether or not this user went through the first-run experience after NUX
// launched. This is necessary for determining which users to keep "tagging"
// with the NUX finch experiment group, and allows a more accurate analysis.
const char kOnboardDuringNUX[] = "browser.onboard_during_nux";
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

// Directory of the last profile used.
const char kProfileLastUsed[] = "profile.last_used";

// List of directories of the profiles last active.
const char kProfilesLastActive[] = "profile.last_active_profiles";

// Total number of profiles created for this Chrome build. Used to tag profile
// directories.
const char kProfilesNumCreated[] = "profile.profiles_created";

// String containing the version of Chrome that the profile was created by.
// If profile was created before this feature was added, this pref will default
// to "1.0.0.0".
const char kProfileCreatedByVersion[] = "profile.created_by_version";

// A map of profile data directory to cached information. This cache can be
// used to display information about profiles without actually having to load
// them.
const char kProfileInfoCache[] = "profile.info_cache";

// A list of profile paths that should be deleted on shutdown. The deletion does
// not happen if the browser crashes, so we remove the profile on next start.
const char kProfilesDeleted[] = "profiles.profiles_deleted";

// Deprecated preference for metric / crash reporting on Android. Use
// kMetricsReportingEnabled instead.
#if defined(OS_ANDROID)
const char kCrashReportingEnabled[] =
    "user_experience_metrics_crash.reporting_enabled";
#endif  // defined(OS_ANDROID)

// This is the location of a list of dictionaries of plugin stability stats.
const char kStabilityPluginStats[] =
    "user_experience_metrics.stability.plugin_stats2";

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

// The keys below are used for the dictionaries in the
// kStabilityPluginStats list.
const char kStabilityPluginName[] = "name";
const char kStabilityPluginLaunches[] = "launches";
const char kStabilityPluginInstances[] = "instances";
const char kStabilityPluginCrashes[] = "crashes";
const char kStabilityPluginLoadingErrors[] = "loading_errors";

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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
const char kOpenPdfDownloadInSystemReader[] =
    "download.open_pdf_in_system_reader";
#endif

#if defined(OS_ANDROID)
// Int (as defined by DownloadPromptStatus) which specifies whether we should
// ask the user where they want to download the file (only for Android).
const char kPromptForDownloadAndroid[] = "download.prompt_for_download_android";

// Boolean which specifies whether we should display the missing SD card error.
// This is only applicable for Android.
const char kShowMissingSdCardErrorAndroid[] =
    "download.show_missing_sd_card_error_android";
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
const char kDefaultTasksByMimeType[] =
    "filebrowser.tasks.default_by_mime_type";

// Map of default tasks, associated by file suffix.
const char kDefaultTasksBySuffix[] =
    "filebrowser.tasks.default_by_suffix";

// Extensions which should be opened upon completion.
const char kDownloadExtensionsToOpen[] = "download.extensions_to_open";

// Dictionary of schemes used by the external protocol handler.
// The value is true if the scheme must be ignored.
const char kExcludedSchemes[] = "protocol_handler.excluded_schemes";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
const char kLastKnownIntranetRedirectOrigin[] = "browser.last_redirect_origin";

// An enum value of how the browser was shut down (see browser_shutdown.h).
const char kShutdownType[] = "shutdown.type";
// Number of processes that were open when the user shut down.
const char kShutdownNumProcesses[] = "shutdown.num_processes";
// Number of processes that were shut down using the slow path.
const char kShutdownNumProcessesSlow[] = "shutdown.num_processes_slow";

// Whether to restart the current Chrome session automatically as the last thing
// before shutting everything down.
const char kRestartLastSessionOnShutdown[] = "restart.last.session.on.shutdown";

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
// Pref name for the policy controlling presentation of full-tab promotional
// and/or educational content.
const char kPromotionalTabsEnabled[] = "browser.promotional_tabs_enabled";
#endif  // !defined(OS_CHROMEOS)

// Boolean that specifies whether or not showing the unsupported OS warning is
// suppressed. False by default. Controlled by the SuppressUnsupportedOSWarning
// policy setting.
const char kSuppressUnsupportedOSWarning[] =
    "browser.suppress_unsupported_os_warning";

// Set before autorestarting Chrome, cleared on clean exit.
const char kWasRestarted[] = "was.restarted";

#endif  // !defined(OS_ANDROID)

// Whether Extensions are enabled.
const char kDisableExtensions[] = "extensions.disabled";

// Customized app page names that appear on the New Tab Page.
const char kNtpAppPageNames[] = "ntp.app_page_names";

// Keeps track of which sessions are collapsed in the Other Devices menu.
const char kNtpCollapsedForeignSessions[] = "ntp.collapsed_foreign_sessions";

#if defined(OS_ANDROID)
// Keeps track of recently closed tabs collapsed state in the Other Devices
// menu.
const char kNtpCollapsedRecentlyClosedTabs[] =
    "ntp.collapsed_recently_closed_tabs";

// Keeps track of snapshot documents collapsed state in the Other Devices menu.
const char kNtpCollapsedSnapshotDocument[] = "ntp.collapsed_snapshot_document";

// Keeps track of sync promo collapsed state in the Other Devices menu.
const char kNtpCollapsedSyncPromo[] = "ntp.collapsed_sync_promo";

// Tracks whether we should show notifications related to content suggestions.
const char kContentSuggestionsNotificationsEnabled[] =
    "ntp.content_suggestions.notifications.enabled";

// Tracks how many notifications the user has ignored, so we can tell when we
// should stop showing them.
const char kContentSuggestionsConsecutiveIgnoredPrefName[] =
    "ntp.content_suggestions.notifications.consecutive_ignored";

// Tracks how many notifications have been sent today, and what day "today" is,
// as an integer YYYYMMDD, in wall time in the local timezone.
// If sent_day changes, sent_count is reset to 0. Allows limiting per-day
// notification count.
const char kContentSuggestionsNotificationsSentDay[] =
    "ntp.content_suggestions.notifications.sent_day";
const char kContentSuggestionsNotificationsSentCount[] =
    "ntp.content_suggestions.notifications.sent_count";
#else
// Holds info for New Tab Page custom background
const char kNtpCustomBackgroundDict[] = "ntp.custom_background_dict";
const char kNtpCustomBackgroundLocalToDevice[] =
    "ntp.custom_background_local_to_device";
#endif  // defined(OS_ANDROID)

// Which page should be visible on the new tab page v4
const char kNtpShownPage[] = "ntp.shown_page";

// A private RSA key for ADB handshake.
const char kDevToolsAdbKey[] = "devtools.adb_key";

// Defines administrator-set availability of developer tools.
const char kDevToolsAvailability[] = "devtools.availability";

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

// A dictionary with generic DevTools settings.
const char kDevToolsPreferences[] = "devtools.preferences";

#if !defined(OS_ANDROID)
// Tracks the number of times the dice signin promo has been shown in the user
// menu.
const char kDiceSigninUserMenuPromoCount[] = "sync_promo.user_menu_show_count";

// Tracks the number of times that we have shown the sign in promo at startup.
const char kSignInPromoStartupCount[] = "sync_promo.startup_count";

// Boolean tracking whether the user chose to skip the sign in promo.
const char kSignInPromoUserSkipped[] = "sync_promo.user_skipped";

// Boolean that specifies if the sign in promo is allowed to show on first run.
// This preference is specified in the master preference file to suppress the
// sign in promo for some installations.
const char kSignInPromoShowOnFirstRunAllowed[] =
    "sync_promo.show_on_first_run_allowed";

// Boolean that specifies if we should show a bubble in the new tab page.
// The bubble is used to confirm that the user is signed into sync.
const char kSignInPromoShowNTPBubble[] = "sync_promo.show_ntp_bubble";
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

// Dictionary that maps web app URLs to Chrome extension IDs.
const char kWebAppsExtensionIDs[] = "web_apps.extension_ids";

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

// The last used printer and its settings.
const char kPrintPreviewStickySettings[] =
    "printing.print_preview_sticky_settings";

// The list of BackgroundContents that should be loaded when the browser
// launches.
const char kRegisteredBackgroundContents[] = "background_contents.registered";

#if defined(OS_WIN)
// Boolean that specifies whether or not showing the welcome page following an
// OS upgrade is enabled. True by default. May be set by master_preferences or
// overridden by the WelcomePageOnOSUpgradeEnabled policy setting.
const char kWelcomePageOnOSUpgradeEnabled[] =
    "browser.welcome_page_on_os_upgrade_enabled";
#endif

// String that lists supported HTTP authentication schemes.
const char kAuthSchemes[] = "auth.schemes";

// Boolean that specifies whether to disable CNAME lookups when generating
// Kerberos SPN.
const char kDisableAuthNegotiateCnameLookup[] =
    "auth.disable_negotiate_cname_lookup";

// Boolean that specifies whether to include the port in a generated Kerberos
// SPN.
const char kEnableAuthNegotiatePort[] = "auth.enable_negotiate_port";

// Whitelist containing servers for which Integrated Authentication is enabled.
const char kAuthServerWhitelist[] = "auth.server_whitelist";

// Whitelist containing servers Chrome is allowed to do Kerberos delegation
// with.
const char kAuthNegotiateDelegateWhitelist[] =
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

#if defined(OS_POSIX)
// Boolean that specifies whether NTLMv2 is enabled.
const char kNtlmV2Enabled[] = "auth.ntlm_v2_enabled";
#endif  // defined(OS_POSIX)

// Boolean that specifies whether to enable revocation checking (best effort)
// by default.
const char kCertRevocationCheckingEnabled[] = "ssl.rev_checking.enabled";

// Boolean that specifies whether to require a successful revocation check if
// a certificate path ends in a locally-trusted (as opposed to publicly
// trusted) trust anchor.
const char kCertRevocationCheckingRequiredLocalAnchors[] =
    "ssl.rev_checking.required_for_local_anchors";

// Boolean that specifies whether to allow the SHA-1 digest algorithm in
// certificate signatures for certificate paths that end in a locally-trusted
// (as opposed to publicly trusted) trust anchor.
const char kCertEnableSha1LocalAnchors[] = "ssl.sha1_enabled_for_local_anchors";

// Boolean that specifies whether to allow certificates from the Legacy
// Symantec Infrastructure, disabling the mitigations documented in
// https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
const char kCertEnableSymantecLegacyInfrastructure[] =
    "ssl.enable_symantec_legacy_infrastructure";

// String specifying the minimum TLS version to negotiate. Supported values
// are "tls1", "tls1.1", "tls1.2", "tls1.3".
const char kSSLVersionMin[] = "ssl.version_min";

// String specifying the maximum TLS version to negotiate. Supported values
// are "tls1.2", "tls1.3"
const char kSSLVersionMax[] = "ssl.version_max";

// String specifying the TLS 1.3 variant to negotiate when negotiating TLS 1.3.
// Supported values are "disabled", which disables TLS 1.3, "draft23", and
// "final".
const char kTLS13Variant[] = "ssl.tls13_variant";

// String specifying the TLS ciphersuites to disable. Ciphersuites are
// specified as a comma-separated list of 16-bit hexadecimal values, with
// the values being the ciphersuites assigned by the IANA registry (e.g.
// "0x0004,0x0005").
const char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";

// List of strings specifying which hosts are allowed to have H2 connections
// coalesced when client certs are also used. This follows rules similar to
// the URLBlacklist format for hostnames: a pattern with a leading dot (e.g.
// ".example.net") matches exactly the hostname following the dot (i.e. only
// "example.net"), and a pattern with no leading dot (e.g. "example.com")
// matches that hostname and all subdomains.
const char kH2ClientCertCoalescingHosts[] =
    "ssl.client_certs.h2_coalescing_hosts";

// Boolean that specifies whether the built-in asynchronous DNS client is used.
const char kBuiltInDnsClientEnabled[] = "async_dns.enabled";

// String containing list of DNS over HTTPS servers to be used.
const char kDnsOverHttpsServers[] = "dns_over_https.servers";
// String contianing list of methods (GET or POST) to use with DNS over HTTPS
// servers, in the same order of the above pref.
const char kDnsOverHttpsServerMethods[] = "dns_over_https.methods";

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

#if defined(OS_CHROMEOS)
// An integer pref that holds enum value of current demo mode configuration.
// Values are defined by DemoSession::DemoModeConfig enum.
const char kDemoModeConfig[] = "demo_mode.config";

// A string pref holding the value of the default locale for demo sessions.
const char kDemoModeDefaultLocale[] = "demo_mode.default_locale";

// Dictionary for transient storage of settings that should go into device
// settings storage before owner has been assigned.
const char kDeviceSettingsCache[] = "signed_settings_cache";

// The hardware keyboard layout of the device. This should look like
// "xkb:us::eng".
const char kHardwareKeyboardLayout[] = "intl.hardware_keyboard";

// An integer pref which shows number of times carrier deal promo
// notification has been shown to user.
const char kCarrierDealPromoShown[] =
    "settings.internet.mobile.carrier_deal_promo_shown";

// A boolean pref of the auto-enrollment decision. Its value is only valid if
// it's not the default value; otherwise, no auto-enrollment decision has been
// made yet.
const char kShouldAutoEnroll[] = "ShouldAutoEnroll";

// An integer pref with the maximum number of bits used by the client in a
// previous auto-enrollment request. If the client goes through an auto update
// during OOBE and reboots into a version of the OS with a larger maximum
// modulus, then it will retry auto-enrollment using the updated value.
const char kAutoEnrollmentPowerLimit[] = "AutoEnrollmentPowerLimit";

// The local state pref that stores device activity times before reporting
// them to the policy server.
const char kDeviceActivityTimes[] = "device_status.activity_times";

// A pref that stores user activity times before reporting them to the policy
// server.
const char kUserActivityTimes[] = "consumer_device_status.activity_times";

// A pref holding the value of the policy used to disable mounting of external
// storage for the user.
const char kExternalStorageDisabled[] = "hardware.external_storage_disabled";

// A pref holding the value of the policy used to limit mounting of external
// storage to read-only mode for the user.
const char kExternalStorageReadOnly[] = "hardware.external_storage_read_only";

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

// Whether to automatically start the enterprise enrollment step during OOBE.
const char kDeviceEnrollmentAutoStart[] = "enrollment.auto_start";

// Whether the user may exit enrollment.
const char kDeviceEnrollmentCanExit[] = "enrollment.can_exit";

// DM token fetched from the DM server during enrollment. Stored for Active
// Directory devices only.
const char kDeviceDMToken[] = "device_dm_token";

// How many times HID detection OOBE dialog was shown.
const char kTimesHIDDialogShown[] = "HIDDialog.shown_how_many_times";

// Dictionary of per-user last input method (used at login screen). Note that
// the pref name is UsersLRUInputMethods for compatibility with previous
// versions.
const char kUsersLastInputMethod[] = "UsersLRUInputMethod";

// A dictionary pref of the echo offer check flag. It sets offer info when
// an offer is checked.
const char kEchoCheckedOffers[] = "EchoCheckedOffers";

// Key name of a dictionary in local state to store cached multiprofle user
// behavior policy value.
const char kCachedMultiProfileUserBehavior[] = "CachedMultiProfileUserBehavior";

// A string pref with initial locale set in VPD or manifest.
const char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the OOBE complete flag (first OOBE part before login).
const char kOobeComplete[] = "OobeComplete";

// The name of the screen that has to be shown if OOBE has been interrupted.
const char kOobeScreenPending[] = "OobeScreenPending";

// A boolean pref to indicate if an eligible controller (either a Chrome OS
// device, or an Android device) is detected during bootstrapping or
// shark/remora setup process. A controller can help the device go through OOBE
// and get enrolled into a domain automatically.
const char kOobeControllerDetected[] = "OobeControllerDetected";

// A boolean pref to indicate if the marketing opt-in screen in OOBE is finished
// for the user.
const char kOobeMarketingOptInScreenFinished[] =
    "OobeMarketingOptInScreenFinished";

// A boolean pref for whether the Goodies promotion webpage has been displayed,
// or otherwise disqualified for auto-display, on this device.
const char kCanShowOobeGoodiesPage[] = "CanShowOobeGoodiesPage";

// A boolean pref of the device registered flag (second part after first login).
const char kDeviceRegistered[] = "DeviceRegistered";

// Boolean pref to signal corrupted enrollment to force the device through
// enrollment recovery flow upon next boot.
const char kEnrollmentRecoveryRequired[] = "EnrollmentRecoveryRequired";

// List of usernames that used certificates pushed by policy before.
// This is used to prevent these users from joining multiprofile sessions.
const char kUsedPolicyCertificates[] = "policy.used_policy_certificates";

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

// The role of the device in the OOBE bootstrapping process. If it's a "slave"
// device, then it's eligible to be enrolled by a "master" device (which could
// be an Android app).
const char kIsBootstrappingSlave[] = "is_oobe_bootstrapping_slave";

// A boolean preference controlling Android status reporting.
const char kReportArcStatusEnabled[] = "arc.status_reporting_enabled";

// Dictionary indicating current network bandwidth throttling settings.
// Contains a boolean (is throttling enabled) and two integers (upload rate
// and download rate in kbits/s to throttle to)
const char kNetworkThrottlingEnabled[] = "net.throttling_enabled";

// Integer pref used by the metrics::DailyEvent owned by
// chromeos::PowerMetricsReporter.
const char kPowerMetricsDailySample[] = "power.metrics.daily_sample";

// Integer prefs used to back event counts reported by
// chromeos::PowerMetricsReporter.
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

// Whether camera-produced media files have been consolidated to one place.
const char kCameraMediaConsolidated[] = "camera_media_consolidated";

// Whether the user is allowed to disconnect and configure VPN connections.
const char kVpnConfigAllowed[] = "vpn_config_allowed";
#endif  // defined(OS_CHROMEOS)

// Whether there is a Flash version installed that supports clearing LSO data.
const char kClearPluginLSODataEnabled[] = "browser.clear_lso_data_enabled";

// Whether we should show Pepper Flash-specific settings.
const char kPepperFlashSettingsEnabled[] =
    "browser.pepper_flash_settings_enabled";

// String which specifies where to store the disk cache.
const char kDiskCacheDir[] = "browser.disk_cache_dir";
// Pref name for the policy specifying the maximal cache size.
const char kDiskCacheSize[] = "browser.disk_cache_size";
// Pref name for the policy specifying the maximal media cache size.
const char kMediaCacheSize[] = "browser.media_cache_size";

// Specifies the release channel that the device should be locked to.
// Possible values: "stable-channel", "beta-channel", "dev-channel", or an
// empty string, in which case the value will be ignored.
// TODO(dubroy): This preference may not be necessary once
// http://crosbug.com/17015 is implemented and the update engine can just
// fetch the correct value from the policy.
const char kChromeOsReleaseChannel[] = "cros.system.releaseChannel";

const char kPerformanceTracingEnabled[] =
    "feedback.performance_tracing_enabled";

// Boolean indicating whether tabstrip uses stacked layout (on touch devices).
// Defaults to false.
const char kTabStripStackedLayout[] = "tab-strip-stacked-layout";

// Indicates that factory reset was requested from options page or reset screen.
const char kFactoryResetRequested[] = "FactoryResetRequested";

// Presence of this value indicates that a TPM firmware update has been
// requested. The value indicates the requested update mode.
const char kFactoryResetTPMFirmwareUpdateMode[] =
    "FactoryResetTPMFirmwareUpdateMode";

// Indicates that debugging features were requested from oobe screen.
const char kDebuggingFeaturesRequested[] = "DebuggingFeaturesRequested";

#if defined(OS_CHROMEOS)
// This setting controls initial device timezone that is used before user
// session started. It is controlled by device owner.
const char kSigninScreenTimezone[] = "settings.signin_screen_timezone";

// This setting starts periodic timezone refresh when not in user session.
// (user session is controlled by user profile preference
// kResolveTimezoneByGeolocation)
//
// Deprecated. Superseeded by kResolveDeviceTimezoneByGeolocationMethod.
// TODO(alemate): https://crbug.com/783367 Remove outdated prefs.
const char kResolveDeviceTimezoneByGeolocation[] =
    "settings.resolve_device_timezone_by_geolocation";

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
#endif  // defined(OS_CHROMEOS)

// Pref name for the policy controlling whether to enable Media Router.
const char kEnableMediaRouter[] = "media_router.enable_media_router";
#if !defined(OS_ANDROID)
// Pref name for the policy controlling whether to force the Cast icon to be
// shown in the toolbar/overflow menu.
const char kShowCastIconInToolbar[] = "media_router.show_cast_icon_in_toolbar";
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// Pref name for the policy controlling the way in which users are notified of
// the need to relaunch the browser for a pending update.
const char kRelaunchNotification[] = "browser.relaunch_notification";
// Pref name for the policy controlling the time period over which users are
// notified of the need to relaunch the browser for a pending update. Values
// are in milliseconds.
const char kRelaunchNotificationPeriod[] =
    "browser.relaunch_notification_period";
#endif  // !defined(OS_ANDROID)

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

#if defined(OS_MACOSX)
// Set to true if the user removed our login item so we should not create a new
// one when uninstalling background apps.
const char kUserRemovedLoginItem[] = "background_mode.user_removed_login_item";

// Set to true if Chrome already created a login item, so there's no need to
// create another one.
const char kChromeCreatedLoginItem[] =
  "background_mode.chrome_created_login_item";

// Set to true once we've initialized kChromeCreatedLoginItem for the first
// time.
const char kMigratedLoginItemPref[] =
  "background_mode.migrated_login_item_pref";

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

// List of protocol handlers.
const char kRegisteredProtocolHandlers[] =
  "custom_handlers.registered_protocol_handlers";

// List of protocol handlers the user has requested not to be asked about again.
const char kIgnoredProtocolHandlers[] =
  "custom_handlers.ignored_protocol_handlers";

// List of protocol handlers registered by policy.
const char kPolicyRegisteredProtocolHandlers[] =
    "custom_handlers.policy.registered_protocol_handlers";

// List of protocol handlers the policy has requested to be ignored.
const char kPolicyIgnoredProtocolHandlers[] =
    "custom_handlers.policy.ignored_protocol_handlers";

// Whether user-specified handlers for protocols and content types can be
// specified.
const char kCustomHandlersEnabled[] = "custom_handlers.enabled";

// Integer that specifies the policy refresh rate for device-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
const char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";

#if !defined(OS_ANDROID)
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
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// This value stores chrome icon's index in the launcher. This should be handled
// separately with app shortcut's index because of ShelfModel's backward
// compatibility. If we add chrome icon index to |kPinnedLauncherApps|, its
// index is also stored in the |kPinnedLauncherApp| pref. It may causes
// creating two chrome icons.
const char kShelfChromeIconIndex[] = "shelf_chrome_icon_index";

const char kPinnedLauncherApps[] = "pinned_launcher_apps";
const char kPolicyPinnedLauncherApps[] = "policy_pinned_launcher_apps";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
// Counts how many more times the 'profile on a network share' warning should be
// shown to the user before the next silence period.
const char kNetworkProfileWarningsLeft[] = "network_profile.warnings_left";
// Tracks the time of the last shown warning. Used to reset
// |network_profile.warnings_left| after a silence period.
const char kNetworkProfileLastWarningTime[] =
    "network_profile.last_warning_time";
#endif

#if defined(OS_CHROMEOS)
// The RLZ brand code, if enabled.
const char kRLZBrand[] = "rlz.brand";
// Whether RLZ pings are disabled.
const char kRLZDisabled[] = "rlz.disabled";
#endif

#if BUILDFLAG(ENABLE_APP_LIST)
// Keeps local state of app list while sync service is not available.
const char kAppListLocalState[] = "app_list.local_state";
#endif

// An integer that is incremented whenever changes are made to app shortcuts.
// Increasing this causes all app shortcuts to be recreated.
const char kAppShortcutsVersion[] = "apps.shortcuts_version";

// A string pref for storing the salt used to compute the pepper device ID.
const char kDRMSalt[] = "settings.privacy.drm_salt";
// A boolean pref that enables the (private) pepper GetDeviceID() call and
// enables the use of remote attestation for content protection.
const char kEnableDRM[] = "settings.privacy.drm_enabled";

// An integer per-profile pref that signals if the watchdog extension is
// installed and active. We need to know if the watchdog extension active for
// ActivityLog initialization before the extension system is initialized.
const char kWatchdogExtensionActive[] =
    "profile.extensions.activity_log.num_consumers_active";

#if defined(OS_ANDROID)
// A list of partner bookmark rename/remove mappings.
// Each list item is a dictionary containing a "url", a "provider_title" and
// "mapped_title" entries, detailing the bookmark target URL (if any), the title
// given by the PartnerBookmarksProvider and either the user-visible renamed
// title or an empty string if the bookmark node was removed.
const char kPartnerBookmarkMappings[] = "partnerbookmarks.mappings";
#endif  // defined(OS_ANDROID)

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

// Whether PAC scripts are given a stripped https:// URL (enabled), or
// the full URL for https:// (disabled).
//
// This is a security feature which is on by default, and prevents PAC
// scripts (which may have been sourced in an untrusted manner) from
// having access to data that is ordinarily protected by a TLS channel
// (i.e. the path and query components of an https:// URL).
//
// This preference is not exposed in the UI, but is overridable using
// a Policy (PacHttpsUrlStrippingEnabled), or using a commandline
// flag --unsafe-pac-url.
//
// The ability to turn off this security feature is not intended to be
// a long-lived feature, but rather an escape-hatch for enterprises
// while rolling out the change to PAC.
const char kPacHttpsUrlStrippingEnabled[] =
    "proxy.pac_https_url_stripping_enabled";

// Whether Guest Mode is enabled within the browser.
const char kBrowserGuestModeEnabled[] = "profile.browser_guest_enabled";

// Whether Adding a new Person is enabled within the user manager.
const char kBrowserAddPersonEnabled[] = "profile.add_person_enabled";

// Whether profile can be used before sign in.
const char kForceBrowserSignin[] = "profile.force_browser_signin";

// Boolean which indicates if the user is allowed to sign into Chrome on the
// next startup.
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";

// Device identifier used by CryptAuth stored in local state. This ID is
// combined with a user ID before being registered with the CryptAuth server,
// so it can't correlate users on the same device.
// Note: This constant was previously specific to EasyUnlock, so the string
//       constant contains "easy_unlock".
const char kCryptAuthDeviceId[] = "easy_unlock.device_id";

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

// A dictionary that maps from supervised user whitelist IDs to their properties
// (name and a list of clients that registered the whitelist).
const char kRegisteredSupervisedUserWhitelists[] =
    "supervised_users.whitelists";

// Boolean that specifies whether the cloud policy will override conflicting
// machine policy.
const char kCloudPolicyOverridesMachinePolicy[] = "policy.cloud_override";

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
// Boolean that indicates whether Chrome enterprise cloud reporting is enabled
// or not.
const char kCloudReportingEnabled[] =
    "enterprise_reporting.chrome_cloud_reporting";
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Policy that indicates how to handle animated images.
const char kAnimationPolicy[] = "settings.a11y.animation_policy";

// A list of URLs (for U2F) or domains (for webauthn) that automatically permit
// direct attestation of a Security Key.
const char kSecurityKeyPermitAttestation[] = "securitykey.permit_attestation";
#endif

const char kBackgroundTracingLastUpload[] = "background_tracing.last_upload";

const char kAllowDinosaurEasterEgg[] =
    "allow_dinosaur_easter_egg";

#if defined(OS_ANDROID)
// Whether the update menu item was clicked. Used to facilitate logging whether
// Chrome was updated after the menu item is clicked.
const char kClickedUpdateMenuItem[] = "omaha.clicked_update_menu_item";
// The latest version of Chrome available when the user clicked on the update
// menu item.
const char kLatestVersionWhenClickedUpdateMenuItem[] =
    "omaha.latest_version_when_clicked_upate_menu_item";
#endif

// Whether or not the user has explicitly set the cloud services preference
// through the first run flow.
const char kMediaRouterCloudServicesPrefSet[] =
    "media_router.cloudservices.prefset";
// Whether or not the user has enabled cloud services with Media Router.
const char kMediaRouterEnableCloudServices[] =
    "media_router.cloudservices.enabled";
// Whether or not the Media Router first run flow has been acknowledged by the
// user.
const char kMediaRouterFirstRunFlowAcknowledged[] =
    "media_router.firstrunflow.acknowledged";
// Whether or not the user has enabled Media Remoting. Defaults to true.
const char kMediaRouterMediaRemotingEnabled[] =
    "media_router.media_remoting.enabled";
// A list of website origins on which the user has chosen to use tab mirroring.
const char kMediaRouterTabMirroringSources[] =
    "media_router.tab_mirroring_sources";

// The base64-encoded representation of the public key to use to validate origin
// trial token signatures.
const char kOriginTrialPublicKey[] = "origin_trials.public_key";

// A list of origin trial features to disable by policy.
const char kOriginTrialDisabledFeatures[] = "origin_trials.disabled_features";

// A list of origin trial tokens to disable by policy.
const char kOriginTrialDisabledTokens[] = "origin_trials.disabled_tokens";

// Policy that indicates the state of updates for the binary components.
const char kComponentUpdatesEnabled[] =
    "component_updates.component_updates_enabled";

#if defined(OS_ANDROID)
// The current level of backoff for showing the location settings dialog for the
// default search engine.
const char kLocationSettingsBackoffLevelDSE[] =
    "location_settings_backoff_level_dse";

// The current level of backoff for showing the location settings dialog for
// sites other than the default search engine.
const char kLocationSettingsBackoffLevelDefault[] =
    "location_settings_backoff_level_default";

// The next time the location settings dialog can be shown for the default
// search engine.
const char kLocationSettingsNextShowDSE[] = "location_settings_next_show_dse";

// The next time the location settings dialog can be shown for sites other than
// the default search engine.
const char kLocationSettingsNextShowDefault[] =
    "location_settings_next_show_default";

// Whether the search geolocation disclosure has been dismissed by the user.
const char kSearchGeolocationDisclosureDismissed[] =
    "search_geolocation_disclosure.dismissed";

// How many times the search geolocation disclosure has been shown.
const char kSearchGeolocationDisclosureShownCount[] =
    "search_geolocation_disclosure.shown_count";

// When the disclosure was shown last.
const char kSearchGeolocationDisclosureLastShowDate[] =
    "search_geolocation_disclosure.last_show_date";

// Whether the metrics for the state of geolocation pre-disclosure being shown
// have been recorded.
const char kSearchGeolocationPreDisclosureMetricsRecorded[] =
    "search_geolocation_pre_disclosure_metrics_recorded";

// Whether the metrics for the state of geolocation post-disclosure being shown
// have been recorded.
const char kSearchGeolocationPostDisclosureMetricsRecorded[] =
    "search_geolocation_post_disclosure_metrics_recorded";
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

#if defined(OS_WIN)
// True if the user is eligible to recieve "desktop to iOS" promotion. This
// vlaue is set by a job that access the growth table to check users with iOS
// devices and phone recovery number and update the eligibility on chrome sync.
const char kIOSPromotionEligible[] = "ios.desktoptomobileeligible";

// True if the "desktop to iOS" promotion was successful, i.e. user installed
// the application and signed in on the iOS client after seeing the promotion
// and receiving the SMS.
const char kIOSPromotionDone[] = "ios.desktop_ios_promo_done";

// Index of the entry point that last initiated sending the SMS to the user for
// the "desktop to iOS" promotion (see DesktopIOSPromotion.IOSSigninReason
// histogram for details).
const char kIOSPromotionSMSEntryPoint[] =
    "ios.desktop_ios_promo_sms_entrypoint";

// Bit mask that represents the Indices of all the entry points shown to the
// user for "desktop to iOS" promotion. Each entry point is represented by
// 1<<entrypoint_value using the values from the Enum
// desktop_ios_promotion::PromotionEntryPoint.
const char kIOSPromotionShownEntryPoints[] =
    "ios.desktop_ios_promo_shown_entrypoints";

// Timestamp of the last "desktop to iOS" promotion last impression. If the
// user sends SMS on that impression then we deal with this timestamp as the
// SMS sending time because after sending the sms the user shouldn't see the
// promotion again (Accuracy to the minutes and seconds is not important).
const char kIOSPromotionLastImpression[] =
    "ios.desktop_ios_promo_last_impression";

// Integer that represents which variation of title and text of the
// "desktop to iOS" promotion was presented to the user.
const char kIOSPromotionVariationId[] = "ios.desktop_ios_promo_variation_id";

// Number of times user has seen the "desktop to iOS" save passwords bubble
// promotion.
const char kNumberSavePasswordsBubbleIOSPromoShown[] =
    "savepasswords_bubble_ios_promo_shown_count";

// True if the user has dismissed the "desktop to iOS" save passwords bubble
// promotion.
const char kSavePasswordsBubbleIOSPromoDismissed[] =
    "savepasswords_bubble_ios_promo_dismissed";

// Number of times the user has seen the "desktop to iOS" bookmarks bubble
// promotion.
const char kNumberBookmarksBubbleIOSPromoShown[] =
    "bookmarks_bubble_ios_promo_shown_count";

// True if the user has dismissed the "desktop to iOS" bookmarks bubble
// promotion.
const char kBookmarksBubbleIOSPromoDismissed[] =
    "bookmarks_bubble_ios_promo_dismissed";

// Number of times user has seen the "desktop to iOS" bookmarks foot note
// promotion.
const char kNumberBookmarksFootNoteIOSPromoShown[] =
    "bookmarks_footnote_ios_promo_shown_count";

// True if the user has dismissed the "desktop to iOS" bookmarks foot note
// promotion.
const char kBookmarksFootNoteIOSPromoDismissed[] =
    "bookmarks_footnote_ios_promo_dismissed";

// Number of times user has seen the "desktop to iOS" history page promotion.
const char kNumberHistoryPageIOSPromoShown[] =
    "history_page_ios_promo_shown_count";

// True if the user has dismissed the "desktop to iOS" history page promotion.
const char kHistoryPageIOSPromoDismissed[] = "history_page_ios_promo_dismissed";

#if defined(GOOGLE_CHROME_BUILD)
// Acts as a cache to remember incompatible applications through restarts. Used
// for the Incompatible Applications Warning feature.
const char kIncompatibleApplications[] = "incompatible_applications";

// Contains the MD5 digest of the current module blacklist cache. Used to detect
// external tampering.
const char kModuleBlacklistCacheMD5Digest[] =
    "module_blacklist_cache_md5_digest";

// Acts as a cache to remember problematic programs through restarts. Used for
// the Incompatible Applications Warning feature.
// Note: Deprecated. Renamed to kIncompatibleApplications.
const char kProblematicPrograms[] = "problematic_programs";

// A boolean value, controlling whether third party software is allowed to
// inject into Chrome's processes.
const char kThirdPartyBlockingEnabled[] = "third_party_blocking_enabled";
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

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

#if defined(OS_ANDROID)
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

// A list of origins (URLs) to treat as "secure origins" for debugging purposes.
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely_treat_insecure_origin_as_secure";

// A list of origins (URLs) that specifies opting into --isolate-origins=...
// (selective Site Isolation).
const char kIsolateOrigins[] = "site_isolation.isolate_origins";

// Boolean that specifies opting into --site-per-process (full Site Isolation).
const char kSitePerProcess[] = "site_isolation.site_per_process";

// Boolean that specifies if the web driver flag is allowed to override policies
// which prevent it from operating normally. (e.g. SitePerProcess.)
const char kWebDriverOverridesIncompatiblePolicies[] =
    "webdriver.override_incompatible_policy";

#if !defined(OS_ANDROID)
// Boolean that specifies whether media (audio/video) autoplay is allowed.
const char kAutoplayAllowed[] = "media.autoplay_allowed";

// Holds URL patterns that specify URLs that will be allowed to autoplay.
const char kAutoplayWhitelist[] = "media.autoplay_whitelist";

// Boolean that specifies whether autoplay blocking is enabled.
const char kBlockAutoplayEnabled[] = "media.block_autoplay";
#endif  // !defined(OS_ANDROID)

// Integer that holds the value of the next persistent notification ID to be
// used.
const char kNotificationNextPersistentId[] = "persistent_notifications.next_id";

// Preference for controlling whether tab lifecycles
// (throttling/freezing/discarding) are enabled.
const char kTabLifecyclesEnabled[] = "tab_lifecycles_enabled";

// Boolean that enables the Enterprise Hardware Platform Extension API for
// extensions installed by enterprise policy.
const char kEnterpriseHardwarePlatformAPIEnabled[] =
    "enterprise_hardware_platform_api.enabled";

}  // namespace prefs
