// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/util.h"

#include <algorithm>

#include "base/rand_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"

namespace browsing_topics {

namespace {

// Note that updating the use case prefixes below will change the pre-existing
// per-user stickiness. Some of the derived data may already have been persisted
// elsewhere. Be sure you are aware of the implications before updating those
// strings. Note also that the version here is just about the hash method, and
// is distinctive from the broader configuration version of the Topics API.
const char kRandomOrTopTopicDecisionPrefix[] =
    "TopicsV1_RandomOrTopTopicDecision|";
const char kRandomTopicIndexDecisionPrefix[] =
    "TopicsV1_RandomTopicIndexDecision|";
const char kTopTopicIndexDecisionPrefix[] = "TopicsV1_TopTopicIndexDecision|";
const char kEpochSwitchTimeDecisionPrefix[] =
    "TopicsV1_EpochSwitchTimeDecision|";
const char kContextDomainStoragePrefix[] = "TopicsV1_ContextDomainStorage|";
const char kTopHostStoragePrefix[] = "TopicsV1_TopHostStorage|";

uint64_t HmacHash(base::span<const uint8_t, 32> hmac_key,
                  const std::string& use_case_prefix,
                  const std::string& data) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(hmac_key));

  uint64_t result;
  CHECK(hmac.Sign(use_case_prefix + data,
                  reinterpret_cast<unsigned char*>(&result), sizeof(result)));

  return result;
}

}  // namespace

std::array<uint8_t, 32> GenerateRandomHmacKey() {
  std::array<uint8_t, 32> result = {};
  base::RandBytes(result.data(), result.size());

  return result;
}

uint64_t HashTopDomainForRandomOrTopTopicDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kRandomOrTopTopicDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForRandomTopicIndexDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kRandomTopicIndexDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForTopTopicIndexDecision(
    base::span<const uint8_t, 32> hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::string epoch_id(reinterpret_cast<const char*>(&time_microseconds),
                       sizeof(time_microseconds));

  return HmacHash(hmac_key, kTopTopicIndexDecisionPrefix,
                  epoch_id + top_domain);
}

uint64_t HashTopDomainForEpochSwitchTimeDecision(
    base::span<const uint8_t, 32> hmac_key,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kEpochSwitchTimeDecisionPrefix, top_domain);
}

HashedDomain HashContextDomainForStorage(base::span<const uint8_t, 32> hmac_key,
                                         const std::string& context_domain) {
  return HashedDomain(
      HmacHash(hmac_key, kContextDomainStoragePrefix, context_domain));
}

HashedHost HashTopHostForStorage(const std::string& top_host) {
  int64_t result;
  crypto::SHA256HashString(kTopHostStoragePrefix + top_host, &result,
                           sizeof(result));
  return HashedHost(result);
}

}  // namespace browsing_topics
