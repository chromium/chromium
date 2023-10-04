// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in:
// ash/constants/ash_switches.cc
// base/base_switches.cc
// etc.
//
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------
// Specifies Accept-Language to send to servers and expose to JavaScript via the
// navigator.language DOM property. language[-country] where language is the 2
// letter code from ISO-639.
const char kAcceptLang[] = "accept-lang";

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[] = "allow-cross-origin-auth-prompt";

// Allow non-secure origins to use the screen capture API and the desktopCapture
// extension API.
const char kAllowHttpScreenCapture[] = "allow-http-screen-capture";

// Allows profiles to be created outside of the user data dir.
// TODO(https://crbug.com/1060366): Various places in Chrome assume that all
// profiles are within the user data dir. Some tests need to violate that
// assumption. The switch should be removed after this workaround is no longer
// needed.
const char kAllowProfilesOutsideUserDir[] = "allow-profiles-outside-user-dir";

// By default, an https page cannot run JavaScript, CSS or plugins from http
// URLs. This provides an override to get the old insecure behavior.
const char kAllowRunningInsecureContent[] = "allow-running-insecure-content";

// Allows Web Push notifications that do not show a notification.
const char kAllowSilentPush[] = "allow-silent-push";

// Specifies that the associated value should be launched in "application"
// mode.
const char kApp[] = "app";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[] = "app-id";

// Overrides the launch url of an app with the specified url. This is used
// along with kAppId to launch a given app with the url corresponding to an item
// in the app's shortcuts menu.
const char kAppLaunchUrlForShortcutsMenuItem[] =
    "app-launch-url-for-shortcuts-menu-item";

// Value of GAIA auth code for --force-app-mode.
const char kAppModeAuthCode[] = "app-mode-auth-code";

// Value of OAuth2 refresh token for --force-app-mode.
const char kAppModeOAuth2Token[] = "app-mode-oauth-token";

// This is used along with kAppId to indicate an app was launched during
// OS login, and which mode the app was launched in.
const char kAppRunOnOsLoginMode[] = "app-run-on-os-login-mode";

// The URL that the webstore APIs download extensions from.
// Note: the URL must contain one '%s' for the extension ID.
const char kAppsGalleryDownloadURL[] = "apps-gallery-download-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[] = "apps-gallery-update-url";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[] = "apps-gallery-url";

// Allowlist for Negotiate Auth servers
const char kAuthServerAllowlist[] = "auth-server-allowlist";

// This flag makes Chrome auto-open DevTools window for each tab. It is
// intended to be used by developers and automation to not require user
// interaction for opening DevTools.
const char kAutoOpenDevToolsForTabs[] = "auto-open-devtools-for-tabs";

// This flag makes Chrome auto-select the provided choice when an extension asks
// permission to start desktop capture. Should only be used for tests. For
// instance, --auto-select-desktop-capture-source="Entire screen" will
// automatically select sharing the entire screen in English locales. The switch
// value only needs to be substring of the capture source name, i.e. "display"
// would match "Built-in display" and "External display", whichever comes first.
const char kAutoSelectDesktopCaptureSource[] =
    "auto-select-desktop-capture-source";

// This flag makes Chrome auto-select a tab with the provided title when
// the media-picker should otherwise be displayed to the user. This switch
// is very similar to kAutoSelectDesktopCaptureSource, but limits selection
// to tabs. This solves the issue of kAutoSelectDesktopCaptureSource being
// liable to accidentally capturing the Chromium window instead of the tab,
// as both have the same title if the tab is focused.
const char kAutoSelectTabCaptureSourceByTitle[] =
    "auto-select-tab-capture-source-by-title";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[] = "check-for-update-interval";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[] = "cipher-suite-blacklist";

// Comma-separated list of BrowserThreads that cause browser process to crash if
// the given browser thread is not responsive. UI/IO are the BrowserThreads that
// are supported.
//
// For example:
//    --crash-on-hang-threads=UI:18,IO:18 --> Crash the browser if UI or IO is
//    not responsive for 18 seconds while the other browser thread is
//    responsive.
const char kCrashOnHangThreads[] = "crash-on-hang-threads";

// Some platforms like ChromeOS default to empty desktop.
// Browser tests may need to add this switch so that at least one browser
// instance is created on startup.
// TODO(nkostylev): Investigate if this switch could be removed.
// (http://crbug.com/148675)
const char kCreateBrowserOnStartupForTests[] =
    "create-browser-on-startup-for-tests";

// Prints licensing information (same content as found in about:credits) and
// quits.
const char kCredits[] = "credits";

// Specifies the http:// endpoint which will be used to serve
// devtools://devtools/custom/<path>
// Or a file:// URL to specify a custom file path to load from for
// devtools://devtools/bundled/<path>
const char kCustomDevtoolsFrontend[] = "custom-devtools-frontend";

// Adds debugging entries such as Inspect Element to context menus of packed
// apps.
const char kDebugPackedApps[] = "debug-packed-apps";

// Passes command line parameters to the DevTools front-end.
const char kDevToolsFlags[] = "devtools-flags";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[] = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
const char kDiagnosticsFormat[] = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
const char kDiagnosticsRecovery[] = "diagnostics-recovery";

#if BUILDFLAG(IS_CHROMEOS)
// Disables the auto maximize feature on ChromeOS so that a browser window
// always starts in normal state. This is used by tests that do not want this
// auto maximizing behavior.
const char kDisableAutoMaximizeForTests[] = "disable-auto-maximize-for-tests";
#endif

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
const char kDisableBackgroundNetworking[] = "disable-background-networking";

// Disable default component extensions with background pages - useful for
// performance tests where these pages may interfere with perf results.
const char kDisableComponentExtensionsWithBackgroundPages[] =
    "disable-component-extensions-with-background-pages";

#if BUILDFLAG(ENABLE_COMPONENT_UPDATER)
const char kDisableComponentUpdate[] = "disable-component-update";
#endif

// Disables installation of default apps on first run. This is used during
// automated testing.
const char kDisableDefaultApps[] = "disable-default-apps";

// Disables Domain Reliability Monitoring.
const char kDisableDomainReliability[] = "disable-domain-reliability";

// Disable extensions.
const char kDisableExtensions[] = "disable-extensions";

// Disable extensions except those specified in a comma-separated list.
const char kDisableExtensionsExcept[] = "disable-extensions-except";

// Disables lazy loading of images and frames.
const char kDisableLazyLoading[] = "disable-lazy-loading";

// Disables NaCl. If kEnableNaCl is also set, this switch takes precedence.
const char kDisableNaCl[] = "disable-nacl";

// Disables print preview (For testing, and for users who don't like us. :[ )
const char kDisablePrintPreview[] = "disable-print-preview";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[] = "disable-prompt-on-repost";

// Disable stack profiling. Stack profiling may change performance. Disabling
// stack profiling is beneficial when comparing performance metrics with a
// build that has it disabled by default.
const char kDisableStackProfiler[] = "disable-stack-profiler";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/119175)
const char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[] = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[] = "disk-cache-size";

// Requests that a running browser process dump its collected histograms to a
// given file. The file is overwritten if it exists.
const char kDumpBrowserHistograms[] = "dump-browser-histograms";

// If the WebRTC logging private API is active, enables audio debug recordings.
const char kEnableAudioDebugRecordingsFromExtension[] =
    "enable-audio-debug-recordings-from-extension";

// Enables the multi-level undo system for bookmarks.
const char kEnableBookmarkUndo[] = "enable-bookmark-undo";

// This applies only when the process type is "service". Enables the Cloud Print
// Proxy component within the service process.
const char kEnableCloudPrintProxy[] = "enable-cloud-print-proxy";

// Enables CriticalPersistedTabData - redesign/replacement for TabState
const char kEnableCriticalPersistedTabData[] =
    "enable-critical-persisted-tab-data";

// Enables Domain Reliability Monitoring.
const char kEnableDomainReliability[] = "enable-domain-reliability";

// Enables a number of UI improvements to downloads, download scanning, and
// download warnings.
const char kEnableDownloadWarningImprovements[] =
    "enable-download-warning-improvements";

// Enables the early process singleton feature. The process singleton will be
// held for the whole lifetime of BrowserImpl (see https://crbug.com/1340599).
const char kEnableEarlyProcessSingleton[] = "enable-early-process-singleton";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

const char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Force enabling HangoutServicesExtension.
const char kEnableHangoutServicesExtensionForTesting[] =
    "enable-hangout-services-extension-for-testing";

#if BUILDFLAG(IS_CHROMEOS)
// Makes Lacros use a location shared across users for browser components.
const char kEnableLacrosSharedComponentsDir[] =
    "enable-lacros-shared-components-dir";
#endif

// Allows NaCl to run in all contexts (such as open web). Note that
// kDisableNaCl disables NaCl in all contexts and takes precedence.
const char kEnableNaCl[] = "enable-nacl";

// Enables the network-related benchmarking extensions.
const char kEnableNetBenchmarking[] = "enable-net-benchmarking";

// Enables a number of potentially annoying security features (strict mixed
// content mode, powerful feature restrictions, etc.)
const char kEnablePotentiallyAnnoyingSecurityFeatures[] =
    "enable-potentially-annoying-security-features";

// Allows overriding the list of restricted ports by passing a comma-separated
// list of port numbers.
const char kExplicitlyAllowedPorts[] = "explicitly-allowed-ports";

// Name of the command line flag to force content verification to be on in one
// of various modes.
const char kExtensionContentVerification[] = "extension-content-verification";

// Values for the kExtensionContentVerification flag.
// See ContentVerifierDelegate::Mode for more explanation.
const char kExtensionContentVerificationBootstrap[] = "bootstrap";
const char kExtensionContentVerificationEnforce[] = "enforce";
const char kExtensionContentVerificationEnforceStrict[] = "enforce_strict";

// Turns on extension install verification if it would not otherwise have been
// turned on.
const char kExtensionsInstallVerification[] = "extensions-install-verification";

// Specifies a comma-separated list of extension ids that should be forced to
// be treated as not from the webstore when doing install verification.
const char kExtensionsNotWebstore[] = "extensions-not-webstore";

// Forces application mode. This hides certain system UI elements and forces
// the app to be installed if it hasn't been already.
const char kForceAppMode[] = "force-app-mode";

#if BUILDFLAG(IS_CHROMEOS)
// Forces developer tools availability, no matter what values the enterprise
// policies DeveloperToolsDisabled and DeveloperToolsAvailability are set to.
const char kForceDevToolsAvailable[] = "force-devtools-available";
#endif

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
const char kForceFirstRun[] = "force-first-run";

// Displays the What's New experience when the browser is started if it has not
// yet been shown for the current milestone (this overrides kNoFirstRun, without
// showing the First Run experience).
const char kForceWhatsNew[] = "force-whats-new";

// Does not show the crash restore bubble when the browser is started during the
// system startup phase in ChromeOS, if the ChromeOS full restore feature is
// enabled, because the ChromeOS full restore notification is shown for the user
// to select restore or not.
const char kHideCrashRestoreBubble[] = "hide-crash-restore-bubble";

// Specifies which page will be displayed in newly-opened tabs. We need this
// for testing purposes so that the UI tests don't depend on what comes up for
// http://google.com.
const char kHomePage[] = "homepage";

// Causes the initial browser opened to be in incognito mode. Further browsers
// may or may not be in incognito mode; see `IncognitoModePrefs`.
const char kIncognito[] = "incognito";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Manually sets the initial preferences file. This is required to change the
// initial preferences when the default file is read-only (eg. on lacros).
// Passing this flag will reset the preferences regardless of whether this is
// the first run.
const char kInitialPreferencesFile[] = "initial-preferences-file";
#endif

// Installs an autogenerated theme based on the given RGB value.
// The format is "r,g,b", where r, g, b are a numeric values from 0 to 255.
const char kInstallAutogeneratedTheme[] = "install-autogenerated-theme";

// Causes Chrome to initiate an installation flow for the given app.
const char kInstallChromeApp[] = "install-chrome-app";

// Causes Chrome to install the unsigned Web Bundle at the given path as a
// developer mode Isolated Web App.
const char kInstallIsolatedWebAppFromFile[] =
    "install-isolated-web-app-from-file";

// Causes Chrome to install a developer mode Isolated Web App whose contents
// are hosted at the given HTTP(S) URL.
const char kInstallIsolatedWebAppFromUrl[] =
    "install-isolated-web-app-from-url";

// Marks a renderer as an Instant process.
const char kInstantProcess[] = "instant-process";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[] = "keep-alive-for-test";

// Enable kiosk mode. Please note this is not Chrome OS kiosk mode.
const char kKioskMode[] = "kiosk";

// Enable automatically pressing the print button in print preview.
const char kKioskModePrinting[] = "kiosk-printing";

// Makes Chrome default browser
const char kMakeDefaultBrowser[] = "make-default-browser";

// Allows setting a different destination ID for connection-monitoring GCM
// messages. Useful when running against a non-prod management server.
const char kMonitoringDestinationID[] = "monitoring-destination-id";

// Requests a native messaging connection be established between the native
// messaging host named by this switch and the extension with ID specified by
// kNativeMessagingConnectExtension.
const char kNativeMessagingConnectHost[] = "native-messaging-connect-host";

// Requests a native messaging connection be established between the extension
// with ID specified by this switch and the native messaging host named by the
// kNativeMessagingConnectHost switch.
const char kNativeMessagingConnectExtension[] =
    "native-messaging-connect-extension";

// If set when kNativeMessagingConnectHost and kNativeMessagingConnectExtension
// are specified, is reflected to the native messaging host as a command line
// parameter.
const char kNativeMessagingConnectId[] = "native-messaging-connect-id";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[] = "no-default-browser-check";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
const char kNoExperiments[] = "no-experiments";

// Skip First Run tasks, whether or not it's actually the First Run, and the
// What's New page. Overridden by kForceFirstRun (for FRE) and kForceWhatsNew
// (for What's New). This does not drop the First Run sentinel and thus doesn't
// prevent first run from occurring the next time chrome is launched without
// this flag. It also does not update the last What's New milestone, so does not
// prevent What's New from occurring the next time chrome is launched without
// this flag.
const char kNoFirstRun[] = "no-first-run";

// Don't send hyperlink auditing pings
const char kNoPings[] = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[] = "no-proxy-server";

// Disables the service process from adding itself as an autorun process. This
// does not delete existing autorun registrations, it just prevents the service
// from registering a new one.
const char kNoServiceAutorun[] = "no-service-autorun";

// Does not automatically open a browser window on startup (used when
// launching Chrome for the purpose of hosting background apps).
const char kNoStartupWindow[] = "no-startup-window";

// Calculate the hash of an MHTML file as it is being saved.
// The browser process will write the serialized MHTML contents to a file and
// calculate its hash as it is streamed back from the renderer via a Mojo data
// pipe.
const char kOnTheFlyMhtmlHashComputation[] =
    "on-the-fly-mhtml-hash-computation";

// Launches URL in new browser window.
const char kOpenInNewWindow[] = "new-window";

// Packages an extension to a .crx installable file from a given directory.
const char kPackExtension[] = "pack-extension";

// Optional PEM private key to use in signing packaged .crx.
const char kPackExtensionKey[] = "pack-extension-key";

// Causes the browser process to crash very early in startup, just before
// crashpad (or breakpad) is initialized.
const char kPreCrashpadCrashTest[] = "pre-crashpad-crash-test";

// Used to mock the response received from the Web Permission Prediction
// Service. Used for testing.
const char kPredictionServiceMockLikelihood[] =
    "prediction-service-mock-likelihood";

// A directory where Chrome looks for json files describing default/preinstalled
// web apps. This overrides any default directory to load preinstalled web apps
// from.
const char kPreinstalledWebAppsDir[] = "preinstalled-web-apps-dir";

// Use IPv6 only for privet HTTP.
const char kPrivetIPv6Only[] = "privet-ipv6-only";

// Outputs the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
const char kProductVersion[] = "product-version";

// Selects directory of profile to associate with the first browser launched.
const char kProfileDirectory[] = "profile-directory";

// Like kProfileDirectory, but selects the profile by email address. If the
// email is not found in any existing profile, this switch has no effect. If
// both kProfileDirectory and kProfileUserName are specified, kProfileDirectory
// takes priority.
const char kProfileEmail[] = "profile-email";

// Forces proxy auto-detection.
const char kProxyAutoDetect[] = "proxy-auto-detect";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";

// Uses the pac script at the given URL
const char kProxyPacUrl[] = "proxy-pac-url";

// Porvides a list of addresses to discover DevTools remote debugging targets.
// The format is <host>:<port>,...,<host>:port.
const char kRemoteDebuggingTargets[] = "remote-debugging-targets";

// Indicates that Chrome was restarted (e.g., after a flag change). This is used
// to ignore the launch when recording the Launch.Mode2 metric.
const char kRestart[] = "restart";

// Indicates the last session should be restored on startup. This overrides the
// preferences value. Note that this does not force automatic session restore
// following a crash, so as to prevent a crash loop. This switch is used to
// implement support for OS-specific "continue where you left off" functionality
// on OS X and Windows.
const char kRestoreLastSession[] = "restore-last-session";

// Disable saving pages as HTML-only, disable saving pages as HTML Complete
// (with a directory of sub-resources). Enable only saving pages as MHTML.
// See http://crbug.com/120416 for how to remove this switch.
const char kSavePageAsMHTML[] = "save-page-as-mhtml";

// This flag sets the checkboxes for sharing audio during screen capture to off
// by default. It is primarily intended to be used for tests.
const char kScreenCaptureAudioDefaultUnchecked[] =
    "screen-capture-audio-default-unchecked";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
const char kSilentDebuggerExtensionAPI[] = "silent-debugger-extension-api";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
const char kSilentLaunch[] = "silent-launch";

// Sets the BrowsingDataLifetime policy to a very short value (shorter than
// normally possible) for testing purposes.
const char kSimulateBrowsingDataLifetime[] = "simulate-browsing-data-lifetime";

// Simulates a critical update being available.
const char kSimulateCriticalUpdate[] = "simulate-critical-update";

// Simulates that elevation is needed to recover upgrade channel.
const char kSimulateElevatedRecovery[] = "simulate-elevated-recovery";

// Simulates that current version is outdated.
const char kSimulateOutdated[] = "simulate-outdated";

// Simulates that current version is outdated and auto-update is off.
const char kSimulateOutdatedNoAU[] = "simulate-outdated-no-au";

// Simulates an update being available.
const char kSimulateUpgrade[] = "simulate-upgrade";

// Sets the IdleTimeout policy to a very short value (shorter than normally
// possible) for testing purposes.
const char kSimulateIdleTimeout[] = "simulate-idle-timeout";

// Specifies the maximum SSL/TLS version ("tls1.2" or "tls1.3").
const char kSSLVersionMax[] = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1.2" or "tls1.3").
const char kSSLVersionMin[] = "ssl-version-min";

// TLS 1.2 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
const char kSSLVersionTLSv12[] = "tls1.2";

// TLS 1.3 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
const char kSSLVersionTLSv13[] = "tls1.3";

// Starts the browser maximized, regardless of any previous settings.
const char kStartMaximized[] = "start-maximized";

// Starts the stack sampling profiler in the child process.
const char kStartStackProfiler[] = "start-stack-profiler";

// Browser test mode for the |kStartStackProfiler| switch. Limits the profile
// durations to be significantly less than the test timeout. On ChromeOS,
// forces the stack sampling profiler to run on all processes as well.
const char kStartStackProfilerBrowserTest[] = "browser-test";

// Interval, in minutes, used for storage pressure notification throttling.
// Useful for developers testing applications that might use non-trivial
// amounts of disk space.
const char kStoragePressureNotificationInterval[] =
    "storage-pressure-notification-interval";

// Sets the supervised user ID for any loaded or newly created profile to the
// given value. Pass an empty string to mark the profile as non-supervised.
// Used for testing.
const char kSupervisedUserId[] = "managed-user-id";

// Frequency in Milliseconds for system log uploads. Should only be used for
// testing purposes.
const char kSystemLogUploadFrequency[] = "system-log-upload-frequency";

// This flag makes Chrome auto-accept/reject requests to capture the current
// tab. It should only be used for tests.
const char kThisTabCaptureAutoAccept[] = "auto-accept-this-tab-capture";
const char kThisTabCaptureAutoReject[] = "auto-reject-this-tab-capture";

// Custom delay for memory log. This should be used only for testing purpose.
const char kTestMemoryLogDelayInMinutes[] = "test-memory-log-delay-in-minutes";

// Passes the name of the current running automated test to Chrome.
const char kTestName[] = "test-name";

// Identifies a list of download sources as trusted, but only if proper group
// policy is set.
const char kTrustedDownloadSources[] = "trusted-download-sources";

// Overrides per-origin quota settings to unlimited storage for any
// apps/origins.  This should be used only for testing purpose.
const char kUnlimitedStorage[] = "unlimited-storage";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
const char kUserDataDir[] = "user-data-dir";

// Uses WinHttp to resolve proxies instead of using Chromium's normal proxy
// resolution logic. This is only supported in Windows.
//
// TODO(https://crbug.com/1032820): Only use WinHttp whenever Chrome is
// exclusively using system proxy configs.
const char kUseSystemProxyResolver[] = "use-system-proxy-resolver";

// Examines a .crx for validity and prints the result.
const char kValidateCrx[] = "validate-crx";

// Prints version information and quits.
const char kVersion[] = "version";

// Sets the delay (in seconds) between proactive prunings of remote-bound
// WebRTC event logs which are pending upload.
// All positive values are legal.
// All negative values are illegal, and ignored.
// If set to 0, the meaning is "no proactive pruning".
const char kWebRtcRemoteEventLogProactivePruningDelta[] =
    "webrtc-event-log-proactive-pruning-delta";

// WebRTC event logs will only be uploaded if the conditions hold for this
// many milliseconds.
const char kWebRtcRemoteEventLogUploadDelayMs[] =
    "webrtc-event-log-upload-delay-ms";

// Normally, remote-bound WebRTC event logs are uploaded only when no
// peer connections are active. With this flag, the upload is never suppressed.
const char kWebRtcRemoteEventLogUploadNoSuppression[] =
    "webrtc-event-log-upload-no-suppression";

// Override WebRTC IP handling policy to mimic the behavior when WebRTC IP
// handling policy is specified in Preferences.
const char kWebRtcIPHandlingPolicy[] = "webrtc-ip-handling-policy";

// Specify the initial window user title: --window-name="My custom title"
const char kWindowName[] = "window-name";

// Specify the initial window position: --window-position=x,y
const char kWindowPosition[] = "window-position";

// Specify the initial window size: --window-size=w,h
const char kWindowSize[] = "window-size";

// Specify the initial window workspace: --window-workspace=id
const char kWindowWorkspace[] = "window-workspace";

// Uses WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is to
// use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[] = "winhttp-proxy-resolver";

// Specifies which category option was clicked in the Windows Jumplist that
// resulted in a browser startup.
const char kWinJumplistAction[] = "win-jumplist-action";

#if BUILDFLAG(IS_ANDROID)
// Android authentication account type for SPNEGO authentication
const char kAuthAndroidNegotiateAccountType[] = "auth-spnego-account-type";

// Forces the device to report being owned by an enterprise. This mimics the
// presence of an app signaling device ownership.
const char kForceDeviceOwnership[] = "force-device-ownership";

// Forces the night mode to be enabled.
const char kForceEnableNightMode[] = "force-enable-night-mode";

// Forces the update menu badge to show.
const char kForceShowUpdateMenuBadge[] = "force-show-update-menu-badge";

// Forces the update menu type to a specific type.
const char kForceUpdateMenuType[] = "force-update-menu-type";

// Forces a custom summary to be displayed below the update menu item.
const char kForceShowUpdateMenuItemCustomSummary[] = "custom_summary";

// Sets the market URL for Chrome for use in testing.
const char kMarketUrlForTesting[] = "market-url-for-testing";

// Force enable user agent overrides to request desktop sites in Clank.
const char kRequestDesktopSites[] = "request-desktop-sites";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Custom crosh command.
const char kCroshCommand[] = "crosh-command";

// Disables logging redirect for testing.
const char kDisableLoggingRedirect[] = "disable-logging-redirect";

// Disables apps on the login screen. By default, they are allowed and can be
// installed through policy.
const char kDisableLoginScreenApps[] = "disable-login-screen-apps";

// Use a short (1 second) timeout for merge session loader throttle testing.
const char kShortMergeSessionTimeoutForTest[] =
    "short-merge-session-timeout-for-test";

// Selects the scheduler configuration specified in the parameter.
const char kSchedulerConfiguration[] = "scheduler-configuration";
const char kSchedulerConfigurationConservative[] = "conservative";
const char kSchedulerConfigurationPerformance[] = "performance";

// Specifies what the default scheduler configuration value is if the user does
// not set one.
const char kSchedulerConfigurationDefault[] = "scheduler-configuration-default";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)
// These flags show the man page on Linux. They are equivalent to each
// other.
const char kHelp[] = "help";
const char kHelpShort[] = "h";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, kwallet6, gnome, gnome-keyring, gnome-libsecret, basic.
// Any other value will lead to Chrome detecting the best backend automatically.
// TODO(crbug.com/571003): Once PasswordStore no longer uses the Keyring or
// KWallet for storing passwords, rename this flag to stop referencing
// passwords. Do not rename it sooner, though; developers and testers might
// rely on it keeping large amounts of testing passwords out of their Keyrings
// or KWallets.
const char kPasswordStore[] = "password-store";

// Enables the feature of allowing the user to disable the backend via a
// setting.
const char kEnableEncryptionSelection[] = "enable-encryption-selection";

// The same as the --class argument in X applications.  Overrides the WM_CLASS
// window property with the given value.
const char kWmClass[] = "class";
#endif

#if BUILDFLAG(IS_MAC)
// Prevents Chrome from quitting when Chrome Apps are open.
const char kAppsKeepChromeAliveInTests[] = "apps-keep-chrome-alive-in-tests";

// Enable user metrics from within the installer.
const char kEnableUserMetrics[] = "enable-user-metrics";

// This is how the metrics client ID is passed from the browser process to its
// children. With Crashpad, the metrics client ID is distinct from the crash
// client ID.
const char kMetricsClientID[] = "metrics-client-id";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
const char kRelauncherProcess[] = "relauncher";

// When switches::kProcessType is switches::kRelauncherProcess, if this switch
// is also present, the relauncher process will unmount and eject a mounted disk
// image and move its disk image file to the trash.  The argument's value must
// be a BSD device name of the form "diskN" or "diskNsM".
const char kRelauncherProcessDMGDevice[] = "dmg-device";

// Indicates whether Chrome should be set as the default browser during
// installation.
const char kMakeChromeDefault[] = "make-chrome-default";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// Force-enables the profile shortcut manager. This is needed for tests since
// they use a custom-user-data-dir which disables this.
const char kEnableProfileShortcutManager[] = "enable-profile-shortcut-manager";

// Indicates that this launch of the browser originated from the installer
// (i.e., following a successful new install or over-install). This triggers
// browser behaviors for this specific launch, such as a welcome announcement
// for accessibility software (see https://crbug.com/1072735).
extern const char kFromInstaller[] = "from-installer";

// Makes Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This only
// shows an error box because the only way to hide Chrome is by uninstalling
// it.
const char kHideIcons[] = "hide-icons";

// Whether or not the browser should warn if the profile is on a network share.
// This flag is only relevant for Windows currently.
const char kNoNetworkProfileWarning[] = "no-network-profile-warning";

// Whether this process should PrefetchVirtualMemory on the contents of
// Chrome.dll. This warms up the pages in memory to speed up startup but might
// not be required in later renderers and/or GPU. For experiment info see
// crbug.com/1350257.
const char kNoPreReadMainDll[] = "no-pre-read-main-dll";

// Used in combination with kNotificationLaunchId to specify the inline reply
// entered in the toast in the Windows Action Center.
const char kNotificationInlineReply[] = "notification-inline-reply";

// Used for launching Chrome when a toast displayed in the Windows Action Center
// has been activated. Should contain the launch ID encoded by Chrome.
const char kNotificationLaunchId[] = "notification-launch-id";

// /prefetch:# arguments for the browser process launched in background mode and
// for the watcher process. Use profiles 5, 6 and 7 as documented on
// kPrefetchArgument* in content_switches.cc.
const char kPrefetchArgumentBrowserBackground[] = "/prefetch:5";
// /prefetch:6 was formerly used by the watcher but is no longer used.
// /prefetch:7 is used by crashpad, which can't depend on constants defined
// here. See crashpad_win.cc for more details.

// See kHideIcons.
const char kShowIcons[] = "show-icons";

// When rendezvousing with an existing process, used to pass the path of the
// shortcut that launched the new Chrome process. This is used to record launch
// metrics.
const char kSourceShortcut[] = "source-shortcut";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[] = "uninstall";

// Specifies that the WebApp with the specified id should be uninstalled.
const char kUninstallAppId[] = "uninstall-app-id";

// Specifies the version of the Progressive-Web-App launcher that launched
// Chrome, used to determine whether to update all launchers.
// NOTE: changing this switch requires adding legacy handling for the previous
// method, as older PWA launchers still using this switch will rely on Chrome to
// update them to use the new method.
const char kPwaLauncherVersion[] = "pwa-launcher-version";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
// Enables support to debug printing subsystem.
const char kDebugPrint[] = "debug-print";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Specifies comma-separated list of extension ids or hosts to grant
// access to CRX file system APIs.
const char kAllowNaClCrxFsAPI[] = "allow-nacl-crxfs-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to file handle APIs.
const char kAllowNaClFileHandleAPI[] = "allow-nacl-file-handle-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to TCP/UDP socket APIs.
const char kAllowNaClSocketAPI[] = "allow-nacl-socket-api";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
const char kEnableNewAppMenuIcon[] = "enable-new-app-menu-icon";

// Causes the browser to launch directly in guest mode.
const char kGuest[] = "guest";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Writes open and installed web apps for each profile to the specified file
// without launching a new browser window or tab. Pass a absolute file path to
// specify where to output the information. Can be used together with optional
// --profile-base-name switch to only write information for a given profile.
const char kListApps[] = "list-apps";

// Pass the basename of the profile directory to specify which profile to get
// information. Only relevant when used with --list-apps switch.
const char kProfileBaseName[] = "profile-base-name";

// Domains and associated SAML attributes for which third-party profile
// management should be enabled. Input should be in JSON format.
const char kProfileManagementAttributes[] = "profile-management-attributes";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
// Custom WebAPK server URL for the sake of testing.
const char kWebApkServerUrl[] = "webapk-server-url";
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// Uses the system default printer as the initially selected destination in
// print preview, instead of the most recently used destination.
const char kUseSystemDefaultPrinter[] = "use-system-default-printer";
#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
// Indicates that this process is the product of a relaunch following migration
// of User Data.
const char kUserDataMigrated[] = "user-data-migrated";
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR VERY NICE FLAGS TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead, please
// put them in alphabetical order above, or in order inside the appropriate
// ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
