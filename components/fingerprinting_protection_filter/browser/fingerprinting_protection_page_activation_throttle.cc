// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include "base/feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"

using subresource_filter::ActivationDecision;
using subresource_filter::mojom::ActivationLevel;
using subresource_filter::mojom::ActivationState;

namespace fingerprinting_protection_filter {

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
  if (GetActivationDecision() == ActivationDecision::ACTIVATED) {
    // TODO(crbug/327005578): Defer to consult UX.
    NotifyResult();
  }
  return PROCEED;
}

const char*
FingerprintingProtectionPageActivationThrottle::GetNameForLogging() {
  return "FingerprintingProtectionPageActivationThrottle";
}

ActivationDecision
FingerprintingProtectionPageActivationThrottle::GetActivationDecision() const {
  if (base::FeatureList::IsEnabled(
          features::kEnableFingerprintingProtectionFilter)) {
    return ActivationDecision::ACTIVATED;
  }
  return ActivationDecision::UNKNOWN;
}

void FingerprintingProtectionPageActivationThrottle::NotifyResult() {
  // TODO(crbug/327005578): Notify UX of the activation decision made.
  ActivationState state;
  state.activation_level = ActivationLevel::kEnabled;
  FingerprintingProtectionWebContentsHelper::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifyPageActivationComputed(navigation_handle(), state);
}

void FingerprintingProtectionPageActivationThrottle::LogMetricsOnChecksComplete(
    ActivationDecision decision,
    ActivationLevel level) const {
  // TODO(crbug/327005578): Log UKM and UMA metrics.
}

}  // namespace fingerprinting_protection_filter
