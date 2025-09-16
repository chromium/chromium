// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_THROTTLE_CREATION_RESULT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_THROTTLE_CREATION_RESULT_H_

namespace fingerprinting_protection_filter {

// Used to record whether we created a RendererUrlLoaderThrottle or the reason
// why we skipped creating one.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RendererThrottleCreationResult {
  kCreate = 0,
  kSkipDisabledForCrossSiteSubframe = 1,
  kSkipFrameResource = 2,
  kSkipWorkerThrottle = 3,
  kSkipNoFrameToken = 4,
  kSkipSameSite = 5,
  kSkipNoRuleset = 6,
  kSkipLocalHost = 7,
  kSkipNonHttp = 8,
  kSkipSubresourceType = 9,
  kMaxValue = kSkipSubresourceType,
};

// Used to record the number of requests that redirect.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RendererThrottleRedirects {
  kSameSiteToSameSiteRedirect = 0,
  kSameSiteToCrossSiteRedirect = 1,
  kCrossSiteToSameSiteRedirect = 2,
  kCrossSiteToCrossSiteRedirect = 3,
  kMaxValue = kCrossSiteToCrossSiteRedirect,
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_THROTTLE_CREATION_RESULT_H_
