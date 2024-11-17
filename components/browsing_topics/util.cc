// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/util.h"

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/common/features.h"

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
const char kEpochIntroductionTimeDecisionPrefix[] =
    "TopicsV1_EpochSwitchTimeDecision|";
const char kEpochPhaseOutTimeDecisionPrefix[] =
    "TopicsV1_EpochPhaseOutTimeDecision|";
const char kContextDomainStoragePrefix[] = "TopicsV1_ContextDomainStorage|";
const char kMainFrameHostStoragePrefix[] = "TopicsV1_MainFrameHostStorage|";

uint64_t HmacHash(ReadOnlyHmacKey hmac_key,
                  const std::string& use_case_prefix,
                  const std::string& data) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(hmac_key));

  uint64_t result;
  CHECK(hmac.Sign(use_case_prefix + data,
                  reinterpret_cast<unsigned char*>(&result), sizeof(result)));

  return result;
}

bool g_hmac_key_overridden = false;

browsing_topics::HmacKey& GetHmacKeyOverrideForTesting() {
  static browsing_topics::HmacKey key;
  return key;
}

std::string GetEpochId(base::Time epoch_calculation_time) {
  int64_t time_microseconds =
      epoch_calculation_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  return std::string(reinterpret_cast<const char*>(&time_microseconds),
                     sizeof(time_microseconds));
}

}  // namespace

bool DoesCalculationFailDueToHanging(CalculatorResultStatus status) {
  return status == CalculatorResultStatus::kHangingAfterApiUsageRequested ||
         status == CalculatorResultStatus::kHangingAfterHistoryRequested ||
         status == CalculatorResultStatus::kHangingAfterModelRequested ||
         status == CalculatorResultStatus::kHangingAfterAnnotationRequested;
}

HmacKey GenerateRandomHmacKey() {
  if (g_hmac_key_overridden)
    return GetHmacKeyOverrideForTesting();

  HmacKey result = {};
  base::RandBytes(result);

  return result;
}

uint64_t HashTopDomainForRandomOrTopTopicDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kRandomOrTopTopicDecisionPrefix,
                  GetEpochId(epoch_calculation_time) + top_domain);
}

uint64_t HashTopDomainForRandomTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kRandomTopicIndexDecisionPrefix,
                  GetEpochId(epoch_calculation_time) + top_domain);
}

uint64_t HashTopDomainForTopTopicIndexDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kTopTopicIndexDecisionPrefix,
                  GetEpochId(epoch_calculation_time) + top_domain);
}

uint64_t HashTopDomainForEpochIntroductionTimeDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kEpochIntroductionTimeDecisionPrefix,
                  GetEpochId(epoch_calculation_time) + top_domain);
}

uint64_t HashTopDomainForEpochPhaseOutTimeDecision(
    ReadOnlyHmacKey hmac_key,
    base::Time epoch_calculation_time,
    const std::string& top_domain) {
  return HmacHash(hmac_key, kEpochPhaseOutTimeDecisionPrefix,
                  GetEpochId(epoch_calculation_time) + top_domain);
}

HashedDomain HashContextDomainForStorage(ReadOnlyHmacKey hmac_key,
                                         const std::string& context_domain) {
  return HashedDomain(
      HmacHash(hmac_key, kContextDomainStoragePrefix, context_domain));
}

HashedHost HashMainFrameHostForStorage(const std::string& main_frame_host) {
  int64_t result;
  crypto::SHA256HashString(kMainFrameHostStoragePrefix + main_frame_host,
                           &result, sizeof(result));
  return HashedHost(result);
}

void OverrideHmacKeyForTesting(ReadOnlyHmacKey hmac_key) {
  g_hmac_key_overridden = true;
  base::ranges::copy(hmac_key, GetHmacKeyOverrideForTesting().begin());
}

}  // namespace browsing_topics
