// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "build/build_config.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in:
// ash/public/cpp/ash_switches.cc
// base/base_switches.cc
// chromeos/chromeos_switches.cc
// etc.
//
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// Allow non-secure origins to use the screen capture API and the desktopCapture
// extension API.
const char kAllowHttpScreenCapture[] = "allow-http-screen-capture";

// Don't block outdated plugins.
const char kAllowOutdatedPlugins[]          = "allow-outdated-plugins";

// By default, an https page cannot run JavaScript, CSS or plugins from http
// URLs. This provides an override to get the old insecure behavior.
const char kAllowRunningInsecureContent[]   = "allow-running-insecure-content";

// Allows Web Push notifications that do not show a notification.
const char kAllowSilentPush[] = "allow-silent-push";

// Specifies that the associated value should be launched in "application"
// mode.
const char kApp[]                           = "app";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Value of GAIA auth code for --force-app-mode.
const char kAppModeAuthCode[]               = "app-mode-auth-code";

// Value of OAuth2 refresh token for --force-app-mode.
const char kAppModeOAuth2Token[]            = "app-mode-oauth-token";

// The URL that the webstore APIs download extensions from.
// Note: the URL must contain one '%s' for the extension ID.
const char kAppsGalleryDownloadURL[]        = "apps-gallery-download-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[]          = "apps-gallery-update-url";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[]                = "apps-gallery-url";

// Enables overriding the path for the default authentication extension.
const char kAuthExtensionPath[]             = "auth-ext-path";

// Whitelist for Negotiate Auth servers
const char kAuthServerWhitelist[]           = "auth-server-whitelist";

// This flag makes Chrome auto-open DevTools window for each tab. It is
// intended to be used by developers and automation to not require user
// interaction for opening DevTools.
const char kAutoOpenDevToolsForTabs[]       = "auto-open-devtools-for-tabs";

// This flag makes Chrome auto-select the provided choice when an extension asks
// permission to start desktop capture. Should only be used for tests. For
// instance, --auto-select-desktop-capture-source="Entire screen" will
// automatically select to share the entire screen in English locales.
const char kAutoSelectDesktopCaptureSource[] =
    "auto-select-desktop-capture-source";

// This flag causes the user engagement checks for showing app banners to be
// bypassed. It is intended to be used by developers who wish to test that their
// sites otherwise meet the criteria needed to show app banners.
const char kBypassAppBannerEngagementChecks[] =
    "bypass-app-banner-engagement-checks";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[]          = "cipher-suite-blacklist";

// Tells chrome to display the cloud print dialog and upload the specified file
// for printing.
const char kCloudPrintFile[]                = "cloud-print-file";

// Specifies the mime type to be used when uploading data from the file
// referenced by cloud-print-file. Defaults to "application/pdf" if
// unspecified.
const char kCloudPrintFileType[]            = "cloud-print-file-type";

// Used with kCloudPrintFile to specify a title for the resulting print job.
const char kCloudPrintJobTitle[]            = "cloud-print-job-title";

// Used with kCloudPrintFile to specify a JSON print ticket for the resulting
// print job. Defaults to null if unspecified.
const char kCloudPrintPrintTicket[]         = "cloud-print-print-ticket";

// The process type value which causes a process to run as a cloud print service
// process.
//
// DO NOT CHANGE THIS VALUE. Cloud printing relies on an external binary
// launching Chrome with this process type.
const char kCloudPrintServiceProcess[]      = "service";

// Setup cloud print proxy for provided printers. This does not start
// service or register proxy for autostart.
const char kCloudPrintSetupProxy[]          = "cloud-print-setup-proxy";

// Comma-separated list of BrowserThreads that cause browser process to crash if
// the given browser thread is not responsive. UI/IO are the BrowserThreads that
// are supported.
//
// For example:
//    --crash-on-hang-threads=UI:18,IO:18 --> Crash the browser if UI or IO is
//    not responsive for 18 seconds while the other browser thread is
//    responsive.
const char kCrashOnHangThreads[]            = "crash-on-hang-threads";

// Some platforms like ChromeOS default to empty desktop.
// Browser tests may need to add this switch so that at least one browser
// instance is created on startup.
// TODO(nkostylev): Investigate if this switch could be removed.
// (http://crbug.com/148675)
const char kCreateBrowserOnStartupForTests[] =
    "create-browser-on-startup-for-tests";

// Specifies the HTTP endpoint which will be used to serve
// chrome-devtools://devtools/custom/<path>
const char kCustomDevtoolsFrontend[] = "custom-devtools-frontend";

// Enables a frame context menu item that toggles the frame in and out of glass
// mode (Windows Vista and up only).
const char kDebugEnableFrameToggle[]        = "debug-enable-frame-toggle";

// Adds debugging entries such as Inspect Element to context menus of packed
// apps.
const char kDebugPackedApps[]               = "debug-packed-apps";

// Prevents permission prompts from appearing by denying instead of showing
// prompts.
const char kDenyPermissionPrompts[] = "deny-permission-prompts";

// Passes command line parameters to the DevTools front-end.
const char kDevToolsFlags[]                 = "devtools-flags";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
const char kDiagnosticsFormat[]             = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
const char kDiagnosticsRecovery[]           = "diagnostics-recovery";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
const char kDisableBackgroundNetworking[]   = "disable-background-networking";

// Disables the bundled PPAPI version of Flash.
const char kDisableBundledPpapiFlash[]      = "disable-bundled-ppapi-flash";

// Disables hardware encoding support for Cast Streaming.
const char kDisableCastStreamingHWEncoding[] =
    "disable-cast-streaming-hw-encoding";

// Disables the client-side phishing detection feature. Note that even if
// client-side phishing detection is enabled, it will only be active if the
// user has opted in to UMA stats and SafeBrowsing is enabled in the
// preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

// Disable default component extensions with background pages - useful for
// performance tests where these pages may interfere with perf results.
const char kDisableComponentExtensionsWithBackgroundPages[] =
    "disable-component-extensions-with-background-pages";

const char kDisableComponentUpdate[]        = "disable-component-update";

// Disables installation of default apps on first run. This is used during
// automated testing.
const char kDisableDefaultApps[]            = "disable-default-apps";

// Disables device discovery notifications.
const char kDisableDeviceDiscoveryNotifications[] =
    "disable-device-discovery-notifications";

// Disables Domain Reliability Monitoring.
const char kDisableDomainReliability[]      = "disable-domain-reliability";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Disable extensions except those specified in a comma-separated list.
const char kDisableExtensionsExcept[] = "disable-extensions-except";

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";

// Disable auto-reload of error pages if offline.
const char kDisableOfflineAutoReload[]      = "disable-offline-auto-reload";

// Disable only auto-reloading error pages when the tab is visible.
const char kDisableOfflineAutoReloadVisibleOnly[] =
    "disable-offline-auto-reload-visible-only";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Disables print preview (For testing, and for users who don't like us. :[ )
const char kDisablePrintPreview[]           = "disable-print-preview";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disables showing the search geolocation disclosure UI. Used for perf testing.
const char kDisableSearchGeolocationDisclosure[] =
    "disable-search-geolocation-disclosure";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/119175)
const char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Enables new UI for net-error page.
const char kNewNetErrorPageUI[] = "new-net-error-page-ui";

// Requests that a running browser process dump its collected histograms to a
// given file. The file is overwritten if it exists.
const char kDumpBrowserHistograms[]         = "dump-browser-histograms";

// Overrides the path of Easy Unlock component app.
const char kEasyUnlockAppPath[]             = "easy-unlock-app-path";

// If the WebRTC logging private API is active, enables audio debug recordings.
const char kEnableAudioDebugRecordingsFromExtension[] =
    "enable-audio-debug-recordings-from-extension";

// Enables the multi-level undo system for bookmarks.
const char kEnableBookmarkUndo[]            = "enable-bookmark-undo";

// This applies only when the process type is "service". Enables the Cloud Print
// Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// Enable device discovery notifications.
const char kEnableDeviceDiscoveryNotifications[] =
    "enable-device-discovery-notifications";

// If true devtools experimental settings are enabled.
const char kEnableDevToolsExperiments[]     = "enable-devtools-experiments";

// Enables Domain Reliability Monitoring.
const char kEnableDomainReliability[] = "enable-domain-reliability";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

const char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Enable the fast unload controller, which speeds up tab/window close by
// running a tab's onunload js handler independently of the GUI -
// crbug.com/142458 .
const char kEnableFastUnload[] = "enable-fast-unload";

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enables tracing for each navigation. It will attempt to trace each navigation
// for 10s, until the buffer is full, or until the next navigation.
// It only works if a URL was provided by --trace-upload-url.
const char kEnableNavigationTracing[] = "enable-navigation-tracing";

// Enables the network-related benchmarking extensions.
const char kEnableNetBenchmarking[]         = "enable-net-benchmarking";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[]       = "enable-offline-auto-reload";

// Only auto-reload error pages when the tab is visible.
const char kEnableOfflineAutoReloadVisibleOnly[] =
    "enable-offline-auto-reload-visible-only";

// Enables a number of potentially annoying security features (strict mixed
// content mode, powerful feature restrictions, etc.)
const char kEnablePotentiallyAnnoyingSecurityFeatures[] =
    "enable-potentially-annoying-security-features";

// Enables the Power overlay in Settings.
const char kEnablePowerOverlay[]            = "enable-power-overlay";

// Enables showing unregistered printers in print preview
const char kEnablePrintPreviewRegisterPromos[] =
    "enable-print-preview-register-promos";

// Enables DevTools server for UI (mus, ash, etc). Value should be the port the
// server is started on. Default port is 9223.
const char kEnableUiDevTools[] = "enable-ui-devtools";

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

// If this flag is present then this command line is being delegated to an
// already running chrome process via the fast path, ie: before chrome.dll is
// loaded. It is useful to tell the difference for tracking purposes.
const char kFastStart[]            = "fast-start";

// Forces Android application mode. This hides certain system UI elements and
// forces the app to be installed if it hasn't been already.
const char kForceAndroidAppMode[] = "force-android-app-mode";

// Forces application mode. This hides certain system UI elements and forces
// the app to be installed if it hasn't been already.
const char kForceAppMode[]                  = "force-app-mode";

// Forces Desktop to iOS promotion to appear in windows whenever an entrypoint
// is triggered.
const char kForceDesktopIOSPromotion[] = "force-desktop-ios-promotion";

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
const char kForceFirstRun[]                 = "force-first-run";

// Shows the modal first run dialog during browser startup. This is shown for
// the "organic" first run experience (Chrome downloaded, empty user data dir).
// This does nothing without --force-first-run also being set.
const char kForceFirstRunDialog[] = "force-first-run-dialog";

// Forces Chrome to use localNTP instead of server (GWS) NTP.
const char kForceLocalNtp[]                 = "force-local-ntp";

// Forces Chrome to use a stacked tab strip layout.
const char kForceStackedTabStripLayout[]    = "force-stacked-tab-strip-layout";

// Specifies which page will be displayed in newly-opened tabs. We need this
// for testing purposes so that the UI tests don't depend on what comes up for
// http://google.com.
const char kHomePage[]                      = "homepage";

// Causes net::URLFetchers to ignore requests for SSL client certificates,
// causing them to attempt an unauthenticated SSL/TLS session. This is intended
// for use when testing various service URLs (eg: kPromoServerURL, kSbURLPrefix,
// kSyncServiceURL, etc).
const char kIgnoreUrlFetcherCertRequests[] = "ignore-urlfetcher-cert-requests";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Causes Chrome to initiate an installation flow for the given app.
const char kInstallChromeApp[]              = "install-chrome-app";

// A list of whitelists to install for a supervised user, for testing.
// The list is of the following form: <id>[:<name>],[<id>[:<name>],...]
const char kInstallSupervisedUserWhitelists[] =
    "install-supervised-user-whitelists";

// Marks a renderer as an Instant process.
const char kInstantProcess[]                = "instant-process";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[]              = "keep-alive-for-test";

// Enable kiosk mode. Please note this is not Chrome OS kiosk mode.
const char kKioskMode[]                     = "kiosk";

// Enable automatically pressing the print button in print preview.
const char kKioskModePrinting[]             = "kiosk-printing";

// Launches an in-process instance of the simple_browser service on startup if
// this Chrome build supports it. See |enable_simple_browser_service_in_process|
// in chrome/common/features.gni.
const char kLaunchInProcessSimpleBrowserSwitch[] =
    "launch-in-process-simple-browser";

// Launches a sandboxed out-of-process instance of the simple_browser service on
// startup if this Chrome build supports it. See
// |enable_simple_browser_service_out_of_process| in chrome/common/features.gni.
const char kLaunchSimpleBrowserSwitch[] = "launch-simple-browser";

// Loads the Media Router component extension on startup.
const char kLoadMediaRouterComponentExtension[] =
    "load-media-router-component-extension";

// Makes Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Forces the maximum disk space to be used by the media cache, in bytes.
const char kMediaCacheSize[]                = "media-cache-size";

// Allows setting a different destination ID for connection-monitoring GCM
// messages. Useful when running against a non-prod management server.
const char kMonitoringDestinationID[]       = "monitoring-destination-id";

// Sets the granularity of events to capture in the network log. The mode can be
// set to one of the following values:
//   "Default"
//   "IncludeCookiesAndCredentials"
//   "IncludeSocketBytes"
//
// See the functions of the corresponding name in net_log_capture_mode.h for a
// description of their meaning.
const char kNetLogCaptureMode[]             = "net-log-capture-mode";

// Controls the position of the new tab button within the tabstrip.
const char kNewTabButtonPosition[] = "new-tab-button-position";
// Possible values for this switch.
const char kNewTabButtonPositionOppositeCaption[] = "opposite-caption";
const char kNewTabButtonPositionLeading[] = "leading";
const char kNewTabButtonPositionAfterTabs[] = "after-tabs";
const char kNewTabButtonPositionTrailing[] = "trailing";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
const char kNoExperiments[]                 = "no-experiments";

// Skip First Run tasks, whether or not it's actually the First Run. Overridden
// by kForceFirstRun. This does not drop the First Run sentinel and thus doesn't
// prevent first run from occuring the next time chrome is launched without this
// flag.
const char kNoFirstRun[]                    = "no-first-run";

// Don't send hyperlink auditing pings
const char kNoPings[]                       = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Disables the service process from adding itself as an autorun process. This
// does not delete existing autorun registrations, it just prevents the service
// from registering a new one.
const char kNoServiceAutorun[]              = "no-service-autorun";

// Does not automatically open a browser window on startup (used when
// launching Chrome for the purpose of hosting background apps).
const char kNoStartupWindow[]               = "no-startup-window";

// Disables checking whether we received an acknowledgment when registering
// a supervised user. Also disables the timeout during registration that waits
// for the ack. Useful when debugging against a server that does not
// support notifications.
const char kNoSupervisedUserAcknowledgmentCheck[]  =
    "no-managed-user-acknowledgment-check";

// Launches URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// The time that a new chrome process which is delegating to an already running
// chrome process started. (See ProcessSingleton for more details.)
const char kOriginalProcessStartTime[]      = "original-process-start-time";

// Contains a list of feature names for which origin trial experiments should
// be disabled. Names should be separated by "|" characters.
const char kOriginTrialDisabledFeatures[] = "origin-trial-disabled-features";

// Contains a list of token signatures for which origin trial experiments should
// be disabled. Tokens should be separated by "|" characters.
const char kOriginTrialDisabledTokens[] = "origin-trial-disabled-tokens";

// Overrides the default public key for checking origin trial tokens.
const char kOriginTrialPublicKey[] = "origin-trial-public-key";

// Packages an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Development flag for permission request API. This flag is needed until
// the API is finalized.
// TODO(bauerb): Remove when this flag is not needed anymore.
const char kPermissionRequestApiScope[]     = "permission-request-api-scope";

// Development flag for permission request API. This flag is needed until
// the API is finalized.
// TODO(bauerb): Remove when this flag is not needed anymore.
const char kPermissionRequestApiUrl[]       = "permission-request-api-url";

// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[]                = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[]             = "ppapi-flash-version";

// Use IPv6 only for privet HTTP.
const char kPrivetIPv6Only[]                   = "privet-ipv6-only";

// Outputs the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
const char kProductVersion[]                = "product-version";

// Selects directory of profile to associate with the first browser launched.
const char kProfileDirectory[]              = "profile-directory";

// Starts the sampling based profiler for the browser process at startup. This
// will only work if chrome has been built with the gyp variable profiling=1.
// The output will go to the value of kProfilingFile.
const char kProfilingAtStart[]              = "profiling-at-start";

// Controls whether profile data is periodically flushed to a file. Normally
// the data gets written on exit but cases exist where chrome doesn't exit
// cleanly (especially when using single-process). A time in seconds can be
// specified.
const char kProfilingFlush[]                = "profiling-flush";

// Forces proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[]               = "proxy-bypass-list";

// Uses the pac script at the given URL
const char kProxyPacUrl[]                   = "proxy-pac-url";

// Porvides a list of addresses to discover DevTools remote debugging targets.
// The format is <host>:<port>,...,<host>:port.
const char kRemoteDebuggingTargets[] = "remote-debugging-targets";

// Indicates the last session should be restored on startup. This overrides the
// preferences value. Note that this does not force automatic session restore
// following a crash, so as to prevent a crash loop. This switch is used to
// implement support for OS-specific "continue where you left off" functionality
// on OS X and Windows.
const char kRestoreLastSession[]            = "restore-last-session";

// Disable saving pages as HTML-only, disable saving pages as HTML Complete
// (with a directory of sub-resources). Enable only saving pages as MHTML.
// See http://crbug.com/120416 for how to remove this switch.
const char kSavePageAsMHTML[]               = "save-page-as-mhtml";

// If true the app list will be shown.
const char kShowAppList[]                   = "show-app-list";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
const char kSilentDebuggerExtensionAPI[]    = "silent-debugger-extension-api";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
const char kSilentLaunch[]                  = "silent-launch";

// Simulates a critical update being available.
const char kSimulateCriticalUpdate[]        = "simulate-critical-update";

// Simulates that elevation is needed to recover upgrade channel.
const char kSimulateElevatedRecovery[]      = "simulate-elevated-recovery";

// Simulates that current version is outdated.
const char kSimulateOutdated[]              = "simulate-outdated";

// Simulates that current version is outdated and auto-update is off.
const char kSimulateOutdatedNoAU[]          = "simulate-outdated-no-au";

// Simulates an update being available.
const char kSimulateUpgrade[]               = "simulate-upgrade";

// Specifies the maximum SSL/TLS version ("tls1.2" or "tls1.3").
const char kSSLVersionMax[] = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1", "tls1.1", "tls1.2", or
// "tls1.3").
const char kSSLVersionMin[] = "ssl-version-min";

// TLS 1.0 mode for the |kSSLVersionMin| switch.
const char kSSLVersionTLSv1[] = "tls1";

// TLS 1.1 mode for the |kSSLVersionMin| switch.
const char kSSLVersionTLSv11[] = "tls1.1";

// TLS 1.2 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
const char kSSLVersionTLSv12[] = "tls1.2";

// TLS 1.2 mode for |kSSLVersionMax| and |kSSLVersionMin| switches.
const char kSSLVersionTLSv13[] = "tls1.3";

// Starts the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Starts the stack sampling profiler in the child process.
const char kStartStackProfiler[]            = "start-stack-profiler";

// Sets the supervised user ID for any loaded or newly created profile to the
// given value. Pass an empty string to mark the profile as non-supervised.
// Used for testing.
const char kSupervisedUserId[]              = "managed-user-id";

// Frequency in Milliseconds for system log uploads. Should only be used for
// testing purposes.
const char kSystemLogUploadFrequency[] = "system-log-upload-frequency";

// Sets the task manager to track extra renderer processes that might not
// normally be displayed in the task manager.
const char kTaskManagerShowExtraRenderers[] =
    "task-manager-show-extra-renderers";

// Passes the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Specifies the enabled TLS 1.3 variant ("disabled", "draft23", "final").
const char kTLS13Variant[] = "tls13-variant";

// This mode disables the TLS 1.3 for the |kTLS13Variant| switch.
const char kTLS13VariantDisabled[] = "disabled";

// This mode enables TLS 1.3 draft-23 for the |kTLS13Variant| switch.
const char kTLS13VariantDraft23[] = "draft23";

// This mode enables TLS 1.3 final for the |kTLS13Variant| switch.
const char kTLS13VariantFinal[] = "final";

// Identifies a list of download sources as trusted, but only if proper group
// policy is set.
const char kTrustedDownloadSources[] = "trusted-download-sources";

// Experimental. Shows a dialog asking the user to try chrome. This flag is to
// be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Overrides per-origin quota settings to unlimited storage for any
// apps/origins.  This should be used only for testing purpose.
const char kUnlimitedStorage[]              = "unlimited-storage";

// Treat given (insecure) origins as secure origins. Multiple origins can be
// supplied as a comma-separated list. For the definition of secure contexts,
// see https://w3c.github.io/webappsec-secure-contexts/
//
// Example:
// --unsafely-treat-insecure-origin-as-secure=http://a.test,http://b.test
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely-treat-insecure-origin-as-secure";

// Pass the full https:// URL to PAC (Proxy Auto Config) scripts. As opposed to
// the default behavior which strips path and query components before passing
// to the PAC scripts.
const char kUnsafePacUrl[] = "unsafe-pac-url";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// Examines a .crx for validity and prints the result.
const char kValidateCrx[]                   = "validate-crx";

// Prints version information and quits.
const char kVersion[]                       = "version";

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

// Specify the initial window position: --window-position=x,y
const char kWindowPosition[]                = "window-position";

// Specify the initial window size: --window-size=w,h
const char kWindowSize[]                    = "window-size";

// Specify the initial window workspace: --window-workspace=id
const char kWindowWorkspace[]               = "window-workspace";

// Uses WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is to
// use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

// Specifies which category option was clicked in the Windows Jumplist that
// resulted in a browser startup.
const char kWinJumplistAction[]             = "win-jumplist-action";

#if defined(GOOGLE_CHROME_BUILD)
// Shows a Google icon next to context menu items powered by Google services.
const char kEnableGoogleBrandedContextMenu[] =
    "enable-google-branded-context-menu";
#endif  // defined(GOOGLE_CHROME_BUILD)

#if !defined(GOOGLE_CHROME_BUILD)
// Enables a live-reload for local NTP resources. This only works when Chrome
// is running from a Chrome source directory.
const char kLocalNtpReload[]                = "local-ntp-reload";
#endif

#if defined(OS_ANDROID)
// Android authentication account type for SPNEGO authentication
const char kAuthAndroidNegotiateAccountType[] = "auth-spnego-account-type";

// Android authentication account type for SPNEGO authentication
const char kChromeHomeSwipeLogicType[] = "chrome-home-swipe-logic";

// Disables Contextual Search.
const char kDisableContextualSearch[] = "disable-contextual-search";

// Enable the accessibility tab switcher.
const char kEnableAccessibilityTabSwitcher[] =
    "enable-accessibility-tab-switcher";

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

// Enables a hung renderer InfoBar allowing the user to close or wait on
// unresponsive web content.
const char kEnableHungRendererInfoBar[] = "enable-hung-renderer-infobar";

// Forces the update menu badge to show.
const char kForceShowUpdateMenuBadge[] = "force-show-update-menu-badge";

// Forces the update menu type to a specific type.
const char kForceUpdateMenuType[] = "force-update-menu-type";

// Forces a custom summary to be displayed below the update menu item.
const char kForceShowUpdateMenuItemCustomSummary[] = "custom_summary";

// Sets the market URL for Chrome for use in testing.
const char kMarketUrlForTesting[] = "market-url-for-testing";

// Specifies Android phone page loading progress bar animation.
const char kProgressBarAnimation[]          = "progress-bar-animation";

// Specifies a base URL for the trusted CDN for tests.
const char kTrustedCDNBaseURLForTests[] = "trusted-cdn-base-url-for-tests";

// Custom WebAPK server URL for the sake of testing.
const char kWebApkServerUrl[] = "webapk-server-url";
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Custom crosh command.
const char kCroshCommand[] = "crosh-command";

// Disables logging redirect for testing.
const char kDisableLoggingRedirect[] = "disable-logging-redirect";

// Disables apps on the login screen. By default, they are allowed and can be
// installed through policy.
const char kDisableLoginScreenApps[] = "disable-login-screen-apps";

// Provides the name of the mojo service running in a mash utility process.
// NOTE: Used by the Chrome OS crash_reporter to identify mash processes. If you
// change or remove the flag please update platform2/crash_reporter.
const char kMashServiceName[] = "mash-service-name";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// These flags show the man page on Linux. They are equivalent to each
// other.
const char kHelp[]                          = "help";
const char kHelpShort[]                     = "h";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, gnome, gnome-keyring, gnome-libsecret, basic. Any other
// value will lead to Chrome detecting the best backend automatically.
// TODO(crbug.com/571003): Once PasswordStore no longer uses the Keyring or
// KWallet for storing passwords, rename this flag to stop referencing
// passwords. Do not rename it sooner, though; developers and testers might
// rely on it keeping large amounts of testing passwords out of their Keyrings
// or KWallets.
const char kPasswordStore[]                 = "password-store";

// Enables the feature of allowing the user to disable the backend via a
// setting.
const char kEnableEncryptionSelection[] = "enable-encryption-selection";

// The same as the --class argument in X applications.  Overrides the WM_CLASS
// window property with the given value.
const char kWmClass[]                       = "class";
#endif

#if defined(OS_MACOSX)
// Prevents Chrome from quitting when Chrome Apps are open.
const char kAppsKeepChromeAliveInTests[]    = "apps-keep-chrome-alive-in-tests";

// Disable the toolkit-views App Info dialog for Mac.
const char kDisableAppInfoDialogMac[] = "disable-app-info-dialog-mac";

// Disables tab detaching in fullscreen mode on Mac.
const char kDisableFullscreenTabDetaching[] =
    "disable-fullscreen-tab-detaching";

// Disables app shim creation for hosted apps on Mac.
const char kDisableHostedAppShimCreation[] = "disable-hosted-app-shim-creation";

// Prevents hosted apps from being opened in windows on Mac.
const char kDisableHostedAppsInWindows[] = "disable-hosted-apps-in-windows";

// Disables use of toolkit-views based native app windows.
const char kDisableMacViewsNativeAppWindows[] =
    "disable-mac-views-native-app-windows";

// Enable user metrics from within the installer.
const char kEnableUserMetrics[] = "enable-user-metrics";

// Enable the toolkit-views App Info dialog for Mac. This is accessible from
// chrome://apps and chrome://extensions and is already enabled on non-mac.
const char kEnableAppInfoDialogMac[] = "enable-app-info-dialog-mac";

// Enables tab detaching in fullscreen mode on Mac.
const char kEnableFullscreenTabDetaching[] = "enable-fullscreen-tab-detaching";

// Enables the fullscreen toolbar to reveal itself for tab strip changes.
const char kEnableFullscreenToolbarReveal[] =
    "enable-fullscreen-toolbar-reveal";

// Allows hosted apps to be opened in windows on Mac.
const char kEnableHostedAppsInWindows[] = "enable-hosted-apps-in-windows";

// Shows a notification when quitting Chrome with hosted apps running. Default
// behavior is to also quit all hosted apps.
const char kHostedAppQuitNotification[] = "enable-hosted-app-quit-notification";

// This is how the metrics client ID is passed from the browser process to its
// children. With Crashpad, the metrics client ID is distinct from the crash
// client ID.
const char kMetricsClientID[]               = "metrics-client-id";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
const char kRelauncherProcess[]             = "relauncher";

// When switches::kProcessType is switches::kRelauncherProcess, if this switch
// is also present, the relauncher process will unmount and eject a mounted disk
// image and move its disk image file to the trash.  The argument's value must
// be a BSD device name of the form "diskN" or "diskNsM".
const char kRelauncherProcessDMGDevice[]    = "dmg-device";

// Indicates whether Chrome should be set as the default browser during
// installation.
const char kMakeChromeDefault[] = "make-chrome-default";
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
// Fallback to XPS. By default connector uses CDD.
const char kEnableCloudPrintXps[]           = "enable-cloud-print-xps";

// Force-enables the profile shortcut manager. This is needed for tests since
// they use a custom-user-data-dir which disables this.
const char kEnableProfileShortcutManager[]  = "enable-profile-shortcut-manager";

// Makes Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This only
// shows an error box because the only way to hide Chrome is by uninstalling
// it.
const char kHideIcons[]                     = "hide-icons";

// Whether or not the browser should warn if the profile is on a network share.
// This flag is only relevant for Windows currently.
const char kNoNetworkProfileWarning[]       = "no-network-profile-warning";

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
const char kPrefetchArgumentWatcher[] = "/prefetch:6";
// /prefetch:7 is used by crashpad, which can't depend on constants defined
// here. See crashpad_win.cc for more details.

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Causes the process to run as a watcher process.
const char kWatcherProcess[]                = "watcher";
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
// Enables support to debug printing subsystem.
const char kDebugPrint[] = "debug-print";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Specifies comma-separated list of extension ids or hosts to grant
// access to CRX file system APIs.
const char kAllowNaClCrxFsAPI[]             = "allow-nacl-crxfs-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to file handle APIs.
const char kAllowNaClFileHandleAPI[]        = "allow-nacl-file-handle-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to TCP/UDP socket APIs.
const char kAllowNaClSocketAPI[]            = "allow-nacl-socket-api";
#endif

#if defined(OS_WIN) || defined(OS_LINUX)
const char kDisableInputImeAPI[] = "disable-input-ime-api";
const char kEnableInputImeAPI[] = "enable-input-ime-api";
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
const char kEnableNewAppMenuIcon[] = "enable-new-app-menu-icon";
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && \
    !defined(GOOGLE_CHROME_BUILD)
// Enables Machine level user cloud policy on Chromium build. This policy is
// always enabled on the branded builds.
const char kEnableMachineLevelUserCloudPolicy[] =
    "enable-machine-level-user-cloud-policy";
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// Uses the system default printer as the initially selected destination in
// print preview, instead of the most recently used destination.
const char kUseSystemDefaultPrinter[] = "use-system-default-printer";
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR VERY NICE FLAGS TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead, please
// put them in alphabetical order above, or in order inside the appropriate
// ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
