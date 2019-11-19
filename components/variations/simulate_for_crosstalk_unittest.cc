// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_seed_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

// These tests mirror testSimulateTrialAssignments and
// testSimulateTrialAssignmentsWithForcedSalt in the internal crosstalk_test.py.
// Both places hard-code the same expected results, ensuring the chromium
// implementation matches the Python implementation.

namespace variations {
namespace {

// These studies must match the ones in crosstalk_test.py.
const char* const kDnsStudyName = "DnsProbe-Attempts";
const char* const kInstantStudyName = "InstantExtended";
constexpr int kDnsGroups = 2;
constexpr int kInstantGroups = 7;
constexpr size_t kMaxLowEntropySize = 8000;

scoped_refptr<base::FieldTrial> CreateDnsStudy(double value) {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrial::CreateSimulatedFieldTrial(kDnsStudyName, 100, "default",
                                                  value));
  trial->AppendGroup("1", 10);
  return trial;
}

scoped_refptr<base::FieldTrial> CreateInstantStudy(double value) {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrial::CreateSimulatedFieldTrial(kInstantStudyName, 100,
                                                  "DefaultGroup", value));
  trial->AppendGroup("Group1", 5);
  trial->AppendGroup("Control1", 5);
  trial->AppendGroup("Group2", 5);
  trial->AppendGroup("Control2", 5);
  trial->AppendGroup("Group3", 5);
  trial->AppendGroup("Control3", 5);
  return trial;
}

// Simulate assigning users with every possible low entropy source to each
// study. Populate |counts| such that count[x][y] is the number of users in both
// DnsProbe-Attempts' group x and InstantExtended's group y. If |salt|, add salt
// to InstantExtended's name.
void countAssignments(int (&counts)[kDnsGroups][kInstantGroups], bool salt) {
  // Must pass |counts| by reference, to prevent decay, for sizeof() to work.
  memset(counts, 0, sizeof(counts));

  for (uint16_t source = 0; source < kMaxLowEntropySize; source++) {
    NormalizedMurmurHashEntropyProvider provider(source, kMaxLowEntropySize);

    double dns_value = provider.GetEntropyForTrial(kDnsStudyName, 0);
    int dns_group = CreateDnsStudy(dns_value)->group();
    ASSERT_GE(dns_group, 0);
    ASSERT_LT(dns_group, kDnsGroups);

    std::string instant_study_name = kInstantStudyName;
    if (salt)
      instant_study_name += "abcdefghijklmnop";
    double instant_value = provider.GetEntropyForTrial(instant_study_name, 0);
    int instant_group = CreateInstantStudy(instant_value)->group();
    ASSERT_GE(instant_group, 0);
    ASSERT_LT(instant_group, kInstantGroups);

    counts[dns_group][instant_group]++;
  }
}

}  // namespace

TEST(SimulateForCrosstalkTest, WithoutSalt) {
  // These must match crosstalk_test.py's testSimulateTrialAssignments.
  int expected[kDnsGroups][kInstantGroups] = {
      {5053, 360, 365, 355, 347, 366, 354},
      {547, 40, 35, 45, 53, 34, 46}};
  int actual[kDnsGroups][kInstantGroups];
  countAssignments(actual, false);
  for (int i = 0; i < kDnsGroups; i++) {
    for (int j = 0; j < kInstantGroups; j++) {
      EXPECT_EQ(expected[i][j], actual[i][j])
          << " at groups " << i << " and " << j;
    }
  }
}

TEST(SimulateForCrosstalkTest, WithSalt) {
  // These must match crosstalk_test.py's
  // testSimulateTrialAssignmentsWithForcedSalt.
  int expected[kDnsGroups][kInstantGroups] = {
      {5029, 362, 372, 365, 360, 357, 355},
      {571, 38, 28, 35, 40, 43, 45}};
  int actual[kDnsGroups][kInstantGroups];
  countAssignments(actual, true);
  for (int i = 0; i < kDnsGroups; i++) {
    for (int j = 0; j < kInstantGroups; j++) {
      EXPECT_EQ(expected[i][j], actual[i][j])
          << " at groups " << i << " and " << j;
    }
  }
}

}  // namespace variations
