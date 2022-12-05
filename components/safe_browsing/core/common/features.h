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

// Killswitch for client side phishing detection. Since client side models are
// run on a large fraction of navigations, crashes due to the model are very
// impactful, even if only a small fraction of users have a bad version of the
// model. This Finch flag allows us to remediate long-tail component versions
// while we fix the root cause.
BASE_DECLARE_FEATURE(kClientSideDetectionKillswitch);

// The client side detection model is a flatbuffer.
BASE_DECLARE_FEATURE(kClientSideDetectionModelIsFlatBuffer);

// Determines the tag to pass to Omaha to get a client side detection model.
BASE_DECLARE_FEATURE(kClientSideDetectionModelTag);

// The parameter name used for getting the tag values from client side detection
// features, `kClientSideDetectionModelTag` and
// `kClientSideDetectionModelHighMemoryTag`.
const char kClientSideDetectionTagParamName[] = "reporter_omaha_tag";

// Enables client side detection referrer chain.
BASE_DECLARE_FEATURE(kClientSideDetectionReferrerChain);

// Enables serving the Android Protego allowlist through the component updater.
BASE_DECLARE_FEATURE(kComponentUpdaterAndroidProtegoAllowlist);

// Controls whether an access token is attached to scanning requests triggered
// by enterprise Connectors.
BASE_DECLARE_FEATURE(kConnectorsScanningAccessToken);

// Controls the non-blocking scanning UI for Connectors scanning requests. If
// this is enabled, the downloaded file(s) will be renamed immediately and the
// scanning will take place without UI when the policy is set to "non-blocking"
// instead of just showing an "Open Now" button with the blocking UI.
BASE_DECLARE_FEATURE(kConnectorsScanningReportOnlyUI);

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

// Enables instructional improvements when users are directed to the security
// settings page to enable Enhanced Safe Browsing. Enables the In-page help
// (IPH) Bubble to be shown when the user is referred from an ESB promotion.
// The ESB option will also be collapsed on page load. If not enabled,
// no IPH bubble will appear and the ESB option will be expanded on page load.
BASE_DECLARE_FEATURE(kEsbIphBubbleAndCollapseSettings);

// Enables collection of signals related to extension activity and uploads
// of telemetry reports to SB servers.
BASE_DECLARE_FEATURE(kExtensionTelemetry);

// Enables data collected by the kExtensionTelemetry to be written and read to
// disk. This data will be uploaded for analysis.
BASE_DECLARE_FEATURE(kExtensionTelemetryPersistence);

// Specifies the upload interval for extension telemetry reports.
extern const base::FeatureParam<int> kExtensionTelemetryUploadIntervalSeconds;

// Specifies the number of writes the telemetry service will perform during
// a full upload interval.
extern const base::FeatureParam<int> kExtensionTelemetryWritesPerInterval;

// Enables collection of telemetry signal whenever an extension invokes the
// tabs.executeScript API call.
BASE_DECLARE_FEATURE(kExtensionTelemetryTabsExecuteScriptSignal);

// Enables reporting of remote hosts contacted by extensions in telemetry.
BASE_DECLARE_FEATURE(kExtensionTelemetryReportContactedHosts);

// Enables reporting of remote hosts contacted by extensions via websockets;
BASE_DECLARE_FEATURE(kExtensionTelemetryReportHostsContactedViaWebSocket);

// Enables collection of potential password theft data and uploads
// telemetry reports to SB servers.
BASE_DECLARE_FEATURE(kExtensionTelemetryPotentialPasswordTheft);

// Enables collection of arguments whenever an extension invokes the
// cookies.getAll API call.
BASE_DECLARE_FEATURE(kExtensionTelemetryCookiesGetAllSignal);

// Enables collection of arguments whenever an extension invokes the
// cookies.get API call.
BASE_DECLARE_FEATURE(kExtensionTelemetryCookiesGetSignal);

// Determines the tag to pass to Omaha to get a file type policy.
BASE_DECLARE_FEATURE(kFileTypePoliciesTag);

// The parameter name used for getting the tag value from
// `kFileTypePoliciesTag`.
const char kFileTypePoliciesTagParamName[] = "policy_omaha_tag";

// Enable logging of the account enhanced protection setting in Protego pings.
BASE_DECLARE_FEATURE(kLogAccountEnhancedProtectionStateInProtegoPings);

// If enabled, the Safe Browsing database will be stored in a separate file and
// mapped into memory.
BASE_DECLARE_FEATURE(kMmapSafeBrowsingDatabase);

// Enables unpacking of nested archives during downloads.
BASE_DECLARE_FEATURE(kNestedArchives);

// Enable omitting non-user gesture from referrer chain.
BASE_DECLARE_FEATURE(kOmitNonUserGesturesFromReferrerChain);

// Controls whether we are using admin rules for filtering URLs, showing warn or
// block intersitial and reporting the interstitial shown event on enterprise
// managed browsers.
BASE_DECLARE_FEATURE(kRealTimeUrlFilteringForEnterprise);

// Bypass RealTime URL Lookup allowlist for enterprise users.
BASE_DECLARE_FEATURE(kRealTimeUrlLookupForEnterpriseAllowlistBypass);

// Controls whether download Client Safe Browsing Reports are sent under the
// new triggers
BASE_DECLARE_FEATURE(kSafeBrowsingCsbrrNewDownloadTrigger);

// Controls whether Client Safe Browsing Reports are sent with a GAIA-tied token
// for Enhanced Safe Browsing users
BASE_DECLARE_FEATURE(kSafeBrowsingCsbrrWithToken);

// Controls whether we are disabling consumer download checks for users using
// the enterprise download checks.
BASE_DECLARE_FEATURE(kSafeBrowsingDisableConsumerCsdForEnterprise);

// Controls whether we are performing enterprise download checks for users
// with the appropriate policies enabled.
BASE_DECLARE_FEATURE(kSafeBrowsingEnterpriseCsd);

// Controls whether cookies are removed when the access token is present.
BASE_DECLARE_FEATURE(kSafeBrowsingRemoveCookiesInAuthRequests);

// Controls whether the new 7z evaluation is performed on downloads.
BASE_DECLARE_FEATURE(kSevenZipEvaluationEnabled);

// Status of the SimplifiedUrlDisplay experiments. This does not control the
// individual experiments, those are controlled by their own feature flags.
// The feature is only set by Finch so that we can differentiate between
// default and control groups of the experiment.
BASE_DECLARE_FEATURE(kSimplifiedUrlDisplay);

// Controls the daily quota for the suspicious site trigger.
BASE_DECLARE_FEATURE(kSuspiciousSiteTriggerQuotaFeature);

// Controls whether to automatically enable Enhanced Protection for desktop
// tailored security users. If not enabled, users of tailored security are
// notified that they can enable Enhanced Protection through an operating system
// notification.
BASE_DECLARE_FEATURE(kTailoredSecurityDesktopNotice);

// Controls whether the integration of tailored security settings is enabled.
BASE_DECLARE_FEATURE(kTailoredSecurityIntegration);

// Specifies which non-resource HTML Elements to collect based on their tag and
// attributes. It's a single param containing a comma-separated list of pairs.
// For example: "tag1,id,tag1,height,tag2,foo" - this will collect elements with
// tag "tag1" that have attribute "id" or "height" set, and elements of tag
// "tag2" if they have attribute "foo" set. All tag names and attributes should
// be lower case.
BASE_DECLARE_FEATURE(kThreatDomDetailsTagAndAttributeFeature);

// Controls whether we send visual features in password reuse pings.
BASE_DECLARE_FEATURE(kVisualFeaturesForReusePings);

// Controls the behavior of visual features in CSD pings. This feature is
// checked for the final size of the visual features and the minimum size of
// the screen.
BASE_DECLARE_FEATURE(kVisualFeaturesSizes);

base::Value::List GetFeatureStatusList();

// Returns the tag used for Client Side Phishing Detection models, as
// computed from the current feature flags.
std::string GetClientSideDetectionTag();

// Returns the tag used for file type policies, as computed from the current
// feature flag.
std::string GetFileTypePoliciesTag();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
