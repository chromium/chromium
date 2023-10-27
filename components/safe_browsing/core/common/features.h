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
// Features list, in alphabetical order.

// Controls various parameters related to occasionally collecting ad samples,
// for example to control how often collection should occur.
BASE_DECLARE_FEATURE(kAdSamplerTriggerFeature);

// Enables adding warning shown timestamp to client safe browsing report.
BASE_DECLARE_FEATURE(kAddWarningShownTSToClientSafeBrowsingReport);

// Enables logging new phishing prevention data.
BASE_DECLARE_FEATURE(kAntiPhishingTelemetry);

// Killswitch for client side phishing detection. Since client side models are
// run on a large fraction of navigations, crashes due to the model are very
// impactful, even if only a small fraction of users have a bad version of the
// model. This Finch flag allows us to remediate long-tail component versions
// while we fix the root cause. This will also halt the model distribution from
// OptimizationGuide.
BASE_DECLARE_FEATURE(kClientSideDetectionKillswitch);

// The client side detection model is a flatbuffer.
BASE_DECLARE_FEATURE(kClientSideDetectionModelIsFlatBuffer);

// Determines the tag to pass to Omaha to get a client side detection model.
BASE_DECLARE_FEATURE(kClientSideDetectionModelTag);

// The parameter name used for getting the tag values from client side detection
// features, `kClientSideDetectionModelTag` and
// `kClientSideDetectionModelHighMemoryTag`.
const char kClientSideDetectionTagParamName[] = "reporter_omaha_tag";

// Enables force request CSD-P ping when RT Lookup Response has FORCE_REQUEST in
// the client_side_detection_type field
BASE_DECLARE_FEATURE(kClientSideDetectionTypeForceRequest);

// Controls whether we prompt encrypted archive deep scans to provide a
// password.
BASE_DECLARE_FEATURE(kDeepScanningEncryptedArchives);

// Controls whether the delayed warning experiment is enabled.
BASE_DECLARE_FEATURE(kDelayedWarnings);
// True if mouse clicks should undelay the warnings immediately when delayed
// warnings feature is enabled.
extern const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks;

// Whether to use download bubble instead of download shelf.
BASE_DECLARE_FEATURE(kDownloadBubble);

// The V2 of the download bubble, consisting of features that were not available
// on the download shelf. This is only eligible to be enabled when
// kDownloadBubble is already enabled.
BASE_DECLARE_FEATURE(kDownloadBubbleV2);

// The kill switch for download tailored warnings. The main control is on the
// server-side.
BASE_DECLARE_FEATURE(kDownloadTailoredWarnings);

// Enables decreased Phishguard password length minimum.
BASE_DECLARE_FEATURE(kEvaluateProtectedPasswordLengthMinimum);

// Specifies the minimum password length for password protection.
extern const base::FeatureParam<int>
    kEvaluateProtectedPasswordLengthMinimumValue;

// Allows the Extension Telemetry Service to accept and use configurations
// sent by the server.
BASE_DECLARE_FEATURE(kExtensionTelemetryConfiguration);

// Allows the Extension Telemetry Service to process installed extension files
// and attach file data to reports.
BASE_DECLARE_FEATURE(kExtensionTelemetryFileData);

// Specifies the max number of files to process per extension in Extension
// Telemetry's File Processor.
extern const base::FeatureParam<int>
    kExtensionTelemetryFileDataMaxFilesToProcess;

// Specifies the max file size to process in Extension Telemetry's File
// Processor.
extern const base::FeatureParam<int>
    kExtensionTelemetryFileDataMaxFileSizeBytes;

// Specifies the interval for extension telemetry to collect offstore extension
// file data.
extern const base::FeatureParam<int>
    kExtensionTelemetryFileDataCollectionIntervalSeconds;

// Specifies the initial delay for extension telemetry to start collecting
// offstore extension file data.
extern const base::FeatureParam<int>
    kExtensionTelemetryFileDataStartupDelaySeconds;

// Allows the Extension Telemetry Service to include file data of extensions
// specified in the --load-extension commandline switch in telemetry reports.
BASE_DECLARE_FEATURE(kExtensionTelemetryFileDataForCommandLineExtensions);

// Enables collection of telemetry signal whenever an extension invokes the
// chrome.tabs API methods.
BASE_DECLARE_FEATURE(kExtensionTelemetryTabsApiSignal);

// Enables collection of telemetry signal whenever an extension invokes the
// tabs.executeScript API call.
BASE_DECLARE_FEATURE(kExtensionTelemetryTabsExecuteScriptSignal);

// Enables reporting of remote hosts contacted by extensions in telemetry.
BASE_DECLARE_FEATURE(kExtensionTelemetryReportContactedHosts);

// Enables reporting of remote hosts contacted by extensions via websockets;
BASE_DECLARE_FEATURE(kExtensionTelemetryReportHostsContactedViaWebSocket);

// Enables intercepting remote hosts contacted by extensions in renderer
// throttles.
BASE_DECLARE_FEATURE(
    kExtensionTelemetryInterceptRemoteHostsContactedInRenderer);

// Enables collection of potential password theft data and uploads
// telemetry reports to SB servers.
BASE_DECLARE_FEATURE(kExtensionTelemetryPotentialPasswordTheft);

// Enables remotely disabling of malicious off-store extensions identified in
// Extension Telemetry service reports.
BASE_DECLARE_FEATURE(kExtensionTelemetryDisableOffstoreExtensions);

// Enables the new text, layout, links, and icons on both the privacy guide
// and on the security settings page for the enhanced protection security
// option.
BASE_DECLARE_FEATURE(kFriendlierSafeBrowsingSettingsEnhancedProtection);

// Enables the new text and layout on both the privacy guide and on the
// security settings page for the standard protection security option.
BASE_DECLARE_FEATURE(kFriendlierSafeBrowsingSettingsStandardProtection);

// Sends hash-prefix real-time lookup requests on navigations for Standard Safe
// Browsing users instead of hash-prefix database lookups.
BASE_DECLARE_FEATURE(kHashPrefixRealTimeLookups);

// This parameter controls the relay URL that will forward the lookup requests
// to the Safe Browsing server. This is similar to the
// kHashRealTimeOverOhttpRelayUrl parameter, but it applies to the
// kHashPrefixRealTimeLookups feature.
extern const base::FeatureParam<std::string> kHashPrefixRealTimeLookupsRelayUrl;

// For hash-prefix real-time lookup requests that are triggered by the lookup
// mechanism experiment (see kSafeBrowsingLookupMechanismExperiment), enables
// sending the requests over OHTTP to anonymize the source of the requests.
BASE_DECLARE_FEATURE(kHashRealTimeOverOhttp);

// This parameter controls the relay URL that will forward the lookup requests
// to the Safe Browsing server. This is similar to the
// kHashPrefixRealTimeLookupsRelayUrl parameter, but it applies to the
// kHashRealTimeOverOhttp feature.
extern const base::FeatureParam<std::string> kHashRealTimeOverOhttpRelayUrl;

// UX improvements to download warnings in the download bubble and
// chrome://downloads page, respectively.
BASE_DECLARE_FEATURE(kImprovedDownloadBubbleWarnings);
BASE_DECLARE_FEATURE(kImprovedDownloadPageWarnings);

// Enable logging of the account enhanced protection setting in Protego pings.
BASE_DECLARE_FEATURE(kLogAccountEnhancedProtectionStateInProtegoPings);

// If enabled, the Safe Browsing database will be stored in a separate file and
// mapped into memory.
BASE_DECLARE_FEATURE(kMmapSafeBrowsingDatabase);

// Whether hash prefix lookups are done on a background thread when
// kMmapSafeBrowsingDatabase is enabled.
extern const base::FeatureParam<bool> kMmapSafeBrowsingDatabaseAsync;

// Enables unpacking of nested archives during downloads.
BASE_DECLARE_FEATURE(kNestedArchives);

// Controls whether we are using red interstitial facelift updates.
BASE_DECLARE_FEATURE(kRedInterstitialFacelift);

// Enables modifying key parameters on the navigation event collection used to
// populate referrer chains.
BASE_DECLARE_FEATURE(kReferrerChainParameters);

// The maximum age entry we keep in memory. Older entries are cleaned up. This
// is independent of the maximum age entry we send to Safe Browsing, which is
// fixed for privacy reasons.
extern const base::FeatureParam<int> kReferrerChainEventMaximumAgeSeconds;

// The maximum number of navigation events we keep in memory.
extern const base::FeatureParam<int> kReferrerChainEventMaximumCount;

// Controls whether download Client Safe Browsing Reports are sent under the
// new triggers
BASE_DECLARE_FEATURE(kSafeBrowsingCsbrrNewDownloadTrigger);

// Controls whether the lookup mechanism experiment is enabled, which runs all
// three lookup mechanisms instead of just real-time URL lookups for ESB users.
// The other two lookup mechanisms are run in the background, and the results
// of the three are logged for comparison purposes. This experiment is also
// known as the hash-prefix real-time lookup experiment, since that mechanism is
// the main comparison anchor.
BASE_DECLARE_FEATURE(kSafeBrowsingLookupMechanismExperiment);
// Controls whether the SafeBrowsingLookupMechanismExperiment (AKA HPRT
// experiment) conditionally logs a Client Safe Browsing Report when the
// experiment ends for URL-level validation purposes. This is only relevant
// while the HPRT experiment is running, which is only enabled for ESB users.
extern const base::FeatureParam<bool>
    kUrlLevelValidationForHprtExperimentEnabled;

#if BUILDFLAG(IS_ANDROID)
// Use new GMSCore API for hash database check on browser URLs.
BASE_DECLARE_FEATURE(kSafeBrowsingNewGmsApiForBrowseUrlDatabaseCheck);
#endif

// Run Safe Browsing code on UI thread.
BASE_DECLARE_FEATURE(kSafeBrowsingOnUIThread);

// Enable adding copy/paste navigation to the referrer chain.
BASE_DECLARE_FEATURE(kSafeBrowsingReferrerChainWithCopyPasteNavigation);

// Controls whether cookies are removed when the access token is present.
BASE_DECLARE_FEATURE(kSafeBrowsingRemoveCookiesInAuthRequests);

// Controls whether to skip Safe Browsing checks on images, CSS and font URLs in
// renderer URL loader throttle.
BASE_DECLARE_FEATURE(kSafeBrowsingSkipImageCssFont);

// Controls whether to skip Safe Browsing checks on all subresource URLs in
// renderer and browser URL loader throttles.
BASE_DECLARE_FEATURE(kSafeBrowsingSkipSubresources);

// Controls whether to skip Safe Browsing checks for WebSockets and Web API
// handshakes.
BASE_DECLARE_FEATURE(kSafeBrowsingSkipSubresources2);

// Controls whether the new 7z evaluation is performed on downloads.
BASE_DECLARE_FEATURE(kSevenZipEvaluationEnabled);

// Status of the SimplifiedUrlDisplay experiments. This does not control the
// individual experiments, those are controlled by their own feature flags.
// The feature is only set by Finch so that we can differentiate between
// default and control groups of the experiment.
BASE_DECLARE_FEATURE(kSimplifiedUrlDisplay);

// Controls whether the download inspection timeout is applied over the entire
// request, or just the network communication.
BASE_DECLARE_FEATURE(kStrictDownloadTimeout);

// Specifies the duration of the timeout, in milliseconds.
extern const base::FeatureParam<int> kStrictDownloadTimeoutMilliseconds;

// Controls the daily quota for the suspicious site trigger.
BASE_DECLARE_FEATURE(kSuspiciousSiteTriggerQuotaFeature);

// Enable a retry for the tailored security dialogs when the dialog fails to
// show for a user whose google account has sync turned on. This feature helps
// run the tailored security logic for users where the integration failed in the
// past.
BASE_DECLARE_FEATURE(kTailoredSecurityRetryForSyncUsers);

#if BUILDFLAG(IS_ANDROID)
// Enable an observer-based retry mechanism for the tailored security dialogs.
// When enabled, the tailored security integration will use tab observers to
// retry the tailored security logic when a WebContents becomes available.
BASE_DECLARE_FEATURE(kTailoredSecurityObserverRetries);
#endif

// Controls whether the integration of tailored security settings is enabled.
BASE_DECLARE_FEATURE(kTailoredSecurityIntegration);

// Enable new updated strings and icons for the Tailored Security dialogs.
BASE_DECLARE_FEATURE(kTailoredSecurityUpdatedMessages);

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

base::Value::List GetFeatureStatusList();

// Returns the tag used for Client Side Phishing Detection models, as
// computed from the current feature flags.
std::string GetClientSideDetectionTag();

// Enables new ESB specific threshold fields in Visual TF Lite model files
BASE_DECLARE_FEATURE(kSafeBrowsingPhishingClassificationESBThreshold);

// Enables client side phishing daily reports limit to be configured via Finch
// for ESB and SBER users
BASE_DECLARE_FEATURE(kSafeBrowsingDailyPhishingReportsLimit);

// Specifies the CSD-Phishing daily reports limit for ESB users
extern const base::FeatureParam<int> kSafeBrowsingDailyPhishingReportsLimitESB;

BASE_DECLARE_FEATURE(kClientSideDetectionModelImageEmbedder);

// Enables HaTS surveys for users encountering red warnings.
BASE_DECLARE_FEATURE(kRedWarningSurvey);

// Specifies the HaTS survey's identifier.
extern const base::FeatureParam<std::string> kRedWarningSurveyTriggerId;

// Specifies which CSBRR report types (and thus, red warning types) we want to
// show HaTS surveys for.
extern const base::FeatureParam<std::string> kRedWarningSurveyReportTypeFilter;

// Specifies whether we want to show HaTS surveys based on if the user bypassed
// the warning or not. Note: specifying any combination of TRUE and FALSE
// corresponds to "don't care."
extern const base::FeatureParam<std::string> kRedWarningSurveyDidProceedFilter;

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
