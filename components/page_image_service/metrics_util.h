// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_METRICS_UTIL_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_METRICS_UTIL_H_

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"

namespace page_image_service {

constexpr char kBackendHistogramName[] = "PageImageService.Backend";
constexpr char kBackendOptimizationGuideResultHistogramName[] =
    "PageImageService.Backend.OptimizationGuide.Result";
constexpr char kBackendSuggestResultHistogramName[] =
    "PageImageService.Backend.Suggest.Result";
constexpr char kConsentStatusHistogramName[] = "PageImageService.ConsentStatus";

// Used in UMA. Must not be renumbered, and must be kept in sync with enums.xml.
enum class PageImageServiceBackend {
  kNoValidBackend = 0,
  kSuggest = 1,
  kOptimizationGuide = 2,
  kMaxValue = kOptimizationGuide,
};

// Used in UMA. Must not be renumbered, and must be kept in sync with enums.xml.
enum class PageImageServiceResult {
  kSuccess = 0,
  kResponseMissing = 1,
  kNoImage = 2,
  kResponseMalformed = 3,
  kMaxValue = kResponseMalformed,
};

// Used in UMA. Must not be renumbered, and must be kept in sync with enums.xml.
enum class PageImageServiceConsentStatus {
  kSuccess = 0,
  kFailure = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut,
};

// Returns a string for each `client_id`. Always returns a non-empty string.
// The returned string doesn't have a period prefixing it.
std::string ClientIdToString(mojom::ClientId client_id);

template <typename T>
void UmaHistogramEnumerationForClient(const std::string& name,
                                      T sample,
                                      mojom::ClientId client_id) {
  // Record it in the unsliced histogram.
  base::UmaHistogramEnumeration(name, sample);

  // Now record it for the sliced by client equivalent.
  std::string client_suffix = ClientIdToString(client_id);
  DCHECK(!client_suffix.empty());
  base::UmaHistogramEnumeration(name + "." + client_suffix, sample);
}

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_METRICS_UTIL_H_
