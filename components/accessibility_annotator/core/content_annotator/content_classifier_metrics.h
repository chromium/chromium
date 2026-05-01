// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_METRICS_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_METRICS_H_

#include <string>
#include <string_view>

#include "url/gurl.h"

class PrefService;

namespace ukm::builders {
class AccessibilityAnnotator_ContentAnnotator_ClassifierResults;
}  // namespace ukm::builders

namespace accessibility_annotator {

// Constants for RAPPOR-style noise addition for UKM metrics.
inline constexpr int kNumBitsForRAPPORMetrics = 4;
inline constexpr uint32_t kNumBucketsForRAPPORMetrics = 16;

// Adds deterministic noise to the true bucket for RAPPOR-style metrics,
// based on the provided seed.
uint32_t AddDeterministicNoise(uint32_t true_bucket, std::string_view seed);

// Returns the user secret for UKM logging, creating and storing it in the
// prefs if it does not yet exist or is expired.
std::string GetOrCreateUkmLoggingUserSecret(PrefService* pref_service);

// Logs the noisy semantic classification score to UKM.
void LogSemanticClassificationValueScore(
    double score,
    std::string_view user_secret,
    const GURL& url,
    ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults&
        ukm_builder);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_METRICS_H_
