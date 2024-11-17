// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/variations/entropy_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <limits>
#include <memory>
#include <numeric>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// Size of the low entropy source for testing.
const uint32_t kMaxLowEntropySize = 8000;

// Field trial names used in unit tests.
const char* const kTestTrialNames[] = { "TestTrial", "AnotherTestTrial",
                                        "NewTabButton" };

// Computes the Chi-Square statistic for |values| assuming they follow a uniform
// distribution, where each entry has expected value |expected_value|.
//
// The Chi-Square statistic is defined as Sum((O-E)^2/E) where O is the observed
// value and E is the expected value.
double ComputeChiSquare(const std::vector<int>& values,
                        double expected_value) {
  double sum = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    const double delta = values[i] - expected_value;
    sum += (delta * delta) / expected_value;
  }
  return sum;
}

// Computes SHA1-based entropy for the given |trial_name| based on
// |entropy_source|
double GenerateSHA1Entropy(const std::string& entropy_source,
                           const std::string& trial_name) {
  SHA1EntropyProvider sha1_provider(entropy_source);
  return sha1_provider.GetEntropyForTrial(trial_name, 0);
}

// Generates normalized MurmurHash-based entropy for the given |trial_name|
// based on |entropy_source| which must be in the range [0, entropy_max).
double GenerateNormalizedMurmurHashEntropy(ValueInRange entropy_source,
                                           const std::string& trial_name) {
  NormalizedMurmurHashEntropyProvider provider(entropy_source);
  return provider.GetEntropyForTrial(trial_name, 0);
}

// Helper interface for testing used to generate entropy values for a given
// field trial. Unlike EntropyProvider, which keeps the low/high entropy source
// value constant and generates entropy for different trial names, instances
// of TrialEntropyGenerator keep the trial name constant and generate low/high
// entropy source values internally to produce each output entropy value.
class TrialEntropyGenerator {
 public:
  virtual ~TrialEntropyGenerator() = default;
  virtual double GenerateEntropyValue() const = 0;
};

// An TrialEntropyGenerator that uses the SHA1EntropyProvider with the high
// entropy source (random UUID with 128 bits of entropy + 13 additional bits of
// entropy corresponding to a low entropy source).
class SHA1EntropyGenerator : public TrialEntropyGenerator {
 public:
  explicit SHA1EntropyGenerator(const std::string& trial_name)
      : trial_name_(trial_name) {
  }

  SHA1EntropyGenerator(const SHA1EntropyGenerator&) = delete;
  SHA1EntropyGenerator& operator=(const SHA1EntropyGenerator&) = delete;

  ~SHA1EntropyGenerator() override = default;

  double GenerateEntropyValue() const override {
    // Use a random UUID + 13 additional bits of entropy to match how the
    // SHA1EntropyProvider is used in metrics_service.cc.
    const int low_entropy_source =
        static_cast<uint16_t>(base::RandInt(0, kMaxLowEntropySize - 1));
    const std::string high_entropy_source =
        base::Uuid::GenerateRandomV4().AsLowercaseString() +
        base::NumberToString(low_entropy_source);
    return GenerateSHA1Entropy(high_entropy_source, trial_name_);
  }

 private:
  const std::string trial_name_;
};

// An TrialEntropyGenerator that uses the normalized MurmurHash entropy provider
// algorithm, using 13-bit low entropy source values.
class NormalizedMurmurHashEntropyGenerator : public TrialEntropyGenerator {
 public:
  explicit NormalizedMurmurHashEntropyGenerator(const std::string& trial_name)
      : trial_name_(trial_name) {}

  NormalizedMurmurHashEntropyGenerator(
      const NormalizedMurmurHashEntropyGenerator&) = delete;
  NormalizedMurmurHashEntropyGenerator& operator=(
      const NormalizedMurmurHashEntropyGenerator&) = delete;

  ~NormalizedMurmurHashEntropyGenerator() override = default;

  double GenerateEntropyValue() const override {
    return GenerateNormalizedMurmurHashEntropy(
        {
            .value =
                static_cast<uint32_t>(base::RandInt(0, kMaxLowEntropySize - 1)),
            .range = kMaxLowEntropySize,
        },
        trial_name_);
  }

 private:
  const std::string trial_name_;
};

// Tests uniformity of a given |entropy_generator| using the Chi-Square Goodness
// of Fit Test.
void PerformEntropyUniformityTest(
    const std::string& trial_name,
    const TrialEntropyGenerator& entropy_generator) {
  // Number of buckets in the simulated field trials.
  const size_t kBucketCount = 20;
  // Max number of iterations to perform before giving up and failing.
  const size_t kMaxIterationCount = 100000;
  // The number of iterations to perform before each time the statistical
  // significance of the results is checked.
  const size_t kCheckIterationCount = 10000;
  // This is the Chi-Square threshold from the Chi-Square statistic table for
  // 19 degrees of freedom (based on |kBucketCount|) with a 99.9% confidence
  // level. See: http://www.medcalc.org/manual/chi-square-table.php
  const double kChiSquareThreshold = 43.82;

  std::vector<int> distribution(kBucketCount);

  for (size_t i = 1; i <= kMaxIterationCount; ++i) {
    const double entropy_value = entropy_generator.GenerateEntropyValue();
    const size_t bucket = static_cast<size_t>(kBucketCount * entropy_value);
    ASSERT_LT(bucket, kBucketCount);
    distribution[bucket] += 1;

    // After |kCheckIterationCount| iterations, compute the Chi-Square
    // statistic of the distribution. If the resulting statistic is greater
    // than |kChiSquareThreshold|, we can conclude with 99.9% confidence
    // that the observed samples do not follow a uniform distribution.
    //
    // However, since 99.9% would still result in a false negative every
    // 1000 runs of the test, do not treat it as a failure (else the test
    // will be flaky). Instead, perform additional iterations to determine
    // if the distribution will converge, up to |kMaxIterationCount|.
    if ((i % kCheckIterationCount) == 0) {
      const double expected_value_per_bucket =
          static_cast<double>(i) / kBucketCount;
      const double chi_square =
          ComputeChiSquare(distribution, expected_value_per_bucket);
      if (chi_square < kChiSquareThreshold)
        break;

      // If |i == kMaxIterationCount|, the Chi-Square statistic did not
      // converge after |kMaxIterationCount|.
      EXPECT_NE(i, kMaxIterationCount) << "Failed for trial " <<
          trial_name << " with chi_square = " << chi_square <<
          " after " << kMaxIterationCount << " iterations.";
    }
  }
}

}  // namespace

TEST(EntropyProviderTest, UseOneTimeRandomizationSHA1) {
  // Simply asserts that two trials using one-time randomization
  // that have different names, normally generate different results.
  //
  // Note that depending on the one-time random initialization, they
  // _might_ actually give the same result, but we know that given the
  // particular client_id we use for unit tests they won't.
  SHA1EntropyProvider entropy_provider("client_id");
  scoped_refptr<base::FieldTrial> trials[] = {
      base::FieldTrialList::FactoryGetFieldTrial("one", 100, "default",
                                                 entropy_provider),
      base::FieldTrialList::FactoryGetFieldTrial("two", 100, "default",
                                                 entropy_provider),
  };

  for (const scoped_refptr<base::FieldTrial>& trial : trials) {
    for (int j = 0; j < 100; ++j)
      trial->AppendGroup(std::string(), 1);
  }

  // The trials are most likely to give different results since they have
  // different names.
  EXPECT_NE(trials[0]->group_name(), trials[1]->group_name());
}

TEST(EntropyProviderTest, UseOneTimeRandomizationNormalizedMurmurHash) {
  // Simply asserts that two trials using one-time randomization
  // that have different names, normally generate different results.
  //
  // Note that depending on the one-time random initialization, they
  // _might_ actually give the same result, but we know that given
  // the particular low_entropy_source we use for unit tests they won't.
  NormalizedMurmurHashEntropyProvider entropy_provider(
      {1234, kMaxLowEntropySize});
  scoped_refptr<base::FieldTrial> trials[] = {
      base::FieldTrialList::FactoryGetFieldTrial("one", 100, "default",
                                                 entropy_provider),
      base::FieldTrialList::FactoryGetFieldTrial("two", 100, "default",
                                                 entropy_provider),
  };

  for (const scoped_refptr<base::FieldTrial>& trial : trials) {
    for (int j = 0; j < 100; ++j)
      trial->AppendGroup(std::string(), 1);
  }

  // The trials are most likely to give different results since they have
  // different names.
  EXPECT_NE(trials[0]->group_name(), trials[1]->group_name());
}

TEST(EntropyProviderTest, UseOneTimeRandomizationWithCustomSeedSHA1) {
  // Ensures that two trials with different names but the same custom seed used
  // for one time randomization produce the same group assignments.
  SHA1EntropyProvider entropy_provider("client_id");
  const uint32_t kCustomSeed = 9001;
  scoped_refptr<base::FieldTrial> trials[] = {
      base::FieldTrialList::FactoryGetFieldTrial("one", 100, "default",
                                                 entropy_provider, kCustomSeed),
      base::FieldTrialList::FactoryGetFieldTrial("two", 100, "default",
                                                 entropy_provider, kCustomSeed),
  };

  for (const scoped_refptr<base::FieldTrial>& trial : trials) {
    for (int j = 0; j < 100; ++j)
      trial->AppendGroup(std::string(), 1);
  }

  // Normally, these trials should produce different groups, but if the same
  // custom seed is used, they should produce the same group assignment.
  EXPECT_EQ(trials[0]->group_name(), trials[1]->group_name());
}

TEST(EntropyProviderTest,
     UseOneTimeRandomizationWithCustomSeedNormalizedMurmurHash) {
  // Ensures that two trials with different names but the same custom seed used
  // for one time randomization produce the same group assignments.
  NormalizedMurmurHashEntropyProvider entropy_provider(
      {1234, kMaxLowEntropySize});
  const uint32_t kCustomSeed = 9001;
  scoped_refptr<base::FieldTrial> trials[] = {
      base::FieldTrialList::FactoryGetFieldTrial("one", 100, "default",
                                                 entropy_provider, kCustomSeed),
      base::FieldTrialList::FactoryGetFieldTrial("two", 100, "default",
                                                 entropy_provider, kCustomSeed),
  };

  for (const scoped_refptr<base::FieldTrial>& trial : trials) {
    for (int j = 0; j < 100; ++j)
      trial->AppendGroup(std::string(), 1);
  }

  // Normally, these trials should produce different groups, but if the same
  // custom seed is used, they should produce the same group assignment.
  EXPECT_EQ(trials[0]->group_name(), trials[1]->group_name());
}

TEST(EntropyProviderTest, SHA1Entropy) {
  const double results[] = { GenerateSHA1Entropy("hi", "1"),
                             GenerateSHA1Entropy("there", "1") };

  EXPECT_NE(results[0], results[1]);
  for (double result : results) {
    EXPECT_LE(0.0, result);
    EXPECT_GT(1.0, result);
  }

  EXPECT_EQ(GenerateSHA1Entropy("yo", "1"),
            GenerateSHA1Entropy("yo", "1"));
  EXPECT_NE(GenerateSHA1Entropy("yo", "something"),
            GenerateSHA1Entropy("yo", "else"));
}

TEST(EntropyProviderTest, NormalizedMurmurHashEntropy) {
  const double results[] = {
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize}, "1"),
      GenerateNormalizedMurmurHashEntropy({4321, kMaxLowEntropySize}, "1")};

  EXPECT_NE(results[0], results[1]);
  for (double result : results) {
    EXPECT_LE(0.0, result);
    EXPECT_GT(1.0, result);
  }

  EXPECT_EQ(
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize}, "1"),
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize}, "1"));
  EXPECT_NE(
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize},
                                          "something"),
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize}, "else"));
}

TEST(EntropyProviderTest, NormalizedMurmurHashEntropyProviderResults) {
  // Verifies that NormalizedMurmurHashEntropyProvider produces expected
  // results. This ensures that the results are the same between platforms and
  // ensures that changes to the implementation do not regress this
  // accidentally.

  EXPECT_DOUBLE_EQ(
      1612 / static_cast<double>(kMaxLowEntropySize),
      GenerateNormalizedMurmurHashEntropy({1234, kMaxLowEntropySize}, "XYZ"));
  EXPECT_DOUBLE_EQ(
      7066 / static_cast<double>(kMaxLowEntropySize),
      GenerateNormalizedMurmurHashEntropy({1, kMaxLowEntropySize}, "Test"));
  EXPECT_DOUBLE_EQ(
      5668 / static_cast<double>(kMaxLowEntropySize),
      GenerateNormalizedMurmurHashEntropy({5000, kMaxLowEntropySize}, "Foo"));
}

TEST(EntropyProviderTest, SHA1EntropyIsUniform) {
  for (const char* name : kTestTrialNames) {
    SHA1EntropyGenerator entropy_generator(name);
    PerformEntropyUniformityTest(name, entropy_generator);
  }
}

TEST(EntropyProviderTest, NormalizedMurmurHashEntropyIsUniform) {
  for (const char* name : kTestTrialNames) {
    NormalizedMurmurHashEntropyGenerator entropy_generator(name);
    PerformEntropyUniformityTest(name, entropy_generator);
  }
}

TEST(EntropyProviderTest, InstantiationWithLimitedEntropyRandomizationSource) {
  const EntropyProviders entropy_providers(
      "client_id", {0, 8000}, "limited_entropy_randomization_source");
  EXPECT_TRUE(entropy_providers.has_limited_entropy());
}

TEST(EntropyProviderTest,
     InstantiationWithLimitedEntropyRandomizationSourceAsEmptyString) {
  const EntropyProviders without_limited_entropy_randomization_source(
      "client_id", {0, 8000},
      /*limited_entropy_randomization_source=*/std::string_view());
  EXPECT_FALSE(
      without_limited_entropy_randomization_source.has_limited_entropy());
}

}  // namespace variations
