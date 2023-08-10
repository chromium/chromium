// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "ppapi/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order.

#if BUILDFLAG(IS_CHROMEOS_ASH)
// If enabled device status collector will add the type of session (Affiliated
// User, Kiosks, Managed Guest Sessions) to the device status report.
BASE_FEATURE(kActivityReportingSessionType,
             "ActivityReportingSessionType",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables logging for adaptive screen brightness on Chrome OS.
BASE_FEATURE(kAdaptiveScreenBrightnessLogging,
             "AdaptiveScreenBrightnessLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kAppManagementAppDetails,
             "AppManagementAppDetails",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kAppDeduplicationServiceFondue,
             "AppDeduplicationServiceFondue",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kAppPreloadService,
             "AppPreloadService",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#endif  // BUILDFLAG(IS_MAC)

// Enables the built-in DNS resolver.
BASE_FEATURE(kAsyncDns,
             "AsyncDns",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable Borealis on Chrome OS.
BASE_FEATURE(kBorealis, "Borealis", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables change picture video mode.
BASE_FEATURE(kChangePictureVideoMode,
             "ChangePictureVideoMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables "usm" service in the list of user services returned by
// userInfo Gaia message.
BASE_FEATURE(kCrOSEnableUSMUserService,
             "CrOSEnableUSMUserService",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables flash component updates on Chrome OS.
BASE_FEATURE(kCrosCompUpdates,
             "CrosCompUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable project Crostini, Linux VMs on Chrome OS.
BASE_FEATURE(kCrostini, "Crostini", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable additional Crostini session status reporting for
// managed devices only, i.e. reports of installed apps and kernel version.
BASE_FEATURE(kCrostiniAdditionalEnterpriseReporting,
             "CrostiniAdditionalEnterpriseReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable advanced access controls for Crostini-related features
// (e.g. restricting VM CLI tools access, restricting Crostini root access).
BASE_FEATURE(kCrostiniAdvancedAccessControls,
             "CrostiniAdvancedAccessControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables infrastructure for applying Ansible playbook to default Crostini
// container.
BASE_FEATURE(kCrostiniAnsibleInfrastructure,
             "CrostiniAnsibleInfrastructure",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables infrastructure for generating Ansible playbooks for the default
// Crostini container from software configurations in JSON schema.
BASE_FEATURE(kCrostiniAnsibleSoftwareManagement,
             "CrostiniAnsibleSoftwareManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for sideloading android apps into Arc via crostini.
BASE_FEATURE(kCrostiniArcSideload,
             "CrostiniArcSideload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new UI for browser created shortcut backed by web app system
// on Chrome OS.
BASE_FEATURE(kCrosWebAppShortcutUiUpdate,
             "CrosWebAppShortcutUiUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// Enables parsing and enforcing Data Leak Prevention policy rules that
// restricts usage of some system features, e.g.clipboard, screenshot, etc.
// for confidential content.
BASE_FEATURE(kDataLeakPreventionPolicy,
             "DataLeakPreventionPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables starting of Data Leak Prevention Files Daemon by sending the
// DLP policy there. The daemon might restrict access to some protected files.
BASE_FEATURE(kDataLeakPreventionFilesRestriction,
             "DataLeakPreventionFilesRestriction",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables passing additional user authentication in requests to DMServer
// (policy fetch, status report upload).
BASE_FEATURE(kDMServerOAuthForChildUser,
             "DMServerOAuthForChildUser",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Whether to allow installed-by-default web apps to be installed or not.
BASE_FEATURE(kPreinstalledWebAppInstallation,
             "DefaultWebAppInstallation",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// An experiment for making preinstalled apps open in a window by default.
BASE_FEATURE(kPreinstalledWebAppWindowExperiment,
             "PreinstalledWebAppWindowExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Enables OS Integration sub managers to execute the
// registration/unregistration functionality and write the new OS states to the
// DB.
BASE_FEATURE(kOsIntegrationSubManagers,
             "OsIntegrationSubManagers",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<OsIntegrationSubManagersStage>::Option
    sub_manager_stages[] = {
        {OsIntegrationSubManagersStage::kWriteConfig, "write_config"},
        {OsIntegrationSubManagersStage::kExecuteAndWriteConfig,
         "execute_and_write_config"}};
const base::FeatureParam<OsIntegrationSubManagersStage>
    kOsIntegrationSubManagersStageParam{
        &kOsIntegrationSubManagers, "stage",
        OsIntegrationSubManagersStage::kWriteConfig, &sub_manager_stages};
#endif

#if BUILDFLAG(IS_CHROMEOS)
// If enabled, specified extensions cannot be closed via the task manager.
BASE_FEATURE(kDesktopTaskManagerEndProcessDisabledForExtension,
             "DesktopTaskManagerEndProcessDisabledForExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Generates customised default offline page that is shown when web app is
// offline if no custom page is provided by developer.
BASE_FEATURE(kPWAsDefaultOfflinePage,
             "PWAsDefaultOfflinePage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When installing default installed PWAs, we wait for service workers
// to cache resources.
BASE_FEATURE(kDesktopPWAsCacheDuringDefaultInstall,
             "DesktopPWAsCacheDuringDefaultInstall",
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

// Whether to parse and enforce the WebAppSettings policy.
BASE_FEATURE(kDesktopPWAsEnforceWebAppSettingsPolicy,
             "DesktopPWAsEnforceWebAppSettingsPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables Desktop PWAs to be auto-started on OS login.
BASE_FEATURE(kDesktopPWAsRunOnOsLogin,
             "DesktopPWAsRunOnOsLogin",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, allow-listed PWAs cannot be closed manually by the user.
BASE_FEATURE(kDesktopPWAsPreventClose,
             "DesktopPWAsPreventClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Runs diagnostics during start up to measure how broken web app icons are to
// feed into metrics.
BASE_FEATURE(kDesktopPWAsIconHealthChecks,
             "DesktopPWAsIconHealthChecks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds a user settings that allows PWAs to be opened with a tab strip.
BASE_FEATURE(kDesktopPWAsTabStripSettings,
             "DesktopPWAsTabStripSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds support for web bundles, making web apps able to be launched offline.
BASE_FEATURE(kDesktopPWAsWebBundles,
             "DesktopPWAsWebBundles",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
// Controls whether Chrome Apps are supported. See https://crbug.com/1221251.
// If the feature is disabled, Chrome Apps continue to work. If enabled, Chrome
// Apps will not launch and will be marked in the UI as deprecated.
BASE_FEATURE(kChromeAppsDeprecation,
             "ChromeAppsDeprecation",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Controls whether force installed and preinstalled apps should be exempt from
// deprecation.
BASE_FEATURE(kKeepForceInstalledPreinstalledApps,
             "KeepForceInstalledPreinstalledApps",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Controls if the 'launch anyways' button is shown.
const base::FeatureParam<bool> kChromeAppsDeprecationHideLaunchAnyways{
    &kChromeAppsDeprecation, "HideLaunchAnyways", true};

// Enables user link capturing on desktop platforms, i.e. Windows, Mac
// Linux amd Fuchsia.
BASE_FEATURE(kDesktopPWAsLinkCapturing,
             "DesktopPWAsLinkCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables notification permission revocation for origins that may send
// disruptive notifications.
BASE_FEATURE(kDisruptiveNotificationPermissionRevocation,
             "DisruptiveNotificationPermissionRevocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable DNS over HTTPS (DoH).
BASE_FEATURE(kDnsOverHttps,
             "DnsOverHttps",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Set whether fallback to insecure DNS is allowed by default. This setting may
// be overridden for individual transactions.
const base::FeatureParam<bool> kDnsOverHttpsFallbackParam{&kDnsOverHttps,
                                                          "Fallback", true};

// Sets whether the DoH setting is displayed in the settings UI.
const base::FeatureParam<bool> kDnsOverHttpsShowUiParam {
  &kDnsOverHttps, "ShowUi",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
      true
#else
      false
#endif
};

// Supply one or more space-separated DoH server URI templates to use when this
// feature is enabled. If no templates are specified, then a hardcoded mapping
// will be used to construct a list of DoH templates associated with the IP
// addresses of insecure resolvers in the discovered configuration.
const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam{
    &kDnsOverHttps, "Templates", ""};

#if BUILDFLAG(IS_ANDROID)
// Enable loading native libraries earlier in startup on Android.
BASE_FEATURE(kEarlyLibraryLoad,
             "EarlyLibraryLoad",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enable the restricted web APIs for high-trusted apps.
BASE_FEATURE(kEnableRestrictedWebApis,
             "EnableRestrictedWebApis",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// Enable WebHID on extension service workers.
BASE_FEATURE(kEnableWebHidOnExtensionServiceWorker,
             "EnableWebHidOnExtensionServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enable WebUSB on extension service workers.
BASE_FEATURE(kEnableWebUsbOnExtensionServiceWorker,
             "EnableWebUsbOnExtensionServiceWorker",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable extended descriptions for key settings in Chrome settings.
BASE_FEATURE(kExtendedSettingsDescriptions,
             "ExtendedSettingsDescriptions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// Lazy initialize IndividualSettings for extensions from enterprise policy
// that are not installed.
BASE_FEATURE(kExtensionDeferredIndividualSettings,
             "ExtensionDeferredIndividualSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// If enabled, this feature's |kExternalInstallDefaultButtonKey| field trial
// parameter value controls which |ExternalInstallBubbleAlert| button is the
// default.
BASE_FEATURE(kExternalExtensionDefaultButtonControl,
             "ExternalExtensionDefaultButtonControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kFileTransferEnterpriseConnector,
             "FileTransferEnterpriseConnector",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFileTransferEnterpriseConnectorUI,
             "FileTransferEnterpriseConnectorUI",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Show Flash deprecation warning to users who have manually enabled Flash.
// https://crbug.com/918428
BASE_FEATURE(kFlashDeprecationWarning,
             "FlashDeprecationWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Controls whether the GeoLanguage system is enabled. GeoLanguage uses IP-based
// coarse geolocation to provide an estimate (for use by other Chrome features
// such as Translate) of the local/regional language(s) corresponding to the
// device's location. If this feature is disabled, the GeoLanguage provider is
// not initialized at startup, and clients calling it will receive an empty list
// of languages.
BASE_FEATURE(kGeoLanguage, "GeoLanguage", base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Happiness Tracking System demo mode for Desktop
// Chrome.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopDemo,
             "HappinessTrackingSurveysForDesktopDemo",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables or disables the Privacy Guide v3 update of the Privacy Guide feature
// in Chrome Settings.
BASE_FEATURE(kPrivacyGuide3,
             "PrivacyGuide3",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Desktop Privacy Guide.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacyGuide,
             "HappinessTrackingSurveysForDesktopPrivacyGuide",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime{
        &kHappinessTrackingSurveysForDesktopPrivacyGuide, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Privacy
// Sandbox.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacySandbox,
             "HappinessTrackingSurveysForDesktopPrivacySandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacySandboxTime{
        &kHappinessTrackingSurveysForDesktopPrivacySandbox, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettings,
             "HappinessTrackingSurveysForDesktopSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Privacy Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy,
             "HappinessTrackingSurveysForDesktopSettingsPrivacy",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "no-sandbox",
        false};
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

BASE_FEATURE(kHappinessTrackingSurveysForNtpPhotosOptOut,
             "HappinessTrackingSurveysForrNtpPhotosOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Chrome What's New.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew,
             "HappinessTrackingSurveysForDesktopWhatsNew",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime{
        &kHappinessTrackingSurveysForDesktopWhatsNew, "whats-new-time",
        base::Seconds(20)};

// Happiness tracking surveys for the M1 Privacy Sandbox settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopM1AdPrivacyPage,
             "HappinessTrackingSurveysForDesktopM1AdPrivacyPage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1AdPrivacyPageTime{
        &kHappinessTrackingSurveysForDesktopM1AdPrivacyPage, "settings-time",
        base::Seconds(20)};
BASE_FEATURE(kHappinessTrackingSurveysForDesktopM1TopicsSubpage,
             "HappinessTrackingSurveysForDesktopM1TopicsSubpage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1TopicsSubpageTime{
        &kHappinessTrackingSurveysForDesktopM1TopicsSubpage, "settings-time",
        base::Seconds(20)};
BASE_FEATURE(kHappinessTrackingSurveysForDesktopM1FledgeSubpage,
             "HappinessTrackingSurveysForDesktopM1FledgeSubpage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1FledgeSubpageTime{
        &kHappinessTrackingSurveysForDesktopM1FledgeSubpage, "settings-time",
        base::Seconds(20)};
BASE_FEATURE(kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpage,
             "HappinessTrackingSurveysForDesktopM1AdMeasurementSubpage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpageTime{
        &kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpage,
        "settings-time", base::Seconds(20)};

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
// Enables or disables the Happiness Tracking System for Unlock.
BASE_FEATURE(kHappinessTrackingSystemUnlock,
             "HappinessTrackingUnlock",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Smart Lock.
BASE_FEATURE(kHappinessTrackingSystemSmartLock,
             "HappinessTrackingSmartLock",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for ARC Games survey.
BASE_FEATURE(kHappinessTrackingSystemArcGames,
             "HappinessTrackingArcGames",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Happiness Tracking System for Audio survey.
BASE_FEATURE(kHappinessTrackingSystemAudio,
             "HappinessTrackingAudio",
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
// Enables the Happiness Tracking System for Privacy Hub post launch survey.
BASE_FEATURE(kHappinessTrackingPrivacyHubPostLaunch,
             "HappinessTrackingPrivacyHubPostLaunch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for OS Settings Search survey.
BASE_FEATURE(kHappinessTrackingOsSettingsSearch,
             "HappinessTrackingOsSettingsSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables the Happiness Tracking System for Borealis games survey.
BASE_FEATURE(kHappinessTrackingBorealisGames,
             "HappinessTrackingBorealisGames",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Hides the origin text from showing up briefly in WebApp windows.
BASE_FEATURE(kHideWebAppOriginText,
             "HideWebAppOriginText",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for crbug.com/1414633.
BASE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers,
             "HttpsOnlyModeForAdvancedProtectionUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for engaged sites. No-op if HttpsFirstModeV2 or
// HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForEngagedSites,
             "HttpsFirstModeV2ForEngagedSites",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for typically secure users. No-op if
// HttpsFirstModeV2 or HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForTypicallySecureUsers,
             "HttpsFirstModeV2ForTypicallySecureUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automatically upgrading main frame navigations to HTTPS.
BASE_FEATURE(kHttpsUpgrades,
             "HttpsUpgrades",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enables immersive fullscreen. The tab strip and toolbar are placed underneath
// the titlebar. The tab strip and toolbar can auto hide and reveal.
BASE_FEATURE(kImmersiveFullscreen,
             "ImmersiveFullscreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Moves the tab strip into the titlebar. kImmersiveFullscreen must be enabled
// for this feature to have an effect.
BASE_FEATURE(kImmersiveFullscreenTabs,
             "ImmersiveFullscreenTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables immersive fullscreen mode for PWA windows. PWA windows will use
// immersive fullscreen mode if and only if both this and kImmersiveFullscreen
// are enabled. PWA windows currently do not use ImmersiveFullscreenTabs even if
// the feature is enabled.
BASE_FEATURE(kImmersiveFullscreenPWAs,
             "ImmersiveFullscreenPWAs",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables scraping of password-expiry information during SAML login flow, which
// can lead to an in-session flow for changing SAML password if it has expired.
// This is safe to enable by default since it does not cause the password-expiry
// information to be stored, or any user-visible change - in order for anything
// to happen, the domain administrator has to intentionally send this extra
// info in the SAML response, and enable the InSessionPasswordChange policy.
// So, this feature is just for disabling the scraping code if it causes
// any unforeseen issues.
BASE_FEATURE(kInSessionPasswordChange,
             "InSessionPasswordChange",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// A feature that controls whether Chrome warns about incompatible applications.
// This feature requires Windows 10 or higher to work because it depends on
// the "Apps & Features" system settings.
BASE_FEATURE(kIncompatibleApplicationsWarning,
             "IncompatibleApplicationsWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, users will see a warning when downloading from Incognito.
BASE_FEATURE(kIncognitoDownloadsWarning,
             "IncognitoDownloadsWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, users will see updated UI in Incognito NTP
BASE_FEATURE(kIncognitoNtpRevamp,
             "IncognitoNtpRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables automatic updates of Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppAutomaticUpdates,
             "IsolatedWebAppAutomaticUpdates",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables Isolated Web App Developer Mode, which allows developers to
// install untrusted Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppDevMode,
             "IsolatedWebAppDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kKioskEnableAppService,
             "KioskEnableAppService",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// When enabled, allows other features to use the k-Anonymity Service.
BASE_FEATURE(kKAnonymityService,
             "KAnonymityService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Origin to use for requests to the k-Anonymity Auth server to get trust
// tokens.
constexpr base::FeatureParam<std::string> kKAnonymityServiceAuthServer{
    &kKAnonymityService, "KAnonymityServiceAuthServer",
    "https://chromekanonymityauth-pa.googleapis.com/"};

// Origin to use as a relay for OHTTP requests to the k-Anonymity Join server.
constexpr base::FeatureParam<std::string> kKAnonymityServiceJoinRelayServer{
    &kKAnonymityService, "KAnonymityServiceJoinRelayServer", ""};

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
    &kKAnonymityService, "KAnonymityServiceQueryRelayServer", ""};

// Origin to use to request k-anonymity status from the k-Anonymity Query
// server.
constexpr base::FeatureParam<std::string> kKAnonymityServiceQueryServer{
    &kKAnonymityService, "KAnonymityServiceQueryServer",
    "https://chromekanonymityquery-pa.googleapis.com/"};

// Minimum amount of time allowed between requesting k-anonymity status from the
// Query server for a distinct group.
constexpr base::FeatureParam<base::TimeDelta> kKAnonymityServiceQueryInterval{
    &kKAnonymityService, "KAnonymityServiceJoinInterval", base::Days(1)};

// When enabled, the k-Anonymity Service will send requests to the Join and
// Query k-anonymity servers.
BASE_FEATURE(kKAnonymityServiceOHTTPRequests,
             "KAnonymityServiceOHTTPRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_MAC)
// Enable screen capture system permission check on Mac 10.15+.
BASE_FEATURE(kMacSystemScreenCapturePermissionCheck,
             "MacSystemScreenCapturePermissionCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Whether to show the Metered toggle in Settings, allowing users to toggle
// whether to treat a WiFi or Cellular network as 'metered'.
BASE_FEATURE(kMeteredShowToggle,
             "MeteredShowToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to show the Hidden toggle in Settings, allowing users to toggle
// whether to treat a WiFi network as having a hidden ssid.
BASE_FEATURE(kShowHiddenNetworkToggle,
             "ShowHiddenNetworkToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the new design of metrics settings.
BASE_FEATURE(kMetricsSettingsAndroid,
             "MetricsSettingsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kMoveWebApp,
             "MoveWebApp",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kMoveWebAppUninstallStartUrlPrefix(
    &kMoveWebApp,
    "uninstallStartUrlPrefix",
    "");
const base::FeatureParam<std::string> kMoveWebAppUninstallStartUrlPattern(
    &kMoveWebApp,
    "uninstallStartUrlPattern",
    "");
const base::FeatureParam<std::string>
    kMoveWebAppInstallStartUrl(&kMoveWebApp, "installStartUrl", "");

// Enables the use of system notification centers instead of using the Message
// Center for displaying the toasts. The feature is hardcoded to enabled for
// Chrome OS.
#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kNativeNotifications,
             "NativeNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSystemNotifications,
             "SystemNotifications",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)

#if BUILDFLAG(IS_MAC)
// Enables the usage of Apple's new Notification API.
BASE_FEATURE(kNewMacNotificationAPI,
             "NewMacNotificationAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

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

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOnConnectNative,
             "OnConnectNative",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables/disables marketing emails for other countries other than US,CA,UK.
BASE_FEATURE(kOobeMarketingAdditionalCountriesSupported,
             "kOobeMarketingAdditionalCountriesSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables/disables marketing emails for double opt-in countries.
BASE_FEATURE(kOobeMarketingDoubleOptInCountriesSupported,
             "kOobeMarketingDoubleOptInCountriesSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables or disabled the OOM intervention.
BASE_FEATURE(kOomIntervention,
             "OomIntervention",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables usage of Parent Access Code in the login flow for reauth and add
// user. Requires |kParentAccessCode| to be enabled.
BASE_FEATURE(kParentAccessCodeForOnlineLogin,
             "ParentAccessCodeForOnlineLogin",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Keep a client-side log of when websites access permission-gated capabilities
// to allow the user to audit usage.
BASE_FEATURE(kPermissionAuditing,
             "PermissionAuditing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using the prediction service for permission prompts. We will keep
// this feature in order to allow us to update the holdback chance via finch.
BASE_FEATURE(kPermissionPredictions,
             "PermissionPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The holdback chance is 30% but it can also be configured/updated
// through finch if needed.
const base::FeatureParam<double> kPermissionPredictionsHoldbackChance(
    &kPermissionPredictions,
    "holdback_chance",
    0.3);

// Enables using the prediction service for geolocation permission prompts.
BASE_FEATURE(kPermissionGeolocationPredictions,
             "PermissionGeolocationPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double>
    kPermissionGeolocationPredictionsHoldbackChance(
        &kPermissionGeolocationPredictions,
        "holdback_chance",
        0.3);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for "Plugin VMs" on Chrome OS.
BASE_FEATURE(kPluginVm, "PluginVm", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allows Chrome to do preconnect when prerender fails.
BASE_FEATURE(kPrerenderFallbackToPreconnect,
             "PrerenderFallbackToPreconnect",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enable improved printer state and error state messaging for Print Preview.
BASE_FEATURE(kPrintPreviewSetupAssistance,
             "PrintPreviewSetupAssistance",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPrivacyGuideAndroid,
             "PrivacyGuideAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrivacyGuideAndroidPostMVP,
             "PrivacyGuideAndroidPostMVP",
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables permanent removal of Legacy Supervised Users on startup.
BASE_FEATURE(kRemoveSupervisedUsersOnStartup,
             "RemoveSupervisedUsersOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Enables extensions module in Safety Check.
BASE_FEATURE(kSafetyCheckExtensions,
             "SafetyCheckExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification permission module in Safety Check.
BASE_FEATURE(kSafetyCheckNotificationPermissions,
             "SafetyCheckNotificationPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit{
        &kSafetyCheckNotificationPermissions,
        "min-engagement-notification-count", 0};
const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit{
        &kSafetyCheckNotificationPermissions,
        "low-engagement-notification-count", 4};

// Enables Safety Hub feature.
BASE_FEATURE(kSafetyHub, "SafetyHub", base::FEATURE_DISABLED_BY_DEFAULT);

// Time between automated runs of the password check.
const base::FeatureParam<base::TimeDelta> kBackgroundPasswordCheckInterval{
    &kSafetyHub, "background-password-check-interval", base::Days(10)};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for multiple scheduler configurations.
BASE_FEATURE(kSchedulerConfiguration,
             "SchedulerConfiguration",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables SmartDim on Chrome OS.
BASE_FEATURE(kSmartDim, "SmartDim", base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables or disables the ability to use the sound content setting to mute a
// website.
BASE_FEATURE(kSoundContentSetting,
             "SoundContentSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables chrome://sys-internals.
BASE_FEATURE(kSysInternals, "SysInternals", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables TPM firmware update capability on Chrome OS.
BASE_FEATURE(kTPMFirmwareUpdate,
             "TPMFirmwareUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Enables the demo version of the Support Tool. The tool will be available in
// chrome://support-tool. See go/support-tool-v1-design for more details.
BASE_FEATURE(kSupportTool, "SupportTool", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Support Tool to include a screenshot in the exported support tool
// packet.
BASE_FEATURE(kSupportToolScreenshot,
             "SupportToolScreenshot",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables copy token button on chrome://support-tool.url-generator page. The
// token can be used in Admin Console to select the requested data collector
// types.
BASE_FEATURE(kSupportToolCopyTokenButton,
             "SupportToolCopyTokenButton",
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

// Show warnings on downloads not delivered over HTTPS.
BASE_FEATURE(kInsecureDownloadWarnings,
             "InsecureDownloadWarnings",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-consent-accept-probability", 0.1};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-consent-decline-probability", 0.5};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-dismiss-probability", 0.5};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkProbability{
        &kTrustSafetySentimentSurvey, "privacy-sandbox-3-notice-ok-probability",
        0.05};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-settings-probability", 0.8};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-learn-more-probability", 0.2};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-consent-accept-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-consent-decline-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkProbability{
        &kTrustSafetySentimentSurvey, "privacy-sandbox-4-notice-ok-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsProbability{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-notice-settings-probability", 0.0};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId{
        &kTrustSafetySentimentSurvey, "privacy-settings-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurvey, "trusted-surface-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId{
        &kTrustSafetySentimentSurvey, "transactions-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-consent-accept-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-consent-decline-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-dismiss-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkTriggerId{
        &kTrustSafetySentimentSurvey, "privacy-sandbox-3-notice-ok-trigger-id",
        ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-settings-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-3-notice-learn-more-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-consent-accept-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-consent-decline-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkTriggerId{
        &kTrustSafetySentimentSurvey, "privacy-sandbox-4-notice-ok-trigger-id",
        ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsTriggerId{
        &kTrustSafetySentimentSurvey,
        "privacy-sandbox-4-notice-settings-trigger-id", ""};
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
// TODO(crbug.com/1382134): Calculate initial probabilities and remove 0.0
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2BrowsingDataProbability{
        &kTrustSafetySentimentSurveyV2, "browsing-data-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2ControlGroupProbability{
        &kTrustSafetySentimentSurveyV2, "control-group-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordCheckProbability{
        &kTrustSafetySentimentSurveyV2, "password-check-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability{
        &kTrustSafetySentimentSurveyV2, "safety-check-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2TrustedSurfaceProbability{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacyGuideProbability{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptProbability{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-consent-accept-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineProbability{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-consent-decline-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkProbability{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-notice-ok-probability", 0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsProbability{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-notice-settings-probability", 0.0};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId{
        &kTrustSafetySentimentSurveyV2, "browsing-data-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId{
        &kTrustSafetySentimentSurveyV2, "control-group-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "password-check-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-check-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptTriggerId{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-consent-accept-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineTriggerId{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-consent-decline-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkTriggerId{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-notice-ok-trigger-id", ""};
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsTriggerId{
        &kTrustSafetySentimentSurveyV2,
        "privacy-sandbox-4-notice-settings-trigger-id", ""};
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
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables user activity event logging for power management on
// Chrome OS.
BASE_FEATURE(kUserActivityEventLogging,
             "UserActivityEventLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAppDedupeInstallUrls,
             "WebAppDedupeInstallUrls",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppManifestIconUpdating,
             "WebAppManifestIconUpdating",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppManifestImmediateUpdating,
             "WebAppManifestImmediateUpdating",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppSyncGeneratedIconBackgroundFix,
             "WebAppSyncGeneratedIconBackgroundFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppSyncGeneratedIconRetroactiveFix,
             "WebAppSyncGeneratedIconRetroactiveFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppSyncGeneratedIconUpdateFix,
             "WebAppSyncGeneratedIconUpdateFix",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate,
             "WebAppManifestPolicyAppIdentityUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// When this feature flag is enabled together with the LacrosAvailability
// policy, the Chrome app Kiosk session uses Lacros-chrome as the web browser to
// launch Chrome apps. When disabled, the Ash-chrome will be used instead.
BASE_FEATURE(kChromeKioskEnableLacros,
             "ChromeKioskEnableLacros",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When this feature flag is enabled together with the LacrosAvailability
// policy, the web (PWA) Kiosk session uses Lacros-chrome as the web browser to
// launch web (PWA) applications. When disabled, the Ash-chrome will be used
// instead.
BASE_FEATURE(kWebKioskEnableLacros,
             "WebKioskEnableLacros",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Allow capturing of WebRTC event logs, and uploading of those logs to Crash.
// Please note that a Chrome policy must also be set, for this to have effect.
// Effectively, this is a kill-switch for the feature.
// TODO(crbug.com/775415): Remove this kill-switch.
BASE_FEATURE(kWebRtcRemoteEventLog,
             "WebRtcRemoteEventLog",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Compress remote-bound WebRTC event logs (if used; see kWebRtcRemoteEventLog).
BASE_FEATURE(kWebRtcRemoteEventLogGzipped,
             "WebRtcRemoteEventLogGzipped",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
// Enables Web Share (navigator.share)
BASE_FEATURE(kWebShare, "WebShare", base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// Enables Web Share (navigator.share) for macOS
BASE_FEATURE(kWebShare, "WebShare", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Populates storage dimensions in UMA log if enabled. Requires diagnostics
// package in the image.
BASE_FEATURE(kUmaStorageDimensions,
             "UmaStorageDimensions",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Allow a Wilco DTC (diagnostics and telemetry controller) on Chrome OS.
// More info about the project may be found here:
// https://docs.google.com/document/d/18Ijj8YlC8Q3EWRzLspIi2dGxg4vIBVe5sJgMPt9SWYo
BASE_FEATURE(kWilcoDtc, "WilcoDtc", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Populates the user type on device type metrics in UMA log if enabled.
BASE_FEATURE(kUserTypeByDeviceTypeMetricsProvider,
             "UserTypeByDeviceTypeMetricsProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// Enables the accelerated default browser flow for Windows 10.
BASE_FEATURE(kWin10AcceleratedDefaultBrowserFlow,
             "Win10AcceleratedDefaultBrowserFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Enables writing basic system profile to the persistent histograms files
// earlier.
BASE_FEATURE(kWriteBasicSystemProfileToPersistentHistogramsFile,
             "WriteBasicSystemProfileToPersistentHistogramsFile",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsParentAccessCodeForOnlineLoginEnabled() {
  return base::FeatureList::IsEnabled(kParentAccessCodeForOnlineLogin);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A feature to indicate whether setting wake time >24hours away is supported by
// the platform's RTC.
// TODO(b/187516317): Remove when the issue is resolved in FW.
BASE_FEATURE(kSupportsRtcWakeOver24Hours,
             "SupportsRtcWakeOver24Hours",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
