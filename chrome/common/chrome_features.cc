// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

// All features in alphabetical order.

#if defined(OS_ANDROID)
const base::Feature kAddToHomescreenMessaging{
    "AddToHomescreenMessaging", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Controls whether web apps can be installed via APKs on Chrome OS.
const base::Feature kApkWebAppInstalls{"ApkWebAppInstalls",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// Enable the new multi-profile-aware app shim mode.
// TODO(https://crbug.com/982024): Delete this flag when feature is complete.
const base::Feature kAppShimMultiProfile{"AppShimMultiProfile",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Can be used to disable RemoteCocoa (hosting NSWindows for apps in the app
// process). For debugging purposes only.
const base::Feature kAppShimRemoteCocoa{"AppShimRemoteCocoa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the "this OS is obsolete" infobar on Mac 10.9.
// TODO(ellyjones): Remove this after the last 10.9 release.
const base::Feature kShow10_9ObsoleteInfobar{"Show109ObsoleteInfobar",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Use the Toolkit-Views Task Manager window.
const base::Feature kViewsTaskManager{"ViewsTaskManager",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
// Enables messaging in site permissions UI informing user when notifications
// are disabled for the entire app.
const base::Feature kAppNotificationStatusMessaging{
    "AppNotificationStatusMessaging", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// App Service related flags. See chrome/services/app_service/README.md.
const base::Feature kAppServiceAsh{"AppServiceAsh",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAppServiceInstanceRegistry{
    "AppServiceInstanceRegistry", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAppServiceIntentHandling{
    "AppServiceIntentHandling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAppServiceShelf{"AppServiceShelf",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

// Enables the built-in DNS resolver.
const base::Feature kAsyncDns {
  "AsyncDns",
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_WIN) || defined(OS_LINUX)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
const base::Feature kBackgroundModeAllowRestart{
    "BackgroundModeAllowRestart", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_LINUX)

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables whether permission prompts are automatically blocked
// after the user has ignored them too many times.
const base::Feature kBlockPromptsIfIgnoredOften{
    "BlockPromptsIfIgnoredOften", base::FEATURE_DISABLED_BY_DEFAULT};

// Once the user declines a notification permission prompt in a WebContents,
// automatically dismiss subsequent prompts in the same WebContents, from any
// origin, until the next user-initiated navigation.
const base::Feature kBlockRepeatedNotificationPermissionPrompts{
    "BlockRepeatedNotificationPermissionPrompts",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Fixes for browser hang bugs are deployed in a field trial in order to measure
// their impact. See crbug.com/478209.
const base::Feature kBrowserHangFixesExperiment{
    "BrowserHangFixesExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables redirecting users who get an interstitial when
// accessing https://support.google.com/chrome/answer/6098869 to local
// connection help content.
const base::Feature kBundledConnectionHelpFeature{
    "BundledConnectionHelp", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the UI to configure caption settings.
const base::Feature kCaptionSettings{"CaptionSettings",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables logging UKMs for background tab activity by TabActivityWatcher.
const base::Feature kTabMetricsLogging{"TabMetricsLogging",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Enables the blocking of third-party modules. This feature requires Windows 8
// or higher because it depends on the ProcessExtensionPointDisablePolicy
// mitigation, which was not available on Windows 7.
// Note: Due to a limitation in the implementation of this feature, it is
// required to start the browser two times to fully enable or disable it.
const base::Feature kThirdPartyModulesBlocking{
    "ThirdPartyModulesBlocking", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the additional TLS 1.3 server-random-based downgrade protection
// described in https://tools.ietf.org/html/rfc8446#section-4.1.3 for
// connections which chain to a local trust anchor. The protection is
// unconditionally enabled for known trust anchors.
//
// This is a MUST-level requirement of TLS 1.3, but may have compatibility
// issues with some outdated buggy TLS-terminating proxies.
const base::Feature kTLS13HardeningForLocalAnchors{
    "TLS13HardeningForLocalAnchors", base::FEATURE_DISABLED_BY_DEFAULT};

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
// Enables the dual certificate verification trial feature.
// https://crbug.com/649026
const base::Feature kCertDualVerificationTrialFeature{
    "CertDualVerificationTrial", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables change picture video mode.
const base::Feature kChangePictureVideoMode{"ChangePictureVideoMode",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables passing additional user authentication in requests to DMServer
// (policy fetch, status report upload).
const base::Feature kDMServerOAuthForChildUser{
    "DMServerOAuthForChildUser", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Enables clearing of browsing data which is older than given time period.
const base::Feature kClearOldBrowsingData{"ClearOldBrowsingData",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kClickToOpenPDFPlaceholder{
    "ClickToOpenPDFPlaceholder", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
const base::Feature kImmersiveFullscreen{"ImmersiveFullscreen",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Shows a setting that allows disabling mouse acceleration.
const base::Feature kAllowDisableMouseAcceleration{
    "AllowDisableMouseAcceleration", base::FEATURE_ENABLED_BY_DEFAULT};

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
    "CrostiniAnsibleInfrastructure", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables infrastructure for generating Ansible playbooks for the default
// Crostini container from software configurations in JSON schema.
const base::Feature kCrostiniAnsibleSoftwareManagement{
    "CrostiniAnsibleSoftwareManagement", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables custom UI for forcibly closing unresponsive windows.
const base::Feature kCrostiniForceClose{"CrostiniForceClose",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the UI overhaul for Cups Printers in settings page.
const base::Feature kCupsPrintersUiOverhaul{"CupsPrintersUiOverhaul",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enable support for "Plugin VMs" on Chrome OS.
const base::Feature kPluginVm{"PluginVm", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable adding a print server on Chrome OS.
const base::Feature kPrintServerUi{"PrintServerUi",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable chrome://terminal.  Terminal System App will only run on
// OS_CHROMEOS, but this flag must be defined for all platforms since
// it is required for SystemWebApp tests.
const base::Feature kTerminalSystemApp{"TerminalSystemApp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable splits in the Terminal System App.
const base::Feature kTerminalSystemAppSplits{"TerminalSystemAppSplits",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable uploading of a zip archive of system logs instead of individual files.
const base::Feature kUploadZippedSystemLogs{"UploadZippedSystemLogs",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Allow a Wilco DTC (diagnostics and telemetry controller) on Chrome OS.
// More info about the project may be found here:
// https://docs.google.com/document/d/18Ijj8YlC8Q3EWRzLspIi2dGxg4vIBVe5sJgMPt9SWYo
const base::Feature kWilcoDtc{"WilcoDtc", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable using tab sharing infobars for desktop capture.
const base::Feature kDesktopCaptureTabSharingInfobar{
    "DesktopCaptureTabSharingInfobar", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables new Desktop PWA support for minimal-ui display mode.
const base::Feature kDesktopMinimalUI{"DesktopMinimalUI",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables new Desktop PWAs implementation that does not use
// extensions.
const base::Feature kDesktopPWAsWithoutExtensions{
    "DesktopPWAsWithoutExtensions", base::FEATURE_DISABLED_BY_DEFAULT};

// When installing default installed PWAs, we wait for service workers
// to cache resources.
const base::Feature kDesktopPWAsCacheDuringDefaultInstall{
    "DesktopPWAsCacheDuringDefaultInstall", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables local PWA installs to update their app manifest data if the site
// changes its manifest.
const base::Feature kDesktopPWAsLocalUpdating {
  "DesktopPWAsLocalUpdating",
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables or disables use of new Desktop PWAs browser controller (that uses the
// universal web_app::AppRegistrar) by extensions-based bookmark apps. Note that
// the new Desktop PWAs implementation (not based on extensions) always uses the
// new browser controller.
const base::Feature kDesktopPWAsUnifiedUiController{
    "DesktopPWAsUnifiedUiController", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables use of new Desktop PWAs launch manager by
// extensions-based bookmark apps. (Note that Bookmark apps not based
// on extensions unconditionally use the new launch manager.)
const base::Feature kDesktopPWAsUnifiedLaunch{
    "DesktopPWAsUnifiedLaunch", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables new Desktop PWAs Unified Sync and Storage (USS)
// implementation that does not use extensions. Requires
// kDesktopPWAsWithoutExtensions to be enabled.
const base::Feature kDesktopPWAsUSS{"DesktopPWAsUSS",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the ability to install PWAs from the omnibox.
const base::Feature kDesktopPWAsOmniboxInstall{
    "DesktopPWAsOmniboxInstall", base::FEATURE_ENABLED_BY_DEFAULT};

// Disables downloads of unsafe file types over HTTP.
const base::Feature kDisallowUnsafeHttpDownloads{
    "DisallowUnsafeHttpDownloads", base::FEATURE_DISABLED_BY_DEFAULT};
const char kDisallowUnsafeHttpDownloadsParamName[] = "MimeTypeList";

// Enable DNS over HTTPS (DoH).
const base::Feature kDnsOverHttps{"DnsOverHttps",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Set whether fallback to insecure DNS is allowed by default. This setting may
// be overridden for individual transactions.
const base::FeatureParam<bool> kDnsOverHttpsFallbackParam{&kDnsOverHttps,
                                                          "Fallback", true};

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

// If enabled, Drive will use FCM for its invalidations.
const base::Feature kDriveFcmInvalidations{"DriveFCMInvalidations",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, policies will use FCM (Firebase Cloud Messaging) for its
// invalidations.
const base::Feature kPolicyFcmInvalidations{"PolicyFCMInvalidations",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables ambient authentication in incognito mode.
// TODO(https://crbug.com/458508): Change to disabled by default after proper
// notice to use the policy to activate when required, M79-M80.
const base::Feature kEnableAmbientAuthenticationInIncognito{
    "EnableAmbientAuthenticationInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables ambient authentication in guest sessions.
// TODO(https://crbug.com/458508): Change to disabled by default after proper
// notice to use the policy to activate when required, M79-M80.
const base::Feature kEnableAmbientAuthenticationInGuestSession{
    "EnableAmbientAuthenticationInGuestSession",
    base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Upload enterprise cloud reporting without the extension.
const base::Feature kEnterpriseReportingInBrowser{
    "EnterpriseReportingInBrowser", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables event-based status reporting for child accounts in Chrome OS.
const base::Feature kEventBasedStatusReporting{
    "EventBasedStatusReporting", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// If enabled, this feature's |kExternalInstallDefaultButtonKey| field trial
// parameter value controls which |ExternalInstallBubbleAlert| button is the
// default.
const base::Feature kExternalExtensionDefaultButtonControl{
    "ExternalExtensionDefaultButtonControl", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Focus Mode which brings up a PWA-like window look.
const base::Feature kFocusMode{"FocusMode", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_VR)

#if BUILDFLAG(ENABLE_OCULUS_VR)
// Controls Oculus support.
const base::Feature kOculusVR{"OculusVR", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
// Controls OpenVR support.
const base::Feature kOpenVR{"OpenVR", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
// Controls Windows Mixed Reality support.
const base::Feature kWindowsMixedReality{"WindowsMixedReality",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_OPENXR)
// Controls OpenXR support.
const base::Feature kOpenXR{"OpenXR", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_OPENXR

#endif  // BUILDFLAG(ENABLE_VR)

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
const base::Feature kGoogleBrandedContextMenu{
    "GoogleBrandedContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Enables or disables the Happiness Tracking System for the device.
const base::Feature kHappinessTrackingSystem{"HappinessTrackingSystem",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Enables or disables the Happiness Tracking System for Desktop Chrome.
const base::Feature kHappinessTrackingSurveysForDesktop{
    "HappinessTrackingSurveysForDesktop", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Happiness Tracking System demo mode for Desktop
// Chrome.
const base::Feature kHappinessTrackingSurveysForDesktopDemo{
    "HappinessTrackingSurveysForDesktopDemo",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

// Enables committed error pages instead of transient navigation entries for
// HTTP auth interstitial pages (i.e. HTTP auth prompts initiated cross-origin).
const base::Feature kHTTPAuthCommittedInterstitials{
    "HTTPAuthCommittedInterstitials", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables navigation suggestions UI for lookalike URLs (e.g. internationalized
// domain names that are visually similar to popular domains or to domains with
// engagement score, such as googlé.com).
const base::Feature kLookalikeUrlNavigationSuggestionsUI{
    "LookalikeUrlNavigationSuggestionsUI", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
// A feature that controls whether Chrome warns about incompatible applications.
// This feature requires Windows 10 or higher to work because it depends on
// the "Apps & Features" system settings.
const base::Feature kIncompatibleApplicationsWarning{
    "IncompatibleApplicationsWarning", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
// Enables or disables the installable ambient badge infobar.
const base::Feature kInstallableAmbientBadgeInfoBar{
    "InstallableAmbientBadgeInfoBar", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Enables or disables intent picker.
const base::Feature kIntentPicker{"IntentPicker",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
const base::Feature kKernelnextVMs{"KernelnextVMs",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Uses KidsManagement UrlClassification instead of SafeSearch for supervised
// accounts.
const base::Feature kKidsManagementUrlClassification{
    "KidsManagementUrlClassification", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
// Uses NSFullSizeContentViewWindowMask where available instead of adding our
// own views to the window frame. This is a temporary kill switch, it can be
// removed once we feel okay about leaving it on.
const base::Feature kMacFullSizeContentView{"MacFullSizeContentView",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#endif

#if defined(OS_MACOSX)
// Enables the Material Design download shelf on Mac.
const base::Feature kMacMaterialDesignDownloadShelf{
    "MacMDDownloadShelf", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MACOSX)
// In case a website is trying to use the camera/microphone, but Chrome itself
// is blocked on the system level to access these, show an icon in the Omnibox,
// which, when clicked, displays a bubble with information on how to toggle
// Chrome's system-level media permissions.
const base::Feature kMacSystemMediaPermissionsInfoUi{
    "MacSystemMediaPermissionsInfoUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable screen capture system permission check on Mac 10.15+.
const base::Feature kMacSystemScreenCapturePermissionCheck{
    "MacSystemScreenCapturePermissionCheck", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Sets whether dismissing the new-tab-page override bubble counts as
// acknowledgement.
const base::Feature kAcknowledgeNtpOverrideOnDeactivate{
    "AcknowledgeNtpOverrideOnDeactivate", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables showing an entry for mixed content in site settings, which controls
// allowing blockable mixed content. When enabled, the mixed content shield is
// not shown on the omnibox, since its functionality is replaced by the
// setting.
const base::Feature kMixedContentSiteSetting{"MixedContentSiteSetting",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
const base::Feature kOnConnectNative{"OnConnectNative",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the use of native notification centers instead of using the Message
// Center for displaying the toasts. The feature is hardcoded to enabled for
// Chrome OS.
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS) && !defined(OS_CHROMEOS)
const base::Feature kNativeNotifications{"NativeNotifications",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)

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

#if defined(OS_POSIX)
// Enables NTLMv2, which implicitly disables NTLMv1.
const base::Feature kNtlmV2Enabled{"NtlmV2Enabled",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Enables or disabled the OOM intervention.
const base::Feature kOomIntervention{"OomIntervention",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Adds the base language code to the Language-Accept headers if at least one
// corresponding language+region code is present in the user preferences.
// For example: "en-US, fr-FR" --> "en-US, en, fr-FR, fr".
const base::Feature kUseNewAcceptLanguageHeader{
    "UseNewAcceptLanguageHeader", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables usage of Parent Access Code to authorize certain actions on child
// user device.
const base::Feature kParentAccessCode{"ParentAccessCode",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables usage of Parent Access Code to authorize change of time actions on
// child user device. Requires |kParentAccessCode| to be enabled.
const base::Feature kParentAccessCodeForTimeChange{
    "ParentAccessCodeForTimeChange", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables enforcement of per-app time limits for child user.
const base::Feature kPerAppTimeLimits{"PerAppTimeLimits",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Delegate permissions to cross-origin iframes when the feature has been
// allowed by feature policy.
const base::Feature kPermissionDelegation{"PermissionDelegation",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Allows prediction operations (e.g., prefetching) on all connection types.
const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes{
    "PredictivePrefetchingAllowedOnAllConnectionTypes",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Allows Chrome to do preconnect when prerender fails.
const base::Feature kPrerenderFallbackToPreconnect{
    "PrerenderFallbackToPreconnect", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to display redesign of the chrome privacy settings page
// to the user.
const base::Feature kPrivacySettingsRedesign{"PrivacySettingsRedesign",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_PLUGINS)
// Show Flash deprecation warning to users who have manually enabled Flash.
// https://crbug.com/918428
const base::Feature kFlashDeprecationWarning{"FlashDeprecationWarning",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// If enabled, Print Preview will use the CloudPrinterHandler instead of the
// cloud print interface to communicate with the cloud print server. This
// prevents Print Preview from making direct network requests. See
// https://crbug.com/829414.
const base::Feature kCloudPrinterHandler{"CloudPrinterHandler",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using quiet prompts for notification permission requests.
const base::Feature kQuietNotificationPrompts{
    "QuietNotificationPrompts", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables permanent removal of Legacy Supervised Users on startup.
const base::Feature kRemoveSupervisedUsersOnStartup{
    "RemoveSupervisedUsersOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Controls whether the user is prompted when sites request attestation.
const base::Feature kSecurityKeyAttestationPrompt{
    "SecurityKeyAttestationPrompt", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kShowTrustedPublisherURL{"ShowTrustedPublisherURL",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

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

// Controls a mode for dynamically process-isolating sites where the user has
// entered a password.  This is intended to be used primarily when full site
// isolation is turned off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
const base::Feature kSiteIsolationForPasswordSites{
  "site-isolation-for-password-sites",
// Enabled by default on Android; see https://crbug.com/849815.  Note that this
// should not affect Android Webview, which does not include this code.
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// kSitePerProcessOnlyForHighMemoryClients is checked before kSitePerProcess,
// and (if enabled) can restrict if kSitePerProcess feature is checked at all -
// no check will be made on devices with low memory (these devices will have no
// Site Isolation via kSitePerProcess trials and won't activate either the
// control or the experiment group).  The threshold for what is considered a
// "low memory" device is set (in MB) via a field trial param with the name
// defined below ("site-per-process-low-memory-cutoff-mb") and compared against
// base::SysInfo::AmountOfPhysicalMemoryMB().
const base::Feature kSitePerProcessOnlyForHighMemoryClients{
    "site-per-process-only-for-high-memory-clients",
    base::FEATURE_DISABLED_BY_DEFAULT};
const char kSitePerProcessOnlyForHighMemoryClientsParamName[] =
    "site-per-process-low-memory-cutoff-mb";

#if defined(OS_CHROMEOS)
// Enables or disables automatic setup of USB printers.
const base::Feature kStreamlinedUsbPrinterSetup{
    "StreamlinedUsbPrinterSetup", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the ability to add a Samba Share to the Files app
const base::Feature kNativeSmb{"NativeSmb", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

// Enables or disables the ability to use the sound content setting to mute a
// website.
const base::Feature kSoundContentSetting{"SoundContentSetting",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables or disables the Javascript API to propagate sync encryption keys.
const base::Feature kSyncEncryptionKeysWebApi{
    "SyncEncryptionKeysWebApi", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Enables or disables chrome://sys-internals.
const base::Feature kSysInternals{"SysInternals",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables the System Web App manager.
const base::Feature kSystemWebApps{"SystemWebApps",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the App Management UI.
const base::Feature kAppManagement{"AppManagement",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page
const base::Feature kTreatUnsafeDownloadsAsActive{
    "TreatUnsafeDownloadsAsActive", base::FEATURE_DISABLED_BY_DEFAULT};
const char kTreatUnsafeDownloadsAsActiveParamName[] = "ExtensionList";

// Enables or disables the intervention that unloads ad iframes with intensive
// resource usage.
const base::Feature kHeavyAdIntervention{"HeavyAdIntervention",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the privacy mitigations for the heavy ad intervention.
// This throttles the amount of interventions that can occur on a given host in
// a time period. It also adds noise to the thresholds used. This is separate
// from the intervention feature so it does not interfere with field trial
// activation, as this blocklist is created for every user, and noise is decided
// prior to seeing a heavy ad.
const base::Feature kHeavyAdPrivacyMitigations{
    "HeavyAdPrivacyMitigations", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kUseDisplayWideColorGamut{"UseDisplayWideColorGamut",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

bool UseDisplayWideColorGamut() {
  auto compute_use_display_wide_color_gamut = []() {
    // Enabled this feature for devices listed in "enabled_models" field trial
    // param. This is a comma separated list.
    std::string enabled_models_list = base::GetFieldTrialParamValueByFeature(
        kUseDisplayWideColorGamut, "enabled_models");
    if (enabled_models_list.empty())
      return false;

    const char* current_model =
        base::android::BuildInfo::GetInstance()->model();
    std::vector<std::string> enabled_models =
        base::SplitString(enabled_models_list, ",", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const std::string& model : enabled_models) {
      if (model == current_model)
        return true;
    }

    return false;
  };

  // As it takes some work to compute this, cache the result.
  static base::NoDestructor<bool> is_wide_color_gamut_enabled(
      compute_use_display_wide_color_gamut());
  return *is_wide_color_gamut_enabled;
}
#endif

#if defined(OS_CHROMEOS)
// Enables or disables the FTL signaling service for CRD sessions in Kiosk mode.
const base::Feature kUseFtlSignalingForCrdHostDelegate{
    "UseFtlSignalingForCrdHostDelegate", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables or disables logging for adaptive screen brightness on Chrome OS.
const base::Feature kAdaptiveScreenBrightnessLogging{
    "AdaptiveScreenBrightnessLogging", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables user activity event logging for power management on
// Chrome OS.
const base::Feature kUserActivityEventLogging{"UserActivityEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables support of libcups APIs from ARC
const base::Feature kArcCupsApi{"ArcCupsApi",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables pin quick unlock.
// TODO(https://crbug.com/935613): Remove this & the backing code.
const base::Feature kQuickUnlockPin{"QuickUnlockPin",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables pin on the login screen.
const base::Feature kQuickUnlockPinSignin{"QuickUnlockPinSignin",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables fingerprint quick unlock.
const base::Feature kQuickUnlockFingerprint{"QuickUnlockFingerprint",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables flash component updates on Chrome OS.
const base::Feature kCrosCompUpdates{"CrosCompUpdates",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables TPM firmware update capability on Chrome OS.
const base::Feature kTPMFirmwareUpdate{"TPMFirmwareUpdate",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables "usm" service in the list of user services returned by
// userInfo Gaia message.
const base::Feature kCrOSEnableUSMUserService{"CrOSEnableUSMUserService",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables SmartDim on Chrome OS.
const base::Feature kSmartDim{"SmartDim", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USBGuard at the lockscreen on Chrome OS.
// TODO(crbug.com/874630): Remove this kill-switch
const base::Feature kUsbguard{"USBGuard", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USB Bouncer for managing a device whitelist for USBGuard on Chrome OS.
const base::Feature kUsbbouncer{"USBBouncer",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enable support for multiple scheduler configurations.
const base::Feature kSchedulerConfiguration{"SchedulerConfiguration",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#endif  // defined(OS_CHROMEOS)

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

// Whether to enable "dark mode" enhancements in Mac Mojave or Windows 10 for
// UIs implemented with web technologies.
const base::Feature kWebUIDarkMode {
  "WebUIDarkMode",
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_ANDROID)
};

#if defined(OS_WIN)
// Enables the accelerated default browser flow for Windows 10.
const base::Feature kWin10AcceleratedDefaultBrowserFlow{
    "Win10AcceleratedDefaultBrowserFlow", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

// Enables writing basic system profile to the persistent histograms files
// earlier.
const base::Feature kWriteBasicSystemProfileToPersistentHistogramsFile{
    "WriteBasicSystemProfileToPersistentHistogramsFile",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables improvements to the chrome://accessibility page.
const base::Feature kAccessibilityInternalsPageImprovements{
    "AccessibilityInternalsPageImprovements",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting time limit for Chrome and PWA's on child user device.
// Requires |kPerAppTimeLimits| to be enabled.
#if defined(OS_CHROMEOS)
const base::Feature kWebTimeLimits{"WebTimeLimits",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace features
