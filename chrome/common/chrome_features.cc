// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "ppapi/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order.

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kAppPreloadService,
             "AppPreloadService",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// When enabled, notifications from PWA's will use the PWA icon and name,
// as long as the PWA is on the start menu.  b/40285965.
BASE_FEATURE(kAppSpecificNotifications,
             "AppSpecificNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, invokes `SetProcessPriorityBoost` to disable priority boosting
// when a thread is taken out of the wait state. The default Windows behavior is
// to boost when taking a thread out of waking state. On other platforms, the
// default is not to boost and implementing boosting regresses input and page
// load metrics. Therefore, we experiment on Windows to determine if operating
// without boosting improves these metrics. This is a field-sampling experiment
// and is not intended to be shipped as is regardless of the outcome but rather
// to gather data before the design phase of enhanced cross-platform scheduling
// primitives.
BASE_FEATURE(kDisableBoostPriority,
             "DisableBoostPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
// Can be used to disable RemoteCocoa (hosting NSWindows for apps in the app
// process). For debugging purposes only.
BASE_FEATURE(kAppShimRemoteCocoa,
             "AppShimRemoteCocoa",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This is used to control the new app close behavior on macOS wherein closing
// all windows for an app leaves the app running.
// https://crbug.com/1080729
BASE_FEATURE(kAppShimNewCloseBehavior,
             "AppShimNewCloseBehavior",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, app shims try to launch chrome silently if chrome isn't already
// running, rather than have chrome launch visibly with a new tab/profile
// selector.
// https://crbug.com/1205537
BASE_FEATURE(kAppShimLaunchChromeSilently,
             "AppShimLaunchChromeSilently",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, notifications coming from PWAs will be displayed via their app
// shim processes, rather than directly by chrome.
// https://crbug.com/938661
BASE_FEATURE(kAppShimNotificationAttribution,
             "AppShimNotificationAttribution",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, app shims used by PWAs will be signed with an ad-hoc signature
// https://crbug.com/40276068
BASE_FEATURE(kUseAdHocSigningForWebAppShims,
             "UseAdHocSigningForWebAppShims",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Enables or disables the Autofill survey triggered by opening a prompt to
// save address info.
BASE_FEATURE(kAutofillAddressSurvey,
             "AutofillAddressSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Autofill survey triggered by opening a prompt to
// save credit card info.
BASE_FEATURE(kAutofillCardSurvey,
             "AutofillCardSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Autofill survey triggered by opening a prompt to
// save password info.
BASE_FEATURE(kAutofillPasswordSurvey,
             "AutofillPasswordSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
BASE_FEATURE(kBackgroundModeAllowRestart,
             "BackgroundModeAllowRestart",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Enable boarding pass detector on Chrome Android.
BASE_FEATURE(kBoardingPassDetector,
             "BoardingPassDetector",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kBoardingPassDetectorUrlParamName[] = "boarding_pass_detector_urls";
const base::FeatureParam<std::string> kBoardingPassDetectorUrlParam(
    &kBoardingPassDetector,
    kBoardingPassDetectorUrlParamName,
    "");
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Enable Borealis on Chrome OS.
BASE_FEATURE(kBorealis, "Borealis", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
// WARNING: These features are launched and the old code paths are in the
// process of being removed. Attempting to run Chrome with the features
// disabled will likely break.
// TODO(crbug.com/390333881): Remove the flags once all references have been
// cleaned up.
BASE_FEATURE(kEnableCertManagementUIV2,
             "EnableCertManagementUIV2_LAUNCHED",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCertManagementUIV2Write,
             "EnableCertManagementUIV2Write_LAUNCHED",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCertManagementUIV2EditCerts,
             "EnableCertManagementUIV2EditCerts_LAUNCHED",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enable project Crostini, Linux VMs on Chrome OS.
BASE_FEATURE(kCrostini, "Crostini", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable advanced access controls for Crostini-related features
// (e.g. restricting VM CLI tools access, restricting Crostini root access).
BASE_FEATURE(kCrostiniAdvancedAccessControls,
             "CrostiniAdvancedAccessControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables infrastructure for generating Ansible playbooks for the default
// Crostini container from software configurations in JSON schema.
BASE_FEATURE(kCrostiniAnsibleSoftwareManagement,
             "CrostiniAnsibleSoftwareManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for sideloading android apps into Arc via crostini.
BASE_FEATURE(kCrostiniArcSideload,
             "CrostiniArcSideload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables distributed model for TPM1.2, i.e., using tpm_managerd and
// attestationd.
BASE_FEATURE(kCryptohomeDistributedModel,
             "CryptohomeDistributedModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables cryptohome UserDataAuth interface, a new dbus interface that is
// fully protobuf and uses libbrillo for dbus instead of the deprecated
// glib-dbus.
BASE_FEATURE(kCryptohomeUserDataAuth,
             "CryptohomeUserDataAuth",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for cryptohome UserDataAuth interface. UserDataAuth is a new
// dbus interface that is fully protobuf and uses libbrillo for dbus instead
// instead of the deprecated glib-dbus.
BASE_FEATURE(kCryptohomeUserDataAuthKillswitch,
             "CryptohomeUserDataAuthKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enables starting of Data Leak Prevention Files Daemon by sending the
// DLP policy there. The daemon might restrict access to some protected files.
BASE_FEATURE(kDataLeakPreventionFilesRestriction,
             "DataLeakPreventionFilesRestriction",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Enables a revamped Delete Browsing Data dialog. This includes UI changes and
// removal of the bulk password deletion option from the dialog.
BASE_FEATURE(kDbdRevampDesktop,
             "DbdRevampDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Whether to allow installed-by-default web apps to be installed or not.
BASE_FEATURE(kPreinstalledWebAppInstallation,
             "DefaultWebAppInstallation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to force migrate preinstalled web apps whenever the old Chrome app
// they're replacing is detected, even if the web app is already installed.
BASE_FEATURE(kPreinstalledWebAppAlwaysMigrate,
             "PreinstalledWebAppAlwaysMigrate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to force migrate the calculator preinstalled web app whenever the
// old Chrome app is detected, even if the calculator web app is already
// installed.
BASE_FEATURE(kPreinstalledWebAppAlwaysMigrateCalculator,
             "PreinstalledWebAppAlwaysMigrateCalculator",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// If enabled, specified extensions cannot be closed via the task manager.
BASE_FEATURE(kDesktopTaskManagerEndProcessDisabledForExtension,
             "DesktopTaskManagerEndProcessDisabledForExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Controls the enablement of structured metrics on Windows, Linux, and Mac.
BASE_FEATURE(kChromeStructuredMetrics,
             "ChromeStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Moves the Extensions "puzzle piece" icon from the title bar into the app menu
// for web app windows.
BASE_FEATURE(kDesktopPWAsElidedExtensionsMenu,
             "DesktopPWAsElidedExtensionsMenu",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables or disables Desktop PWAs to be auto-started on OS login.
BASE_FEATURE(kDesktopPWAsRunOnOsLogin,
             "DesktopPWAsRunOnOsLogin",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, allow-listed PWAs cannot be closed manually by the user.
BASE_FEATURE(kDesktopPWAsPreventClose,
             "DesktopPWAsPreventClose",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kPwaNavigationCapturingWithScopeExtensions,
             "DesktopPWAsLinkCapturingWithScopeExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds a user settings that allows PWAs to be opened with a tab strip.
BASE_FEATURE(kDesktopPWAsTabStripSettings,
             "DesktopPWAsTabStripSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows fullscreen to claim whole display area when in windowing mode
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDisplayEdgeToEdgeFullscreen,
             "DisplayEdgeToEdgeFullscreen",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Controls whether Chrome Apps are supported. See https://crbug.com/1221251.
// If the feature is disabled, Chrome Apps continue to work. If enabled, Chrome
// Apps will not launch and will be marked in the UI as deprecated.
BASE_FEATURE(kChromeAppsDeprecation,
             "ChromeAppsDeprecation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new create shortcut flow where fire and forget entities are
// created from three dot menu > Save and Share > Create Shortcut instead of
// PWAs.
BASE_FEATURE(kShortcutsNotApps,
             "ShortcutsNotApps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the opening of the desktop and highlighting of the shortcut created
// as part of the new Create Shortcut flow. Requires kShortcutsNotApps to be
// enabled to work.
BASE_FEATURE(kShortcutsNotAppsRevealDesktop,
             "ShortcutsNotAppsRevealDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kFileTransferEnterpriseConnector,
             "FileTransferEnterpriseConnector",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFileTransferEnterpriseConnectorUI,
             "FileTransferEnterpriseConnectorUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kForcedAppRelaunchOnPlaceholderUpdate,
             "ForcedAppRelaunchOnPlaceholderUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Controls whether the GeoLanguage system is enabled. GeoLanguage uses IP-based
// coarse geolocation to provide an estimate (for use by other Chrome features
// such as Translate) of the local/regional language(s) corresponding to the
// device's location. If this feature is disabled, the GeoLanguage provider is
// not initialized at startup, and clients calling it will receive an empty list
// of languages.
BASE_FEATURE(kGeoLanguage, "GeoLanguage", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the actor component of Glic is enabled.
BASE_FEATURE(kGlicActor, "GlicActor", base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kGlicActorActorObservationDelay{
    &kGlicActor, "glic-actor-observation-delay", base::Seconds(3)};

#if BUILDFLAG(ENABLE_GLIC)
// Controls whether the Glic feature is enabled.
BASE_FEATURE(kGlic, "Glic", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the Glic feature is always detached.
BASE_FEATURE(kGlicDetached, "GlicDetached", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Glic feature's z order changes based on the webclient
// mode.
BASE_FEATURE(kGlicZOrderChanges,
             "GlicZOrderChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to sync @google.com account cookies. This is only for development and
// testing.
BASE_FEATURE(kGlicDevelopmentSyncGoogleCookies,
             "GlicDevelopmentCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kGlicStatusIconOpenMenuWithSecondaryClick{
    &kGlic, "open-status-icon-menu-with-secondary-click", true};

// Controls whether the simplified version of the border should be used.
BASE_FEATURE(kGlicForceSimplifiedBorder,
             "GlicForceSimplifiedBorder",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kGlicPreLoadingTimeMs{
    &kGlic, "glic-pre-loading-time-ms", 200};

const base::FeatureParam<int> kGlicMinLoadingTimeMs{
    &kGlic, "glic-min-loading-time-ms", 1000};

const base::FeatureParam<int> kGlicMaxLoadingTimeMs{
    &kGlic, "glic-max-loading-time-ms", 15000};

const base::FeatureParam<int> kGlicReloadMaxLoadingTimeMs{
    &kGlic, "glic-reload-max-loading-time-ms", 30000};

const base::FeatureParam<int> kGlicInitialWidth{&kGlic, "glic-initial-width",
                                                352};
const base::FeatureParam<int> kGlicInitialHeight{&kGlic, "glic-initial-height",
                                                 86};

const base::FeatureParam<int> kGlicFreInitialWidth{
    &kGlic, "glic-fre-initial-width", 512};
const base::FeatureParam<int> kGlicFreInitialHeight{
    &kGlic, "glic-fre-initial-height", 512};

// Quality value in the range [0, 100]. For use with gfx::JPEGCodec::Encode().
const base::FeatureParam<int> kGlicScreenshotEncodeQuality{
    &kGlic, "glic-screenshot-encode-quality", 100};

const base::FeatureParam<std::string> kGlicDefaultHotkey{
    &kGlic, "glic-default-hotkey", ""};

BASE_FEATURE(kGlicURLConfig,
             "GlicURLConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kGlicGuestURL{
    &kGlicURLConfig, "glic-guest-url",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "https://gemini.google.com/glic"
#else
    ""
#endif
};

BASE_FEATURE_PARAM(std::string,
                   kGlicUserStatusUrl,
                   &kGlicUserStatusCheck,
                   "glic-user-status-url",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                   "https://geminiweb-pa.googleapis.com/v1/glicStatus"
#else
                   ""
#endif
);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kGlicUserStatusRequestDelay,
                   &kGlicUserStatusCheck,
                   "glic-user-status-request-delay",
                   base::Hours(23));

BASE_FEATURE_PARAM(std::string,
                   kGeminiOAuth2Scope,
                   &kGlicUserStatusCheck,
                   "glic-user-status-oauth2-scope",
                   "https://www.googleapis.com/auth/gemini");

BASE_FEATURE_PARAM(double,
                   kGlicUserStatusRequestDelayJitter,
                   &kGlicUserStatusCheck,
                   "glic-user-status-request-delay-jitter",
                   0.005);
BASE_FEATURE(kGlicFreURLConfig,
             "GlicFreURLConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kGlicFreURL,
                   &kGlicFreURLConfig,
                   "glic-fre-url",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                   "https://gemini.google.com/glic/intro?"
#else
                   ""
#endif
);

BASE_FEATURE(kGlicLearnMoreURLConfig,
             "GlicLearnMoreURLConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kGlicShortcutsLearnMoreURL,
                   &kGlicLearnMoreURLConfig,
                   "glic-shortcuts-learn-more-url",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kGlicLauncherToggleLearnMoreURL,
                   &kGlicLearnMoreURLConfig,
                   "glic-shortcuts-launcher-toggle-learn-more-url",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kGlicLocationToggleLearnMoreURL,
                   &kGlicLearnMoreURLConfig,
                   "glic-shortcuts-location-toggle-learn-more-url",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kGlicTabAccessToggleLearnMoreURL,
                   &kGlicLearnMoreURLConfig,
                   "glic-shortcuts-tab-access-toggle-learn-more-url",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kGlicSettingsPageLearnMoreURL,
                   &kGlicLearnMoreURLConfig,
                   "glic-settings-page-learn-more-url",
                   "");

BASE_FEATURE(kGlicCSPConfig,
             "GlicCSPConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);
// TODO(crbug.com/378951332): Set appropriate default.
const base::FeatureParam<std::string> kGlicAllowedOriginsOverride{
    &kGlicCSPConfig, "glic-allowed-origins-override",
    // Space-delimited set of allowed origins.
    "https://*.google.com"};

// Enable/disable Glic web client responsiveness check feature.
BASE_FEATURE(kGlicClientResponsivenessCheck,
             "GlicClientResponsivenessCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);
// TODO(crbug.com/402184931): Set appropriate default for the 3 following
// parameters.
// Time interval for periodically sending responsiveness check to the web client
// in milliseconds.
const base::FeatureParam<int> kGlicClientResponsivenessCheckIntervalMs{
    &kGlicClientResponsivenessCheck,
    "glic-client-responsiveness-check-interval-ms", 5000};
// Maximum time to wait for glicWebClientCheckResponsive response during a
// responsiveness check before flagging the web client as unresponsive.
const base::FeatureParam<int> kGlicClientResponsivenessCheckTimeoutMs{
    &kGlicClientResponsivenessCheck,
    "glic-client-responsiveness-check-timeout-ms", 500};
// Maximum time for showing client unresponsive UI before going to the error
// state.
const base::FeatureParam<int> kGlicClientUnresponsiveUiMaxTimeMs{
    &kGlicClientResponsivenessCheck, "glic-client-unresponsive-ui-max-time-ms",
    5000};

BASE_FEATURE(kGlicUseShaderCache,
             "GlicUseShaderCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicKeyboardShortcutNewBadge,
             "GlicKeyboardShortcutNewBadge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicAppMenuNewBadge,
             "GlicAppMenuNewBadge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicDebugWebview,
             "GlicDebugWebview",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicScrollTo, "GlicScrollTo", base::FEATURE_DISABLED_BY_DEFAULT);
// Controls whether we enforce that documentId (a currently optional parameter)
// is set (and fail the request if it's not).
const base::FeatureParam<bool> kGlicScrollToEnforceDocumentId{
    &kGlicScrollTo, "glic-scroll-to-enforce-document-id", false};

// Controls whether the web client should resize itself to fit the window.
BASE_FEATURE(kGlicSizingFitWindow,
             "GlicSizingFitWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicWarming, "GlicWarming", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicDisableWarming,
             "GlicDisableWarming",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the amount of time from the GlicButtonController scheduling
// preload to the start of preloading (if preloading is possible).
const base::FeatureParam<int> kGlicWarmingDelayMs{
    &kGlicWarming, "glic-warming-delay-ms", 30 * 1000};

// Adds noise to the warming delay. The effective delay is increased by a
// random positive number of milliseconds between 0 and kGlicWarmingJitterMs.
const base::FeatureParam<int> kGlicWarmingJitterMs{
    &kGlicWarming, "glic-warming-jitter-ms", 10 * 1000};

BASE_FEATURE(kGlicFreWarming,
             "GlicFreWarming",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicWarmMultiple,
             "GlicWarmMultiple",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicTieredRollout,
             "GlicTieredRollout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicRollout, "GlicRollout", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicUserStatusCheck,
             "GlicUserStatusCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicClosedCaptioning,
             "GlicClosedCaptioning",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicPageContextEligibility,
             "GlicPageContextEligibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kGlicPageContextEligibilityAllowNoMetadata{
    &kGlicPageContextEligibility,
    "glic-page-context-eligibility-allow-no-metadata", true};

BASE_FEATURE(kGlicUnloadOnClose,
             "GlicUnloadOnClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicApiActivationGating,
             "GlicApiActivationGating",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicGetUserProfileInfoApiActivationGating,
             "GlicGetUserProfileInfoApiActivationGating",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicWebClientUnresponsiveMetrics,
             "GlicWebClientUnresponsiveMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_GLIC)

// Force Privacy Guide to be available even if it would be unavailable
// otherwise. This is meant for development and test purposes only.
BASE_FEATURE(kPrivacyGuideForceAvailable,
             "PrivacyGuideForceAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Defines if the linked services setting is eligible to be shown in Chrome
// settings.
BASE_FEATURE(kLinkedServicesSetting,
             "LinkedServicesSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Happiness Tracking System demo mode for Desktop
// Chrome.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopDemo,
             "HappinessTrackingSurveysForDesktopDemo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysConfiguration,
             "HappinessTrackingSurveysConfiguration",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kHappinessTrackingSurveysHostedUrl{
    &kHappinessTrackingSurveysConfiguration, "custom-url",
    "https://www.google.com/chrome/hats/index_m129.html"};

// Enables or disables the Happiness Tracking System for COEP issues in Chrome
// DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesCOEP,
             "HaTSDesktopDevToolsIssuesCOEP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Mixed Content issues in
// Chrome DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesMixedContent,
             "HaTSDesktopDevToolsIssuesMixedContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for same-site cookies
// issues in Chrome DevTools on Desktop.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite,
             "HappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Heavy Ad issues in
// Chrome DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesHeavyAd,
             "HaTSDesktopDevToolsIssuesHeavyAd",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for CSP issues in Chrome
// DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesCSP,
             "HaTSDesktopDevToolsIssuesCSP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Desktop Privacy Guide.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacyGuide,
             "HappinessTrackingSurveysForDesktopPrivacyGuide",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime{
        &kHappinessTrackingSurveysForDesktopPrivacyGuide, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettings,
             "HappinessTrackingSurveysForDesktopSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsTime{
        &kHappinessTrackingSurveysForDesktopSettings, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Privacy Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy,
             "HappinessTrackingSurveysForDesktopSettingsPrivacy",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "no-guide", false};
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyTime{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// NTP Modules.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopNtpModules,
             "HappinessTrackingSurveysForDesktopNtpModules",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for History Embeddings.
BASE_FEATURE(kHappinessTrackingSurveysForHistoryEmbeddings,
             "HappinessTrackingSurveysForHistoryEmbeddings",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForHistoryEmbeddingsDelayTime(
        &kHappinessTrackingSurveysForHistoryEmbeddings,
        "HappinessTrackingSurveysForHistoryEmbeddingsDelayTime",
        base::Seconds(20));

BASE_FEATURE(kHappinessTrackingSurveysForNtpPhotosOptOut,
             "HappinessTrackingSurveysForrNtpPhotosOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Wallpaper Search.
BASE_FEATURE(kHappinessTrackingSurveysForWallpaperSearch,
             "HappinessTrackingSurveysForWallpaperSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Chrome What's New.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew,
             "HappinessTrackingSurveysForDesktopWhatsNew",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime{
        &kHappinessTrackingSurveysForDesktopWhatsNew, "whats-new-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Chrome security page.
BASE_FEATURE(kHappinessTrackingSurveysForSecurityPage,
             "HappinessTrackingSurveysForSecurityPage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForSecurityPageTime{
        &kHappinessTrackingSurveysForSecurityPage, "security-page-time",
        base::Seconds(15)};
const base::FeatureParam<std::string>
    kHappinessTrackingSurveysForSecurityPageTriggerId{
        &kHappinessTrackingSurveysForSecurityPage, "security-page-trigger-id",
        ""};
const base::FeatureParam<bool>
    kHappinessTrackingSurveysForSecurityPageRequireInteraction{
        &kHappinessTrackingSurveysForSecurityPage,
        "security-page-require-interaction", false};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Enables or disables the Happiness Tracking System for the General survey.
BASE_FEATURE(kHappinessTrackingSystem,
             "HappinessTrackingSystem",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Bluetooth revamp
// survey.
BASE_FEATURE(kHappinessTrackingSystemBluetoothRevamp,
             "HappinessTrackingSystemBluetoothRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for the Battery life
// survey.
BASE_FEATURE(kHappinessTrackingSystemBatteryLife,
             "HappinessTrackingSystemBatteryLife",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for the Peripherals
// survey.
BASE_FEATURE(kHappinessTrackingSystemPeripherals,
             "HappinessTrackingSystemPeripherals",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for the Ent survey.
BASE_FEATURE(kHappinessTrackingSystemEnt,
             "HappinessTrackingSystemEnt",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for the Stability survey.
BASE_FEATURE(kHappinessTrackingSystemStability,
             "HappinessTrackingSystemStability",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for the Performance survey.
BASE_FEATURE(kHappinessTrackingSystemPerformance,
             "HappinessTrackingSystemPerformance",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Onboarding Experience.
BASE_FEATURE(kHappinessTrackingSystemOnboarding,
             "HappinessTrackingOnboardingExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for ARC Games survey.
BASE_FEATURE(kHappinessTrackingSystemArcGames,
             "HappinessTrackingArcGames",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Audio survey.
BASE_FEATURE(kHappinessTrackingSystemAudio,
             "HappinessTrackingAudio",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Audio Output
// Processing.
BASE_FEATURE(kHappinessTrackingSystemAudioOutputProc,
             "HappinessTrackingAudioOutputProc",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Bluetooth Audio survey.
BASE_FEATURE(kHappinessTrackingSystemBluetoothAudio,
             "HappinessTrackingBluetoothAudio",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Personalization Avatar survey.
BASE_FEATURE(kHappinessTrackingPersonalizationAvatar,
             "HappinessTrackingPersonalizationAvatar",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Personalization Screensaver survey.
BASE_FEATURE(kHappinessTrackingPersonalizationScreensaver,
             "HappinessTrackingPersonalizationScreensaver",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Personalization Wallpaper survey.
BASE_FEATURE(kHappinessTrackingPersonalizationWallpaper,
             "HappinessTrackingPersonalizationWallpaper",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Media App PDF survey.
BASE_FEATURE(kHappinessTrackingMediaAppPdf,
             "HappinessTrackingMediaAppPdf",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Camera App survey.
BASE_FEATURE(kHappinessTrackingSystemCameraApp,
             "HappinessTrackingCameraApp",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Photos Experience survey.
BASE_FEATURE(kHappinessTrackingPhotosExperience,
             "HappinessTrackingPhotosExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for General Camera survey.
BASE_FEATURE(kHappinessTrackingGeneralCamera,
             "HappinessTrackingGeneralCamera",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Prioritized General Camera survey.
BASE_FEATURE(kHappinessTrackingGeneralCameraPrioritized,
             "HappinessTrackingGeneralCameraPrioritized",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for OS Settings Search survey.
BASE_FEATURE(kHappinessTrackingOsSettingsSearch,
             "HappinessTrackingOsSettingsSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Borealis games survey.
BASE_FEATURE(kHappinessTrackingBorealisGames,
             "HappinessTrackingBorealisGames",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for ChromeOS Launcher survey. This
// survey is enabled to 25% of users.
BASE_FEATURE(kHappinessTrackingLauncherAppsFinding,
             "HappinessTrackingLauncherAppsFinding",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for ChromeOS Launcher survey. This
// survey is enabled to 75% of users.
BASE_FEATURE(kHappinessTrackingLauncherAppsNeeding,
             "HappinessTrackingLauncherAppsNeeding",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for the Office integration.
BASE_FEATURE(kHappinessTrackingOffice,
             "HappinessTrackingOffice",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables HTTPS-First Mode in a balanced configuration that doesn't warn on
// HTTP when HTTPS can't be reasonably expected.
BASE_FEATURE(kHttpsFirstBalancedMode,
             "HttpsFirstBalancedMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Automatically enables HTTPS-First Mode in a balanced configuration when
// possible.
BASE_FEATURE(kHttpsFirstBalancedModeAutoEnable,
             "HttpsFirstBalancedModeAutoEnable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a dialog-based UI for HTTPS-First Mode.
BASE_FEATURE(kHttpsFirstDialogUi,
             "HttpsFirstDialogUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for crbug.com/1414633.
BASE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers,
             "HttpsOnlyModeForAdvancedProtectionUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for engaged sites. No-op if HttpsFirstModeV2 or
// HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForEngagedSites,
             "HttpsFirstModeV2ForEngagedSites",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for typically secure users. No-op if
// HttpsFirstModeV2 or HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForTypicallySecureUsers,
             "HttpsFirstModeV2ForTypicallySecureUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automatically upgrading main frame navigations to HTTPS.
BASE_FEATURE(kHttpsUpgrades, "HttpsUpgrades", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode by default in Incognito Mode. (The related feature
// kHttpsFirstModeIncognitoNewSettings controls whether new settings controls
// are available for opting out of this default behavior.)
BASE_FEATURE(kHttpsFirstModeIncognito,
             "HttpsFirstModeIncognito",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes the binary opt-in to HTTPS-First Mode with a tri-state setting (HFM
// everywhere, HFM in Incognito, or no HFM) with HFM-in-Incognito the new
// default setting. This feature is dependent on kHttpsFirstModeIncognito.
BASE_FEATURE(kHttpsFirstModeIncognitoNewSettings,
             "HttpsFirstModeIncognitoNewSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enables immersive fullscreen. The tab strip and toolbar are placed underneath
// the titlebar. The tab strip and toolbar can auto hide and reveal.
BASE_FEATURE(kImmersiveFullscreen,
             "ImmersiveFullscreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables immersive fullscreen mode for PWA windows. PWA windows will use
// immersive fullscreen mode if and only if both this and kImmersiveFullscreen
// are enabled. PWA windows currently do not use ImmersiveFullscreenTabs even if
// the feature is enabled.
BASE_FEATURE(kImmersiveFullscreenPWAs,
             "ImmersiveFullscreenPWAs",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// A feature that controls whether Chrome warns about incompatible applications.
// This feature requires Windows 10 or higher to work because it depends on
// the "Apps & Features" system settings.
BASE_FEATURE(kIncompatibleApplicationsWarning,
             "IncompatibleApplicationsWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables Isolated Web App Developer Mode, which allows developers to
// install untrusted Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppDevMode,
             "IsolatedWebAppDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables users on unmanaged devices to install Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppUnmanagedInstall,
             "IsolatedWebAppUnmanagedInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables users to install isolated web apps in managed guest sessions.
BASE_FEATURE(kIsolatedWebAppManagedGuestSessionInstall,
             "IsolatedWebAppManagedGuestSessionInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables bundle cache for isolated web apps in kiosk and managed guest
// session.
BASE_FEATURE(kIsolatedWebAppBundleCache,
             "IsolatedWebAppBundleCache",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// When enabled, allows other features to use the k-Anonymity Service.
BASE_FEATURE(kKAnonymityService,
             "KAnonymityService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Origin to use for requests to the k-Anonymity Auth server to get trust
// tokens.
constexpr base::FeatureParam<std::string> kKAnonymityServiceAuthServer{
    &kKAnonymityService, "KAnonymityServiceAuthServer",
    "https://chromekanonymityauth-pa.googleapis.com/"};

// Origin to use as a relay for OHTTP requests to the k-Anonymity Join server.
constexpr base::FeatureParam<std::string> kKAnonymityServiceJoinRelayServer{
    &kKAnonymityService, "KAnonymityServiceJoinRelayServer",
    "https://google-ohttp-relay-join.fastly-edge.com/"};

// Origin to use to notify the k-Anonymity Join server of group membership.
constexpr base::FeatureParam<std::string> kKAnonymityServiceJoinServer{
    &kKAnonymityService, "KAnonymityServiceJoinServer",
    "https://chromekanonymity-pa.googleapis.com/"};

// Minimum amount of time allowed between notifying the Join server of
// membership in a distinct group.
constexpr base::FeatureParam<base::TimeDelta> kKAnonymityServiceJoinInterval{
    &kKAnonymityService, "KAnonymityServiceJoinInterval", base::Days(1)};

// Origin to use as a relay for OHTTP requests to the k-Anonymity Query server.
constexpr base::FeatureParam<std::string> kKAnonymityServiceQueryRelayServer{
    &kKAnonymityService, "KAnonymityServiceQueryRelayServer",
    "https://google-ohttp-relay-query.fastly-edge.com/"};

// Origin to use to request k-anonymity status from the k-Anonymity Query
// server.
constexpr base::FeatureParam<std::string> kKAnonymityServiceQueryServer{
    &kKAnonymityService, "KAnonymityServiceQueryServer",
    "https://chromekanonymityquery-pa.googleapis.com/"};

// Minimum amount of time allowed between requesting k-anonymity status from the
// Query server for a distinct group.
constexpr base::FeatureParam<base::TimeDelta> kKAnonymityServiceQueryInterval{
    &kKAnonymityService, "KAnonymityServiceQueryInterval", base::Days(1)};

// When enabled, the k-Anonymity Service will send requests to the Join and
// Query k-anonymity servers.
BASE_FEATURE(kKAnonymityServiceOHTTPRequests,
             "KAnonymityServiceOHTTPRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the k-Anonymity Service can use a persistent storage to cache
// public keys.
BASE_FEATURE(kKAnonymityServiceStorage,
             "KAnonymityServiceStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLinuxLowMemoryMonitor,
             "LinuxLowMemoryMonitor",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Values taken from the low-memory-monitor documentation and also apply to the
// portal API:
// https://hadess.pages.freedesktop.org/low-memory-monitor/gdbus-org.freedesktop.LowMemoryMonitor.html
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel{
    &kLinuxLowMemoryMonitor, "moderate_level", 50};
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel{
    &kLinuxLowMemoryMonitor, "critical_level", 255};
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kListWebAppsSwitch,
             "ListWebAppsSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Whether to show the Hidden toggle in Settings, allowing users to toggle
// whether to treat a WiFi network as having a hidden ssid.
BASE_FEATURE(kShowHiddenNetworkToggle,
             "ShowHiddenNetworkToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables the use of system notification centers instead of using the Message
// Center for displaying the toasts. The feature is hardcoded to enabled for
// Chrome OS.
BASE_FEATURE(kNativeNotifications,
             "NativeNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSystemNotifications,
             "SystemNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enables the usage of Apple's new Notification API.
BASE_FEATURE(kNewMacNotificationAPI,
             "NewMacNotificationAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
// Enables new UX for files policy restrictions on ChromeOS.
BASE_FEATURE(kNewFilesPolicyUX,
             "NewFilesPolicyUX",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// When kNoReferrers is enabled, most HTTP requests will provide empty
// referrers instead of their ordinary behavior.
BASE_FEATURE(kNoReferrers, "NoReferrers", base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Changes behavior of requireInteraction for notifications. Instead of staying
// on-screen until dismissed, they are instead shown for a very long time.
BASE_FEATURE(kNotificationDurationLongForRequireInteraction,
             "NotificationDurationLongForRequireInteraction",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOfflineAutoFetch,
             "OfflineAutoFetch",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
BASE_FEATURE(kOnConnectNative,
             "OnConnectNative",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables or disables the OOM intervention.
BASE_FEATURE(kOomIntervention,
             "OomIntervention",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// Changes behavior of App Launch Prefetch to ignore chrome browser launches
// after acquiry of the singleton.
BASE_FEATURE(kOverridePrefetchOnSingleton,
             "OverridePrefetchOnSingleton",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Skips requesting the Parent Access Code for reauth.
BASE_FEATURE(kSkipParentAccessCodeForReauth,
             "SkipParentAccessCodeForReauth",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable support for "Plugin VMs" on Chrome OS.
BASE_FEATURE(kPluginVm, "PluginVm", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allows Chrome to do preconnect when prerender fails.
BASE_FEATURE(kPrerenderFallbackToPreconnect,
             "PrerenderFallbackToPreconnect",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enable the ChromeOS print preview to be opened instead of the browser print
// preview.
BASE_FEATURE(kPrintPreviewCrosPrimary,
             "PrintPreviewCrosPrimary",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use managed per-printer print job options set via
// DevicePrinters/PrinterBulkConfiguration policy in print preview.
BASE_FEATURE(kUseManagedPrintJobOptionsInPrintPreview,
             "UseManagedPrintJobOptionsInPrintPreview",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
BASE_FEATURE(kPushMessagingBackgroundMode,
             "PushMessagingBackgroundMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shows a confirmation dialog when updates to a PWAs icon has been detected.
BASE_FEATURE(kPwaUpdateDialogForIcon,
             "PwaUpdateDialogForIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using quiet prompts for notification permission requests.
BASE_FEATURE(kQuietNotificationPrompts,
             "QuietNotificationPrompts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables recording additional web app related debugging data to be displayed
// in: chrome://web-app-internals
BASE_FEATURE(kRecordWebAppDebugInfo,
             "RecordWebAppDebugInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification permission revocation for abusive origins.
BASE_FEATURE(kAbusiveNotificationPermissionRevocation,
             "AbusiveOriginNotificationPermissionRevocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables permanent removal of Legacy Supervised Users on startup.
BASE_FEATURE(kRemoveSupervisedUsersOnStartup,
             "RemoveSupervisedUsersOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kSafetyHubExtensionsUwSTrigger,
             "SafetyHubExtensionsUwSTrigger",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables extensions that do not display proper privacy practices in the
// Safety Hub Extension Reivew Panel.
BASE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger,
             "SafetyHubExtensionsNoPrivacyPracticesTrigger",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables offstore extensions to be shown in the Safety Hub Extension
// review panel.
BASE_FEATURE(kSafetyHubExtensionsOffStoreTrigger,
             "SafetyHubExtensionsOffStoreTrigger",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables Safety Hub feature.
BASE_FEATURE(kSafetyHub, "SafetyHub", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyHubThreeDotDetails,
             "SafetyHubThreeDotDetails",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyHubDisruptiveNotificationRevocation,
             "SafetyHubDisruptiveNotificationRevocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationExperimentVersion{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"experiment_version", /*default_value=*/0};

constexpr base::FeatureParam<bool>
    kSafetyHubDisruptiveNotificationRevocationShadowRun{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"shadow_run", /*default_value=*/true};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinNotificationCount{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_notification_count", /*default_value=*/3};

constexpr base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"max_engagement_score", /*default_value=*/0.0};

constexpr base::FeatureParam<base::TimeDelta>
    kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"waiting_time_as_proposed", /*default_value=*/base::Days(0)};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationNotificationTimeoutSeconds{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"notification_timeout_seconds",
        /*default_value=*/7 * 24 * 3600};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_false_positive_cooldown", /*default_value=*/0};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"max_false_positive_period", /*default_value=*/14};

// TODO(crbug.com/406472515): Site engagement score increase on navigation
// happens at the same time as us detecting the navigation. If the score delta
// is 0, the initial navigation won't trigger marking the site as false
// positive.
constexpr base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_engagement_score_delta", /*default_value=*/0.0};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationUserRegrantWaitingPeriod{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"user_regrant_waiting_period", /*default_value=*/7};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"waiting_for_metrics_days", /*default_value=*/7};

#if BUILDFLAG(IS_ANDROID)
// Enables Safety Hub card in magic stack.
BASE_FEATURE(kSafetyHubMagicStack,
             "SafetyHubMagicStack",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Safety Hub followup work.
BASE_FEATURE(kSafetyHubFollowup,
             "SafetyHubFollowup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Safety Hub organic HaTS survey on Android.
BASE_FEATURE(kSafetyHubAndroidOrganicSurvey,
             "SafetyHubAndroidOrganicSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kSafetyHubAndroidOrganicTriggerId(
    &kSafetyHubAndroidOrganicSurvey,
    "trigger_id",
    /*default_value=*/
    "");

// Enables Safety Hub HaTS survey on Android.
BASE_FEATURE(kSafetyHubAndroidSurvey,
             "SafetyHubAndroidSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kSafetyHubAndroidTriggerId(
    &kSafetyHubAndroidSurvey,
    "trigger_id",
    /*default_value=*/"");

// Enables new triggers for the Safety Hub HaTS survey on Android.
BASE_FEATURE(kSafetyHubAndroidSurveyV2,
             "SafetyHubAndroidSurveyV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Weak and Reused passwords in Safety Hub.
BASE_FEATURE(kSafetyHubWeakAndReusedPasswords,
             "SafetyHubWeakAndReusedPasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the local passwords module in Safety Hub.
BASE_FEATURE(kSafetyHubLocalPasswordsModule,
             "SafetyHubLocalPasswordsModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the unified passwords module in Safety Hub.
BASE_FEATURE(kSafetyHubUnifiedPasswordsModule,
             "SafetyHubUnifiedPasswordsModule",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Enables Safety Hub services on start up feature.
BASE_FEATURE(kSafetyHubServicesOnStartUp,
             "SafetyHubServicesOnStartUp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the Trust Safety Sentiment Survey for Safety Hub.
BASE_FEATURE(kSafetyHubTrustSafetySentimentSurvey,
             "TrustSafetySentimentSurveyForSafetyHub",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the A/B Experiment Survey for Safety Hub.
BASE_FEATURE(kSafetyHubHaTSOneOffSurvey,
             "SafetyHubHaTSOneOffSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentControlTriggerId{
        &kSafetyHubHaTSOneOffSurvey, "safety-hub-ab-control-trigger-id", ""};
const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentNotificationTriggerId{
        &kSafetyHubHaTSOneOffSurvey, "safety-hub-ab-notification-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentInteractionTriggerId{
        &kSafetyHubHaTSOneOffSurvey, "safety-hub-ab-interaction-trigger-id",
        ""};
#endif  // !BUILDFLAG(IS_ANDROID)

// Time between automated runs of the password check.
const base::FeatureParam<base::TimeDelta> kBackgroundPasswordCheckInterval{
    &kSafetyHub, "background-password-check-interval", base::Days(30)};

// When the password check didn't run at its scheduled time (e.g. client was
// offline) it will be scheduled to run within this time frame. Changing the
// value  will flaten the picks on rush hours, e.g: 1h will cause higher
// picks than 4h.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<base::TimeDelta> kPasswordCheckOverdueInterval{
    &kSafetyHub, "password-check-overdue-interval", base::Hours(4)};

// Password check runs randomly based on the weight of each day. Parameters
// below will be used to adjust weights, if necessary. Weight to randomly
// schedule for Mondays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckMonWeight{
    &kSafetyHub, "password-check-mon-weight", 6};

// Weight to randomly schedule for Tuesdays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckTueWeight{
    &kSafetyHub, "password-check-tue-weight", 9};

// Weight to randomly schedule for Wednesdays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckWedWeight{
    &kSafetyHub, "password-check-wed-weight", 9};

// Weight to randomly schedule for Thursdays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckThuWeight{
    &kSafetyHub, "password-check-thu-weight", 9};

// Weight to randomly schedule for Fridays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckFriWeight{
    &kSafetyHub, "password-check-fri-weight", 9};

// Weight to randomly schedule for Saturdays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckSatWeight{
    &kSafetyHub, "password-check-sat-weight", 6};

// Weight to randomly schedule for Sundays.
COMPONENT_EXPORT(CHROME_FEATURES)
const base::FeatureParam<int> kPasswordCheckSunWeight{
    &kSafetyHub, "password-check-sun-weight", 6};

// Engagement limits Notification permissions module.
const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit{
        &kSafetyHub, "min-engagement-notification-count", 0};
const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit{
        &kSafetyHub, "low-engagement-notification-count", 4};

const char kPasswordCheckNotificationIntervalName[] =
    "password-check-notification-interval";
const char kRevokedPermissionsNotificationIntervalName[] =
    "revoked-permissions-notification-interval";
const char kNotificationPermissionsNotificationIntervalName[] =
    "notification-permissions-notification-interval";
const char kSafeBrowsingNotificationIntervalName[] =
    "safe-browsing-notification-interval";

// Interval to show notification for compromised password in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta> kPasswordCheckNotificationInterval{
    &kSafetyHub, kPasswordCheckNotificationIntervalName, base::Days(0)};

// Interval to show notification for revoked permissions in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta>
    kRevokedPermissionsNotificationInterval{
        &kSafetyHub, kRevokedPermissionsNotificationIntervalName,
        base::Days(10)};

// Interval to show notification for notification permissions in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta>
    kNotificationPermissionsNotificationInterval{
        &kSafetyHub, kNotificationPermissionsNotificationIntervalName,
        base::Days(10)};

// Interval to show notification for safe browsing in Safety Hub notifications.
const base::FeatureParam<base::TimeDelta> kSafeBrowsingNotificationInterval{
    &kSafetyHub, kSafeBrowsingNotificationIntervalName, base::Days(90)};

// Controls whether SCT audit reports are queued and the rate at which they
// should be sampled. Default sampling rate is 1/10,000 certificates.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSCTAuditing, "SCTAuditing", base::FEATURE_ENABLED_BY_DEFAULT);
#else
// This requires backend infrastructure and a data collection policy.
// Non-Chrome builds should not use Chrome's infrastructure.
BASE_FEATURE(kSCTAuditing, "SCTAuditing", base::FEATURE_DISABLED_BY_DEFAULT);
#endif
constexpr base::FeatureParam<double> kSCTAuditingSamplingRate{
    &kSCTAuditing, "sampling_rate", 0.0001};

// SCT auditing hashdance allows Chrome clients who are not opted-in to Enhanced
// Safe Browsing Reporting to perform a k-anonymous query to see if Google knows
// about an SCT seen in the wild. If it hasn't been seen, then it is considered
// a security incident and uploaded to Google.
BASE_FEATURE(kSCTAuditingHashdance,
             "SCTAuditingHashdance",
             base::FEATURE_ENABLED_BY_DEFAULT);

// An estimated high bound for the time it takes Google to ingest updates to an
// SCT log. Chrome will wait for at least this time plus the Log's Maximum Merge
// Delay after an SCT's timestamp before performing a hashdance lookup query.
const base::FeatureParam<base::TimeDelta> kSCTLogExpectedIngestionDelay{
    &kSCTAuditingHashdance,
    "sct_log_expected_ingestion_delay",
    base::Hours(1),
};

// A random delay will be added to the expected log ingestion delay between zero
// and this maximum. This prevents a burst of queries once a new SCT is issued.
const base::FeatureParam<base::TimeDelta> kSCTLogMaxIngestionRandomDelay{
    &kSCTAuditingHashdance,
    "sct_log_max_ingestion_random_delay",
    base::Hours(1),
};

// Alternative to switches::kSitePerProcess, for turning on full site isolation.
// Launch bug: https://crbug.com/810843.  This is a //chrome-layer feature to
// avoid turning on site-per-process by default for *all* //content embedders
// (e.g. this approach lets ChromeCast avoid site-per-process mode).
//
// TODO(alexmos): Move this and the other site isolation features below to
// browser_features, as they are only used on the browser side.
BASE_FEATURE(kSitePerProcess,
             "SitePerProcess",
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(ENABLE_ANDROID_SITE_ISOLATION)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// The default behavior to opt devtools users out of
// kProcessPerSiteUpToMainFrameThreshold.
BASE_FEATURE(kProcessPerSiteSkipDevtoolsUsers,
             "ProcessPerSiteSkipDevtoolsUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The default behavior to opt enterprise users out of
// kProcessPerSiteUpToMainFrameThreshold.
BASE_FEATURE(kProcessPerSiteSkipEnterpriseUsers,
             "ProcessPerSiteSkipEnterpriseUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts "ProcessPerSiteUpToMainFrameThreshold" to the default search
// engine. Has no effect if "ProcessPerSiteUpToMainFrameThreshold" is disabled.
// Note: The "ProcessPerSiteUpToMainFrameThreshold" feature is defined in
// //content.
BASE_FEATURE(kProcessPerSiteForDSE,
             "ProcessPerSiteForDSE",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables the SkyVault (cloud-first) changes, some of which are also controlled
// by policies: removing local storage, saving downloads and screen captures to
// the cloud, and related UX changes, primarily in the Files App.
BASE_FEATURE(kSkyVault, "SkyVault", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the SkyVault V2 changes, which are also controlled by policies:
// LocalUserFilesAllowed, DownloadDirectory and ScreenCaptureLocation.
BASE_FEATURE(kSkyVaultV2, "SkyVaultV2", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the SkyVault V3 changes, which improve the resilience of file uploads
// and error handling.
BASE_FEATURE(kSkyVaultV3, "SkyVaultV3", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables SmartDim on Chrome OS.
BASE_FEATURE(kSmartDim, "SmartDim", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables chrome://sys-internals.
BASE_FEATURE(kSysInternals, "SysInternals", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables TPM firmware update capability on Chrome OS.
BASE_FEATURE(kTPMFirmwareUpdate,
             "TPMFirmwareUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// Enables the Support Tool to include a screenshot in the exported support tool
// packet.
BASE_FEATURE(kSupportToolScreenshot,
             "SupportToolScreenshot",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// Enables the blocking of third-party modules. This feature requires Windows 8
// or higher because it depends on the ProcessExtensionPointDisablePolicy
// mitigation, which was not available on Windows 7.
// Note: Due to a limitation in the implementation of this feature, it is
// required to start the browser two times to fully enable or disable it.
BASE_FEATURE(kThirdPartyModulesBlocking,
             "ThirdPartyModulesBlocking",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page. As of M89, mixed downloads are blocked on all platforms.
BASE_FEATURE(kTreatUnsafeDownloadsAsActive,
             "TreatUnsafeDownloadsAsActive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TrustSafetySentimentSurvey
#if !BUILDFLAG(IS_ANDROID)
// Enables surveying of users of Trust & Safety features with HaTS.
BASE_FEATURE(kTrustSafetySentimentSurvey,
             "TrustSafetySentimentSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The minimum and maximum time after a user has interacted with a Trust and
// Safety they are eligible to be surveyed.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMinTimeToPrompt{
        &kTrustSafetySentimentSurvey, "min-time-to-prompt", base::Minutes(2)};
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMaxTimeToPrompt{
        &kTrustSafetySentimentSurvey, "max-time-to-prompt", base::Minutes(60)};
// The maximum and minimum range for the random number of NTPs that the user
// must at least visit after interacting with a Trust and Safety feature to be
// eligible for a survey.
const base::FeatureParam<int> kTrustSafetySentimentSurveyNtpVisitsMinRange{
    &kTrustSafetySentimentSurvey, "ntp-visits-min-range", 2};
const base::FeatureParam<int> kTrustSafetySentimentSurveyNtpVisitsMaxRange{
    &kTrustSafetySentimentSurvey, "ntp-visits-max-range", 4};
// The feature area probabilities for each feature area considered as part of
// the Trust & Safety sentiment survey.
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySettingsProbability{
        &kTrustSafetySentimentSurvey, "privacy-settings-probability", 0.6};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTrustedSurfaceProbability{
        &kTrustSafetySentimentSurvey, "trusted-surface-probability", 0.4};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTransactionsProbability{
        &kTrustSafetySentimentSurvey, "transactions-probability", 0.05};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId{
        &kTrustSafetySentimentSurvey, "privacy-settings-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurvey, "trusted-surface-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId{
        &kTrustSafetySentimentSurvey, "transactions-trigger-id", ""};
// The time the user must remain on settings after interacting with a privacy
// setting to be considered.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyPrivacySettingsTime{&kTrustSafetySentimentSurvey,
                                                   "privacy-settings-time",
                                                   base::Seconds(20)};
// The time the user must have the Trusted Surface bubble open to be considered.
// Alternatively the user can interact with the bubble, in which case this time
// is irrelevant.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTrustedSurfaceTime{
        &kTrustSafetySentimentSurvey, "trusted-surface-time", base::Seconds(5)};
// The time the user must remain on settings after visiting the password
// manager page.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTransactionsPasswordManagerTime{
        &kTrustSafetySentimentSurvey, "transactions-password-manager-time",
        base::Seconds(20)};

#endif

// TrustSafetySentimentSurveyV2
#if !BUILDFLAG(IS_ANDROID)
// Enables the second version of the sentiment survey for users of Trust &
// Safety features, using HaTS.
BASE_FEATURE(kTrustSafetySentimentSurveyV2,
             "TrustSafetySentimentSurveyV2",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The minimum and maximum time after a user has interacted with a Trust and
// Safety feature that they are eligible to be surveyed.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinTimeToPrompt{
        &kTrustSafetySentimentSurveyV2, "min-time-to-prompt", base::Minutes(2)};
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MaxTimeToPrompt{&kTrustSafetySentimentSurveyV2,
                                                 "max-time-to-prompt",
                                                 base::Minutes(60)};
// The maximum and minimum range for the random number of NTPs that the user
// must at least visit after interacting with a Trust and Safety feature to be
// eligible for a survey.
const base::FeatureParam<int> kTrustSafetySentimentSurveyV2NtpVisitsMinRange{
    &kTrustSafetySentimentSurveyV2, "ntp-visits-min-range", 2};
const base::FeatureParam<int> kTrustSafetySentimentSurveyV2NtpVisitsMaxRange{
    &kTrustSafetySentimentSurveyV2, "ntp-visits-max-range", 4};
// The minimum time that has to pass in the current session before a user can be
// eligible to be considered for the baseline control group.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinSessionTime{
        &kTrustSafetySentimentSurveyV2, "min-session-time", base::Seconds(30)};
// The feature area probabilities for each feature area considered as part of
// the Trust & Safety sentiment survey.
// TODO(crbug.com/40245476): Calculate initial probabilities and remove 0.0
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2BrowsingDataProbability{
        &kTrustSafetySentimentSurveyV2, "browsing-data-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2ControlGroupProbability{
        &kTrustSafetySentimentSurveyV2, "control-group-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2DownloadWarningUIProbability{
        &kTrustSafetySentimentSurveyV2, "download-warning-ui-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordCheckProbability{
        &kTrustSafetySentimentSurveyV2, "password-check-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordProtectionUIProbability{
        &kTrustSafetySentimentSurveyV2, "password-protection-ui-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability{
        &kTrustSafetySentimentSurveyV2, "safety-check-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationProbability{
        &kTrustSafetySentimentSurveyV2, "safety-hub-notification-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionProbability{
        &kTrustSafetySentimentSurveyV2, "safety-hub-interaction-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2TrustedSurfaceProbability{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacyGuideProbability{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialProbability{
        &kTrustSafetySentimentSurveyV2,
        "safe-browsing-interstitial-probability", 0.0};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId{
        &kTrustSafetySentimentSurveyV2, "browsing-data-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId{
        &kTrustSafetySentimentSurveyV2, "control-group-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2DownloadWarningUITriggerId{
        &kTrustSafetySentimentSurveyV2, "download-warning-ui-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "password-check-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordProtectionUITriggerId{
        &kTrustSafetySentimentSurveyV2, "password-protection-ui-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-check-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-hub-interaction-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-hub-notification-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId{
        &kTrustSafetySentimentSurveyV2, "safe-browsing-interstitial-trigger-id",
        ""};
// The time the user must have the Trusted Surface bubble open to be considered.
// Alternatively the user can interact with the bubble, in which case this time
// is irrelevant.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTime{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-time",
        base::Seconds(5)};
#endif

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kUseChromiumUpdater,
             "UseChromiumUpdater",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAppManifestIconUpdating,
             "WebAppManifestIconUpdating",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate,
             "WebAppManifestPolicyAppIdentityUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts the WebUI scripts able to use the generated code cache according to
// embedder-specified heuristics.
BASE_FEATURE(kRestrictedWebUICodeCache,
             "RestrictedWebUICodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Defines a comma-separated list of resource names able to use the generated
// code cache when RestrictedWebUICodeCache is enabled.
const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources{
    &kRestrictedWebUICodeCache, "RestrictedWebUICodeCacheResources", ""};

#if BUILDFLAG(IS_CHROMEOS)
// Populates storage dimensions in UMA log if enabled. Requires diagnostics
// package in the image.
BASE_FEATURE(kUmaStorageDimensions,
             "UmaStorageDimensions",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// Enables the accelerated default browser flow for Windows 10.
BASE_FEATURE(kWin10AcceleratedDefaultBrowserFlow,
             "Win10AcceleratedDefaultBrowserFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
bool IsParentAccessCodeForReauthEnabled() {
  return !base::FeatureList::IsEnabled(kSkipParentAccessCodeForReauth);
}

// A feature to indicate whether setting wake time >24hours away is supported by
// the platform's RTC.
// TODO(b/187516317): Remove when the issue is resolved in FW.
BASE_FEATURE(kSupportsRtcWakeOver24Hours,
             "SupportsRtcWakeOver24Hours",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A feature to enable event based log uploads. See
// go/cros-eventbasedlogcollection-dd.
BASE_FEATURE(kEventBasedLogUpload,
             "EventBasedLogUpload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A feature to enable periodic log upload migration. This includes using new
// mechanism for collecting, exporting and uploading logs. See
// go/legacy-log-upload-migration.
BASE_FEATURE(kPeriodicLogUploadMigration,
             "PeriodicLogUploadMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to enable periodic log K12 user age classification. See
// go/teachers-on-chromeos-data.
BASE_FEATURE(kK12AgeClassificationMetricsProvider,
             "K12AgeClassificationMetricsProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to enable periodic log class management enabled policy.
BASE_FEATURE(kClassManagementEnabledMetricsProvider,
             "ClassManagementEnabledMetricsProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// A feature to disable shortcut creation from the Chrome UI, and instead use
// that to create DIY apps.
BASE_FEATURE(kDisableShortcutsEnableDiy,
             "DisableShortcutsEnableDiy",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
