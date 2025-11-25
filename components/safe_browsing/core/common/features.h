// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_

#include <stddef.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"

namespace safe_browsing {
// Features list
//
// These options group the lines into blocks without newlines, then
// sorts by the name of the BASE_DECLARE_FEATURE in each block. It's
// recommended to keep all FeatureParams for a given Feature in the same
// block as as the Feature declaration.
//
// clang-format off
// keep-sorted start allow_yaml_lists=yes sticky_prefixes=[""] group_prefixes=["#if", "#else", "#endif", "extern const base::FeatureParam", "//", "BASE_DECLARE_FEATURE", "BASE_DECLARE_FEATURE_PARAM"] by_regex=["BASE_DECLARE_FEATURE\\(.*\\);"] skip_lines=2
// clang-format on

// Controls various parameters related to occasionally collecting ad samples,
// for example to control how often collection should occur.
BASE_DECLARE_FEATURE(kAdSamplerTriggerFeature);

// Enables adding warning shown timestamp to client safe browsing report.
BASE_DECLARE_FEATURE(kAddWarningShownTSToClientSafeBrowsingReport);

// Enables automatic revocation of notification permissions after the user has
// received a number of notifications with a suspicious verdict from the
// on-device model.
BASE_DECLARE_FEATURE(kAutoRevokeSuspiciousNotification);
// The number of days in which suspicious notification will be counted toward
// `kAutoRevokeSuspiciousNotificationMinNotificationCount`.
extern const base::FeatureParam<int>
    kAutoRevokeSuspiciousNotificationLookBackPeriod;
// Notification permissions with site engagement score of
// kAutoRevokeSuspiciousNotificationEngagementScoreCutOff or higher will not be
// revoked due to suspicious content reason to prevent false positive
// revocations.
extern const base::FeatureParam<double>
    kAutoRevokeSuspiciousNotificationEngagementScoreCutOff;
// The minimum number of suspicious notification warning the user have received
// during `kAutoRevokeSuspiciousNotificationLookBackPeriod` before the
// notification permission is revoked.
extern const base::FeatureParam<int>
    kAutoRevokeSuspiciousNotificationMinNotificationCount;

// Enables Bundled Security Settings UI on chrome://settings/security
BASE_DECLARE_FEATURE(kBundledSecuritySettings);

// Expand CSPP beyond phishing and trigger when clipboard copy API is called on
// the page.
BASE_DECLARE_FEATURE(kClientSideDetectionClipboardCopyApi);
extern const base::FeatureParam<double> kCsdClipboardCopyApiHCAcceptanceRate;
extern const base::FeatureParam<int> kCsdClipboardCopyApiMaxLength;
extern const base::FeatureParam<int> kCsdClipboardCopyApiMinLength;
extern const base::FeatureParam<double> kCsdClipboardCopyApiSampleRate;
extern const base::FeatureParam<bool> kCSDClipboardCopyApiProcessPayload;

// Enables sending a CSD ping on the detection of a credit card form.
BASE_DECLARE_FEATURE(kClientSideDetectionCreditCardForm);
// Sets the high-confidence allowlist acceptance rate for determining whether
// to send a CSD ping triggered by a credit card form.
extern const base::FeatureParam<double> kCsdCreditCardFormHCAcceptanceRate;
// Sets the percentage of credit card forms that trigger a CSD ping.
extern const base::FeatureParam<double> kCsdCreditCardFormSampleRate;
// Sets the maximum site visit count allowed when sending a CSD ping.
// If the user has visited more times than this max, then the CSD ping is
// blocked.
extern const base::FeatureParam<int> kCsdCreditCardFormMaxUserVisit;
// Specifies whether to allow pre-classification to continue on a credit card
// form detection event after logging telemetry.
extern const base::FeatureParam<bool> kCsdCreditCardFormPingOnDetection;
// Specifies whether to allow pre-classification to continue on a credit card
// form interaction event after logging telemetry.
extern const base::FeatureParam<bool> kCsdCreditCardFormPingOnInteraction;
// Specifies whether to filter credit card CSD pings based on whether the user
// is on a new site.
extern const base::FeatureParam<bool> kCsdCreditCardFormEnableNewSiteFilter;
// Specifies whether to filter credit card CSD pings based on what heuristic
// was used to detect the form.
extern const base::FeatureParam<bool> kCsdCreditCardFormEnableHeuristicFilter;
// Specifies whether to filter credit card CSD pings based on the referring app.
extern const base::FeatureParam<bool>
    kCsdCreditCardFormEnableReferringAppFilter;

// Killswitch for Llama forced trigger info redirect chain check.
BASE_DECLARE_FEATURE(kClientSideDetectionForcedLlamaRedirectChainKillswitch);

// Killswitch for client side phishing detection. Since client side models are
// run on a large fraction of navigations, crashes due to the model are very
// impactful, even if only a small fraction of users have a bad version of the
// model. This Finch flag allows us to remediate long-tail component versions
// while we fix the root cause. This will also halt the model distribution from
// OptimizationGuide.
BASE_DECLARE_FEATURE(kClientSideDetectionKillswitch);

// Inquire the on device model when the forced llama trigger info in
// RTLookupResponse asks to scan the page.
BASE_DECLARE_FEATURE(
    kClientSideDetectionLlamaForcedTriggerInfoForScamDetection);

// Killswitch for force request redirect chain check.
BASE_DECLARE_FEATURE(kClientSideDetectionRedirectChainKillswitch);

BASE_DECLARE_FEATURE(kClientSideDetectionRetryLimit);
extern const base::FeatureParam<int> kClientSideDetectionRetryLimitTime;

// Send a sample CSPP ping when a URL matches the CSD allowlist and all other
// preclassification check conditions pass.
BASE_DECLARE_FEATURE(kClientSideDetectionSamplePing);

#if BUILDFLAG(IS_ANDROID)
// Send IntelligentScanInfo in CSD pings on Android.
BASE_DECLARE_FEATURE(kClientSideDetectionSendIntelligentScanInfoAndroid);
#endif

// Pass the LlamaTriggerRuleInfo from RTLookupResponse to ClientPhishingRequest
// if it exists and the force request mechanism occurs.
BASE_DECLARE_FEATURE(kClientSideDetectionSendLlamaForcedTriggerInfo);

// Show a warning to the user based on the
// IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2.
BASE_DECLARE_FEATURE(kClientSideDetectionShowLlamaScamVerdictWarning);

#if BUILDFLAG(IS_ANDROID)
// Show a warning to the user that factors in the IntelligentScanVerdict from
// ClientPhishingResponse on Android.
BASE_DECLARE_FEATURE(kClientSideDetectionShowScamVerdictWarningAndroid);
#endif

// Expand CSPP beyond phishing and trigger when vibration API is called on the
// web page.
BASE_DECLARE_FEATURE(kClientSideDetectionVibrationApi);

// Set a RESIZE_BEST preference for image resizing algorithm in Client Side
// Detection renderer processes for both image classification and image
// embedding. This experiment is done to see if the resizing algorithm
// preference will send clearer screenshots for server side evaluation.
BASE_DECLARE_FEATURE(kConditionalImageResize);

// Creates and sends CSBRRs when notification permissions are accepted for an
// abusive site whose interstitial has been bypassed.
BASE_DECLARE_FEATURE(kCreateNotificationsAcceptedClientSafeBrowsingReports);

// Creates and sends CSBRRs when warnings are first shown to users.
BASE_DECLARE_FEATURE(kCreateWarningShownClientSafeBrowsingReports);

// Controls whether the delayed warning experiment is enabled.
BASE_DECLARE_FEATURE(kDelayedWarnings);
// True if mouse clicks should undelay the warnings immediately when delayed
// warnings feature is enabled.
extern const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks;

// Sends the WebProtect content scanning request to the corresponding regional
// DLP endpoint based on ChromeDataRegionSetting policy.
BASE_DECLARE_FEATURE(kDlpRegionalizedEndpoints);

// Enables HaTS surveys for users encountering desktop download warnings on the
// download bubble or the downloads page.
BASE_DECLARE_FEATURE(kDownloadWarningSurvey);
// The time interval after which to consider a download warning ignored, and
// potentially show the survey for ignoring a download bubble warning.
extern const base::FeatureParam<int> kDownloadWarningSurveyIgnoreDelaySeconds;
// Gives the type of the download warning HaTS survey that the user is eligible
// for. This should be set in the fieldtrial config along with the trigger ID
// for the corresponding survey (as en_site_id). The int value corresponds to
// the value of DownloadWarningHatsType enum (see
// //c/b/download/download_warning_desktop_hats_util.h).
extern const base::FeatureParam<int> kDownloadWarningSurveyType;

// Enabled additional device and network information to RealTimeUrlCheck
// requests, WP scan requests, and reporting events. These will be visible from
// the chrome://safe-browsing page.
BASE_DECLARE_FEATURE(kEnhancedFieldsForSecOps);

// Enables Enhanced Safe Browsing promos for iOS.
BASE_DECLARE_FEATURE(kEnhancedSafeBrowsingPromo);

// Adds support for enterprise deep scans initiated through the file system
// access API.
BASE_DECLARE_FEATURE(kEnterpriseFileSystemAccessDeepScan);

// Enables showing an updated Password Reuse UI for enterprise users.
BASE_DECLARE_FEATURE(kEnterprisePasswordReuseUiRefresh);

// Makes the Enhanced Protection a syncable setting.
// Check the design doc (go/esb-as-a-synced-setting-dd) for further details.
BASE_DECLARE_FEATURE(kEsbAsASyncedSetting);

// Controls whether Safe Browsing Extended Reporting (SBER) is deprecated.
// When this feature flag is enabled:
// - the Extended Reporting toggle will not be displayed on
//   chrome://settings/security
// - features will not depend on the SBER preference value,
//   safebrowsing.scout_reporting_enabled
BASE_DECLARE_FEATURE(kExtendedReportingRemovePrefDependency);

// Controls whether Safe Browsing Extended Reporting (SBER) is deprecated for
// Chrome on iOS.
// This has the same behavior as kExtendedReportingRemovePrefDependency but
// is separate for rollout purposes.
BASE_DECLARE_FEATURE(kExtendedReportingRemovePrefDependencyIos);

// Allows the Extension Telemetry Service to accept and use configurations
// sent by the server.
BASE_DECLARE_FEATURE(kExtensionTelemetryConfiguration);

// Enables collection of telemetry signal whenever an extension invokes the
// declarativeNetRequest actions.
BASE_DECLARE_FEATURE(kExtensionTelemetryDeclarativeNetRequestActionSignal);

// Allows the Extension Telemetry Service to include file data of extensions
// specified in the --load-extension commandline switch in telemetry reports.
BASE_DECLARE_FEATURE(kExtensionTelemetryFileDataForCommandLineExtensions);

// Enables the search hijacking signal in extension telemetry.
BASE_DECLARE_FEATURE(kExtensionTelemetrySearchHijackingSignal);
// The default interval between heuristic checks.
extern const base::FeatureParam<int>
    kExtensionTelemetrySearchHijackingSignalHeuristicCheckIntervalSeconds;
// The default threshold value (omnibox searches - SERP landings) that
// results in a heuristic match.
extern const base::FeatureParam<int>
    kExtensionTelemetrySearchHijackingSignalHeuristicThreshold;

// Enables reporting of external app redirects
BASE_DECLARE_FEATURE(kExternalAppRedirectTelemetry);

// Replace the high confidence allowlist check gating notification warnings with
// a check of the global cache list specific to safe notification sites.
BASE_DECLARE_FEATURE(kGlobalCacheListForGatingNotificationProtections);

// Whether to provide Google Play Protect status in APK telemetry pings
BASE_DECLARE_FEATURE(kGooglePlayProtectInApkTelemetry);

// Whether Google Play Protect should supercede file-type warnings
BASE_DECLARE_FEATURE(kGooglePlayProtectReducesWarnings);

// Communicated to the server to determine DBSC on google.com. This
// allows us to slice metrics by google.com DBSC state without any
// Google-specific code in the net stack.
BASE_DECLARE_FEATURE(kGoogleStandardDeviceBoundSessionCredentials);

// Sends hash-prefix real-time lookup requests on navigations for Standard Safe
// Browsing users instead of hash-prefix database lookups.
// Note: This feature flag should not be cleaned up even though the feature has
// launched. This is kept as a killswitch because it controls whether we try to
// use the third-party dependency set by `kHashPrefixRealTimeLookupsRelayUrl`.
BASE_DECLARE_FEATURE(kHashPrefixRealTimeLookups);
// This parameter controls the relay URL that will forward the lookup requests
// to the Safe Browsing server.
extern const base::FeatureParam<std::string> kHashPrefixRealTimeLookupsRelayUrl;
// This parameter controls the key fetch URL that will be used to fetch the HPKE
// key that will be used to encrypt the lookup requests.
extern const base::FeatureParam<std::string>
    kHashPrefixRealTimeLookupsKeyFetchUrl;

// Send sample hash-prefix real-time lookups for real-time lookups to catch
// "false positives" where real-time lookup says safe but hash-prefix lookup
// says unsafe.
// Check the design doc (go/sample-esb-ping-send-hprt) for further
// details.
BASE_DECLARE_FEATURE(kHashPrefixRealTimeLookupsSamplePing);
// Determines the percentage of ESB lookups that we sample to send a background
// HPRT lookup. The value should be between 0 and 100.
extern const base::FeatureParam<int> kHashPrefixRealTimeLookupsSampleRate;

// If enabled, fetching lists from Safe Browsing and performing checks on those
// lists uses the v5 APIs instead of the v4 Update API. There is no change to
// how often the checks are triggered (they are still not in real time).
BASE_DECLARE_FEATURE(kLocalListsUseSBv5);

#if BUILDFLAG(IS_ANDROID)
// Enables ClientDownloadRequests for APK downloads on Android.
BASE_DECLARE_FEATURE(kMaliciousApkDownloadCheck);
// Sampling percentage for ClientDownloadRequests for APK downloads on Android.
// If this parameter is N, then a given (supported) download has a N% chance of
// sending a ClientDownloadRequest. The value should be between 0 and 100, and
// defaults to 100 (i.e. no downsampling).
BASE_DECLARE_FEATURE_PARAM(int, kMaliciousApkDownloadCheckSamplePercentage);
// Allows a fieldtrial config to override the APK download check service URL. If
// empty (default), the default hardcoded URL will be used.
extern const base::FeatureParam<std::string>
    kMaliciousApkDownloadCheckServiceUrlOverride;
// If true, then ClientDownloadRequests for APK downloads on Android are
// telemetry-only, and only for Enhanced Protection users. If false (default),
// then ClientDownloadRequests for APK downloads on Android are active for all
// Safe Browsing-enabled users, and may show warnings.
BASE_DECLARE_FEATURE_PARAM(bool, kMaliciousApkDownloadCheckTelemetryOnly);
#endif

// TODO(crbug.com/449960661): Remove this flag once the MigrateAccountPrefs
// feature is launched and the regression of users with ESB enhanced protection
// is resolved.
//  When enabled, this feature fixes a flaw in the Tailored Security service's
//  handling of failed requests for the Enhanced Safe Browsing (ESB) setting.
//  Previously, a network error would cause the service to incorrectly assume
//  ESB was disabled. With this fix, the service preserves the last known state
//  of the ESB bit during a failed request, preventing transient errors from
//  disabling user protection.
BASE_DECLARE_FEATURE(kModifiedESBFetchErrorHandling);

// When enabled, the Password Leak detection toggle is moved out from under the
// 'Standard protection' Safe Browsing option to the top-level 'Privacy and
// security' page.
BASE_DECLARE_FEATURE(kMovePasswordLeakDetectionToggleIos);

// Enable the collection of Notification Telemetry to track potentially abusive
// notifications.
BASE_DECLARE_FEATURE(kNotificationTelemetry);

// Enable the collection of ServiceWorkerBehaviors via the
// NotificationTelemetryService.
BASE_DECLARE_FEATURE(kNotificationTelemetrySwb);
// Specifies the polling interval in minutes.
extern const base::FeatureParam<int> kNotificationTelemetrySwbPollingInterval;
// Determines whether CSBRRs are sent to Safe Browsing.
extern const base::FeatureParam<bool> kNotificationTelemetrySwbSendReports;

// Enables HaTS surveys for users encountering red warnings.
BASE_DECLARE_FEATURE(kRedWarningSurvey);
// Specifies whether we want to show HaTS surveys based on if the user bypassed
// the warning or not. Note: specifying any combination of TRUE and FALSE
// corresponds to "don't care."
extern const base::FeatureParam<std::string> kRedWarningSurveyDidProceedFilter;
// Specifies which CSBRR report types (and thus, red warning types) we want to
// show HaTS surveys for.
extern const base::FeatureParam<std::string> kRedWarningSurveyReportTypeFilter;
// Specifies the HaTS survey's identifier.
extern const base::FeatureParam<std::string> kRedWarningSurveyTriggerId;

// If enabled, advanced protection program users are shown relaunch to apply
// update required.
BASE_DECLARE_FEATURE(kRelaunchNotificationForAdvancedProtection);

// Enables reporting notification contents and metadata to the server, upon user
// consent.
BASE_DECLARE_FEATURE(kReportNotificationContentDetectionData);
// Determines how often we should log the reported notification to the server.
// For the default rate of 100, the notification will always be reported where a
// rate of 0 means there is no reporting. This will help limit data volume, if
// it becomes excessive.
extern const base::FeatureParam<int>
    kReportNotificationContentDetectionDataRate;

// Enables client side phishing daily reports limit to be configured via Finch
// for ESB and SBER users
BASE_DECLARE_FEATURE(kSafeBrowsingDailyPhishingReportsLimit);
// Specifies the CSD-Phishing daily reports limit for ESB users
extern const base::FeatureParam<int> kSafeBrowsingDailyPhishingReportsLimitESB;

#if BUILDFLAG(IS_ANDROID)
// Enables sync checker to check allowlist first on Chrome on Android. This is
// an optimization to improve the speed of Safe Browsing checks.
// See go/skip-sync-hpd-allowlist-android for details.
BASE_DECLARE_FEATURE(kSafeBrowsingSyncCheckerCheckAllowlist);
#endif

// Enables saving gaia password hash from the Profile Picker sign-in flow.
BASE_DECLARE_FEATURE(kSavePasswordHashFromProfilePicker);

// Enables showing manual notification auto-revocations in Safety Hub, allowing
// users to review and manage the revoked permissions.
BASE_DECLARE_FEATURE(kShowManualNotificationRevocationsSafetyHub);

// Enables replacing notification contents with a Chrome warning when the
// on-device model returns a sufficiently suspicious verdict.
BASE_DECLARE_FEATURE(kShowWarningsForSuspiciousNotifications);
// Determines the minimum "suspicious" score returned from the notification
// content LiteRT model that warrants showing a warning. If the score is higher
// than this threshold, then the notification contents will be replaced with a
// warning. By default, no notifications will be replaced by a warning.
extern const base::FeatureParam<int>
    kShowWarningsForSuspiciousNotificationsScoreThreshold;
// The default button order when showing notification warnings is that the
// "Show notification" and "Always allow" buttons are secondary buttons and
// "Unsubscribe" is the primary button. If this parameter is true, the order of
// the buttons should be swapped where "Unsubscribe" is the secondary button.
extern const base::FeatureParam<bool>
    kShowWarningsForSuspiciousNotificationsShouldSwapButtons;

// Controls the daily quota for the suspicious site trigger.
BASE_DECLARE_FEATURE(kSuspiciousSiteTriggerQuotaFeature);

// Controls whether the integration of tailored security settings is enabled.
BASE_DECLARE_FEATURE(kTailoredSecurityIntegration);

// Specifies which non-resource HTML Elements to collect based on their tag and
// attributes. It's a single param containing a comma-separated list of pairs.
// For example: "tag1,id,tag1,height,tag2,foo" - this will collect elements with
// tag "tag1" that have attribute "id" or "height" set, and elements of tag
// "tag2" if they have attribute "foo" set. All tag names and attributes should
// be lower case.
BASE_DECLARE_FEATURE(kThreatDomDetailsTagAndAttributeFeature);

// Controls the behavior of visual features in CSD pings. This feature is
// checked for the final size of the visual features and the minimum size of
// the screen.
BASE_DECLARE_FEATURE(kVisualFeaturesSizes);

// keep-sorted end

base::Value::List GetFeatureStatusList();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
