// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_

#include <stddef.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"

namespace base {
class ListValue;
}  // namespace base

namespace safe_browsing {
// Features list, in alphabetical order.

// Controls whether accuracy tips should be enabled.
extern const base::Feature kAccuracyTipsFeature;

// Controls various parameters related to occasionally collecting ad samples,
// for example to control how often collection should occur.
extern const base::Feature kAdSamplerTriggerFeature;

// Enables including some information in protection requests sent to Safe
// Browsing.
extern const base::Feature kBetterTelemetryAcrossReports;

// The client side detection model is a flatbuffer.
extern const base::Feature kClientSideDetectionModelIsFlatBuffer;

// Determines the tag to pass to Omaha to get a client side detection model.
extern const base::Feature kClientSideDetectionModelTag;

// The parameter name used for getting the tag values from client side detection
// features, `kClientSideDetectionModelTag` and
// `kClientSideDetectionModelHighMemoryTag`.
const char kClientSideDetectionTagParamName[] = "reporter_omaha_tag";

// Enables client side detection referrer chain.
extern const base::Feature kClientSideDetectionReferrerChain;

// Controls whether an access token is attached to scanning requests triggered
// by enterprise Connectors.
extern const base::Feature kConnectorsScanningAccessToken;

// Controls the non-blocking scanning UI for Connectors scanning requests. If
// this is enabled, the downloaded file(s) will be renamed immediately and the
// scanning will take place without UI when the policy is set to "non-blocking"
// instead of just showing an "Open Now" button with the blocking UI.
extern const base::Feature kConnectorsScanningReportOnlyUI;

// Controls whether the delayed warning experiment is enabled.
extern const base::Feature kDelayedWarnings;
// True if mouse clicks should undelay the warnings immediately when delayed
// warnings feature is enabled.
extern const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks;

// Whether to use download bubble instead of download shelf.
extern const base::Feature kDownloadBubble;

// Enables Enhanced Safe Browsing.
extern const base::Feature kEnhancedProtection;

// Enables collection of signals related to extension activity and uploads
// of telemetry reports to SB servers.
extern const base::Feature kExtensionTelemetry;

// Enables data collected by the kExtensionTelemetry to be written and read to
// disk. This data will be uploaded for analysis.
extern const base::Feature kExtensionTelemetryPersistence;

// Specifies the upload interval for extension telemetry reports.
extern const base::FeatureParam<int> kExtensionTelemetryUploadIntervalSeconds;
// Enables collection of telemetry signal whenever an extension invokes the
// tabs.executeScript API call.
extern const base::Feature kExtensionTelemetryTabsExecuteScriptSignal;

// Enables reporting of remote hosts contacted by extensions in telemetry.
extern const base::Feature kExtensionTelemetryReportContactedHosts;

// Determines the tag to pass to Omaha to get a file type policy.
extern const base::Feature kFileTypePoliciesTag;

// The parameter name used for getting the tag value from
// `kFileTypePoliciesTag`.
const char kFileTypePoliciesTagParamName[] = "policy_omaha_tag";

// Enable omitting non-user gesture from referrer chain.
extern const base::Feature kOmitNonUserGesturesFromReferrerChain;

// Controls whether Client Safe Browsing Reports are sent with a GAIA-tied token
// for Enhanced Safe Browsing users
extern const base::Feature kSafeBrowsingCsbrrWithToken;

// Controls whether users will see an account compromise specific warning
// when Safe Browsing determines a file is associated with stealing cookies.
extern const base::Feature kSafeBrowsingCTDownloadWarning;

// Controls whether we are performing enterprise download checks for users
// with the appropriate policies enabled.
extern const base::Feature kSafeBrowsingEnterpriseCsd;

// Controls whether we are disabling consumer download checks for users using
// the enterprise download checks.
extern const base::Feature kSafeBrowsingDisableConsumerCsdForEnterprise;

// Controls whether page load tokens are added to Safe Browsing requests.
extern const base::Feature kSafeBrowsingPageLoadToken;

// Controls whether cookies are removed when the access token is present.
extern const base::Feature kSafeBrowsingRemoveCookiesInAuthRequests;

// Controls the daily quota for the suspicious site trigger.
extern const base::Feature kSuspiciousSiteTriggerQuotaFeature;

// Controls whether to send sample pings of Protego allowlist domains on
// the allowlist to Safe Browsing.
extern const base::Feature kSendSampledPingsForProtegoAllowlistDomains;

// Status of the SimplifiedUrlDisplay experiments. This does not control the
// individual experiments, those are controlled by their own feature flags.
// The feature is only set by Finch so that we can differentiate between
// default and control groups of the experiment.
extern const base::Feature kSimplifiedUrlDisplay;

// Controls whether the integration of tailored security settings is enabled.
extern const base::Feature kTailoredSecurityIntegration;

// Specifies which non-resource HTML Elements to collect based on their tag and
// attributes. It's a single param containing a comma-separated list of pairs.
// For example: "tag1,id,tag1,height,tag2,foo" - this will collect elements with
// tag "tag1" that have attribute "id" or "height" set, and elements of tag
// "tag2" if they have attribute "foo" set. All tag names and attributes should
// be lower case.
extern const base::Feature kThreatDomDetailsTagAndAttributeFeature;

// Controls the daily quota for data collection triggers. It's a single param
// containing a comma-separated list of pairs. The format of the param is
// "T1,Q1,T2,Q2,...Tn,Qn", where Tx is a TriggerType and Qx is how many reports
// that trigger is allowed to send per day.
// TODO(crbug.com/744869): This param should be deprecated after ad sampler
// launch in favour of having a unique quota feature and param per trigger.
// Having a single shared feature makes it impossible to run multiple trigger
// trials simultaneously.
extern const base::Feature kTriggerThrottlerDailyQuotaFeature;

// Controls whether Chrome uses new download warning UX.
extern const base::Feature kUseNewDownloadWarnings;

// Controls the behavior of visual features in CSD pings. This feature is
// checked for the final size of the visual features and the minimum size of
// the screen.
extern const base::Feature kVisualFeaturesSizes;

// Controls whether we send visual features in CSPP pings.
extern const base::Feature kVisualFeaturesInCsppPings;

base::ListValue GetFeatureStatusList();

// Returns the tag used for Client Side Phishing Detection models, as
// computed from the current feature flags.
std::string GetClientSideDetectionTag();

// Returns the tag used for file type policies, as computed from the current
// feature flag.
std::string GetFileTypePoliciesTag();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_FEATURES_H_
