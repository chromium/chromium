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
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

struct TestStudyConfig {
  std::string_view name;
  bool add_salt = false;
  bool add_google_web_experiment_id = false;
  bool use_limited_entropy = false;
};

const int kLayerId = 1;
const int kLayerMemberId = 101;

const TestStudyConfig kTestStudyConfigs[] = {
    {.name = "Study_NoSalt"},
    {.name = "Study_WithSalt", .add_salt = true},
    {.name = "Study_NoSalt_LowEntropy", .add_google_web_experiment_id = true},
    {.name = "Study_WithSalt_LowEntropy",
     .add_salt = true,
     .add_google_web_experiment_id = true},

    // Limited entropy test configs:
    {.name = "Study_NoSalt_LimitedEntropy", .use_limited_entropy = true},
    {.name = "Study_WithSalt_LimitedEntropy",
     .add_salt = true,
     .use_limited_entropy = true},
    {.name = "Study_NoSalt_WithGoogleId_LimitedEntropy",
     .add_google_web_experiment_id = true,
     .use_limited_entropy = true},
    {.name = "Study_WithSalt_WithGoogleId_LimitedEntropy",
     .add_salt = true,
     .add_google_web_experiment_id = true,
     .use_limited_entropy = true},
};

// Adds a permanently consistent study with many groups to the seed. This
// ensures that they are sensitive to any changes in the group assignment
// algorithms, which should not change between releases. The function also
// applies any settings from `config`.
void SetupStudy(VariationsSeed* seed, const TestStudyConfig& config) {
  Study* study = seed->add_study();
  study->set_name(std::string(config.name));
  study->set_consistency(Study_Consistency_PERMANENT);
  if (config.add_salt) {
    study->set_randomization_seed(0x1234);
  }

  for (int i = 0; i < 100; i++) {
    Study::Experiment* experiment = study->add_experiment();
    experiment->set_name(base::StringPrintf("group%02d", i));
    experiment->set_probability_weight(1);
    if (config.add_google_web_experiment_id) {
      experiment->set_google_web_experiment_id(i);
    }
  }

  if (config.use_limited_entropy) {
    LayerMemberReference* reference = study->mutable_layer();
    reference->set_layer_id(kLayerId);
    reference->add_layer_member_ids(kLayerMemberId);
  }
}

void SetupLimitedLayer(VariationsSeed* seed) {
  Layer* layer = seed->add_layers();
  layer->set_id(1);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  Layer_LayerMember* layer_member = layer->add_members();
  layer_member->set_id(101);
  Layer_LayerMember_SlotRange* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);
}

void SetupSeed(VariationsSeed* seed) {
  bool add_limited_layer = false;
  for (const TestStudyConfig& config : kTestStudyConfigs) {
    SetupStudy(seed, config);
    add_limited_layer |= config.use_limited_entropy;
  }
  // This ensures there is only one limited layer in the seed. It is referenced
  // by all test studies.
  if (add_limited_layer) {
    SetupLimitedLayer(seed);
  }
}

void ProcessSeed(EntropyProviders&& entropy_providers) {
  VariationsSeed seed;
  SetupSeed(&seed);
  auto client_state = CreateDummyClientFilterableState();
  base::FeatureList feature_list;
  VariationsLayers layers(seed, entropy_providers);
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_state, base::BindRepeating(NoopUIStringOverrideCallback),
      entropy_providers, layers, &feature_list);
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
  ProcessSeed(EntropyProviders("", {0, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group87");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group22");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyId0_LowEntropy0) {
  ProcessSeed(EntropyProviders("clientid_0", {0, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group64");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group15");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyId1_LowEntropy0) {
  ProcessSeed(EntropyProviders("clientid_1", {0, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group02");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group40");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group10");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group22");
}

TEST(VariationsAntishuffleTest, HighEntropyNil_LowEntropy7999) {
  ProcessSeed(EntropyProviders("", {7999, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group43");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group48");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

TEST(VariationsAntishuffleTest, HighEntropyId0_LowEntropy7999) {
  ProcessSeed(EntropyProviders("clientid_0", {7999, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group64");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group15");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

TEST(VariationsAntishuffleTest, HighEntropyId1_LowEntropy7999) {
  ProcessSeed(EntropyProviders("clientid_1", {7999, 8000},
                               /*limited_entropy_value=*/"not_used"));
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt"), "group02");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt"), "group40");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_NoSalt_LowEntropy"),
            "group97");
  EXPECT_EQ(base::FieldTrialList::FindFullName("Study_WithSalt_LowEntropy"),
            "group48");
}

TEST(VariationsAntishuffleTest, LimitedEntropyNil) {
  ProcessSeed(EntropyProviders("not_used", {0, 8000},
                               /*limited_entropy_value=*/std::string_view()));
  EXPECT_FALSE(
      base::FieldTrialList::TrialExists("Study_NoSalt_LimitedEntropy"));
  EXPECT_FALSE(
      base::FieldTrialList::TrialExists("Study_WithSalt_LimitedEntropy"));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(
      "Study_NoSalt_WithGoogleId_LimitedEntropy"));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(
      "Study_WithSalt_WithGoogleId_LimitedEntropy"));

  EXPECT_TRUE(base::FieldTrialList::TrialExists("Study_NoSalt"));
  EXPECT_TRUE(base::FieldTrialList::TrialExists("Study_WithSalt"));
  EXPECT_TRUE(base::FieldTrialList::TrialExists("Study_NoSalt_LowEntropy"));
  EXPECT_TRUE(base::FieldTrialList::TrialExists("Study_WithSalt_LowEntropy"));
}

TEST(VariationsAntishuffleTest, LimitedEntropyRandomizationSource) {
  struct RandomizationConfig {
    std::string limited_entropy_value;
    struct Expectation {
      std::string study;
      std::string group;
    };
    std::vector<Expectation> expectations;
  } test_cases[] = {
      // Group selections of "Study_WithSalt_LimitedEntropy" and
      // "Study_WithSalt_WithGoogleId_LimitedEntropy" are the same because they
      // are randomized from the same entropy provider, entropy value, and study
      // salt.
      {"limited_0",
       {{"Study_NoSalt_LimitedEntropy", "group03"},
        {"Study_WithSalt_LimitedEntropy", "group02"},
        {"Study_NoSalt_WithGoogleId_LimitedEntropy", "group19"},
        {"Study_WithSalt_WithGoogleId_LimitedEntropy", "group02"}}},

      {"limited_1",
       {{"Study_NoSalt_LimitedEntropy", "group63"},
        {"Study_WithSalt_LimitedEntropy", "group17"},
        {"Study_NoSalt_WithGoogleId_LimitedEntropy", "group57"},
        {"Study_WithSalt_WithGoogleId_LimitedEntropy", "group17"}}}};

  for (const auto& test_case : test_cases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    ProcessSeed(EntropyProviders("not_used", {0, 8000},
                                 test_case.limited_entropy_value));
    for (const auto& expectation : test_case.expectations) {
      EXPECT_EQ(base::FieldTrialList::FindFullName(expectation.study),
                expectation.group);
    }
  }
}

}  // namespace variations
