// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/subresource_filter/content/shared/browser/page_activation_throttle_delegate.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_throttle.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;

FingerprintingProtectionPageActivationThrottle::
    FingerprintingProtectionPageActivationThrottle(
        content::NavigationHandle* handle,
        subresource_filter::PageActivationThrottleDelegate* delegate)
    : NavigationThrottle(handle), delegate_(delegate) {}

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
  if (delegate_) {
    activation_level = delegate_->OnPageActivationComputed(
        navigation_handle(), activation_level, &decision);
  }
  FingerprintingProtectionWebContentsHelper::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifyPageActivationComputed(navigation_handle(), decision);

  LogMetricsOnChecksComplete(decision, activation_level);
}

void FingerprintingProtectionPageActivationThrottle::LogMetricsOnChecksComplete(
    ActivationDecision decision,
    ActivationLevel level) const {
  // TODO(crbug/327005578): Log UKM metrics.
  UMA_HISTOGRAM_ENUMERATION(ActivationLevelHistogramName, level);
  UMA_HISTOGRAM_ENUMERATION(ActivationDecisionHistogramName, decision,
                            ActivationDecision::ACTIVATION_DECISION_MAX);
}

}  // namespace fingerprinting_protection_filter
