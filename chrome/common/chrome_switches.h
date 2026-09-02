// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the shared command-line switches used by code in the Chrome
// directory that don't have anywhere more specific to go.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include <string_view>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/chrome_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

// Don't add more switch files here. This is linked into some places like the
// installer where dependencies should be limited. Instead, have files
// directly include your switch file.

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// media/base/media_switches.cc or ui/gl/gl_switches.cc or one of the
// .cc files corresponding to the *_switches.h files included above
// instead.
//
// Want to remove obsolete switches? Ensure that the switch isn't still in use
// by the Android Java code (ChromeSwitches.java.tmpl) under an aliased name.
// Also perform a string search to make sure the switch isn't in use only by a
// build-configuration, e.g. BUILDFLAG(GOOGLE_CHROME_BRANDING), that is not
// indexed for cross-reference or built by the CQ bots.
// -----------------------------------------------------------------------------

// All switches in alphabetical order.

// Specifies Accept-Language to send to servers and expose to JavaScript via the
// navigator.language DOM property. language[-country] where language is the 2
// letter code from ISO-639.
inline constexpr char kAcceptLang[] = "accept-lang";

#if BUILDFLAG(IS_MAC)
// Only if we're running in an unsigned build, passing this flag will allow
// app shims whose code signature does not match what chrome is expecting to
// still connect to chrome. This is used by some tests to allow the test to
// pretend to be a valid app shim.
inline constexpr char kAllowAppShimSignatureMismatchForTests[] =
    "allow-appshim-signature-mismatch-for-tests";

#endif

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
inline constexpr char kAllowCrossOriginAuthPrompt[] =
    "allow-cross-origin-auth-prompt";

// Allow non-secure origins to use the screen capture API and the desktopCapture
// extension API.
inline constexpr char kAllowHttpScreenCapture[] = "allow-http-screen-capture";

// By default, an https page cannot run JavaScript, CSS or plugins from http
// URLs. This provides an override to get the old insecure behavior.
inline constexpr char kAllowRunningInsecureContent[] =
    "allow-running-insecure-content";

// Allows Web Push notifications that do not show a notification.
inline constexpr char kAllowSilentPush[] = "allow-silent-push";

// Allows an unpacked Perfetto UI extension to be trusted.
inline constexpr char kAllowUnpackedPerfettoExtension[] =
    "allow-unpacked-perfetto-extension";

// Allows DevTools frontend from remote origins to load local file:// resources.
// This should only be enabled when explicitly needed for remote debugging
// with local source maps.
inline constexpr char kAllowUnsafeDevToolsRemoteFileLoading[] =
    "allow-unsafe-devtools-remote-file-loading";

// Specifies that the associated value should be launched in "application"
// mode.
inline constexpr char kApp[] = "app";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
inline constexpr char kAppId[] = "app-id";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kAppId) ==
              std::string_view(ash::chrome_switches::kAppId));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Overrides the launch url of an app with the specified url. This is used
// along with kAppId to launch a given app with the url corresponding to an item
// in the app's shortcuts menu.
inline constexpr char kAppLaunchUrlForShortcutsMenuItem[] =
    "app-launch-url-for-shortcuts-menu-item";

// This is used along with kAppId to indicate an app was launched during
// OS login, and which mode the app was launched in.
inline constexpr char kAppRunOnOsLoginMode[] = "app-run-on-os-login-mode";

// A process type (switches::kProcessType) that is used by App Shim processes.
// See chrome/app_shim/app_shim_main_delegate.mm.
inline constexpr char kAppShim[] = "app-shim";

// Overrides the update url used by webstore extensions.
inline constexpr char kAppsGalleryUpdateURL[] = "apps-gallery-update-url";

// Allowlist for Negotiate Auth servers
inline constexpr char kAuthServerAllowlist[] = "auth-server-allowlist";

// This flag makes Chrome auto-open DevTools window for each tab. It is
// intended to be used by developers and automation to not require user
// interaction for opening DevTools.
inline constexpr char kAutoOpenDevToolsForTabs[] =
    "auto-open-devtools-for-tabs";

// This flag makes Chrome auto-select the provided choice when an extension asks
// permission to start desktop capture. Should only be used for tests. For
// instance, --auto-select-desktop-capture-source="Entire screen" will
// automatically select sharing the entire screen in English locales. The switch
// value only needs to be substring of the capture source name, i.e. "display"
// would match "Built-in display" and "External display", whichever comes first.
inline constexpr char kAutoSelectDesktopCaptureSource[] =
    "auto-select-desktop-capture-source";

// This flag makes Chrome auto-select any screen when an extension asks
// permission to start desktop capture. Should only be used for tests.
// kAutoSelectDesktopCaptureSource (see above) can be also be used to
// auto-select screens. But it have the problem that you need to know the name
// of a screen to auto-select it. The name of screens can't be set, are
// different for different platforms, and are different if you have one or
// several screens. So it's hard to use for auto-selecting screens.
// This flag does not care what the screen name is, but it also gives no
// control. Any screen could be chosen. It is useful in tests where we don't
// care which screen is auto-selected.
inline constexpr char kAutoSelectScreenCaptureSource[] =
    "auto-select-screen-capture-source";

// This flag makes Chrome auto-select a tab with the provided title when
// the media-picker should otherwise be displayed to the user. This switch
// is very similar to kAutoSelectDesktopCaptureSource, but limits selection
// to tabs. This solves the issue of kAutoSelectDesktopCaptureSource being
// liable to accidentally capturing the Chromium window instead of the tab,
// as both have the same title if the tab is focused.
inline constexpr char kAutoSelectTabCaptureSourceByTitle[] =
    "auto-select-tab-capture-source-by-title";

// This flag makes Chrome auto-select a window with the provided title when
// the media-picker should otherwise be displayed to the user. This switch
// is very similar to kAutoSelectDesktopCaptureSource, but limits selection
// to the window.
inline constexpr char kAutoSelectWindowCaptureSourceByTitle[] =
    "auto-select-window-capture-source-by-title";

// Automatically signs the user into Chrome when signing in to other Google
// services on the web. This makes it easier for automated browsers to sign in.
inline constexpr char kBrowserSigninAutoAccept[] =
    "auto-accept-browser-signin-for-tests";

// If specified, allows syncing multiple profiles to the same account. Used for
// multi-client E2E tests.
inline constexpr char kBypassAccountAlreadyUsedByAnotherProfileCheck[] =
    "bypass-account-already-used-by-another-profile-check";

// This flag makes Chrome auto-reject requests capture a tab/window/screen.
inline constexpr char kCaptureAutoReject[] = "auto-reject-capture";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
inline constexpr char kCheckForUpdateIntervalSec[] =
    "check-for-update-interval";

// Comma-separated list of SSL cipher suites to disable.
inline constexpr char kCipherSuiteBlacklist[] = "cipher-suite-blacklist";

// Prints licensing information (same content as found in about:credits) and
// quits.
inline constexpr char kCredits[] = "credits";

// Specifies the http:// endpoint which will be used to serve
// devtools://devtools/custom/<path>
// Or a file:// URL to specify a custom file path to load from for
// devtools://devtools/bundled/<path>
inline constexpr char kCustomDevtoolsFrontend[] = "custom-devtools-frontend";

// Adds debugging entries such as Inspect Element to context menus of packed
// apps.
inline constexpr char kDebugPackedApps[] = "debug-packed-apps";

// Passes command line parameters to the DevTools front-end.
inline constexpr char kDevToolsFlags[] = "devtools-flags";

// Specifies a stringified JSON dictionary defining allowlist and blocklist
// pattern rules for DevTools-controlled navigations.
// If an allowlist is provided (even if empty), all top-level navigations to
// non-matching URLs are blocked. If both allowlist and blocklist match a URL,
// the more specific pattern determines the outcome.
// Example: '{ \
//             "allowlist": ["[*.]foo.com"], \
//             "blocklist": ["[*.]bar.com"] \
//           }'
inline constexpr char kDevToolsNavigationGatingRules[] =
    "devtools-navigation-gating-rules";

// Triggers a plethora of diagnostic modes.
inline constexpr char kDiagnostics[] = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
inline constexpr char kDiagnosticsFormat[] = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
inline constexpr char kDiagnosticsRecovery[] = "diagnostics-recovery";

#if BUILDFLAG(IS_CHROMEOS)
// Disables the auto maximize feature on ChromeOS so that a browser window
// always starts in normal state. This is used by tests that do not want this
// auto maximizing behavior.
inline constexpr char kDisableAutoMaximizeForTests[] =
    "disable-auto-maximize-for-tests";

#endif

// Disable auto-reload of pages on top-level error.
inline constexpr char kDisableAutoReload[] = "disable-auto-reload";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
inline constexpr char kDisableBackgroundNetworking[] =
    "disable-background-networking";

// Disable default component extensions with background pages - useful for
// performance tests where these pages may interfere with perf results.
inline constexpr char kDisableComponentExtensionsWithBackgroundPages[] =
    "disable-component-extensions-with-background-pages";

inline constexpr char kDisableComponentUpdate[] = "disable-component-update";

// Disables crashpad initialization for testing. The crashpad binary will not
// run, and thus will not detect and symbolize crashes.
inline constexpr char kDisableCrashpadForTesting[] =
    "disable-crashpad-for-testing";

// Disables installation of default apps on first run. This is used during
// automated testing.
inline constexpr char kDisableDefaultApps[] = "disable-default-apps";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kDisableDefaultApps) ==
              std::string_view(ash::chrome_switches::kDisableDefaultApps));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Disables Domain Reliability Monitoring.
inline constexpr char kDisableDomainReliability[] =
    "disable-domain-reliability";

// Disables lazy loading of images and frames.
inline constexpr char kDisableLazyLoading[] = "disable-lazy-loading";

// Disables print preview (For testing, and for users who don't like us. :[ )
inline constexpr char kDisablePrintPreview[] = "disable-print-preview";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
inline constexpr char kDisablePromptOnRepost[] = "disable-prompt-on-repost";

// Disable stack profiling. Stack profiling may change performance. Disabling
// stack profiling is beneficial when comparing performance metrics with a
// build that has it disabled by default.
inline constexpr char kDisableStackProfiler[] = "disable-stack-profiler";

// Disable startup of the updater process.
inline constexpr char kDisableUpdaterScheduler[] = "disable-updater-scheduler";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
inline constexpr char kDiskCacheDir[] = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
inline constexpr char kDiskCacheSize[] = "disk-cache-size";

#if BUILDFLAG(IS_MAC)
// Skips initializing the shares NSApplication instance in ChromeTestSuite.
inline constexpr char kDoNotCreateNSAppForTests[] =
    "do-not-create-nsapp-for-tests";

#endif

// Do not de-elevate the browser on launch. Used after de-elevating to prevent
// infinite loops.
inline constexpr char kDoNotDeElevateOnLaunch[] = "do-not-de-elevate";

// Requests that a running browser process dump its collected histograms to a
// given file. The file is overwritten if it exists.
inline constexpr char kDumpBrowserHistograms[] = "dump-browser-histograms";

// If the WebRTC logging private API is active, enables audio debug recordings.
inline constexpr char kEnableAudioDebugRecordingsFromExtension[] =
    "enable-audio-debug-recordings-from-extension";

// Enable auto-reload of pages on top-level error.
inline constexpr char kEnableAutoReload[] = "enable-auto-reload";

// Enables the multi-level undo system for bookmarks.
inline constexpr char kEnableBookmarkUndo[] = "enable-bookmark-undo";

// Enables Domain Reliability Monitoring.
inline constexpr char kEnableDomainReliability[] = "enable-domain-reliability";

#if BUILDFLAG(IS_CHROMEOS)
// If enabled, DevTools will allow creating pwa_handler, to enable executing
// CDP methods (i.e. PWA.install) on browsers connected remotely
inline constexpr char kEnableDevToolsPwaHandler[] =
    "enable-devtools-pwa-handler";

#endif

// Enables logging for extension activity.
inline constexpr char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

inline constexpr char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Force enabling HangoutServicesExtension.
inline constexpr char kEnableHangoutServicesExtensionForTesting[] =
    "enable-hangout-services-extension-for-testing";

// Enables the network-related benchmarking extensions.
inline constexpr char kEnableNetBenchmarking[] = "enable-net-benchmarking";

// Enables a number of potentially annoying security features (strict mixed
// content mode, powerful feature restrictions, etc.)
inline constexpr char kEnablePotentiallyAnnoyingSecurityFeatures[] =
    "enable-potentially-annoying-security-features";

// Enables verbose debug logs for Talk to Chrome (TTC).
inline constexpr char kEnableTtcDebugLogs[] = "enable-ttc-debug-logs";

// Allows experimental ai extension APIs to be used in stable channel.
// This disables chrome sign-in if set, regardless of channel.
inline constexpr char kExperimentalAiStableChannel[] =
    "experimental-ai-stable-channel";

// Allows overriding the list of restricted ports by passing a comma-separated
// list of port numbers.
inline constexpr char kExplicitlyAllowedPorts[] = "explicitly-allowed-ports";

// Name of the command line flag to allow the ai data collection extension API.
inline constexpr char kExtensionAiDataCollection[] =
    "enable-extension-ai-data-collection";

// Name of the command line flag to force content verification to be on in one
// of various modes.
inline constexpr char kExtensionContentVerification[] =
    "extension-content-verification";

// Values for the kExtensionContentVerification flag.
// See ContentVerifierDelegate::Mode for more explanation.
inline constexpr char kExtensionContentVerificationBootstrap[] = "bootstrap";

inline constexpr char kExtensionContentVerificationEnforce[] = "enforce";

inline constexpr char kExtensionContentVerificationEnforceStrict[] =
    "enforce_strict";

// Name of the command line flag to allow the experimental actor API.
inline constexpr char kExtensionExperimentalActor[] =
    "enable-extension-actor-api";

// Specifies the variation of Zero State extensions toolbar recommendation to
// show.
// When a user with zero extensions installed clicks on the extensions puzzle
// piece in the Chrome toolbar, Chrome displays a submenu suggesting the user
// to explore the Chrome Web Store.
// Forces application mode. This hides certain system UI elements and forces
// the app to be installed if it hasn't been already.
inline constexpr char kForceAppMode[] = "force-app-mode";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kForceAppMode) ==
              std::string_view(ash::chrome_switches::kForceAppMode));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
inline constexpr char kForceFirstRun[] = "force-first-run";

// Forces immediate platform policy refresh (not cloud policy) when Chrome is
// already running. The switch prevents a new browser window from opening and
// only triggers the policy refresh. Useful for testing and automation to avoid
// waiting for the next scheduled refresh interval. No-op if Chrome is not
// already running.
inline constexpr char kRefreshPlatformPolicy[] = "refresh-platform-policy";

// Displays the What's New experience when the browser is started if it has not
// yet been shown for the current milestone (this overrides kNoFirstRun, without
// showing the First Run experience).
inline constexpr char kForceWhatsNew[] = "force-whats-new";

// Does not show the crash restore bubble when the browser is started during the
// system startup phase in ChromeOS, if the ChromeOS full restore feature is
// enabled, because the ChromeOS full restore notification is shown for the user
// to select restore or not.
inline constexpr char kHideCrashRestoreBubble[] = "hide-crash-restore-bubble";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kHideCrashRestoreBubble) ==
              std::string_view(ash::chrome_switches::kHideCrashRestoreBubble));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Specifies which page will be displayed in newly-opened tabs. We need this
// for testing purposes so that the UI tests don't depend on what comes up for
// http://google.com.
inline constexpr char kHomePage[] = "homepage";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kHomePage) ==
              std::string_view(ash::chrome_switches::kHomePage));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// Causes the browser to simulate a screen lock event shortly after startup.
// Optional value specifies the delay in seconds (defaults to 5).
// Used for manual testing of Smart Restart.
inline constexpr char kSimulateLockScreenSmartRestart[] =
    "simulate-lock-screen-smart-restart";

// Triggers the import of passwords on startup.
inline constexpr char kImportPasswords[] = "import-passwords";

#endif

// Causes the initial browser opened to be in incognito mode. Further browsers
// may or may not be in incognito mode; see `IncognitoModePrefs`.
inline constexpr char kIncognito[] = "incognito";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kIncognito) ==
              std::string_view(ash::chrome_switches::kIncognito));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Specifies that the main-thread Isolate should initialize in foreground mode.
// If not specified, the the Isolate will start in background mode for extension
// processes and foreground mode otherwise.
inline constexpr char kInitIsolateAsForeground[] = "init-isolate-as-foreground";

// Installs an autogenerated theme based on the given RGB value.
// The format is "r,g,b", where r, g, b are a numeric values from 0 to 255.
inline constexpr char kInstallAutogeneratedTheme[] =
    "install-autogenerated-theme";

// Causes Chrome to initiate an installation flow for the given app.
inline constexpr char kInstallChromeApp[] = "install-chrome-app";

// Causes Chrome to install the unsigned Web Bundle at the given path as a
// developer mode Isolated Web App.
inline constexpr char kInstallIsolatedWebAppFromFile[] =
    "install-isolated-web-app-from-file";

// Causes Chrome to install a developer mode Isolated Web App whose contents
// are hosted at the given HTTP(S) URL.
inline constexpr char kInstallIsolatedWebAppFromUrl[] =
    "install-isolated-web-app-from-url";

// Marks a renderer as an Instant process.
inline constexpr char kInstantProcess[] = "instant-process";

// Used for testing - keeps browser alive after last browser window closes.
inline constexpr char kKeepAliveForTest[] = "keep-alive-for-test";

// Enable kiosk mode. Please note this is not Chrome OS kiosk mode.
inline constexpr char kKioskMode[] = "kiosk";

// Enable automatically pressing the print button in print preview.
inline constexpr char kKioskModePrinting[] = "kiosk-printing";

// Makes Chrome default browser
inline constexpr char kMakeDefaultBrowser[] = "make-default-browser";

// Requests a native messaging connection be established between the native
// messaging host named by this switch and the extension with ID specified by
// kNativeMessagingConnectExtension.
inline constexpr char kNativeMessagingConnectHost[] =
    "native-messaging-connect-host";

// Requests a native messaging connection be established between the extension
// with ID specified by this switch and the native messaging host named by the
// kNativeMessagingConnectHost switch.
inline constexpr char kNativeMessagingConnectExtension[] =
    "native-messaging-connect-extension";

// If set when kNativeMessagingConnectHost and kNativeMessagingConnectExtension
// are specified, is reflected to the native messaging host as a command line
// parameter.
inline constexpr char kNativeMessagingConnectId[] =
    "native-messaging-connect-id";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
inline constexpr char kNoDefaultBrowserCheck[] = "no-default-browser-check";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
inline constexpr char kNoExperiments[] = "no-experiments";

// Skip First Run tasks as well as not showing additional dialogs, prompts or
// bubbles. Suppressing dialogs, prompts, and bubbles is important as this
// switch is used by automation (including performance benchmarks) where it's
// important only a browser window is shown.
//
// This may not actually be the first run or the What's New page. Its effect can
// be partially ignored by adding kForceFirstRun (for FRE), kForceWhatsNew (for
// What's New) and/or kIgnoreNoFirstRunForSearchEngineChoiceScreen (for the DSE
// choice screen). This does not drop the First Run sentinel and thus doesn't
// prevent first run from occurring the next time chrome is launched without
// this flag. It also does not update the last What's New milestone, so does not
// prevent What's New from occurring the next time chrome is launched without
// this flag.
inline constexpr char kNoFirstRun[] = "no-first-run";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kNoFirstRun) ==
              std::string_view(ash::chrome_switches::kNoFirstRun));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Don't send hyperlink auditing pings
inline constexpr char kNoPings[] = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
inline constexpr char kNoProxyServer[] = "no-proxy-server";

// Does not automatically open a browser window on startup (used when
// launching Chrome for the purpose of hosting background apps).
inline constexpr char kNoStartupWindow[] = "no-startup-window";

// Overrides the default URL for the Notebook Home WebUI.
inline constexpr char kNotebookHomeURL[] = "notebook-home-url";

// Calculate the hash of an MHTML file as it is being saved.
// The browser process will write the serialized MHTML contents to a file and
// calculate its hash as it is streamed back from the renderer via a Mojo data
// pipe.
inline constexpr char kOnTheFlyMhtmlHashComputation[] =
    "on-the-fly-mhtml-hash-computation";

// Directly launches the Omnibox Everywhere desktop UI widget.
inline constexpr char kOmniboxEverywhere[] = "omnibox-everywhere";

// Launches URL in new browser window.
inline constexpr char kOpenInNewWindow[] = "new-window";

// Activates an existing tab or app window by URL or app id before creating
// anything new. Syntax: comma-ordered selectors. Bare URLs are exact.
// Add a trailing * for prefix. app:<app-id> targets PWAs.
// Example: --focus=https://meet.google.com/*,app:abc123
inline constexpr char kFocus[] = "focus";

// Specifies a file path to write JSON focus result information.
inline constexpr char kFocusResultFile[] = "focus-result-file";

// Packages an extension to a .crx installable file from a given directory.
inline constexpr char kPackExtension[] = "pack-extension";

// Optional PEM private key to use in signing packaged .crx.
inline constexpr char kPackExtensionKey[] = "pack-extension-key";

// Causes the browser process to crash very early in startup, just before
// crashpad (or breakpad) is initialized.
inline constexpr char kPreCrashpadCrashTest[] = "pre-crashpad-crash-test";

// Used to mock the response received from the Web Permission Prediction
// Service. Used for testing.
inline constexpr char kPredictionServiceMockLikelihood[] =
    "prediction-service-mock-likelihood";

// A directory where Chrome looks for json files describing default/preinstalled
// web apps. This overrides any default directory to load preinstalled web apps
// from.
inline constexpr char kPreinstalledWebAppsDir[] = "preinstalled-web-apps-dir";

// Use IPv6 only for privet HTTP.
inline constexpr char kPrivetIPv6Only[] = "privet-ipv6-only";

// Outputs the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
inline constexpr char kProductVersion[] = "product-version";

// Selects directory of profile to associate with the first browser launched.
inline constexpr char kProfileDirectory[] = "profile-directory";

// If provided with kProfileDirectory, does not create the profile if the
// profile directory doesn't exist.
inline constexpr char kIgnoreProfileDirectoryIfNotExists[] =
    "ignore-profile-directory-if-not-exists";

// Like kProfileDirectory, but selects the profile by email address. If the
// email is not found in any existing profile, this switch has no effect. If
// both kProfileDirectory and kProfileEmail are specified, kProfileDirectory
// takes priority.
inline constexpr char kProfileEmail[] = "profile-email";

// If provided with kProfileEmail, prompts the user to create a new profile with
// kProfileEmail as the email address if that email is not found in any existing
// profile.
inline constexpr char kCreateProfileEmailIfNotExists[] =
    "create-profile-email-if-not-exists";

// Forces proxy auto-detection.
inline constexpr char kProxyAutoDetect[] = "proxy-auto-detect";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_host_matching_rules.h" for the format of these
// rules.
inline constexpr char kProxyBypassList[] = "proxy-bypass-list";

// Uses the pac script at the given URL
inline constexpr char kProxyPacUrl[] = "proxy-pac-url";

// Uses a specified proxy server, overrides system settings.
inline constexpr char kProxyServer[] = "proxy-server";

// Provides a list of addresses to discover DevTools remote debugging targets.
// The format is <host>:<port>,...,<host>:port.
inline constexpr char kRemoteDebuggingTargets[] = "remote-debugging-targets";

// Indicates that all corrupted extensions should be repaired if they are
// are enabled by policy. This is mainly used after a user data downgrade.
inline constexpr char kRepairAllValidExtensions[] =
    "repair-all-valid-extensions";

// Indicates that Chrome was restarted (e.g., after a flag change). This is used
// to ignore the launch when recording the Launch.Mode2 metric.
inline constexpr char kRestart[] = "restart";

// Indicates the last session should be restored on startup. This overrides the
// preferences value. Note that this does not force automatic session restore
// following a crash, so as to prevent a crash loop. This switch is used to
// implement support for OS-specific "continue where you left off" functionality
// on OS X and Windows.
inline constexpr char kRestoreLastSession[] = "restore-last-session";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kRestoreLastSession) ==
              std::string_view(ash::chrome_switches::kRestoreLastSession));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Indicates that the URL in the command line should open in the active tab
// instead of a new tab. In case of multiple URLS given as arguments, the
// first one will replace the active tab.
inline constexpr char kSameTab[] = "same-tab";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
inline constexpr char kSilentDebuggerExtensionAPI[] =
    "silent-debugger-extension-api";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
inline constexpr char kSilentLaunch[] = "silent-launch";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kSilentLaunch) ==
              std::string_view(ash::chrome_switches::kSilentLaunch));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Sets the BrowsingDataLifetime policy to a very short value (shorter than
// normally possible) for testing purposes.
inline constexpr char kSimulateBrowsingDataLifetime[] =
    "simulate-browsing-data-lifetime";

// Simulates a critical update being available.
inline constexpr char kSimulateCriticalUpdate[] = "simulate-critical-update";

// Simulates that current version is outdated.
inline constexpr char kSimulateOutdated[] = "simulate-outdated";

// Simulates that current version is outdated and auto-update is off.
inline constexpr char kSimulateOutdatedNoAU[] = "simulate-outdated-no-au";

// Simulates an update being available.
inline constexpr char kSimulateUpgrade[] = "simulate-upgrade";

// Sets the IdleTimeout policy to a very short value (shorter than normally
// possible) for testing purposes.
inline constexpr char kSimulateIdleTimeout[] = "simulate-idle-timeout";

// Specifies the maximum SSL/TLS version ("tls1.2" or "tls1.3").
inline constexpr char kSSLVersionMax[] = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1.2" or "tls1.3").
inline constexpr char kSSLVersionMin[] = "ssl-version-min";

// TLS 1.2 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
inline constexpr char kSSLVersionTLSv12[] = "tls1.2";

// TLS 1.3 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
inline constexpr char kSSLVersionTLSv13[] = "tls1.3";

// Starts the browser maximized, regardless of any previous settings.
inline constexpr char kStartMaximized[] = "start-maximized";

// Starts the stack sampling profiler in the child process.
inline constexpr char kStartStackProfiler[] = "start-stack-profiler";

// Browser test mode for the |kStartStackProfiler| switch. Limits the profile
// durations to be significantly less than the test timeout. On ChromeOS,
// forces the stack sampling profiler to run on all processes as well.
inline constexpr char kStartStackProfilerBrowserTest[] = "browser-test";

// Interval, in minutes, used for storage pressure notification throttling.
// Useful for developers testing applications that might use non-trivial
// amounts of disk space.
inline constexpr char kStoragePressureNotificationInterval[] =
    "storage-pressure-notification-interval";

// This flag sets the checkboxes for sharing system audio during window or
// screen capture to on by default. It is primarily intended to be used for
// tests.
inline constexpr char kSystemAudioCaptureDefaultChecked[] =
    "system-audio-capture-default_checked";

// This flag sets the checkboxes for sharing audio during tab capture to off
// by default. It is primarily intended to be used for tests.
inline constexpr char kTabCaptureAudioDefaultUnchecked[] =
    "tab-capture-audio-default-unchecked";

// These flags make Chrome auto-accept/reject requests to capture the current
// tab. It should only be used for tests.
inline constexpr char kThisTabCaptureAutoAccept[] =
    "auto-accept-this-tab-capture";

inline constexpr char kThisTabCaptureAutoReject[] =
    "auto-reject-this-tab-capture";

// Custom delay for memory log. This should be used only for testing purpose.
inline constexpr char kTestMemoryLogDelayInMinutes[] =
    "test-memory-log-delay-in-minutes";

// Identifies a list of download sources as trusted, but only if proper group
// policy is set.
inline constexpr char kTrustedDownloadSources[] = "trusted-download-sources";

// Specifies the TalkToChrome bundle URL.
inline constexpr char kTtcBundleUrl[] = "ttc-bundle-url";

// Overrides per-origin quota settings to unlimited storage for any
// apps/origins.  This should be used only for testing purpose.
inline constexpr char kUnlimitedStorage[] = "unlimited-storage";

// Disables warnings about self-XSS attacks when pasting into the DevTools
// console.
inline constexpr char kUnsafelyDisableDevToolsSelfXssWarnings[] =
    "unsafely-disable-devtools-self-xss-warnings";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
inline constexpr char kUserDataDir[] = "user-data-dir";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kUserDataDir) ==
              std::string_view(ash::chrome_switches::kUserDataDir));
#endif  // BUILDFLAG(IS_CHROMEOS)

// Uses WinHttp to resolve proxies instead of using Chromium's normal proxy
// resolution logic. This is only supported in Windows.
//
// TODO(crbug.com/40111093): Only use WinHttp whenever Chrome is
// exclusively using system proxy configs.
inline constexpr char kUseSystemProxyResolver[] = "use-system-proxy-resolver";

// Examines a .crx for validity and prints the result.
inline constexpr char kValidateCrx[] = "validate-crx";

// Prints version information and quits.
inline constexpr char kVersion[] = "version";

// Sets the delay (in seconds) between proactive prunings of remote-bound
// WebRTC event logs which are pending upload.
// All positive values are legal.
// All negative values are illegal, and ignored.
// If set to 0, the meaning is "no proactive pruning".
inline constexpr char kWebRtcRemoteEventLogProactivePruningDelta[] =
    "webrtc-event-log-proactive-pruning-delta";

// WebRTC event logs will only be uploaded if the conditions hold for this
// many milliseconds.
inline constexpr char kWebRtcRemoteEventLogUploadDelayMs[] =
    "webrtc-event-log-upload-delay-ms";

// Normally, remote-bound WebRTC event logs are uploaded only when no
// peer connections are active. With this flag, the upload is never suppressed.
inline constexpr char kWebRtcRemoteEventLogUploadNoSuppression[] =
    "webrtc-event-log-upload-no-suppression";

// Override WebRTC IP handling policy to mimic the behavior when WebRTC IP
// handling policy is specified in Preferences.
inline constexpr char kWebRtcIPHandlingPolicy[] = "webrtc-ip-handling-policy";

// Force What's New on Desktop to request from the staging environment.
inline constexpr char kWhatsNewUseStaging[] = "whats-new-use-staging";

// Specify the initial window user title: --window-name="My custom title"
inline constexpr char kWindowName[] = "window-name";

// Specify the initial window position: --window-position=x,y
inline constexpr char kWindowPosition[] = "window-position";

// Specify the initial window size: --window-size=w,h
inline constexpr char kWindowSize[] = "window-size";

// Specify the initial window workspace: --window-workspace=id
inline constexpr char kWindowWorkspace[] = "window-workspace";

// Uses WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is to
// use Chromium's network stack to fetch, and V8 to evaluate.
inline constexpr char kWinHttpProxyResolver[] = "winhttp-proxy-resolver";

// Specifies which category option was clicked in the Windows Jumplist that
// resulted in a browser startup.
inline constexpr char kWinJumplistAction[] = "win-jumplist-action";

#if BUILDFLAG(IS_ANDROID)
// If enabled Entra SSO will accept authentication headers from a specific list
// of non-production Microsoft Authentication broker apps.
inline constexpr char kAndroidEntraSsoAllowDebugBrokers[] =
    "android-entra-sso-allow-debug-brokers";

// Android authentication account type for SPNEGO authentication
inline constexpr char kAuthAndroidNegotiateAccountType[] =
    "auth-spnego-account-type";

// Disable the default browser promo.
inline constexpr char kDisableDefaultBrowserPromo[] =
    "disable-default-browser-promo";

// Forces the night mode to be enabled.
inline constexpr char kForceEnableNightMode[] = "force-enable-night-mode";

// Forces the update menu badge to show.
inline constexpr char kForceShowUpdateMenuBadge[] =
    "force-show-update-menu-badge";

// Forces a custom summary to be displayed below the update menu item.
inline constexpr char kForceShowUpdateMenuItemCustomSummary[] =
    "custom_summary";

// Forces the update menu type to a specific type.
inline constexpr char kForceUpdateMenuType[] = "force-update-menu-type";

// Sets the market URL for Chrome for use in testing.
inline constexpr char kMarketUrlForTesting[] = "market-url-for-testing";

// Force enable user agent overrides to request desktop sites in Clank.
inline constexpr char kRequestDesktopSites[] = "request-desktop-sites";

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// If enabled, overrides the target playout delay for a casting mirroring
// session. The value will be parsed as milliseconds. Lowering this value will
// result in a lower end to end latency, but could come at the cost of other
// quality standards such as dropped frames or FPS.
inline constexpr char kCastMirroringTargetPlayoutDelay[] =
    "cast-mirroring-target-playout-delay";

#endif

#if !BUILDFLAG(IS_CHROMEOS)
// Enables saving webpages as MHTML (Webpage, Single) by default, instead of
// saving as HTML with a directory of sub-resources. (Webpage, Complete).
// See http://crbug.com/40179885 for how to remove this switch.
inline constexpr char kSavePageAsMHTML[] = "save-page-as-mhtml";

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
// These flags show the man page on Linux. They are equivalent to each
// other.
inline constexpr char kHelp[] = "help";

inline constexpr char kHelpShort[] = "h";

// The same as the --class argument in X applications.  Overrides the WM_CLASS
// window property with the given value.
inline constexpr char kWmClass[] = "class";

#endif

#if BUILDFLAG(IS_MAC)
// Prevents Chrome from quitting when Chrome Apps are open.
inline constexpr char kAppsKeepChromeAliveInTests[] =
    "apps-keep-chrome-alive-in-tests";

// Enable user metrics from within the installer.
inline constexpr char kEnableUserMetrics[] = "enable-user-metrics";

// This is how the metrics client ID is passed from the browser process to its
// children. With Crashpad, the metrics client ID is distinct from the crash
// client ID.
inline constexpr char kMetricsClientID[] = "metrics-client-id";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
inline constexpr char kRelauncherProcess[] = "relauncher";

// When switches::kProcessType is switches::kRelauncherProcess, if this switch
// is also present, the relauncher process will unmount and eject a mounted disk
// image and move its disk image file to the trash.  The argument's value must
// be a BSD device name of the form "diskN" or "diskNsM".
inline constexpr char kRelauncherProcessDMGDevice[] = "dmg-device";

// Indicates whether Chrome should be set as the default browser during
// installation.
inline constexpr char kMakeChromeDefault[] = "make-chrome-default";

// A process type (switches::kProcessType) that cleans up the browser's
// temporary code sign clone.
inline constexpr char kCodeSignCloneCleanupProcess[] =
    "code-sign-clone-cleanup";

// When switches::kProcessType is switches::kCodeSignCloneCleanupProcess this
// switch is required. The value must be the unique suffix portion of the
// temporary directory that contains the clone. The full path will be
// reconstructed by the cleanup process.
inline constexpr char kUniqueTempDirSuffix[] = "unique-temp-dir-suffix";

// A process type that exits cleanly with code 0 without performing work.
inline constexpr char kNoOpForTestingProcess[] = "no-op-for-testing";

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// Force-enables the profile shortcut manager. This is needed for tests since
// they use a custom-user-data-dir which disables this.
inline constexpr char kEnableProfileShortcutManager[] =
    "enable-profile-shortcut-manager";

// Indicates that this launch of the browser originated from the Legacy Browser
// Support for Edge extension's native host. This is recorded in UMA.
inline constexpr char kFromBrowserSwitcher[] = "from-browser-switcher";

// Indicates that this launch of the browser originated from the installer
// (i.e., following a successful new install or over-install). This triggers
// browser behaviors for this specific launch, such as a welcome announcement
// for accessibility software (see https://crbug.com/40685905).
inline constexpr char kFromInstaller[] = "from-installer";

// Makes Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This only
// shows an error box because the only way to hide Chrome is by uninstalling
// it.
inline constexpr char kHideIcons[] = "hide-icons";

// Whether or not the browser should warn if the profile is on a network share.
// This flag is only relevant for Windows currently.
inline constexpr char kNoNetworkProfileWarning[] = "no-network-profile-warning";

// Whether this process should PrefetchVirtualMemory on the contents of
// Chrome.dll. This warms up the pages in memory to speed up startup but might
// not be required in later renderers and/or GPU. For experiment info see
// crbug.com/40234091.
inline constexpr char kNoPreReadMainDll[] = "no-pre-read-main-dll";

// Used in combination with kNotificationLaunchId to specify the inline reply
// entered in the toast in the Windows Action Center.
inline constexpr char kNotificationInlineReply[] = "notification-inline-reply";

// Used for launching Chrome when a toast displayed in the Windows Action Center
// has been activated. Should contain the launch ID encoded by Chrome.
inline constexpr char kNotificationLaunchId[] = "notification-launch-id";

// Specifies the version of the Progressive-Web-App launcher that launched
// Chrome, used to determine whether to update all launchers.
// NOTE: changing this switch requires adding legacy handling for the previous
// method, as older PWA launchers still using this switch will rely on Chrome to
// update them to use the new method.
inline constexpr char kPwaLauncherVersion[] = "pwa-launcher-version";

// See kHideIcons.
inline constexpr char kShowIcons[] = "show-icons";

// When rendezvousing with an existing process, used to indicate that the
// StartupInfoW of the new Chrome process had dwFlags == STARTF_TITLEISAPPID.
// This is used to record launch metrics.
inline constexpr char kSourceAppId[] = "source-app-id";

// When rendezvousing with an existing process, used to pass the path of the
// shortcut that launched the new Chrome process. This is used to record launch
// metrics.
inline constexpr char kSourceShortcut[] = "source-shortcut";

// Identifies Chrome instances that start in foreground mode at startup to
// record related metrics.
inline constexpr char kStartupForegroundLaunch[] = "startup-foreground-launch";

// Runs un-installation steps that were done by chrome first-run.
inline constexpr char kUninstall[] = "uninstall";

// Specifies that the WebApp with the specified id should be uninstalled.
inline constexpr char kUninstallAppId[] = "uninstall-app-id";

// Specifies that the browser is running isolated and should not attempt to
// start a second isolated browser.
inline constexpr char kIsolated[] = "isolated";

// Passes the Win32 HANDLE value (as an integer) of the parent process
// to wait for during relaunch.
inline constexpr char kWaitForParentHandle[] = "wait-for-parent-handle";

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
// Enables support to debug printing subsystem.
inline constexpr char kDebugPrint[] = "debug-print";

#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
// Causes the browser to launch directly in guest mode.
inline constexpr char kGuest[] = "guest";

#endif

// Overrides the glic guest URL.
inline constexpr char kGlicGuestURL[] = "glic-guest-url";

inline constexpr char kSkillsV2Origin[] = "skills-v2-origin";

// Overrides the Gemini Enterprise settings JSON dictionary for local
// development.
inline constexpr char kGlicGeminiEnterpriseSettingsOverride[] =
    "glic-gemini-enterprise-settings-override";

inline constexpr char kGlicAlwaysOpenFre[] = "glic-always-open-fre";

inline constexpr char kGlicAlwaysSkipFre[] = "glic-always-skip-fre";

inline constexpr char kGlicExperimentalFreURL[] = "glic-experimental-fre-url";

inline constexpr char kGlicShortcutsLearnMoreURL[] =
    "glic-shortcuts-learn-more-url";

// Use --glic-open-on-startup=attached or --glic-open-on-startup=detached.
inline constexpr char kGlicOpenOnStartup[] = "glic-open-on-startup";

// List of allowed origins in the glic webview, as a space-separated list.
inline constexpr char kGlicAllowedOrigins[] = "glic-webui-allowed-origins";

// Automation is intended to be passed in addition to glic-dev. It further
// disables functionality to make basic testing easier.
inline constexpr char kGlicAutomation[] = "glic-automation";

// Dev mode for glic only exposed via command line flag.
inline constexpr char kGlicDev[] = "glic-dev";

// If this flag is set, then the page navigating will not trigger a reload.
inline constexpr char kGlicSkipReloadAfterNavigation[] =
    "glic-skip-reload-after-navigation";

// Whether additional logging is enabled in the glic api host.
inline constexpr char kGlicHostLogging[] = "glic-host-logging";

// List of URL patterns in the glic webview to redirect to an admin blocked
// panel, as a space-separated list.
inline constexpr char kGlicAdminRedirectPatterns[] =
    "glic-admin-redirect-patterns";

// Whether to show web actuation toggle in the Chrome AI settings page.
inline constexpr char kGlicAlwaysShowWebActuationToggle[] =
    "glic-always-show-web-actuation-toggle";

// Configure preset guest URLs for manual testing. These are saved to local
// state prefs and can be selected to override the default glic guest URL
// through corresponding entries in chrome://flags.
inline constexpr char kGlicGuestUrlPresetAutopush[] =
    "glic-guest-url-preset-autopush";

inline constexpr char kGlicGuestUrlPresetStaging[] =
    "glic-guest-url-preset-staging";

inline constexpr char kGlicGuestUrlPresetPreprod[] =
    "glic-guest-url-preset-preprod";

inline constexpr char kGlicGuestUrlPresetProd[] = "glic-guest-url-preset-prod";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Writes open and installed web apps for each profile to the specified file
// without launching a new browser window or tab. Pass a absolute file path
// to specify where to output the information. Can be used together with
// optional
// --profile-base-name switch to only write information for a given profile.
inline constexpr char kListApps[] = "list-apps";

// Pass the basename of the profile directory to specify which profile to get
// information. Only relevant when used with --list-apps switch.
inline constexpr char kProfileBaseName[] = "profile-base-name";

// Domains and associated SAML attributes for which third-party profile
// management should be enabled. Input should be in JSON format.
inline constexpr char kProfileManagementAttributes[] =
    "profile-management-attributes";

#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// Custom WebAPK server URL for the sake of testing.
inline constexpr char kWebApkServerUrl[] = "webapk-server-url";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kWebApkServerUrl) ==
              std::string_view(ash::chrome_switches::kWebApkServerUrl));
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// Uses the system default printer as the initially selected destination in
// print preview, instead of the most recently used destination.
inline constexpr char kUseSystemDefaultPrinter[] = "use-system-default-printer";

#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
// Indicates that this process is the product of a relaunch following migration
// of User Data.
inline constexpr char kUserDataMigrated[] = "user-data-migrated";

#endif

#if BUILDFLAG(CHROME_FOR_TESTING)
// Overrides the behavior of the sign-in dialog when creating a new profile for
// an enterprise account.
// Valid values are "accept-new-profile", "accept-current-profile", and
// "cancel".
inline constexpr char kEnterpriseSigninDialogBehaviorForTesting[] =
    "enterprise-signin-dialog-behavior-for-testing";

#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
