// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;
using ::subresource_filter::mojom::ActivationState;

// TODO(https://crbug.com/40280666): This doesn't actually throttle any
// navigations - use a different object to kick off the
// `ProfileInteractionManager`.
FingerprintingProtectionPageActivationThrottle::
    FingerprintingProtectionPageActivationThrottle(
        content::NavigationHandle* handle,
        privacy_sandbox::TrackingProtectionSettings*
            tracking_protection_settings,
        PrefService* prefs,
        bool is_incognito)
    : NavigationThrottle(handle),
      profile_interaction_manager_(std::make_unique<ProfileInteractionManager>(
          tracking_protection_settings,
          prefs)),
      is_incognito_(is_incognito) {}

FingerprintingProtectionPageActivationThrottle::
    ~FingerprintingProtectionPageActivationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
FingerprintingProtectionPageActivationThrottle::WillRedirectRequest() {
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
FingerprintingProtectionPageActivationThrottle::WillProcessResponse() {
  NotifyResult(GetActivationDecision());
  return PROCEED;
}

const char*
FingerprintingProtectionPageActivationThrottle::GetNameForLogging() {
  return kPageActivationThrottleNameForLogging;
}

ActivationDecision
FingerprintingProtectionPageActivationThrottle::GetActivationDecision() const {
  if (!features::IsFingerprintingProtectionFeatureEnabled()) {
    return ActivationDecision::UNKNOWN;
  }
  if (fingerprinting_protection_filter::features::kActivationLevel.Get() ==
      ActivationLevel::kDisabled) {
    return ActivationDecision::ACTIVATION_DISABLED;
  }
  // Either enabled or dry_run
  return ActivationDecision::ACTIVATED;
}

void FingerprintingProtectionPageActivationThrottle::
    NotifyPageActivationComputed(ActivationState activation_state,
                                 ActivationDecision activation_decision) {
  auto* web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          navigation_handle()->GetWebContents());
  // Making sure the WebContentsHelper exists is outside the scope of this
  // class.
  if (web_contents_helper) {
    web_contents_helper->NotifyPageActivationComputed(
        navigation_handle(), activation_state, activation_decision);
  }
}

void FingerprintingProtectionPageActivationThrottle::NotifyResult(
    ActivationDecision decision) {
  // The ActivationDecision should only be UNKNOWN when the flag is disabled.
  if (decision == ActivationDecision::UNKNOWN) {
    return;
  }
  ActivationLevel activation_level = features::kActivationLevel.Get();
  if (profile_interaction_manager_.get()) {
    activation_level = profile_interaction_manager_->OnPageActivationComputed(
        navigation_handle(), activation_level, &decision);
  }

  // Populate ActivationState.
  ActivationState activation_state;
  activation_state.activation_level = activation_level;
  activation_state.measure_performance =
      GetEnablePerformanceMeasurements(is_incognito_);
  activation_state.enable_logging =
      features::IsFingerprintingProtectionConsoleLoggingEnabled();

  NotifyPageActivationComputed(activation_state, decision);
  LogMetricsOnChecksComplete(decision, activation_level);
}

void FingerprintingProtectionPageActivationThrottle::LogMetricsOnChecksComplete(
    ActivationDecision decision,
    ActivationLevel level) const {
  UMA_HISTOGRAM_ENUMERATION(ActivationLevelHistogramName, level);
  UMA_HISTOGRAM_ENUMERATION(ActivationDecisionHistogramName, decision,
                            ActivationDecision::ACTIVATION_DECISION_MAX);
}

namespace {

bool GetMeasurePerformance(double performance_measurement_rate) {
  return base::ThreadTicks::IsSupported() &&
         (performance_measurement_rate == 1 ||
          base::RandDouble() < performance_measurement_rate);
}

bool MeasurePerformance(bool use_incognito_param) {
  double performance_measurement_rate = GetFieldTrialParamByFeatureAsDouble(
      use_incognito_param
          ? features::kEnableFingerprintingProtectionFilterInIncognito
          : features::kEnableFingerprintingProtectionFilter,
      features::kPerformanceMeasurementRateParam, 0.0);
  return GetMeasurePerformance(performance_measurement_rate);
}

}  // namespace

// Whether we record enhanced performance measurements is dependent on the
// performance measurement rate which may differ between incognito and
// non-incognito modes.
bool FingerprintingProtectionPageActivationThrottle::
    GetEnablePerformanceMeasurements(bool is_incognito) const {
  bool use_incognito_param =
      features::IsFingerprintingProtectionEnabledInIncognito(is_incognito);
  return MeasurePerformance(use_incognito_param);
}

}  // namespace fingerprinting_protection_filter
