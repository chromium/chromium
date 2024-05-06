// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/shared/browser/page_activation_throttle_delegate.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {
enum class ActivationDecision;
}

namespace fingerprinting_protection_filter {

// Navigation throttle responsible for activating subresource filtering on page
// loads that match the Fingerprinting Protection Filtering criteria.
class FingerprintingProtectionPageActivationThrottle
    : public content::NavigationThrottle {
 public:
  // |delegate| is allowed to be null, in which case the client creating this
  // throttle will not be able to adjust activation decisions made by the
  // throttle.
  FingerprintingProtectionPageActivationThrottle(
      content::NavigationHandle* handle,
      subresource_filter::PageActivationThrottleDelegate* delegate);

  FingerprintingProtectionPageActivationThrottle(
      const FingerprintingProtectionPageActivationThrottle&) = delete;
  FingerprintingProtectionPageActivationThrottle& operator=(
      const FingerprintingProtectionPageActivationThrottle&) = delete;

  ~FingerprintingProtectionPageActivationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  void CheckCurrentUrl();
  virtual void NotifyResult(subresource_filter::ActivationDecision decision);

  void LogMetricsOnChecksComplete(
      subresource_filter::ActivationDecision decision,
      subresource_filter::mojom::ActivationLevel level) const;

  subresource_filter::ActivationDecision GetActivationDecision() const;

  // May be null. If non-null, must outlive this class.
  raw_ptr<subresource_filter::PageActivationThrottleDelegate> delegate_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing fingerprinting blocking checks.
  bool deferring_ = false;

  base::WeakPtrFactory<FingerprintingProtectionPageActivationThrottle>
      weak_ptr_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_
