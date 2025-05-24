// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_breakage_exception.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::ScopedTimers;
using ::subresource_filter::mojom::ActivationLevel;
using ::subresource_filter::mojom::ActivationState;

// TODO(https://crbug.com/40280666): This doesn't actually throttle any
// navigations - use a different object to kick off the
// `ProfileInteractionManager`.
FingerprintingProtectionPageActivationThrottle::
    FingerprintingProtectionPageActivationThrottle(
        content::NavigationThrottleRegistry& registry,
        HostContentSettingsMap* content_settings,
        privacy_sandbox::TrackingProtectionSettings*
            tracking_protection_settings,
        PrefService* prefs,
        bool is_incognito)
    : NavigationThrottle(registry),
      content_settings_(content_settings),
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

bool FingerprintingProtectionPageActivationThrottle::
    IsFpActivationDeterminedByFeatureFlags(GetActivationResult* result) const {
  // There are two, disjoint ways to gate FPP using flags:
  //
  // 1) `FingerprintingProtectionUx` -- This flag enables the FPP setting in the
  //    Tracking Protection settings UX, and the value of that setting dictates
  //    whether FPP is enabled (unless there's an exception). Currently FPP
  //    can only be enabled this way in Incognito mode. When using this flag,
  //    3pc is assumed to be blocked so the functionality of "enable only if
  //    3pc is blocked" is moot.
  //
  // 2) `EnableFingerprintingProtectionFilter(InIncognito)` -- These flags
  //    enable FPP in regular or Incognito mode, respectively, and also have the
  //    param `activation_level`. When using these flags, we can also use the
  //    param `enable_only_if_3pc_blocked`. These flags will be used for silent
  //    FPP experiments.
  //

  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kFingerprintingProtectionUx)) {
    // Gate path (1).
    if (tracking_protection_settings_ == nullptr) {
      // If the Tracking Protection UX is enabled, we should never see a null
      // TrackingProtectionSettings. If we do, treat it like a disabled flag.
      *result = {.level = ActivationLevel::kDisabled,
                 .decision = ActivationDecision::UNKNOWN};
      return true;
    }
    if (!tracking_protection_settings_->IsFpProtectionEnabled()) {
      // Disabled by TP setting.
      *result = {.level = ActivationLevel::kDisabled,
                 .decision = ActivationDecision::ACTIVATION_DISABLED};
      return true;
    }

    // TP setting enabled, so FPP should be enabled unless the URL has an
    // exception, checked later in `GetActivation()`.
    return false;
  }

  // Gate path (2).
  if (!features::IsFingerprintingProtectionEnabledForIncognitoState(
          is_incognito_)) {
    // Feature flag disabled.
    *result = {.level = ActivationLevel::kDisabled,
               .decision = ActivationDecision::UNKNOWN};
    return true;
  }

  if (features::kActivationLevel.Get() == ActivationLevel::kDisabled) {
    // The `activation_level` feature param can be used to force disable, e.g.
    // for an experiment.
    *result = {.level = ActivationLevel::kDisabled,
               .decision = ActivationDecision::ACTIVATION_DISABLED};
    return true;
  }

  if (features::kActivationLevel.Get() == ActivationLevel::kDryRun) {
    // Dry run => enable FPP, ignoring exceptions.
    *result = {.level = ActivationLevel::kDryRun,
               .decision = ActivationDecision::ACTIVATED};
    return true;
  }

  if (prefs_ != nullptr) {
    // Disable FPP if `enable_only_if_3pc_blocked` is true, and 3pc not blocked.

    // We use prefs::kCookieControlsMode to check third-party cookie blocking
    // rather than TrackingProtectionSettings API because the latter only covers
    // the 3PCD case, whereas the pref covers both the 3PCD case and the case
    // where the user blocks 3PC.
    bool is_3pc_blocked =
        static_cast<content_settings::CookieControlsMode>(
            prefs_->GetInteger(prefs::kCookieControlsMode)) ==
        content_settings::CookieControlsMode::kBlockThirdParty;

    if (features::kEnableOnlyIf3pcBlocked.Get() && !is_3pc_blocked) {
      *result = {.level = ActivationLevel::kDisabled,
                 .decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET};
      return true;
    }
  }

  // FPP enabled by flags, so FPP should be enabled unless the URL has an
  // exception, checked later in `GetActivation()`.
  return false;
}

bool FingerprintingProtectionPageActivationThrottle::
    DoesUrlHaveRefreshHeuristicException() const {
  bool has_breakage_exception = false;
  if (features::IsFingerprintingProtectionRefreshHeuristicExceptionEnabled(
          is_incognito_)) {
    auto has_exception_timer = ScopedTimers::StartIf(
        features::SampleEnablePerformanceMeasurements(is_incognito_),
        [](base::TimeDelta latency_sample) {
          UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
              HasRefreshCountExceptionWallDurationHistogramName, latency_sample,
              base::Microseconds(1), base::Seconds(10), 50);
        });
    has_breakage_exception =
        HasBreakageException(navigation_handle()->GetURL(), *prefs_);
  }
  if (has_breakage_exception) {
    UMA_HISTOGRAM_BOOLEAN(HasRefreshCountExceptionHistogramName, true);
    ukm::SourceId source_id =
        ukm::ConvertToSourceId(navigation_handle()->GetNavigationId(),
                               ukm::SourceIdType::NAVIGATION_ID);
    ukm::builders::FingerprintingProtectionException(source_id)
        .SetSource(static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC))
        .Record(ukm::UkmRecorder::Get());
  }
  return has_breakage_exception;
}

bool FingerprintingProtectionPageActivationThrottle::
    DoesUrlHaveTrackingProtectionException() const {
  // Check for a tracking protection exception. When UB is not available, also
  // check for a COOKIES exception for the top-level site.
  if ((!base::FeatureList::IsEnabled(privacy_sandbox::kActUserBypassUx) &&
       HasContentSettingsCookieException()) ||
      HasTrackingProtectionException()) {
    ukm::SourceId source_id =
        ukm::ConvertToSourceId(navigation_handle()->GetNavigationId(),
                               ukm::SourceIdType::NAVIGATION_ID);
    ExceptionSource exception_source = HasTrackingProtectionException()
                                           ? ExceptionSource::USER_BYPASS
                                           : ExceptionSource::COOKIES;
    ukm::builders::FingerprintingProtectionException(source_id)
        .SetSource(static_cast<int64_t>(exception_source))
        .Record(ukm::UkmRecorder::Get());
    return true;
  }
  return false;
}

GetActivationResult
FingerprintingProtectionPageActivationThrottle::GetActivation() const {
  GetActivationResult activation_based_on_flags;
  if (IsFpActivationDeterminedByFeatureFlags(&activation_based_on_flags)) {
    return activation_based_on_flags;
  }

  if (DoesUrlHaveRefreshHeuristicException()) {
    return {.level = ActivationLevel::kDisabled,
            .decision = ActivationDecision::URL_ALLOWLISTED};
  }

  if (DoesUrlHaveTrackingProtectionException()) {
    return {.level = ActivationLevel::kDisabled,
            .decision = ActivationDecision::URL_ALLOWLISTED};
  }

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
      features::SampleEnablePerformanceMeasurements(is_incognito_);
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

bool ShouldMeasurePerformance(double performance_measurement_rate) {
  return base::ThreadTicks::IsSupported() &&
         (performance_measurement_rate == 1 ||
          base::RandDouble() < performance_measurement_rate);
}

}  // namespace

bool FingerprintingProtectionPageActivationThrottle::
    GetEnablePerformanceMeasurements(bool is_incognito) const {
  // Performance measurement rate may differ between incognito and
  // non-incognito modes.
  double performance_measurement_rate = GetFieldTrialParamByFeatureAsDouble(
      is_incognito ? features::kEnableFingerprintingProtectionFilterInIncognito
                   : features::kEnableFingerprintingProtectionFilter,
      features::kPerformanceMeasurementRateParam, 0.0);
  return ShouldMeasurePerformance(performance_measurement_rate);
}

bool FingerprintingProtectionPageActivationThrottle::
    HasContentSettingsCookieException() const {
  if (content_settings_ == nullptr) {
    return false;
  }
  content_settings::SettingInfo setting_info;
  auto setting = content_settings_->GetContentSetting(
      GURL(), navigation_handle()->GetURL(), ContentSettingsType::COOKIES,
      &setting_info);
  return setting == ContentSetting::CONTENT_SETTING_ALLOW &&
         setting_info.secondary_pattern != ContentSettingsPattern::Wildcard();
}

bool FingerprintingProtectionPageActivationThrottle::
    HasTrackingProtectionException() const {
  return tracking_protection_settings_ != nullptr &&
         tracking_protection_settings_->HasTrackingProtectionException(
             navigation_handle()->GetURL());
}

}  // namespace fingerprinting_protection_filter
