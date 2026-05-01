// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/content_annotator/content_classifier_metrics.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace accessibility_annotator {

uint32_t AddDeterministicNoise(uint32_t true_bucket, std::string_view seed) {
  uint32_t base_hash = base::PersistentHash(seed);

  static_assert(
      kNumBitsForRAPPORMetrics == 4,
      "The following logic relies on having exactly 4 bits in the metrics");

  // Extract 4 decision coins (Bits 0-3); 1 means "keep the bit",
  // 0 means "replace with random noise".
  uint32_t coins = base_hash & 0x0F;

  // Extract 4 noise bits (Bits 4-7).
  uint32_t noise = (base_hash >> 4) & 0x0F;

  // Merge the bits.
  return (true_bucket & coins) | (noise & ~coins);
}

std::string GetOrCreateUkmLoggingUserSecret(PrefService* pref_service) {
  std::string ukm_user_secret =
      pref_service->GetString(prefs::kUkmLoggingUserSecret);
  base::Time creation_time =
      pref_service->GetTime(prefs::kUkmLoggingUserSecretCreationTime);

  if (ukm_user_secret.empty() ||
      (base::Time::Now() - creation_time) >= base::Days(28)) {
    ukm_user_secret = base::UnguessableToken::Create().ToString();
    pref_service->SetString(prefs::kUkmLoggingUserSecret, ukm_user_secret);
    pref_service->SetTime(prefs::kUkmLoggingUserSecretCreationTime,
                          base::Time::Now());
  }
  return ukm_user_secret;
}

void LogSemanticClassificationValueScore(
    double score,
    std::string_view user_secret,
    const GURL& url,
    ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults&
        ukm_builder) {
  if (user_secret.empty()) {
    return;
  }
  double clamped_score = std::clamp(score, 0.0, 1.0);
  uint32_t bucketed_score =
      std::min(static_cast<uint32_t>(
                   std::floor(clamped_score * kNumBucketsForRAPPORMetrics)),
               kNumBucketsForRAPPORMetrics - 1);

  // Implement Permanent Randomized Response (PRR).
  uint32_t noisy_score = AddDeterministicNoise(
      bucketed_score, base::StrCat({user_secret, url.host(), url.path()}));

  ukm_builder.SetSemanticClassificationValueScore(
      static_cast<int64_t>(noisy_score));
}

}  // namespace accessibility_annotator
