// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/util.h"

#include "base/bits.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_topics {

namespace {

std::string GenerateRandomUniformString(int min_length, int max_length) {
  DCHECK_GT(min_length, 0);
  DCHECK_LT(max_length, 30);
  DCHECK_LE(min_length, max_length);

  int min_bound = (1 << min_length);
  int max_bound = (1 << (max_length + 1)) - 1;

  int random_length =
      base::bits::Log2Floor(base::RandInt(min_bound, max_bound));

  return base::RandBytesAsString(random_length);
}

std::string GenerateRandomDomainOrHost() {
  return GenerateRandomUniformString(/*min_length=*/3, /*max_length=*/20);
}

// Check for some properties of uniform random distribution:
// - The average of values is 0.5 * UINT64_MAX.
// - 5% of values mod 100 is less than 5.
// - Chances of each bit to be 0 and 1 are equally likely.
void CheckUniformRandom(base::RepeatingCallback<uint64_t()> uint64_generator) {
  constexpr double kExpectedAverage =
      static_cast<double>(std::numeric_limits<uint64_t>::max()) * 0.5;

  constexpr int kRepeatTimes = 1000000;
  constexpr double kExpectedBitSum = kRepeatTimes * 0.5;
  constexpr double kExpectedCountModHundredBelowFive = kRepeatTimes * 0.05;

  constexpr double kToleranceMultipler = 0.02;

  int bit_sum[64] = {0};
  int count_mod_hundred_below_five = 0;

  double cumulative_average = 0.0;

  for (int i = 0; i < kRepeatTimes; ++i) {
    uint64_t random_value = uint64_generator.Run();

    cumulative_average = (i * cumulative_average + random_value) / (i + 1);

    count_mod_hundred_below_five += (random_value % 100 < 5);

    for (int j = 0; j < 64; ++j) {
      bit_sum[j] += (random_value >> j) & 1;
    }
  }

  ASSERT_LT(kExpectedAverage * (1 - kToleranceMultipler), cumulative_average);
  ASSERT_GT(kExpectedAverage * (1 + kToleranceMultipler), cumulative_average);

  ASSERT_LT(kExpectedCountModHundredBelowFive * (1 - kToleranceMultipler),
            count_mod_hundred_below_five);
  ASSERT_GT(kExpectedCountModHundredBelowFive * (1 + kToleranceMultipler),
            count_mod_hundred_below_five);

  for (int i = 0; i < 64; ++i) {
    ASSERT_LT(kExpectedBitSum * (1 - kToleranceMultipler), bit_sum[i]);
    ASSERT_GT(kExpectedBitSum * (1 + kToleranceMultipler), bit_sum[i]);
  }
}

}  // namespace

class BrowsingTopicsUtilTest : public testing::Test {};

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomOrTopTopicDecision_VariableHmacKey) {
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    HmacKey hmac_key = GenerateRandomHmacKey();

    return HashTopDomainForRandomOrTopTopicDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomOrTopTopicDecision_VariableEpochCalculationTime) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    epoch_calculation_time += base::Days(7);

    return HashTopDomainForRandomOrTopTopicDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomOrTopTopicDecision_VariableTopDomain) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string top_domain = GenerateRandomDomainOrHost();

    return HashTopDomainForRandomOrTopTopicDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForTopTopicIndexDecision_VariableHmacKey) {
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    HmacKey hmac_key = GenerateRandomHmacKey();

    return HashTopDomainForTopTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomTopicIndexDecision_VariableEpochCalculationTime) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    epoch_calculation_time += base::Days(7);

    return HashTopDomainForRandomTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomTopicIndexDecision_VariableTopDomain) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string top_domain = GenerateRandomDomainOrHost();

    return HashTopDomainForRandomTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForRandomTopicIndexDecision_VariableHmacKey) {
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    HmacKey hmac_key = GenerateRandomHmacKey();

    return HashTopDomainForRandomTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForTopTopicIndexDecision_VariableEpochCalculationTime) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  std::string top_domain = GenerateRandomDomainOrHost();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    epoch_calculation_time += base::Days(7);

    return HashTopDomainForTopTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForTopTopicIndexDecision_VariableTopDomain) {
  HmacKey hmac_key = GenerateRandomHmacKey();
  base::Time epoch_calculation_time = base::Time::Now();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string top_domain = GenerateRandomDomainOrHost();

    return HashTopDomainForTopTopicIndexDecision(
        hmac_key, epoch_calculation_time, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForEpochSwitchTimeDecision_VariableHmacKey) {
  std::string top_domain = GenerateRandomDomainOrHost();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    HmacKey hmac_key = GenerateRandomHmacKey();

    return HashTopDomainForEpochSwitchTimeDecision(hmac_key, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashTopDomainForEpochSwitchTimeDecision_VariableContextDomain) {
  HmacKey hmac_key = GenerateRandomHmacKey();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string top_domain = GenerateRandomDomainOrHost();

    return HashTopDomainForEpochSwitchTimeDecision(hmac_key, top_domain);
  }));
}

TEST_F(BrowsingTopicsUtilTest, HashContextDomainForStorage_VariableHmacKey) {
  std::string context_domain = GenerateRandomDomainOrHost();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    HmacKey hmac_key = GenerateRandomHmacKey();

    return static_cast<uint64_t>(
        HashContextDomainForStorage(hmac_key, context_domain).value());
  }));
}

TEST_F(BrowsingTopicsUtilTest,
       HashContextDomainForStorage_VariableContextDomain) {
  HmacKey hmac_key = GenerateRandomHmacKey();

  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string context_domain = GenerateRandomDomainOrHost();

    return static_cast<uint64_t>(
        HashContextDomainForStorage(hmac_key, context_domain).value());
  }));
}

TEST_F(BrowsingTopicsUtilTest, HashMainFrameHostForStorage) {
  CheckUniformRandom(base::BindLambdaForTesting([&]() {
    std::string main_frame_host = GenerateRandomDomainOrHost();

    return static_cast<uint64_t>(
        HashMainFrameHostForStorage(main_frame_host).value());
  }));
}

}  // namespace browsing_topics
