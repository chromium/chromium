// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_UTIL_H_
#define COMPONENTS_BROWSING_TOPICS_UTIL_H_

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/browsing_topics/common/common_types.h"

namespace browsing_topics {

// Generate a 256 bit random hmac key.
std::array<uint8_t, 32> GenerateRandomHmacKey();

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding whether to return the random or top topic. The `hmac_key` is
// per-use and `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForRandomOrTopTopicDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding which random topic among the full taxonomy should be returned. The
// `hmac_key` is per-use and `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForRandomTopicIndexDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user, per-epoch hash of `top_domain` for the purpose of
// deciding which top topic to return. The `hmac_key` is per-user and
// `epoch_calculation_time` represents the epoch.
uint64_t HashTopDomainForTopTopicIndexDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain);

// Returns a per-user hash of `top_domain` for the purpose of deciding the epoch
// switch-over time. The `hmac_key` is per-user.
uint64_t HashTopDomainForEpochSwitchTimeDecision(
    base::span<const uint8_t, 32> hmac_key,
    const std::string& top_domain);

// Returns a per-user hash of `context_domain` to be stored more efficiently in
// disk and memory. The `hmac_key` is per-user. A per-user hash is necessary to
// prevent a context from learning the topics that don't belong to it via
// collision attack.
HashedDomain HashContextDomainForStorage(base::span<const uint8_t, 32> hmac_key,
                                         const std::string& context_domain);

// Returns a hash of `top_host` to be stored more efficiently in disk and
// memory.
HashedHost HashTopHostForStorage(const std::string& top_host);

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_UTIL_H_
