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

#if defined(OS_ANDROID)
// Enables showing an adaptive action button in the top toolbar.
const base::Feature kAdaptiveButtonInTopToolbar{
    "AdaptiveButtonInTopToolbar", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables logging for adaptive screen brightness on Chrome OS.
const base::Feature kAdaptiveScreenBrightnessLogging{
    "AdaptiveScreenBrightnessLogging", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Shows a setting that allows disabling mouse acceleration.
const base::Feature kAllowDisableMouseAcceleration{
    "AllowDisableMouseAcceleration", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Always reinstall system web apps, instead of only doing so after version
// upgrade or locale changes.
const base::Feature kAlwaysReinstallSystemWebApps{
    "ReinstallSystemWebApps", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kAndroidDarkSearch{"AndroidDarkSearch",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Controls whether web apps can be installed via APKs on Chrome OS.
const base::Feature kApkWebAppInstalls{"ApkWebAppInstalls",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
// App Service related flags. See components/services/app_service/README.md.
const base::Feature kAppServiceAdaptiveIcon{"AppServiceAdaptiveIcon",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAppServiceExternalProtocol{
    "AppServiceExternalProtocol", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_MAC)
// Can be used to disable RemoteCocoa (hosting NSWindows for apps in the app
// process). For debugging purposes only.
const base::Feature kAppShimRemoteCocoa{"AppShimRemoteCocoa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// This is used to control the new app close behavior on macOS wherein closing
// all windows for an app leaves the app running.
// https://crbug.com/1080729
const base::Feature kAppShimNewCloseBehavior{"AppShimNewCloseBehavior",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MAC)

// Enables the built-in DNS resolver.
const base::Feature kAsyncDns {
  "AsyncDns",
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
const base::Feature kBackgroundModeAllowRestart{
    "BackgroundModeAllowRestart", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable Borealis on Chrome OS.
const base::Feature kBorealis{"Borealis", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables change picture video mode.
const base::Feature kChangePictureVideoMode{"ChangePictureVideoMode",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
const base::Feature kChromeCleanupScanCompletedNotification{
    "ChromeCleanupScanCompletedNotification",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Enables clearing of browsing data which is older than given time period.
const base::Feature kClearOldBrowsingData{"ClearOldBrowsingData",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kClientStorageAccessContextAuditing{
    "ClientStorageAccessContextAuditing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSettingsRedesign{"ContentSettingsRedesign",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kContinuousSearch{"ContinuousSearch",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of Chrome's threads to use only LITTLE cores on big.LITTLE
// architectures.
const base::Feature kCpuAffinityRestrictToLittleCores{
    "CpuAffinityRestrictToLittleCores", base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of Chrome's threads to use only LITTLE cores on big.LITTLE
// architectures when power mode is idle.
const base::Feature kPowerSchedulerThrottleIdle{
    "PowerSchedulerThrottleIdle", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables parsing and enforcing Data Leak Prevention policy rules that
// restricts usage of some system features, e.g.clipboard, screenshot, etc.
// for confidential content.
const base::Feature kDataLeakPreventionPolicy{"DataLeakPreventionPolicy",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables passing additional user authentication in requests to DMServer
// (policy fetch, status report upload).
const base::Feature kDMServerOAuthForChildUser{
    "DMServerOAuthForChildUser", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Whether to allow installed-by-default web apps to be installed or not.
const base::Feature kDefaultWebAppInstallation{
    "DefaultWebAppInstallation", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Updates the default set of pinned apps in the Chrome OS shelf for new
// profiles.
const base::Feature kDefaultPinnedAppsUpdate2021Q2{
    "DefaultPinnedAppsUpdate2021Q2", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable using tab sharing infobars for desktop capture.
const base::Feature kDesktopCaptureTabSharingInfobar{
    "DesktopCaptureTabSharingInfobar", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Desktop PWA installs to have a menu of shortcuts associated with
// the app icon in the taskbar on Windows, or the dock on macOS or Linux.
const base::Feature kDesktopPWAsAppIconShortcutsMenu{
    "DesktopPWAsAppIconShortcutsMenu", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables Desktop PWAs shortcuts menu to be visible and executable in ChromeOS
// UI surfaces.
const base::Feature kDesktopPWAsAppIconShortcutsMenuUI{
    "DesktopPWAsAppIconShortcutsMenuUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables attention badging for PWA icons in the shelf and launcher.
const base::Feature kDesktopPWAsAttentionBadgingCrOS{
    "DesktopPWAsAttentionBadgingCrOS", base::FEATURE_ENABLED_BY_DEFAULT};
constexpr base::FeatureParam<std::string> kDesktopPWAsAttentionBadgingCrOSParam{
    &kDesktopPWAsAttentionBadgingCrOS, "badge-source",
    switches::kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications};
#endif

// When installing default installed PWAs, we wait for service workers
// to cache resources.
const base::Feature kDesktopPWAsCacheDuringDefaultInstall{
    "DesktopPWAsCacheDuringDefaultInstall", base::FEATURE_ENABLED_BY_DEFAULT};

// Moves the Extensions "puzzle piece" icon from the title bar into the app menu
// for web app windows.
const base::Feature kDesktopPWAsElidedExtensionsMenu{
  "DesktopPWAsElidedExtensionsMenu",
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Replaces the origin text flash in web app titlebars with the name of the app.
const base::Feature kDesktopPWAsFlashAppNameInsteadOfOrigin{
    "DesktopPWAsFlashAppNameInsteadOfOrigin",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Desktop PWAs to be auto-started on OS login.
const base::Feature kDesktopPWAsRunOnOsLogin {
  "DesktopPWAsRunOnOsLogin",
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables or disables usage of shared LevelDB instance (ModelTypeStoreService).
// If this flag is disabled, the new Web Apps system uses its own isolated
// LevelDB instance for manual testing purposes. Requires
// kDesktopPWAsWithoutExtensions to be enabled.
// TODO(crbug.com/877898): Delete this feature flag before
// kDesktopPWAsWithoutExtensions launch.
const base::Feature kDesktopPWAsSharedStoreService{
    "DesktopPWAsSharedStoreService", base::FEATURE_ENABLED_BY_DEFAULT};

// Adds a tab strip to PWA windows, used for UI experimentation.
// TODO(crbug.com/897314): Enable this feature.
const base::Feature kDesktopPWAsTabStrip{"DesktopPWAsTabStrip",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Makes user navigations via links within web app scopes get captured tab
// tabbed app windows.
// TODO(crbug.com/897314): Enable this feature.
const base::Feature kDesktopPWAsTabStripLinkCapturing{
    "DesktopPWAsTabStripLinkCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables new Desktop PWAs implementation that does not use
// extensions.
const base::Feature kDesktopPWAsWithoutExtensions{
    "DesktopPWAsWithoutExtensions", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable DNS over HTTPS (DoH).
const base::Feature kDnsOverHttps {
  "DnsOverHttps",
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || \
    defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Provides a mechanism to remove providers from the dropdown list in the
// settings UI. Separate multiple provider ids with commas. See the
// mapping in net/dns/dns_util.cc for provider ids.
const base::FeatureParam<std::string> kDnsOverHttpsDisabledProvidersParam{
    &kDnsOverHttps, "DisabledProviders", ""};

// Set whether fallback to insecure DNS is allowed by default. This setting may
// be overridden for individual transactions.
const base::FeatureParam<bool> kDnsOverHttpsFallbackParam{&kDnsOverHttps,
                                                          "Fallback", true};

// Sets whether the DoH setting is displayed in the settings UI.
const base::FeatureParam<bool> kDnsOverHttpsShowUiParam {
  &kDnsOverHttps, "ShowUi",
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || \
    defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
// Enable changing default downloads storage location on Android.
const base::Feature kDownloadsLocationChange{"DownloadsLocationChange",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Enable loading native libraries earlier in startup on Android.
const base::Feature kEarlyLibraryLoad{"EarlyLibraryLoad",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables all registered system web apps, regardless of their respective
// feature flags.
const base::Feature kEnableAllSystemWebApps{"EnableAllSystemWebApps",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
// Enables ephemeral Guest profiles on desktop.
extern const base::Feature kEnableEphemeralGuestProfilesOnDesktop{
    "EnableEphemeralGuestProfilesOnDesktop", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_MAC)

#if defined(OS_WIN)
// Enables users to create a desktop shortcut for incognito mode.
const base::Feature kEnableIncognitoShortcutOnDesktop{
    "EnableIncognitoShortcutOnDesktop", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable the restricted web APIs for high-trusted apps.
const base::Feature kEnableRestrictedWebApis{"EnableRestrictedWebApis",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable web app uninstallation from Windows settings or control panel.
const base::Feature kEnableWebAppUninstallFromOsSettings{
    "EnableWebAppUninstallFromOsSettings", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
const base::Feature kEnterpriseReportingApiKeychainRecreation{
    "EnterpriseReportingApiKeychainRecreation",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Upload enterprise cloud reporting from Chrome OS.
const base::Feature kEnterpriseReportingInChromeOS{
    "EnterpriseReportingInChromeOS", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables event-based status reporting for child accounts in Chrome OS.
const base::Feature kEventBasedStatusReporting{
    "EventBasedStatusReporting", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Enables real-time reporting for extension request
const base::Feature kEnterpriseRealtimeExtensionRequest{
    "EnterpriseRealtimeExtensionRequest", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<base::TimeDelta>
    kEnterpiseRealtimeExtensionRequestThrottleDelay{
        &kEnterpriseRealtimeExtensionRequest, "throttle_delay",
        base::TimeDelta::FromMinutes(1)};
#endif

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

// Enables Focus Mode which brings up a PWA-like window look.
const base::Feature kFocusMode{"FocusMode", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables using GDI to print text as simply text.
const base::Feature kGdiTextPrinting{"GdiTextPrinting",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Controls whether the GeoLanguage system is enabled. GeoLanguage uses IP-based
// coarse geolocation to provide an estimate (for use by other Chrome features
// such as Translate) of the local/regional language(s) corresponding to the
// device's location. If this feature is disabled, the GeoLanguage provider is
// not initialized at startup, and clients calling it will receive an empty list
// of languages.
const base::Feature kGeoLanguage{"GeoLanguage",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
#if !defined(OS_ANDROID)
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

// Enables or disables the Happiness Tracking System for Layout panel in Chrome
// DevTools on Desktop.
const base::Feature kHaTSDesktopDevToolsLayoutPanel{
    "HaTSDesktopDevToolsLayoutPanel", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Desktop Privacy
// Sandbox.
const base::Feature kHappinessTrackingSurveysForDesktopPrivacySandbox{
    "HappinessTrackingSurveysForDesktopPrivacySandbox",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Settings.
const base::Feature kHappinessTrackingSurveysForDesktopSettings{
    "HappinessTrackingSurveysForDesktopSettings",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "no-sandbox",
        false};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Privacy Settings.
const base::Feature kHappinessTrackingSurveysForDesktopSettingsPrivacy{
    "HappinessTrackingSurveysForDesktopSettingsPrivacy",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// NTP Modules.
const base::Feature kHappinessTrackingSurveysForDesktopNtpModules{
    "HappinessTrackingSurveysForDesktopNtpModules",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables the Happiness Tracking System for the device.
const base::Feature kHappinessTrackingSystem{"HappinessTrackingSystem",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
// Enables or disables the Happiness Tracking System for Onboarding Experience.
const base::Feature kHappinessTrackingSystemOnboarding{
    "HappinessTrackingOnboardingExperience", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Hides the origin text from showing up briefly in WebApp windows.
const base::Feature kHideWebAppOriginText{"HideWebAppOriginText",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
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

#if defined(OS_WIN)
// A feature that controls whether Chrome warns about incompatible applications.
// This feature requires Windows 10 or higher to work because it depends on
// the "Apps & Features" system settings.
const base::Feature kIncompatibleApplicationsWarning{
    "IncompatibleApplicationsWarning", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_MAC) || defined(OS_WIN) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// When enabled, removes any theme or background customization done by the user
// on the Incognito UI.
const base::Feature kIncognitoBrandConsistencyForDesktop{
    "IncognitoBrandConsistencyForDesktop", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Support sharing in Chrome OS intent handling.
const base::Feature kIntentHandlingSharing{"IntentHandlingSharing",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
// Allow user to have preference for PWA in the intent picker.
const base::Feature kIntentPickerPWAPersistence{
    "IntentPickerPWAPersistence", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

// If enabled, CloudPolicyInvalidator and RemoteCommandInvalidator instances
// will have unique owner name.
const base::Feature kInvalidatorUniqueOwnerName{
    "InvalidatorUniqueOwnerName", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kKernelnextVMs{"KernelnextVMs",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kLacrosWebApps{"LacrosWebApps",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables LiteVideos, a data-saving optimization that throttles media requests
// to reduce the bitrate of adaptive media streams. Only for Lite mode users
// (formerly DataSaver).
const base::Feature kLiteVideo{"LiteVideo", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
// Uses NSFullSizeContentViewWindowMask where available instead of adding our
// own views to the window frame. This is a temporary kill switch, it can be
// removed once we feel okay about leaving it on.
const base::Feature kMacFullSizeContentView{"MacFullSizeContentView",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#endif

#if defined(OS_MAC)
// Enables the Material Design download shelf on Mac.
const base::Feature kMacMaterialDesignDownloadShelf{
    "MacMDDownloadShelf", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MAC)
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

#if defined(OS_ANDROID)
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

#if defined(OS_MAC)
// Enables the usage of Apple's new Notification API on macOS 10.14+
const base::Feature kNewMacNotificationAPI{"NewMacNotificationAPI",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_MAC

// When kNoReferrers is enabled, most HTTP requests will provide empty
// referrers instead of their ordinary behavior.
const base::Feature kNoReferrers{"NoReferrers",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Changes behavior of requireInteraction for notifications. Instead of staying
// on-screen until dismissed, they are instead shown for a very long time.
const base::Feature kNotificationDurationLongForRequireInteraction{
    "NotificationDurationLongForRequireInteraction",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_WIN

#if defined(OS_MAC)
// Shows alert notifications via a helper app in a utility process.
const base::Feature kNotificationsViaHelperApp{
    "NotificationsViaHelperApp", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_MAC

#if defined(OS_POSIX)
// Enables NTLMv2, which implicitly disables NTLMv1.
const base::Feature kNtlmV2Enabled{"NtlmV2Enabled",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
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

// Enables or disables the marketing opt-in screen in OOBE
const base::Feature kOobeMarketingScreen{"OobeMarketingScreen",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
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

// Enables using the prediction service for permission prompts.
const base::Feature kPermissionPredictions{"PermissionPredictions",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for "Plugin VMs" on Chrome OS.
const base::Feature kPluginVm{"PluginVm", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Allows prediction operations (e.g., prefetching) on all connection types.
const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes{
    "PredictivePrefetchingAllowedOnAllConnectionTypes",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPrefixWebAppWindowsWithAppName{
    "PrefixWebAppWindowsWithAppName", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Chrome to do preconnect when prerender fails.
const base::Feature kPrerenderFallbackToPreconnect{
    "PrerenderFallbackToPreconnect", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables additional contextual entry points to privacy settings.
const base::Feature kPrivacyAdvisor{"PrivacyAdvisor",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the privacy sandbox settings page.
const base::Feature kPrivacySandboxSettings{"PrivacySandboxSettings",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string> kPrivacySandboxSettingsURL{
    &kPrivacySandboxSettings, "website-url",
    "https://web.dev/digging-into-the-privacy-sandbox/"};

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables fingerprint quick unlock.
const base::Feature kQuickUnlockFingerprint{"QuickUnlockFingerprint",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables using quiet prompts for notification permission requests.
const base::Feature kQuietNotificationPrompts{"QuietNotificationPrompts",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables recording additional web app related debugging data to be displayed
// in: chrome://internals/web-app
const base::Feature kRecordWebAppDebugInfo{"RecordWebAppDebugInfo",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables notification permission revocation for abusive origins.
const base::Feature kAbusiveNotificationPermissionRevocation{
    "AbusiveOriginNotificationPermissionRevocation",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoveStatusBarInWebApps{
    "RemoveStatusBarInWebApps",
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

#if defined(OS_ANDROID)
const base::Feature kRequestDesktopSiteForTablets{
    "RequestDesktopSiteForTablets", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
const base::Feature kSafetyCheckChromeCleanerChild{
    "SafetyCheckChromeCleanerChild", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kSafetyCheckWeakPasswords{
    "SafetyCheckWeakPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable support for multiple scheduler configurations.
const base::Feature kSchedulerConfiguration{"SchedulerConfiguration",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Controls whether SCT audit reports are queued and the rate at which they
// should be sampled.
const base::Feature kSCTAuditing{"SCTAuditing",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<double> kSCTAuditingSamplingRate{
    &kSCTAuditing, "sampling_rate", 0.0};

const base::Feature kSearchHistoryLink{"SearchHistoryLink",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the user is prompted when sites request attestation.
const base::Feature kSecurityKeyAttestationPrompt{
    "SecurityKeyAttestationPrompt", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kSharesheet{"Sharesheet", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kSharesheetContentPreviews{
    "SharesheetContentPreviews", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kChromeOSSharingHub{"ChromeOSSharingHub",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_MAC)
// Enables the "this OS is obsolete" infobar on Mac 10.10.
// TODO(ellyjones): Remove this after the last 10.10 release.
const base::Feature kShow10_10ObsoleteInfobar{
    "Show1010ObsoleteInfobar", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MAC)

// Alternative to switches::kSitePerProcess, for turning on full site isolation.
// Launch bug: https://crbug.com/810843.  This is a //chrome-layer feature to
// avoid turning on site-per-process by default for *all* //content embedders
// (e.g. this approach lets ChromeCast avoid site-per-process mode).
//
// TODO(alexmos): Move this and the other site isolation features below to
// browser_features, as they are only used on the browser side.
const base::Feature kSitePerProcess {
  "site-per-process",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables SmartDim on Chrome OS.
const base::Feature kSmartDim{"SmartDim", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using smbfs for accessing SMB file shares.
const base::Feature kSmbFs{"SmbFs", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables or disables the ability to use the sound content setting to mute a
// website.
const base::Feature kSoundContentSetting{"SoundContentSetting",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables receiving and sending bookmark apps creation through APPS
// sync. Will be removed in M92 https://crbug.com/1185374.
const base::Feature kSyncBookmarkApps{"SyncBookmarkApps",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables chrome://sys-internals.
const base::Feature kSysInternals{"SysInternals",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables TPM firmware update capability on Chrome OS.
const base::Feature kTPMFirmwareUpdate{"TPMFirmwareUpdate",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Enables logging UKMs for background tab activity by TabActivityWatcher.
const base::Feature kTabMetricsLogging{"TabMetricsLogging",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables the teamfood flags.
const base::Feature kTeamfoodFlags{"TeamfoodFlags",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables the blocking of third-party modules. This feature requires Windows 8
// or higher because it depends on the ProcessExtensionPointDisablePolicy
// mitigation, which was not available on Windows 7.
// Note: Due to a limitation in the implementation of this feature, it is
// required to start the browser two times to fully enable or disable it.
const base::Feature kThirdPartyModulesBlocking{
    "ThirdPartyModulesBlocking", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page
const base::Feature kTreatUnsafeDownloadsAsActive{
    "TreatUnsafeDownloadsAsActive", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable uploading of a zip archive of system logs instead of individual files.
const base::Feature kUploadZippedSystemLogs{"UploadZippedSystemLogs",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Enables using NotificationCompat.Builder to create Android notifications.
const base::Feature kUseNotificationCompatBuilder{
    "UseNotificationCompatBuilder", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables or disables user activity event logging for power management on
// Chrome OS.
const base::Feature kUserActivityEventLogging{"UserActivityEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
const base::Feature kWebAppManifestIconUpdating{
    "WebAppManifestIconUpdating", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
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

#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
// Enables Web Share (navigator.share)
const base::Feature kWebShare{"WebShare", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MAC)
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
#if defined(OS_MAC) || defined(OS_WIN) || defined(OS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_MAC) || defined(OS_WIN) || defined(OS_ANDROID) ||
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

#if defined(OS_WIN)
// Enables the accelerated default browser flow for Windows 10.
const base::Feature kWin10AcceleratedDefaultBrowserFlow{
    "Win10AcceleratedDefaultBrowserFlow", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

const base::Feature kWindowNaming{"WindowNaming",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

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

}  // namespace features
