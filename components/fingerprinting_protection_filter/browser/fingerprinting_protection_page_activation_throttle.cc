// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;

// TODO(https://crbug.com/40280666): This doesn't actually throttle any
// navigations - use a different object to kick off the
// `ProfileInteractionManager`.
FingerprintingProtectionPageActivationThrottle::
    FingerprintingProtectionPageActivationThrottle(
        content::NavigationHandle* handle,
        privacy_sandbox::TrackingProtectionSettings*
            tracking_protection_settings,
        PrefService* prefs)
    : NavigationThrottle(handle),
      profile_interaction_manager_(std::make_unique<ProfileInteractionManager>(
          tracking_protection_settings,
          prefs)) {}

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
  return "FingerprintingProtectionPageActivationThrottle";
}

ActivationDecision
FingerprintingProtectionPageActivationThrottle::GetActivationDecision() const {
  if (!base::FeatureList::IsEnabled(
          features::kEnableFingerprintingProtectionFilter)) {
    return ActivationDecision::UNKNOWN;
  }
  if (fingerprinting_protection_filter::features::kActivationLevel.Get() ==
      ActivationLevel::kDisabled) {
    return ActivationDecision::ACTIVATION_DISABLED;
  }
  // Either enabled or dry_run
  return ActivationDecision::ACTIVATED;
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
  subresource_filter::mojom::ActivationState activation_state;
  activation_state.activation_level = activation_level;
  auto* web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          navigation_handle()->GetWebContents());
  // Making sure the WebContentsHelper exists is outside the scope of this
  // class.
  if (web_contents_helper) {
    web_contents_helper->NotifyPageActivationComputed(navigation_handle(),
                                                      activation_state);
  }

  LogMetricsOnChecksComplete(decision, activation_level);
}

void FingerprintingProtectionPageActivationThrottle::LogMetricsOnChecksComplete(
    ActivationDecision decision,
    ActivationLevel level) const {
  UMA_HISTOGRAM_ENUMERATION(ActivationLevelHistogramName, level);
  UMA_HISTOGRAM_ENUMERATION(ActivationDecisionHistogramName, decision,
                            ActivationDecision::ACTIVATION_DECISION_MAX);

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::FingerprintingProtection builder(source_id);

  builder.SetActivationDecision(static_cast<int64_t>(decision));
  if (level == ActivationLevel::kDryRun) {
    DCHECK_EQ(ActivationDecision::ACTIVATED, decision);
    builder.SetDryRun(true);
  }
  if (decision == ActivationDecision::URL_ALLOWLISTED &&
      profile_interaction_manager_) {
    builder.SetAllowlistSource(static_cast<int64_t>(
        profile_interaction_manager_->GetTrackingProtectionSettingSource(
            navigation_handle()->GetURL())));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace fingerprinting_protection_filter
