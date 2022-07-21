// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "ppapi/buildflags/buildflags.h"

namespace features {

// All features in alphabetical order.

#if BUILDFLAG(IS_CHROMEOS_ASH)
// If enabled device status collector will add the type of session (Affiliated
// User, Kiosks, Managed Guest Sessions) to the device status report.
const base::Feature kActivityReportingSessionType{
    "ActivityReportingSessionType", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables logging for adaptive screen brightness on Chrome OS.
const base::Feature kAdaptiveScreenBrightnessLogging{
    "AdaptiveScreenBrightnessLogging", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Shows settings to adjust and disable touchpad haptic feedback.
const base::Feature kAllowDisableTouchpadHapticFeedback{
    "AllowDisableTouchpadHapticFeedback", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Shows settings to adjust the touchpad haptic click settings.
const base::Feature kAllowTouchpadHapticClickSettings{
    "AllowTouchpadHapticClickSettings", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
const base::Feature kAnonymousUpdateChecks{"AnonymousUpdateChecks",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAppDiscoveryForOobe{"AppDiscoveryForOobe",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAppManagementAppDetails{"AppManagementAppDetails",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAppDeduplicationService{"AppDeduplicationService",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAppProvisioningStatic{"AppProvisioningStatic",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
// Can be used to disable RemoteCocoa (hosting NSWindows for apps in the app
// process). For debugging purposes only.
const base::Feature kAppShimRemoteCocoa{"AppShimRemoteCocoa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// This is used to control the new app close behavior on macOS wherein closing
// all windows for an app leaves the app running.
// https://crbug.com/1080729
const base::Feature kAppShimNewCloseBehavior{"AppShimNewCloseBehavior",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_MAC)

// Enables the built-in DNS resolver.
const base::Feature kAsyncDns {
  "AsyncDns",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
// Enables or disables the Autofill survey triggered by opening a prompt to
// save address info.
const base::Feature kAutofillAddressSurvey{"AutofillAddressSurvey",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Autofill survey triggered by opening a prompt to
// save credit card info.
const base::Feature kAutofillCardSurvey{"AutofillCardSurvey",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Autofill survey triggered by opening a prompt to
// save password info.
const base::Feature kAutofillPasswordSurvey{"AutofillPasswordSurvey",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
const base::Feature kBackgroundModeAllowRestart{
    "BackgroundModeAllowRestart", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kBlockMigratedDefaultChromeAppSync{
    "BlockMigratedDefaultChromeAppSync", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable Borealis on Chrome OS.
const base::Feature kBorealis{"Borealis", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables change picture video mode.
const base::Feature kChangePictureVideoMode{"ChangePictureVideoMode",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClientStorageAccessContextAuditing{
    "ClientStorageAccessContextAuditing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kConsolidatedSiteStorageControls{
    "ConsolidatedSiteStorageControls", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables "usm" service in the list of user services returned by
// userInfo Gaia message.
const base::Feature kCrOSEnableUSMUserService{"CrOSEnableUSMUserService",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables flash component updates on Chrome OS.
const base::Feature kCrosCompUpdates{"CrosCompUpdates",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable project Crostini, Linux VMs on Chrome OS.
const base::Feature kCrostini{"Crostini", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable additional Crostini session status reporting for
// managed devices only, i.e. reports of installed apps and kernel version.
const base::Feature kCrostiniAdditionalEnterpriseReporting{
    "CrostiniAdditionalEnterpriseReporting", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable advanced access controls for Crostini-related features
// (e.g. restricting VM CLI tools access, restricting Crostini root access).
const base::Feature kCrostiniAdvancedAccessControls{
    "CrostiniAdvancedAccessControls", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables infrastructure for applying Ansible playbook to default Crostini
// container.
const base::Feature kCrostiniAnsibleInfrastructure{
    "CrostiniAnsibleInfrastructure", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables infrastructure for generating Ansible playbooks for the default
// Crostini container from software configurations in JSON schema.
const base::Feature kCrostiniAnsibleSoftwareManagement{
    "CrostiniAnsibleSoftwareManagement", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for sideloading android apps into Arc via crostini.
const base::Feature kCrostiniArcSideload{"CrostiniArcSideload",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables custom UI for forcibly closing unresponsive windows.
const base::Feature kCrostiniForceClose{"CrostiniForceClose",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables distributed model for TPM1.2, i.e., using tpm_managerd and
// attestationd.
const base::Feature kCryptohomeDistributedModel{
    "CryptohomeDistributedModel", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables cryptohome UserDataAuth interface, a new dbus interface that is
// fully protobuf and uses libbrillo for dbus instead of the deprecated
// glib-dbus.
const base::Feature kCryptohomeUserDataAuth{"CryptohomeUserDataAuth",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for cryptohome UserDataAuth interface. UserDataAuth is a new
// dbus interface that is fully protobuf and uses libbrillo for dbus instead
// instead of the deprecated glib-dbus.
const base::Feature kCryptohomeUserDataAuthKillswitch{
    "CryptohomeUserDataAuthKillswitch", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enables Privacy Hub for ChromeOS.
const base::Feature kCrosPrivacyHub{"CrosPrivacyHub",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables parsing and enforcing Data Leak Prevention policy rules that
// restricts usage of some system features, e.g.clipboard, screenshot, etc.
// for confidential content.
const base::Feature kDataLeakPreventionPolicy{"DataLeakPreventionPolicy",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables starting of Data Leak Prevention Files Daemon by sending the
// DLP policy there. The daemond might restrict access to some protected files.
const base::Feature kDataLeakPreventionFilesRestriction{
    "DataLeakPreventionFilesRestriction", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// When enabled, newly installed ARC apps will not capture links clicked in the
// browser by default. Users can still enable link capturing for apps through
// the intent picker or settings.
const base::Feature kDefaultLinkCapturingInBrowser{
    "DefaultLinkCapturingInBrowser", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables passing additional user authentication in requests to DMServer
// (policy fetch, status report upload).
const base::Feature kDMServerOAuthForChildUser{
    "DMServerOAuthForChildUser", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !BUILDFLAG(IS_ANDROID)
// Whether to allow installed-by-default web apps to be installed or not.
const base::Feature kPreinstalledWebAppInstallation{
    "DefaultWebAppInstallation", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether to run the PreinstalledWebAppDuplicationFixer code during start up.
const base::Feature kPreinstalledWebAppDuplicationFixer{
    "PreinstalledWebAppDuplicationFixer", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// API that allows PWAs manually minimizing, maximizing and restoring windows.
const base::Feature kDesktopPWAsAdditionalWindowingControls{
    "DesktopPWAsAdditionalWindowingControls",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When installing default installed PWAs, we wait for service workers
// to cache resources.
const base::Feature kDesktopPWAsCacheDuringDefaultInstall{
    "DesktopPWAsCacheDuringDefaultInstall", base::FEATURE_ENABLED_BY_DEFAULT};

// Generates customised default offline page that is shown when web app is
// offline if no custom page is provided by developer.
#if BUILDFLAG(IS_ANDROID)
const base::Feature kAndroidPWAsDefaultOfflinePage{
    "AndroidPWAsDefaultOfflinePage", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kDesktopPWAsDefaultOfflinePage{
    "DesktopPWAsDefaultOfflinePage", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

// Moves the Extensions "puzzle piece" icon from the title bar into the app menu
// for web app windows.
const base::Feature kDesktopPWAsElidedExtensionsMenu {
  "DesktopPWAsElidedExtensionsMenu",
#if BUILDFLAG(IS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Whether to parse and enforce the WebAppSettings policy.
const base::Feature kDesktopPWAsEnforceWebAppSettingsPolicy{
    "DesktopPWAsEnforceWebAppSettingsPolicy", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables showing a detailed install dialog for user installs.
const base::Feature kDesktopPWAsDetailedInstallDialog{
    "DesktopPWAsDetailedInstallDialog", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Desktop PWAs to be auto-started on OS login.
const base::Feature kDesktopPWAsRunOnOsLogin {
  "DesktopPWAsRunOnOsLogin",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Adds a user settings that allows PWAs to be opened with a tab strip.
const base::Feature kDesktopPWAsTabStripSettings{
    "DesktopPWAsTabStripSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Adds support for web bundles, making web apps able to be launched offline.
const base::Feature kDesktopPWAsWebBundles{"DesktopPWAsWebBundles",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
// Controls whether Chrome Apps are supported. See https://crbug.com/1221251.
// If the feature is disabled, Chrome Apps continue to work. If enabled, Chrome
// Apps will not launch and will be marked in the UI as deprecated.
const base::Feature kChromeAppsDeprecation{"ChromeAppsDeprecation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
//  Controls whether force installed and preinstalled apps should be exempt from
//  deprecation.
const base::Feature kKeepForceInstalledPreinstalledApps{
    "KeepForceInstalledPreinstalledApps", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable DNS over HTTPS (DoH).
const base::Feature kDnsOverHttps {
  "DnsOverHttps",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables the DNS-Over-HTTPS in the DNS proxy.
const base::Feature kDnsProxyEnableDOH{"DnsProxyEnableDOH",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// Enable loading native libraries earlier in startup on Android.
const base::Feature kEarlyLibraryLoad{"EarlyLibraryLoad",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// Under this flag Java bootstrap (aka startup) tasks that are run before native
// initialization will not be specially prioritized by being posted at the front
// of the Looper's queue.
const base::Feature kElidePrioritizationOfPreNativeBootstrapTasks = {
    "ElidePrioritizationOfPreNativeBootstrapTasks",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enable the restricted web APIs for high-trusted apps.
const base::Feature kEnableRestrictedWebApis{"EnableRestrictedWebApis",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable web app uninstallation from Windows settings or control panel.
const base::Feature kEnableWebAppUninstallFromOsSettings{
    "EnableWebAppUninstallFromOsSettings", base::FEATURE_ENABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
// Enable WebHID on extension service workers.
const base::Feature kEnableWebHidOnExtensionServiceWorker{
    "EnableWebHidOnExtensionServiceWorker", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !BUILDFLAG(IS_ANDROID)
// Lazy initialize IndividualSettings for extensions from enterprise policy
// that are not installed.
const base::Feature kExtensionDeferredIndividualSettings{
    "ExtensionDeferredIndividualSettings", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Controls whether the user justification text field is visible on the
// extension request dialog.
const base::Feature kExtensionWorkflowJustification{
    "ExtensionWorkflowJustification", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, this feature's |kExternalInstallDefaultButtonKey| field trial
// parameter value controls which |ExternalInstallBubbleAlert| button is the
// default.
const base::Feature kExternalExtensionDefaultButtonControl{
    "ExternalExtensionDefaultButtonControl", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_PLUGINS)
// Show Flash deprecation warning to users who have manually enabled Flash.
// https://crbug.com/918428
const base::Feature kFlashDeprecationWarning{"FlashDeprecationWarning",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Controls whether the GeoLanguage system is enabled. GeoLanguage uses IP-based
// coarse geolocation to provide an estimate (for use by other Chrome features
// such as Translate) of the local/regional language(s) corresponding to the
// device's location. If this feature is disabled, the GeoLanguage provider is
// not initialized at startup, and clients calling it will receive an empty list
// of languages.
const base::Feature kGeoLanguage{"GeoLanguage",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Happiness Tracking System demo mode for Desktop
// Chrome.
const base::Feature kHappinessTrackingSurveysForDesktopDemo{
    "HappinessTrackingSurveysForDesktopDemo",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for COEP issues in Chrome
// DevTools on Desktop.
const base::Feature kHaTSDesktopDevToolsIssuesCOEP{
    "HaTSDesktopDevToolsIssuesCOEP", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Mixed Content issues in
// Chrome DevTools on Desktop.
const base::Feature kHaTSDesktopDevToolsIssuesMixedContent{
    "HaTSDesktopDevToolsIssuesMixedContent", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for same-site cookies
// issues in Chrome DevTools on Desktop.
const base::Feature
    kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite{
        "HappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite",
        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Heavy Ad issues in
// Chrome DevTools on Desktop.
const base::Feature kHaTSDesktopDevToolsIssuesHeavyAd{
    "HaTSDesktopDevToolsIssuesHeavyAd", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for CSP issues in Chrome
// DevTools on Desktop.
const base::Feature kHaTSDesktopDevToolsIssuesCSP{
    "HaTSDesktopDevToolsIssuesCSP", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Desktop Privacy Guide.
const base::Feature kHappinessTrackingSurveysForDesktopPrivacyGuide{
    "HappinessTrackingSurveysForDesktopPrivacyGuide",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime{
        &kHappinessTrackingSurveysForDesktopPrivacyGuide, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Privacy
// Sandbox.
const base::Feature kHappinessTrackingSurveysForDesktopPrivacySandbox{
    "HappinessTrackingSurveysForDesktopPrivacySandbox",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacySandboxTime{
        &kHappinessTrackingSurveysForDesktopPrivacySandbox, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Settings.
const base::Feature kHappinessTrackingSurveysForDesktopSettings{
    "HappinessTrackingSurveysForDesktopSettings",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Privacy Settings.
const base::Feature kHappinessTrackingSurveysForDesktopSettingsPrivacy{
    "HappinessTrackingSurveysForDesktopSettingsPrivacy",
    base::FEATURE_DISABLED_BY_DEFAULT};
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
const base::Feature kHappinessTrackingSurveysForDesktopNtpModules{
    "HappinessTrackingSurveysForDesktopNtpModules",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHappinessTrackingSurveysForNtpPhotosOptOut{
    "HappinessTrackingSurveysForrNtpPhotosOptOut",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Chrome What's New.
const base::Feature kHappinessTrackingSurveysForDesktopWhatsNew{
    "HappinessTrackingSurveysForDesktopWhatsNew",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime{
        &kHappinessTrackingSurveysForDesktopWhatsNew, "whats-new-time",
        base::Seconds(20)};

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables the Happiness Tracking System for the General survey.
const base::Feature kHappinessTrackingSystem{"HappinessTrackingSystem",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for the Ent survey.
const base::Feature kHappinessTrackingSystemEnt{
    "HappinessTrackingSystemEnt", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for the Stability survey.
const base::Feature kHappinessTrackingSystemStability{
    "HappinessTrackingSystemStability", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for the Performance survey.
const base::Feature kHappinessTrackingSystemPerformance{
    "HappinessTrackingSystemPerformance", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for Onboarding Experience.
const base::Feature kHappinessTrackingSystemOnboarding{
    "HappinessTrackingOnboardingExperience", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for Unlock.
const base::Feature kHappinessTrackingSystemUnlock{
    "HappinessTrackingUnlock", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for Smart Lock.
const base::Feature kHappinessTrackingSystemSmartLock{
    "HappinessTrackingSmartLock", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for ARC Games survey.
const base::Feature kHappinessTrackingSystemArcGames{
    "HappinessTrackingArcGames", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for Audio survey.
const base::Feature kHappinessTrackingSystemAudio{
    "HappinessTrackingAudio", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables the Happiness Tracking System for Personalization Avatar survey.
const base::Feature kHappinessTrackingPersonalizationAvatar{
    "HappinessTrackingPersonalizationAvatar",
    base::FEATURE_DISABLED_BY_DEFAULT};
// Enables the Happiness Tracking System for Personalization Screensaver survey.
const base::Feature kHappinessTrackingPersonalizationScreensaver{
    "HappinessTrackingPersonalizationScreensaver",
    base::FEATURE_DISABLED_BY_DEFAULT};
// Enables the Happiness Tracking System for Personalization Wallpaper survey.
const base::Feature kHappinessTrackingPersonalizationWallpaper{
    "HappinessTrackingPersonalizationWallpaper",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Hides the origin text from showing up briefly in WebApp windows.
const base::Feature kHideWebAppOriginText{"HideWebAppOriginText",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Sets whether the HTTPS-Only Mode setting is displayed in the settings UI.
const base::Feature kHttpsOnlyMode{"HttpsOnlyMode",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_MAC)
const base::Feature kImmersiveFullscreen{"ImmersiveFullscreen",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
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
const base::Feature kInSessionPasswordChange{"InSessionPasswordChange",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// A feature that controls whether Chrome warns about incompatible applications.
// This feature requires Windows 10 or higher to work because it depends on
// the "Apps & Features" system settings.
const base::Feature kIncompatibleApplicationsWarning{
    "IncompatibleApplicationsWarning", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, keeps Incognito UI consistent regardless of any selected theme.
const base::Feature kIncognitoBrandConsistencyForAndroid{
    "IncognitoBrandConsistencyForAndroid", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, users will see a warning when downloading from Incognito.
const base::Feature kIncognitoDownloadsWarning{
    "IncognitoDownloadsWarning", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// When enabled, users will see updated UI in Incognito NTP
const base::Feature kIncognitoNtpRevamp{"IncognitoNtpRevamp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, removes any entry points to the history UI from Incognito mode.
const base::Feature kUpdateHistoryEntryPointsInIncognito{
    "UpdateHistoryEntryPointsInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
const base::Feature kLinuxLowMemoryMonitor{"LinuxLowMemoryMonitor",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
// Values taken from the low-memory-monitor documentation and also apply to the
// portal API:
// https://hadess.pages.freedesktop.org/low-memory-monitor/gdbus-org.freedesktop.LowMemoryMonitor.html
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel{
    &kLinuxLowMemoryMonitor, "moderate_level", 50};
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel{
    &kLinuxLowMemoryMonitor, "critical_level", 255};
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const base::Feature kListWebAppsSwitch{"ListWebAppsSwitch",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_MAC)
// Enable screen capture system permission check on Mac 10.15+.
const base::Feature kMacSystemScreenCapturePermissionCheck{
    "MacSystemScreenCapturePermissionCheck", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Whether to show the Metered toggle in Settings, allowing users to toggle
// whether to treat a WiFi or Cellular network as 'metered'.
const base::Feature kMeteredShowToggle{"MeteredShowToggle",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to show the Hidden toggle in Settings, allowing users to toggle
// whether to treat a WiFi network as having a hidden ssid.
const base::Feature kShowHiddenNetworkToggle{"ShowHiddenNetworkToggle",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the new design of metrics settings.
const base::Feature kMetricsSettingsAndroid{"MetricsSettingsAndroid",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kMoveWebApp{
    "MoveWebApp", base::FeatureState::FEATURE_DISABLED_BY_DEFAULT};
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
const base::Feature kNativeNotifications{"NativeNotifications",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSystemNotifications{"SystemNotifications",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)

#if BUILDFLAG(IS_MAC)
// Enables the usage of Apple's new Notification API on macOS 10.14+
const base::Feature kNewMacNotificationAPI{"NewMacNotificationAPI",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_MAC)

// When kNoReferrers is enabled, most HTTP requests will provide empty
// referrers instead of their ordinary behavior.
const base::Feature kNoReferrers{"NoReferrers",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_WIN)
// Changes behavior of requireInteraction for notifications. Instead of staying
// on-screen until dismissed, they are instead shown for a very long time.
const base::Feature kNotificationDurationLongForRequireInteraction{
    "NotificationDurationLongForRequireInteraction",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kOnConnectNative{"OnConnectNative",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables/disables marketing emails for other countries other than US,CA,UK.
const base::Feature kOobeMarketingAdditionalCountriesSupported{
    "kOobeMarketingAdditionalCountriesSupported",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables/disables marketing emails for double opt-in countries.
const base::Feature kOobeMarketingDoubleOptInCountriesSupported{
    "kOobeMarketingDoubleOptInCountriesSupported",
    base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
// Enables or disabled the OOM intervention.
const base::Feature kOomIntervention{"OomIntervention",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables usage of Parent Access Code in the login flow for reauth and add
// user. Requires |kParentAccessCode| to be enabled.
const base::Feature kParentAccessCodeForOnlineLogin{
    "ParentAccessCodeForOnlineLogin", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Keep a client-side log of when websites access permission-gated capabilities
// to allow the user to audit usage.
const base::Feature kPermissionAuditing{"PermissionAuditing",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using the prediction service for permission prompts. We will keep
// this feature in order to allow us to update the holdback chance via finch.
const base::Feature kPermissionPredictions{"PermissionPredictions",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// The holdback chance is 30% but it can also be configured/updated
// through finch if needed.
const base::FeatureParam<double> kPermissionPredictionsHoldbackChance(
    &kPermissionPredictions,
    "holdback_chance",
    0.3);

// Enables using the prediction service for geolocation permission prompts.
const base::Feature kPermissionGeolocationPredictions{
    "PermissionGeolocationPredictions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<double>
    kPermissionGeolocationPredictionsHoldbackChance(
        &kPermissionGeolocationPredictions,
        "holdback_chance",
        0.3);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for "Plugin VMs" on Chrome OS.
const base::Feature kPluginVm{"PluginVm", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Allows prediction operations (e.g., prefetching) on all connection types.
const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes{
    "PredictivePrefetchingAllowedOnAllConnectionTypes",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPrefixWebAppWindowsWithAppName{
    "PrefixWebAppWindowsWithAppName", base::FEATURE_ENABLED_BY_DEFAULT};

// Allows Chrome to do preconnect when prerender fails.
const base::Feature kPrerenderFallbackToPreconnect{
    "PrerenderFallbackToPreconnect", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPrivacyGuide2{"PrivacyGuide2",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPrivacyGuideAndroid{"PrivacyGuideAndroid",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

// Shows a confirmation dialog when updates to a PWAs icon has been detected.
const base::Feature kPwaUpdateDialogForIcon{"PwaUpdateDialogForIcon",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Shows a confirmation dialog when updates to a PWAs name has been detected.
const base::Feature kPwaUpdateDialogForName{"PwaUpdateDialogForName",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using quiet prompts for notification permission requests.
const base::Feature kQuietNotificationPrompts{"QuietNotificationPrompts",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables recording additional web app related debugging data to be displayed
// in: chrome://web-app-internals
const base::Feature kRecordWebAppDebugInfo{"RecordWebAppDebugInfo",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables notification permission revocation for abusive origins.
const base::Feature kAbusiveNotificationPermissionRevocation{
    "AbusiveOriginNotificationPermissionRevocation",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRemoveStatusBarInWebApps {
  "RemoveStatusBarInWebApps",
#if BUILDFLAG(IS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables permanent removal of Legacy Supervised Users on startup.
const base::Feature kRemoveSupervisedUsersOnStartup{
    "RemoveSupervisedUsersOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
const base::Feature kRequestDesktopSiteForTablets{
    "RequestDesktopSiteForTablets", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for multiple scheduler configurations.
const base::Feature kSchedulerConfiguration{"SchedulerConfiguration",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Controls whether SCT audit reports are queued and the rate at which they
// should be sampled. Default sampling rate is 1/10,000 certificates.
#if BUILDFLAG(IS_ANDROID)
const base::Feature kSCTAuditing{"SCTAuditing",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kSCTAuditing{"SCTAuditing",
                                 base::FEATURE_ENABLED_BY_DEFAULT};
#endif
constexpr base::FeatureParam<double> kSCTAuditingSamplingRate{
    &kSCTAuditing, "sampling_rate", 0.0001};

// SCT auditing hashdance allows Chrome clients who are not opted-in to Enhanced
// Safe Browsing Reporting to perform a k-anonymous query to see if Google knows
// about an SCT seen in the wild. If it hasn't been seen, then it is considered
// a security incident and uploaded to Google.
const base::Feature kSCTAuditingHashdance{"SCTAuditingHashdance",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

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

// Controls whether the user is prompted when sites request attestation.
const base::Feature kSecurityKeyAttestationPrompt{
    "SecurityKeyAttestationPrompt", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kSharesheetCopyToClipboard{
    "SharesheetCopyToClipboard", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Alternative to switches::kSitePerProcess, for turning on full site isolation.
// Launch bug: https://crbug.com/810843.  This is a //chrome-layer feature to
// avoid turning on site-per-process by default for *all* //content embedders
// (e.g. this approach lets ChromeCast avoid site-per-process mode).
//
// TODO(alexmos): Move this and the other site isolation features below to
// browser_features, as they are only used on the browser side.
const base::Feature kSitePerProcess {
  "SitePerProcess",
#if BUILDFLAG(IS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables SmartDim on Chrome OS.
const base::Feature kSmartDim{"SmartDim", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables or disables the ability to use the sound content setting to mute a
// website.
const base::Feature kSoundContentSetting{"SoundContentSetting",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables chrome://sys-internals.
const base::Feature kSysInternals{"SysInternals",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables TPM firmware update capability on Chrome OS.
const base::Feature kTPMFirmwareUpdate{"TPMFirmwareUpdate",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Enables logging UKMs for background tab activity by TabActivityWatcher.
const base::Feature kTabMetricsLogging{"TabMetricsLogging",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the demo version of the Support Tool. The tool will be available in
// chrome://support-tool. See go/support-tool-v1-design for more details.
const base::Feature kSupportTool{"SupportTool",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_WIN)
// Enables the blocking of third-party modules. This feature requires Windows 8
// or higher because it depends on the ProcessExtensionPointDisablePolicy
// mitigation, which was not available on Windows 7.
// Note: Due to a limitation in the implementation of this feature, it is
// required to start the browser two times to fully enable or disable it.
const base::Feature kThirdPartyModulesBlocking{
    "ThirdPartyModulesBlocking", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page. As of M89, mixed downloads are blocked on all platforms.
const base::Feature kTreatUnsafeDownloadsAsActive{
    "TreatUnsafeDownloadsAsActive", base::FEATURE_ENABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
// Enables surveying of users of Trust & Safety features with HaTS.
const base::Feature kTrustSafetySentimentSurvey{
    "TrustSafetySentimentSurvey", base::FEATURE_DISABLED_BY_DEFAULT};
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable uploading of a zip archive of system logs instead of individual files.
const base::Feature kUploadZippedSystemLogs{"UploadZippedSystemLogs",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables user activity event logging for power management on
// Chrome OS.
const base::Feature kUserActivityEventLogging{"UserActivityEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kWebAppManifestIconUpdating{
    "WebAppManifestIconUpdating", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

const base::Feature kWebAppManifestPolicyAppIdentityUpdate{
    "WebAppManifestPolicyAppIdentityUpdate", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// When this feature flag is enabled together with the LacrosAvailability
// policy, the Chrome app Kiosk session uses Lacros-chrome as the web browser to
// launch Chrome apps. When disabled, the Ash-chrome will be used instead.
const base::Feature kChromeKioskEnableLacros{"ChromeKioskEnableLacros",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When this feature flag is enabled together with the LacrosAvailability
// policy, the web (PWA) Kiosk session uses Lacros-chrome as the web browser to
// launch web (PWA) applications. When disabled, the Ash-chrome will be used
// instead.
const base::Feature kWebKioskEnableLacros{"WebKioskEnableLacros",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the Ash browser only manages system web apps, and non-system
// web apps are managed by the Lacros browser. When disabled, the Ash browser
// manages all web apps.
const base::Feature kWebAppsCrosapi{"WebAppsCrosapi",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
// Allow capturing of WebRTC event logs, and uploading of those logs to Crash.
// Please note that a Chrome policy must also be set, for this to have effect.
// Effectively, this is a kill-switch for the feature.
// TODO(crbug.com/775415): Remove this kill-switch.
const base::Feature kWebRtcRemoteEventLog{"WebRtcRemoteEventLog",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
// Compress remote-bound WebRTC event logs (if used; see kWebRtcRemoteEventLog).
const base::Feature kWebRtcRemoteEventLogGzipped{
    "WebRtcRemoteEventLogGzipped", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
// Enables Web Share (navigator.share)
const base::Feature kWebShare{"WebShare", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_MAC)
// Enables Web Share (navigator.share) for macOS
const base::Feature kWebShare{"WebShare", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables setting time limit for Chrome and PWA's on child user device.
// Requires |kPerAppTimeLimits| to be enabled.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kWebTimeLimits{"WebTimeLimits",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Whether to enable "dark mode" enhancements in Mac Mojave or Windows 10 for
// UIs implemented with web technologies.
const base::Feature kWebUIDarkMode {
  "WebUIDarkMode",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Populates storage dimensions in UMA log if enabled. Requires diagnostics
// package in the image.
const base::Feature kUmaStorageDimensions{"UmaStorageDimensions",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
// Allow a Wilco DTC (diagnostics and telemetry controller) on Chrome OS.
// More info about the project may be found here:
// https://docs.google.com/document/d/18Ijj8YlC8Q3EWRzLspIi2dGxg4vIBVe5sJgMPt9SWYo
const base::Feature kWilcoDtc{"WilcoDtc", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Populates the user type on device type metrics in UMA log if enabled.
const base::Feature kUserTypeByDeviceTypeMetricsProvider{
    "UserTypeByDeviceTypeMetricsProvider", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_WIN)
// Enables the accelerated default browser flow for Windows 10.
const base::Feature kWin10AcceleratedDefaultBrowserFlow{
    "Win10AcceleratedDefaultBrowserFlow", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN)

// Enables writing basic system profile to the persistent histograms files
// earlier.
const base::Feature kWriteBasicSystemProfileToPersistentHistogramsFile{
    "WriteBasicSystemProfileToPersistentHistogramsFile",
    base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsParentAccessCodeForOnlineLoginEnabled() {
  return base::FeatureList::IsEnabled(kParentAccessCodeForOnlineLogin);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables omnibox trigger prerendering.
const base::Feature kOmniboxTriggerForPrerender2{
    "OmniboxTriggerForPrerender2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSupportSearchSuggestionForPrerender2{
    "SupportSearchSuggestionForPrerender2", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<SearchSuggestionPrerenderImplementationType>::Option
    search_suggestion_implementation_types[] = {
        {SearchSuggestionPrerenderImplementationType::kUsePrefetch,
         "use_prefetch"},
        {SearchSuggestionPrerenderImplementationType::kIgnorePrefetch,
         "ignore_prefetch"}};
const base::FeatureParam<SearchSuggestionPrerenderImplementationType>
    kSearchSuggestionPrerenderImplementationTypeParam{
        &kSupportSearchSuggestionForPrerender2, "implementation_type",
        SearchSuggestionPrerenderImplementationType::kIgnorePrefetch,
        &search_suggestion_implementation_types};

// Enables omnibox trigger no state prefetch. Only one of
// kOmniboxTriggerForPrerender2 or kOmniboxTriggerForNoStatePrefetch can be
// enabled in the experiment.
// TODO(crbug.com/1267731): Remove this flag once the experiments are completed.
const base::Feature kOmniboxTriggerForNoStatePrefetch{
    "OmniboxTriggerForNoStatePrefetch", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A feature to indicate whether setting wake time >24hours away is supported by
// the platform's RTC.
// TODO(b/187516317): Remove when the issue is resolved in FW.
const base::Feature kSupportsRtcWakeOver24Hours{
    "SupportsRtcWakeOver24Hours", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const base::Feature kUseWebAppDBInsteadOfExternalPrefs{
    "UseWebAppDBInsteadOfExternalPrefs", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
