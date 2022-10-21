// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// Creates a seed with a variety of permanent consistency studies, with many
// groups to ensure that they are sensitive to any changes in the group
// assignment algorithms, which should not change between releases.
std::unique_ptr<VariationsSeed> ConstructSeed() {
  auto seed = std::make_unique<VariationsSeed>();
  {
    Study* study = seed->add_study();
    study->set_name("Study_NoSalt");
    study->set_consistency(Study_Consistency_PERMANENT);
    for (int i = 0; i < 100; i++) {
      Study::Experiment* experiment = study->add_experiment();
      experiment->set_name(base::StringPrintf("group%02d", i));
      experiment->set_probability_weight(1);
    }
  }

  {
    Study* study = seed->add_study();
    study->set_name("Study_WithSalt");
    study->set_randomization_seed(0x1234);
    study->set_consistency(Study_Consistency_PERMANENT);
    for (int i = 0; i < 100; i++) {
      Study::Experiment* experiment = study->add_experiment();
      experiment->set_name(base::StringPrintf("group%02d", i));
      experiment->set_probability_weight(1);
    }
  }

  {
    Study* study = seed->add_study();
    study->set_name("Study_NoSalt_LowEntropy");
    study->set_consistency(Study_Consistency_PERMANENT);
    for (int i = 0; i < 100; i++) {
      Study::Experiment* experiment = study->add_experiment();
      experiment->set_name(base::StringPrintf("group%02d", i));
      experiment->set_google_web_experiment_id(i);
      experiment->set_probability_weight(1);
    }
  }

  {
    Study* study = seed->add_study();
    study->set_name("Study_WithSalt_LowEntropy");
    study->set_randomization_seed(0x1234);
    study->set_consistency(Study_Consistency_PERMANENT);
    for (int i = 0; i < 100; i++) {
      Study::Experiment* experiment = study->add_experiment();
      experiment->set_name(base::StringPrintf("group%02d", i));
      experiment->set_google_web_experiment_id(i);
      experiment->set_probability_weight(1);
    }
  }
  return seed;
}

void ProcessSeed(EntropyProviders&& entropy_providers) {
  auto seed = ConstructSeed();
  auto client_state = CreateDummyClientFilterableState();
  base::FeatureList feature_list;
  VariationsSeedProcessor().CreateTrialsFromSeed(
      *seed, *client_state, base::BindRepeating(NoopUIStringOverrideCallback),
      entropy_providers, &feature_list);
}

}  // namespace

// The following tests differ only by the input entropy providers, and they
// each check that the group assignments for the permanent consistency studies
// match the values from when the test was written. New changes should not
// break consistency with these assignments.
// The HighEntropyNil tests pass an empty high entropy, which simulates the
// behavior clients that disable high entropy randomization, such as clients
// not opted in to UMA, and clients on platforms like webview where high entropy
// randomization is not supported.

TEST(VariationsAntishuffleTest, HighEntropyNil_LowEntropy0) {
  ProcessSeed(EntropyProviders("", {0, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group87");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group22");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyId0_LowEntropy0) {
  ProcessSeed(EntropyProviders("clientid_0", {0, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group64");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group15");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyId1_LowEntropy0) {
  ProcessSeed(EntropyProviders("clientid_1", {0, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group02");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group40");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyNil_LowEntropy7999) {
  ProcessSeed(EntropyProviders("", {7999, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group43");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group48");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

TEST(VariationsAntishuffleTest, HighEntropyId0_LowEntropy7999) {
  ProcessSeed(EntropyProviders("clientid_0", {7999, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group64");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group15");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

TEST(VariationsAntishuffleTest, HighEntropyId1_LowEntropy7999) {
  ProcessSeed(EntropyProviders("clientid_1", {7999, 8000}));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group02");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group40");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

}  // namespace variations
