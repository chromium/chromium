// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_
#define COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace private_insights {

// Enables Private Insights.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
BASE_DECLARE_FEATURE(kPrivateInsightsFeature);

// The interval between periodic tasks run by PrivateInsightsService.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta> kPrivateInsightsUploadInterval;

// Enables Private AI Compute error reporting over Private Insights.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
BASE_DECLARE_FEATURE(kPrivateInsightsPaicErrorReporting);

// Enables using Attestation Transparency Verifier in Private Insights.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<bool> kFcpUseAttestationTransparencyVerifier;

// FCP client configuration parameters.

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<std::string> kFcpServerUri;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<std::string> kFcpPopulationNameContextualCues;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta> kFcpConditionPollingPeriod;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<bool> kFcpLogTensorflowErrorMessages;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta>
    kFcpExecutionTeardownGracePeriod;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta>
    kFcpExecutionTeardownExtendedPeriod;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<int> kFcpHttpRetryMaxAttempts;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta> kFcpHttpRetryDelay;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<bool> kFcpDisableHttpRequestBodyCompression;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta>
    kFcpWaitingPeriodForCancellation;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta> kFcpTransientErrorsRetryDelay;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<base::TimeDelta> kFcpPermanentErrorsRetryDelay;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<bool> kFcpEnablePrivacyIdGeneration;

COMPONENT_EXPORT(PRIVATE_INSIGHTS)
extern const base::FeatureParam<int> kMaxContextualCueEvents;

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_
