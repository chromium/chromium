// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

// These tests mirror testSimulateTrialAssignments and
// testSimulateTrialAssignmentsWithForcedSalt in the internal crosstalk_test.py.
// Both places hard-code the same expected results, ensuring the chromium
// implementation matches the Python implementation.

namespace variations {
namespace {

using AssignmentCounts = std::map<std::string, std::map<std::string, int>>;

// These studies must match the ones in crosstalk_test.py.
const char* const kDnsStudyName = "DnsProbe-Attempts";
const char* const kInstantStudyName = "InstantExtended";
constexpr uint32_t kMaxLowEntropySize = 8000;

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
AssignmentCounts CountAssignments(bool salt) {
  AssignmentCounts counts;

  for (uint16_t source = 0; source < kMaxLowEntropySize; source++) {
    NormalizedMurmurHashEntropyProvider provider({source, kMaxLowEntropySize});

    double dns_value = provider.GetEntropyForTrial(kDnsStudyName, 0);
    std::string dns_name = CreateDnsStudy(dns_value)->group_name();

    std::string instant_study_name = kInstantStudyName;
    if (salt)
      instant_study_name += "abcdefghijklmnop";
    double instant_value = provider.GetEntropyForTrial(instant_study_name, 0);
    std::string instant_name = CreateInstantStudy(instant_value)->group_name();

    counts[dns_name][instant_name]++;
  }
  return counts;
}

}  // namespace

TEST(SimulateForCrosstalkTest, WithoutSalt) {
  // These must match crosstalk_test.py's testSimulateTrialAssignments.
  AssignmentCounts expected = {
      {
          "default",
          {
              {"Group1", 360},
              {"Control1", 365},
              {"Group2", 355},
              {"Control2", 347},
              {"Group3", 366},
              {"Control3", 354},
              {"DefaultGroup", 5053},
          },
      },
      {
          "1",
          {
              {"Group1", 40},
              {"Control1", 35},
              {"Group2", 45},
              {"Control2", 53},
              {"Group3", 34},
              {"Control3", 46},
              {"DefaultGroup", 547},
          },
      },
  };
  AssignmentCounts actual = CountAssignments(false);
  ASSERT_EQ(expected, actual);
}

TEST(SimulateForCrosstalkTest, WithSalt) {
  // These must match crosstalk_test.py's
  // testSimulateTrialAssignmentsWithForcedSalt.
  AssignmentCounts expected = {
      {
          "default",
          {
              {"Group1", 362},
              {"Control1", 372},
              {"Group2", 365},
              {"Control2", 360},
              {"Group3", 357},
              {"Control3", 355},
              {"DefaultGroup", 5029},
          },
      },
      {
          "1",
          {
              {"Group1", 38},
              {"Control1", 28},
              {"Group2", 35},
              {"Control2", 40},
              {"Group3", 43},
              {"Control3", 45},
              {"DefaultGroup", 571},
          },
      },
  };
  AssignmentCounts actual = CountAssignments(true);
  ASSERT_EQ(expected, actual);
}

}  // namespace variations
