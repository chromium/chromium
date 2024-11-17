// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
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
      tracking_protection_settings_(tracking_protection_settings),
      prefs_(prefs),
      is_incognito_(is_incognito) {}

FingerprintingProtectionPageActivationThrottle::
    ~FingerprintingProtectionPageActivationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
FingerprintingProtectionPageActivationThrottle::WillRedirectRequest() {
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
FingerprintingProtectionPageActivationThrottle::WillProcessResponse() {
  NotifyResult(GetActivation());
  return PROCEED;
}

const char*
FingerprintingProtectionPageActivationThrottle::GetNameForLogging() {
  return kPageActivationThrottleNameForLogging;
}

GetActivationResult
FingerprintingProtectionPageActivationThrottle::GetActivation() const {
  if (!features::IsFingerprintingProtectionFeatureEnabled()) {
    // Feature flag disabled.
    return {.level = ActivationLevel::kDisabled,
            .decision = ActivationDecision::UNKNOWN};
  }

  if (features::kActivationLevel.Get() == ActivationLevel::kDisabled) {
    // Feature flag enabled, but disabled by feature param.
    return {.level = ActivationLevel::kDisabled,
            .decision = ActivationDecision::ACTIVATION_DISABLED};
  }

  if (features::kActivationLevel.Get() == ActivationLevel::kDryRun) {
    // Activated for dry run
    return {.level = ActivationLevel::kDryRun,
            .decision = ActivationDecision::ACTIVATED};
  }
  // At this point, we know that
  // features::IsFingerprintingProtectionFeatureEnabled() and that
  // features::kActivationLevel.Get() is ActivationLevel::kEnabled.

  if (prefs_ != nullptr) {
    // We use prefs::kCookieControlsMode to check third-party cookie blocking
    // rather than TrackingProtectionSettings API because the latter only covers
    // the 3PCD case, whereas the pref covers both the 3PCD case and the case
    // where the user blocks 3PC.
    bool is_3pc_blocked =
        static_cast<content_settings::CookieControlsMode>(
            prefs_->GetInteger(prefs::kCookieControlsMode)) ==
        content_settings::CookieControlsMode::kBlockThirdParty;

    if (features::kEnableOnlyIf3pcBlocked.Get() && !is_3pc_blocked) {
      // FP disabled by only_if_3pc_blocked param.
      return {.level = ActivationLevel::kDisabled,
              .decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET};
    }
  }

  // If we have a reference to TrackingProtectionSettings, use it to check for
  // a URL-level exclusion.
  if (tracking_protection_settings_ != nullptr &&
      tracking_protection_settings_->GetTrackingProtectionSetting(
          navigation_handle()->GetURL()) == CONTENT_SETTING_ALLOW) {
    // FP disabled by a Tracking Protection exception for the current URL.
    return {.level = ActivationLevel::kDisabled,
            .decision = ActivationDecision::URL_ALLOWLISTED};
  }

  // FP enabled
  return {.level = ActivationLevel::kEnabled,
          .decision = ActivationDecision::ACTIVATED};
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
    GetActivationResult activation_result) {
  // The ActivationDecision is only UNKNOWN when the feature flag is disabled.
  if (activation_result.decision == ActivationDecision::UNKNOWN) {
    return;
  }

  // Populate ActivationState.
  ActivationState activation_state;
  activation_state.activation_level = activation_result.level;
  activation_state.measure_performance =
      GetEnablePerformanceMeasurements(is_incognito_);
  activation_state.enable_logging =
      features::IsFingerprintingProtectionConsoleLoggingEnabled();

  NotifyPageActivationComputed(activation_state, activation_result.decision);
  LogMetricsOnChecksComplete(activation_result.decision,
                             activation_result.level);
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
