// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_features.h"

#include "base/time/time.h"

namespace private_insights {

BASE_FEATURE(kPrivateInsightsFeature, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPrivateInsightsUploadInterval{
    &kPrivateInsightsFeature, "upload_interval", base::Minutes(30)};

BASE_FEATURE(kPrivateInsightsPaicErrorReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kFcpUseAttestationTransparencyVerifier{
    &kPrivateInsightsFeature, "fcp_use_attestation_transparency_verifier",
    false};

const base::FeatureParam<std::string> kFcpServerUri{&kPrivateInsightsFeature,
                                                    "fcp_server_uri", ""};

const base::FeatureParam<std::string> kFcpPopulationNameContextualCues{
    &kPrivateInsightsFeature, "fcp_population_name_contextual_cues",
    "private_insights/contextual_cues"};

const base::FeatureParam<base::TimeDelta> kFcpConditionPollingPeriod{
    &kPrivateInsightsFeature, "fcp_condition_polling_period",
    base::TimeDelta()};

const base::FeatureParam<bool> kFcpLogTensorflowErrorMessages{
    &kPrivateInsightsFeature, "fcp_log_tensorflow_error_messages", true};

const base::FeatureParam<base::TimeDelta> kFcpExecutionTeardownGracePeriod{
    &kPrivateInsightsFeature, "fcp_execution_teardown_grace_period",
    base::TimeDelta()};

const base::FeatureParam<base::TimeDelta> kFcpExecutionTeardownExtendedPeriod{
    &kPrivateInsightsFeature, "fcp_execution_teardown_extended_period",
    base::TimeDelta()};

const base::FeatureParam<int> kFcpHttpRetryMaxAttempts{
    &kPrivateInsightsFeature, "fcp_http_retry_max_attempts", 3};

const base::FeatureParam<base::TimeDelta> kFcpHttpRetryDelay{
    &kPrivateInsightsFeature, "fcp_http_retry_delay", base::Milliseconds(5000)};

const base::FeatureParam<bool> kFcpDisableHttpRequestBodyCompression{
    &kPrivateInsightsFeature, "fcp_disable_http_request_body_compression",
    false};

const base::FeatureParam<base::TimeDelta> kFcpWaitingPeriodForCancellation{
    &kPrivateInsightsFeature, "fcp_waiting_period_for_cancellation",
    base::Seconds(10)};

const base::FeatureParam<base::TimeDelta> kFcpTransientErrorsRetryDelay{
    &kPrivateInsightsFeature, "fcp_transient_errors_retry_delay",
    base::Minutes(15)};

const base::FeatureParam<base::TimeDelta> kFcpPermanentErrorsRetryDelay{
    &kPrivateInsightsFeature, "fcp_permanent_errors_retry_delay",
    base::Hours(4)};

const base::FeatureParam<bool> kFcpEnablePrivacyIdGeneration{
    &kPrivateInsightsFeature, "fcp_enable_privacy_id_generation", false};

const base::FeatureParam<int> kMaxContextualCueEvents{
    &kPrivateInsightsFeature, "max_contextual_cue_events", 20};

}  // namespace private_insights
