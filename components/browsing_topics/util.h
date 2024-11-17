// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_UTIL_H_
#define COMPONENTS_BROWSING_TOPICS_UTIL_H_

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/browsing_topics/common/common_types.h"

namespace browsing_topics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CalculatorResultStatus {
  kSuccess = 0,
  kFailurePermissionDenied = 1,
  kFailureApiUsageContextQueryError = 2,
  kFailureAnnotationExecutionError = 3,
  kFailureTaxonomyVersionNotSupportedInBinary = 4,
  kHangingAfterApiUsageRequested = 5,
  kHangingAfterHistoryRequested = 6,
  kHangingAfterModelRequested = 7,
  kHangingAfterAnnotationRequested = 8,
  kTerminated = 9,

  kMaxValue = kTerminated,
};

bool DoesCalculationFailDueToHanging(CalculatorResultStatus status);

using HmacKey = std::array<uint8_t, 32>;
using ReadOnlyHmacKey = base::span<const uint8_t, 32>;

// Generate a 256 bit random hmac key.
HmacKey GenerateRandomHmacKey();

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding whether to return the random or top topic. The `hmac_key` is
// per-use and `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForRandomOrTopTopicDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding which random topic among the full taxonomy should be returned. The
// `hmac_key` is per-use and `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForRandomTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding which top topic to return. The `hmac_key` is per-user and
// `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForTopTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding the epoch introduction time. The `hmac_key` is per-user and
// `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForEpochIntroductionTimeDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding the epoch phase out time. The `hmac_key` is per-user and
// `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForEpochPhaseOutTimeDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user hash of `context_domain` to be stored more efficiently in
// disk and memory. The `hmac_key` is per-user. A per-user hash is necessary to
// prevent a context from learning the topics that don't belong to it via
// collision attack.
HashedDomain HashContextDomainForStorage(ReadOnlyHmacKey hmac_key,
                                         const std::string& context_domain);

// Returns a hash of `main_frame_host` to be stored more efficiently in disk and
// memory.
HashedHost HashMainFrameHostForStorage(const std::string& main_frame_host);

// Override the key to be returned for subsequent invocations of
// `GenerateRandomHmacKey()`.
void OverrideHmacKeyForTesting(ReadOnlyHmacKey hmac_key);

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_UTIL_H_
