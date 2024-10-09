// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/variations/variations_seed_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace variations {
namespace {

// Constants for testing associating command line flags with trial groups.
const char kFlagStudyName[] = "flag_test_trial";
const char kFlagGroup1Name[] = "flag_group1";
const char kFlagGroup2Name[] = "flag_group2";
const char kNonFlagGroupName[] = "non_flag_group";
const char kOtherGroupName[] = "other_group";
const char kForcingFlag1[] = "flag_test1";
const char kForcingFlag2[] = "flag_test2";

const VariationID kExperimentId = 123;

// Adds an experiment to |study| with the specified |name| and |probability|.
Study::Experiment* AddExperiment(const std::string& name,
                                 int probability,
                                 Study* study) {
  Study::Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

// Adds a Study to |seed| and populates it with test data associating command
// line flags with trials groups. The study will contain three groups, a
// default group that isn't associated with a flag, and two other groups, both
// associated with different flags.
Study* CreateStudyWithFlagGroups(int default_group_probability,
                                 int flag_group1_probability,
                                 int flag_group2_probability,
                                 VariationsSeed* seed) {
  DCHECK_GE(default_group_probability, 0);
  DCHECK_GE(flag_group1_probability, 0);
  DCHECK_GE(flag_group2_probability, 0);
  Study* study = seed->add_study();
  study->set_name(kFlagStudyName);
  study->set_default_experiment_name(kNonFlagGroupName);

  AddExperiment(kNonFlagGroupName, default_group_probability, study);
  AddExperiment(kFlagGroup1Name, flag_group1_probability, study)
      ->set_forcing_flag(kForcingFlag1);
  AddExperiment(kFlagGroup2Name, flag_group2_probability, study)
      ->set_forcing_flag(kForcingFlag2);

  return study;
}

BASE_FEATURE(kDisabled, "Disabled", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnabled, "Enabled", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRepeated, "Repeated", base::FEATURE_DISABLED_BY_DEFAULT);

// Gets the group name of the study associated with a feature or empty string.
std::string AssociatedStudyGroup(const base::Feature& feature) {
  auto* trial = base::FeatureList::GetFieldTrial(feature);
  return trial ? trial->group_name() : "";
}

// Create a filterable state for use in these tests.
// This differs from |CreateDummyClientFilterableState()| by setting membership
// of a specific google group (which some tests rely on).
uint64_t kExampleGoogleGroup = 123456;
std::unique_ptr<ClientFilterableState> CreateTestClientFilterableState() {
  auto client_state = std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }), base::BindOnce([] {
        return base::flat_set<uint64_t>({kExampleGoogleGroup});
      }));
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::PHONE;
  return client_state;
}

// Add a filter to |study| that filters on a Google group which matches the
// client filterable state.
void AddGoogleGroupFilter(Study& study) {
  Study::Filter* filter = study.mutable_filter();
  filter->add_google_group(kExampleGoogleGroup);
  // Also add a platform filter that matches both the environments we're
  // testing in the typed tests.
  filter->add_platform(Study::PLATFORM_ANDROID);
  filter->add_platform(Study::PLATFORM_ANDROID_WEBVIEW);
}

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}

  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() = default;

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const std::u16string& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;
};

}  // namespace

// ChromeEnvironment calls CreateTrialsFromSeed with arguments similar to
// chrome.
class ChromeEnvironment {
 public:
  bool HasHighEntropy() { return true; }
  bool HasLimitedEntropy() { return true; }

  void CreateTrialsFromSeed(
      const VariationsSeed& seed,
      base::FeatureList* feature_list,
      const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
    auto client_state = CreateTestClientFilterableState();
    client_state->platform = Study::PLATFORM_ANDROID;

    MockEntropyProviders entropy_providers({
        .low_entropy = kAlwaysUseLastGroup,
        .high_entropy = kAlwaysUseFirstGroup,
        .limited_entropy = kAlwaysUseFirstGroup,
    });

    VariationsLayers layers(seed, entropy_providers);
    // This should mimic the call through SetUpFieldTrials from
    // components/variations/service/variations_service.cc
    VariationsSeedProcessor().CreateTrialsFromSeed(
        seed, *client_state, callback, entropy_providers, layers, feature_list);
  }
};

// WebViewEnvironment calls CreateTrialsFromSeed with arguments similar to
// WebView.
class WebViewEnvironment {
 public:
  bool HasHighEntropy() { return false; }
  bool HasLimitedEntropy() { return false; }

  void CreateTrialsFromSeed(
      const VariationsSeed& seed,
      base::FeatureList* feature_list,
      const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
    auto client_state = CreateTestClientFilterableState();
    client_state->platform = Study::PLATFORM_ANDROID_WEBVIEW;

    MockEntropyProviders entropy_providers({
        .low_entropy = kAlwaysUseLastGroup,
    });

    VariationsLayers layers(seed, entropy_providers);
    // This should mimic the call through SetUpFieldTrials from
    // android_webview/browser/aw_feature_list_creator.cc
    VariationsSeedProcessor().CreateTrialsFromSeed(
        seed, *client_state, callback, entropy_providers, layers, feature_list);
  }
};

template <typename Environment>
class VariationsSeedProcessorTest : public ::testing::Test {
 public:
  VariationsSeedProcessorTest() = default;
  VariationsSeedProcessorTest(const VariationsSeedProcessorTest&) = delete;
  VariationsSeedProcessorTest& operator=(const VariationsSeedProcessorTest&) =
      delete;

  ~VariationsSeedProcessorTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

  void CreateTrialsFromSeed(const VariationsSeed& seed) {
    base::FeatureList feature_list;
    env.CreateTrialsFromSeed(seed, &feature_list,
                             override_callback_.callback());
  }

  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            base::FeatureList* feature_list) {
    env.CreateTrialsFromSeed(seed, feature_list, override_callback_.callback());
  }

 protected:
  Environment env;
  TestOverrideStringCallback override_callback_;
};

using EnvironmentTypes =
    ::testing::Types<ChromeEnvironment, WebViewEnvironment>;
TYPED_TEST_SUITE(VariationsSeedProcessorTest, EnvironmentTypes);

TYPED_TEST(VariationsSeedProcessorTest, EmitStudyCountMetric) {
  struct StudyCountMetricTestParams {
    VariationsSeed seed;
    int expected_study_count;
  };

  VariationsSeed zero_study_seed;
  VariationsSeed one_study_seed;
  Study* study = one_study_seed.add_study();
  study->set_name("MyStudy");
  AddExperiment("Enabled", 1, study);
  std::vector<StudyCountMetricTestParams> test_cases = {
      {.seed = zero_study_seed, .expected_study_count = 0},
      {.seed = one_study_seed, .expected_study_count = 1}};

  for (const StudyCountMetricTestParams& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    this->CreateTrialsFromSeed(test_case.seed);
    histogram_tester.ExpectUniqueSample("Variations.AppliedSeed.StudyCount",
                                        test_case.expected_study_count, 1);
  }
}

TYPED_TEST(VariationsSeedProcessorTest, IgnoreExpiryDateStudy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  // Set an expiry far in the future.
  study->set_expiry_date(std::numeric_limits<int64_t>::max());

  this->CreateTrialsFromSeed(seed);
  // No trial should be created, since expiry_date is not supported.
  EXPECT_EQ("", base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, AllowForceGroupAndVariationId) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  study->mutable_experiment(1)->set_google_web_experiment_id(kExperimentId);

  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));

  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                        kFlagStudyName, kFlagGroup1Name);
  EXPECT_EQ(kExperimentId, id);
}

TYPED_TEST(VariationsSeedProcessorTest,
           AllowForceGroupAndVariationId_FirstParty) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  Study::Experiment* experiment1 = study->mutable_experiment(1);
  experiment1->set_google_web_experiment_id(kExperimentId);
  experiment1->set_google_web_visibility(Study::FIRST_PARTY);

  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));

  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
                                        kFlagStudyName, kFlagGroup1Name);
  EXPECT_EQ(kExperimentId, id);
}

// Test that the group for kForcingFlag1 is forced.
TYPED_TEST(VariationsSeedProcessorTest, ForceGroupWithFlag1) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  CreateStudyWithFlagGroups(100, 0, 0, &seed);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

// Test that the group for kForcingFlag1 is forced.
TYPED_TEST(VariationsSeedProcessorTest, ForceGroupWithFlag1_LowAnonymity) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  AddGoogleGroupFilter(*study);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));

  // This study should be marked as low anonymity, and therefore only returned
  // by |FieldTrialListIncludingLowAnonymity|.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(active_groups.size(), 0u);

  base::FieldTrial::ActiveGroups active_groups_including_low_anonymity;
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(
          &active_groups_including_low_anonymity);
  EXPECT_EQ(active_groups_including_low_anonymity.size(), 1u);
}

// Test that the group for kForcingFlag2 is forced.
TYPED_TEST(VariationsSeedProcessorTest, ForceGroupWithFlag2) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  VariationsSeed seed;
  CreateStudyWithFlagGroups(100, 0, 0, &seed);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup2Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, FieldTrialOverride) {
  struct Case {
    std::string name;
    std::optional<int> experiment_id;
    std::optional<int> triggering_experiment_id;
    bool overridden = false;

    int expected_experiment_id = 0;
    int expected_triggering_id = 0;
  };

  std::vector<Case> cases = {
      {
          .name = "Override Enabled with experiment id",
          .experiment_id = kExperimentId,
          .overridden = true,
          .expected_experiment_id = 0,
          .expected_triggering_id = 0,
      },
      {
          .name = "Enabled with experiment id",
          .experiment_id = kExperimentId,
          .overridden = false,
          .expected_experiment_id = kExperimentId,
          .expected_triggering_id = 0,
      },
      {
          .name = "Override Enabled with triggering id",
          .triggering_experiment_id = kExperimentId,
          .overridden = true,
          .expected_experiment_id = 0,
          .expected_triggering_id = kExperimentId,
      },
      {
          .name = "Enabled with triggering id",
          .triggering_experiment_id = kExperimentId,
          .overridden = false,
          .expected_experiment_id = 0,
          .expected_triggering_id = kExperimentId,
      },
  };

  for (auto& c : cases) {
    SCOPED_TRACE(c.name);
    base::test::ScopedFeatureList empty_state;
    empty_state.InitWithEmptyFeatureAndFieldTrialLists();

    VariationsSeed seed;
    Study* study = seed.add_study();
    study->set_name(kRepeated.name);
    Study::Experiment* experiment = AddExperiment("Enabled", 1, study);
    experiment->mutable_feature_association()->add_enable_feature(
        kRepeated.name);
    if (c.experiment_id) {
      experiment->set_google_web_experiment_id(*c.experiment_id);
    }
    if (c.triggering_experiment_id) {
      experiment->set_google_web_trigger_experiment_id(
          *c.triggering_experiment_id);
    }

    base::FieldTrialList::CreateFieldTrial(
        "Repeated", "Enabled", /*is_low_anonymity=*/false, c.overridden);

    auto feature_list = std::make_unique<base::FeatureList>();
    this->CreateTrialsFromSeed(seed, feature_list.get());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(c.expected_experiment_id,
              GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                   "Repeated", "Enabled"));
    EXPECT_EQ(c.expected_triggering_id,
              GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
                                   "Repeated", "Enabled"));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kRepeated));

    testing::ClearAllVariationIDs();
  }
}

TYPED_TEST(VariationsSeedProcessorTest, ForceGroup_ChooseFirstGroupWithFlag) {
  // Add the flag to the command line arguments so the flag group is forced.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  VariationsSeed seed;
  CreateStudyWithFlagGroups(100, 0, 0, &seed);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, ForceGroup_DontChooseGroupWithFlag) {
  // The two flag groups are given high probability, which would normally make
  // them very likely to be chosen. They won't be chosen since flag groups are
  // never chosen when their flag isn't present.
  VariationsSeed seed;
  CreateStudyWithFlagGroups(1, 999, 999, &seed);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, CreateTrialForRegisteredGroup) {
  base::FieldTrialList::CreateFieldTrial(kFlagStudyName, kOtherGroupName);

  // Create an arbitrary study that does not have group named |kOtherGroupName|.
  VariationsSeed seed;
  CreateStudyWithFlagGroups(100, 0, 0, &seed);
  // Creating the trial should not crash.
  this->CreateTrialsFromSeed(seed);
  // And the previous group should still be selected.
  EXPECT_EQ(kOtherGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, OverrideUIStrings) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_default_experiment_name("B");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  Study::Experiment* experiment1 = AddExperiment("A", 0, study);
  Study::Experiment::OverrideUIString* override =
      experiment1->add_override_ui_string();

  override->set_name_hash(1234);
  override->set_value("test");

  Study::Experiment* experiment2 = AddExperiment("B", 1, study);

  this->CreateTrialsFromSeed(seed);

  const TestOverrideStringCallback::OverrideMap& overrides =
      this->override_callback_.overrides();

  EXPECT_TRUE(overrides.empty());

  study->set_name("Study2");
  experiment1->set_probability_weight(1);
  experiment2->set_probability_weight(0);

  this->CreateTrialsFromSeed(seed);

  EXPECT_EQ(1u, overrides.size());
  auto it = overrides.find(1234);
  EXPECT_EQ(u"test", it->second);
}

TYPED_TEST(VariationsSeedProcessorTest, OverrideUIStringsWithForcingFlag) {
  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  ASSERT_EQ(kForcingFlag1, study->experiment(1).forcing_flag());

  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  Study::Experiment::OverrideUIString* override =
      study->mutable_experiment(1)->add_override_ui_string();
  override->set_name_hash(1234);
  override->set_value("test");

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study->name()));

  const TestOverrideStringCallback::OverrideMap& overrides =
      this->override_callback_.overrides();
  EXPECT_EQ(1u, overrides.size());
  auto it = overrides.find(1234);
  EXPECT_EQ(u"test", it->second);
}

TYPED_TEST(VariationsSeedProcessorTest, VariationParams) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_default_experiment_name("B");

  Study::Experiment* experiment1 = AddExperiment("A", 1, study);
  Study::Experiment::Param* param = experiment1->add_param();
  param->set_name("x");
  param->set_value("y");

  Study::Experiment* experiment2 = AddExperiment("B", 0, study);

  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ("y", base::GetFieldTrialParamValue("Study1", "x"));

  study->set_name("Study2");
  experiment1->set_probability_weight(0);
  experiment2->set_probability_weight(1);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(std::string(), base::GetFieldTrialParamValue("Study2", "x"));
}

TYPED_TEST(VariationsSeedProcessorTest, VariationParamsWithForcingFlag) {
  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  ASSERT_EQ(kForcingFlag1, study->experiment(1).forcing_flag());
  Study::Experiment::Param* param = study->mutable_experiment(1)->add_param();
  param->set_name("x");
  param->set_value("y");

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study->name()));
  EXPECT_EQ("y", base::GetFieldTrialParamValue(study->name(), "x"));
}

TYPED_TEST(VariationsSeedProcessorTest, StartsActive) {
  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name("A");
  study1->set_default_experiment_name("Default");
  AddExperiment("AA", 100, study1);
  AddExperiment("Default", 0, study1);

  Study* study2 = seed.add_study();
  study2->set_name("B");
  study2->set_default_experiment_name("Default");
  AddExperiment("BB", 100, study2);
  AddExperiment("Default", 0, study2);
  study2->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  Study* study3 = seed.add_study();
  study3->set_name("C");
  study3->set_default_experiment_name("Default");
  AddExperiment("CC", 100, study3);
  AddExperiment("Default", 0, study3);
  study3->set_activation_type(Study::ACTIVATE_ON_QUERY);

  VariationsSeedProcessor seed_processor;
  this->CreateTrialsFromSeed(seed);

  // Non-specified and ACTIVATE_ON_QUERY should not start active, but
  // ACTIVATE_ON_STARTUP should.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("A"));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("B"));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("C"));

  EXPECT_EQ("AA", base::FieldTrialList::FindFullName("A"));
  EXPECT_EQ("BB", base::FieldTrialList::FindFullName("B"));
  EXPECT_EQ("CC", base::FieldTrialList::FindFullName("C"));

  // Now, all studies should be active.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("A"));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("B"));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("C"));
}

TYPED_TEST(VariationsSeedProcessorTest, StartsActiveWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  this->CreateTrialsFromSeed(seed);
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kFlagStudyName));

  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TYPED_TEST(VariationsSeedProcessorTest, ForcingFlagAlreadyForced) {
  VariationsSeed seed;
  Study* study = CreateStudyWithFlagGroups(100, 0, 0, &seed);
  ASSERT_EQ(kNonFlagGroupName, study->experiment(0).name());
  Study::Experiment::Param* param = study->mutable_experiment(0)->add_param();
  param->set_name("x");
  param->set_value("y");
  study->mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);

  base::FieldTrialList::CreateFieldTrial(kFlagStudyName, kNonFlagGroupName);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  this->CreateTrialsFromSeed(seed);
  // The previously forced experiment should still hold.
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(study->name()));

  // Check that params and experiment ids correspond.
  EXPECT_EQ("y", base::GetFieldTrialParamValue(study->name(), "x"));
  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                        kFlagStudyName, kNonFlagGroupName);
  EXPECT_EQ(kExperimentId, id);
}

TYPED_TEST(VariationsSeedProcessorTest, FeatureEnabledOrDisableByTrial) {
  static BASE_FEATURE(kFeatureOffByDefault, "kOff",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureOnByDefault, "kOn",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(kUnrelatedFeature, "kUnrelated",
                      base::FEATURE_DISABLED_BY_DEFAULT);

  struct {
    const char* enable_feature;
    const char* disable_feature;
    bool expected_feature_off_state;
    bool expected_feature_on_state;
  } test_cases_raw[] = {
      {nullptr, nullptr, false, true},
      {kFeatureOnByDefault.name, nullptr, false, true},
      {kFeatureOffByDefault.name, nullptr, true, true},
      {nullptr, kFeatureOnByDefault.name, false, false},
      {nullptr, kFeatureOffByDefault.name, false, true},
  };
  const auto test_cases = base::span(test_cases_raw);

  for (size_t i = 0; i < test_cases.size(); i++) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    // Needed for base::FeatureList::GetInstance() when creating field trials.
    base::test::ScopedFeatureList base_scoped_feature_list;
    base_scoped_feature_list.Init();

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

    VariationsSeed seed;
    Study* study = seed.add_study();
    study->set_name("Study1");
    study->set_default_experiment_name("B");
    AddExperiment("B", 0, study);

    Study::Experiment* experiment = AddExperiment("A", 1, study);
    Study::Experiment::FeatureAssociation* association =
        experiment->mutable_feature_association();
    if (test_case.enable_feature)
      association->add_enable_feature(test_case.enable_feature);
    else if (test_case.disable_feature)
      association->add_disable_feature(test_case.disable_feature);

    this->CreateTrialsFromSeed(seed, feature_list.get());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // |kUnrelatedFeature| should not be affected.
    EXPECT_FALSE(base::FeatureList::IsEnabled(kUnrelatedFeature));

    // Before the associated feature is queried, the trial shouldn't be active.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));

    EXPECT_EQ(test_case.expected_feature_off_state,
              base::FeatureList::IsEnabled(kFeatureOffByDefault));
    EXPECT_EQ(test_case.expected_feature_on_state,
              base::FeatureList::IsEnabled(kFeatureOnByDefault));

    // The field trial should get activated if it had a feature association.
    const bool expected_field_trial_active =
        test_case.enable_feature || test_case.disable_feature;
    EXPECT_EQ(expected_field_trial_active,
              base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, FeatureAssociationAndForcing) {
  static BASE_FEATURE(kFeatureOffByDefault, "kFeatureOffByDefault",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureOnByDefault, "kFeatureOnByDefault",
                      base::FEATURE_ENABLED_BY_DEFAULT);

  enum OneHundredPercentGroup {
    DEFAULT_GROUP,
    ENABLE_GROUP,
    DISABLE_GROUP,
  };

  const char kDefaultGroup[] = "Default";
  const char kEnabledGroup[] = "Enabled";
  const char kDisabledGroup[] = "Disabled";
  const char kForcedOnGroup[] = "ForcedOn";
  const char kForcedOffGroup[] = "ForcedOff";

  struct {
    const raw_ref<const base::Feature> feature;
    const char* enable_features_command_line;
    const char* disable_features_command_line;
    OneHundredPercentGroup one_hundred_percent_group;

    const char* expected_group;
    bool expected_feature_state;
    bool expected_trial_activated;
  } test_cases_raw[] = {
      // Check what happens without and command-line forcing flags - that the
      // |one_hundred_percent_group| gets correctly selected and does the right
      // thing w.r.t. to affecting the feature / activating the trial.
      {ToRawRef(kFeatureOffByDefault), "", "", DEFAULT_GROUP, kDefaultGroup,
       false, true},
      {ToRawRef(kFeatureOffByDefault), "", "", ENABLE_GROUP, kEnabledGroup,
       true, true},
      {ToRawRef(kFeatureOffByDefault), "", "", DISABLE_GROUP, kDisabledGroup,
       false, true},

      // Do the same as above, but for kFeatureOnByDefault feature.
      {ToRawRef(kFeatureOnByDefault), "", "", DEFAULT_GROUP, kDefaultGroup,
       true, true},
      {ToRawRef(kFeatureOnByDefault), "", "", ENABLE_GROUP, kEnabledGroup, true,
       true},
      {ToRawRef(kFeatureOnByDefault), "", "", DISABLE_GROUP, kDisabledGroup,
       false, true},

      // Test forcing each feature on and off through the command-line and that
      // the correct associated experiment gets chosen.
      {ToRawRef(kFeatureOffByDefault), kFeatureOffByDefault.name, "",
       DEFAULT_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOffByDefault), "", kFeatureOffByDefault.name,
       DEFAULT_GROUP, kForcedOffGroup, false, true},
      {ToRawRef(kFeatureOnByDefault), kFeatureOnByDefault.name, "",
       DEFAULT_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOnByDefault), "", kFeatureOnByDefault.name,
       DEFAULT_GROUP, kForcedOffGroup, false, true},

      // Check that even if a feature should be enabled or disabled based on the
      // the experiment probability weights, the forcing flag association still
      // takes precedence. This is 4 cases as above, but with different values
      // for |one_hundred_percent_group|.
      {ToRawRef(kFeatureOffByDefault), kFeatureOffByDefault.name, "",
       ENABLE_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOffByDefault), "", kFeatureOffByDefault.name,
       ENABLE_GROUP, kForcedOffGroup, false, true},
      {ToRawRef(kFeatureOnByDefault), kFeatureOnByDefault.name, "",
       ENABLE_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOnByDefault), "", kFeatureOnByDefault.name,
       ENABLE_GROUP, kForcedOffGroup, false, true},
      {ToRawRef(kFeatureOffByDefault), kFeatureOffByDefault.name, "",
       DISABLE_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOffByDefault), "", kFeatureOffByDefault.name,
       DISABLE_GROUP, kForcedOffGroup, false, true},
      {ToRawRef(kFeatureOnByDefault), kFeatureOnByDefault.name, "",
       DISABLE_GROUP, kForcedOnGroup, true, true},
      {ToRawRef(kFeatureOnByDefault), "", kFeatureOnByDefault.name,
       DISABLE_GROUP, kForcedOffGroup, false, true},
  };
  const auto test_cases = base::span(test_cases_raw);

  for (size_t i = 0; i < test_cases.size(); i++) {
    const auto& test_case = test_cases[i];
    const int group = test_case.one_hundred_percent_group;
    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "]: %s [%s] [%s] %d", i, test_case.feature->name,
        test_case.enable_features_command_line,
        test_case.disable_features_command_line, static_cast<int>(group)));

    // Needed for base::FeatureList::GetInstance() when creating field trials.
    base::test::ScopedFeatureList base_scoped_feature_list;
    base_scoped_feature_list.Init();

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitFromCommandLine(test_case.enable_features_command_line,
                                      test_case.disable_features_command_line);

    VariationsSeed seed;
    Study* study = seed.add_study();
    study->set_name("Study1");
    study->set_default_experiment_name(kDefaultGroup);
    AddExperiment(kDefaultGroup, group == DEFAULT_GROUP ? 1 : 0, study);

    Study::Experiment* feature_enable =
        AddExperiment(kEnabledGroup, group == ENABLE_GROUP ? 1 : 0, study);
    feature_enable->mutable_feature_association()->add_enable_feature(
        test_case.feature->name);

    Study::Experiment* feature_disable =
        AddExperiment(kDisabledGroup, group == DISABLE_GROUP ? 1 : 0, study);
    feature_disable->mutable_feature_association()->add_disable_feature(
        test_case.feature->name);

    AddExperiment(kForcedOnGroup, 0, study)
        ->mutable_feature_association()
        ->set_forcing_feature_on(test_case.feature->name);
    AddExperiment(kForcedOffGroup, 0, study)
        ->mutable_feature_association()
        ->set_forcing_feature_off(test_case.feature->name);

    this->CreateTrialsFromSeed(seed, feature_list.get());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // Trial should not be activated initially, but later might get activated
    // depending on the expected values.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
    EXPECT_EQ(test_case.expected_feature_state,
              base::FeatureList::IsEnabled(*test_case.feature));
    EXPECT_EQ(test_case.expected_trial_activated,
              base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, DefaultAssociatedFeatures) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("Study1");
  {
    auto* feature_association =
        AddExperiment("NotSelected1", 0, study)->mutable_feature_association();
    feature_association->add_disable_feature(kEnabled.name);
    feature_association->add_enable_feature(kDisabled.name);
    feature_association->add_disable_feature(kRepeated.name);
  }
  {
    auto* feature_association =
        AddExperiment("NotSelected2", 0, study)->mutable_feature_association();
    feature_association->add_enable_feature(kRepeated.name);
  }
  AddExperiment("Expected", 100, study);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  this->CreateTrialsFromSeed(seed, feature_list.get());
  base::test::ScopedFeatureList base_scoped_feature_list;
  base_scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // All features should be associated with the group with no features, but
  // none should have their state changed.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kDisabled));
  EXPECT_EQ(AssociatedStudyGroup(kDisabled), "Expected");
  EXPECT_TRUE(base::FeatureList::IsEnabled(kEnabled));
  EXPECT_EQ(AssociatedStudyGroup(kEnabled), "Expected");
  EXPECT_FALSE(base::FeatureList::IsEnabled(kRepeated));
  EXPECT_EQ(AssociatedStudyGroup(kRepeated), "Expected");
}

TYPED_TEST(VariationsSeedProcessorTest, NonDefaultAssociatedFeatures) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("Study1");
  {
    auto* feature_association =
        AddExperiment("NotSelected1", 0, study)->mutable_feature_association();
    feature_association->add_disable_feature(kEnabled.name);
    feature_association->add_enable_feature(kDisabled.name);
    feature_association->add_disable_feature(kRepeated.name);
  }
  {
    auto* feature_association =
        AddExperiment("Expected", 100, study)->mutable_feature_association();
    feature_association->add_enable_feature(kRepeated.name);
  }
  AddExperiment("Default", 0, study);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  this->CreateTrialsFromSeed(seed, feature_list.get());
  base::test::ScopedFeatureList base_scoped_feature_list;
  base_scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Only the feature explicitly associated with the group should be enabled
  // or have it's state changed.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kDisabled));
  EXPECT_EQ(AssociatedStudyGroup(kDisabled), "");
  EXPECT_TRUE(base::FeatureList::IsEnabled(kEnabled));
  EXPECT_EQ(AssociatedStudyGroup(kEnabled), "");
  EXPECT_TRUE(base::FeatureList::IsEnabled(kRepeated));
  EXPECT_EQ(AssociatedStudyGroup(kRepeated), "Expected");
}

TYPED_TEST(VariationsSeedProcessorTest, DefaultAssociatedFeaturesOnStartup) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  {
    auto* feature_association =
        AddExperiment("NotSelected1", 0, study)->mutable_feature_association();
    feature_association->add_disable_feature(kEnabled.name);
    feature_association->add_enable_feature(kDisabled.name);
    feature_association->add_disable_feature(kRepeated.name);
  }
  {
    auto* feature_association =
        AddExperiment("NotSelected2", 0, study)->mutable_feature_association();
    feature_association->add_enable_feature(kRepeated.name);
  }
  AddExperiment("Expected", 100, study);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  this->CreateTrialsFromSeed(seed, feature_list.get());
  base::test::ScopedFeatureList base_scoped_feature_list;
  base_scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Nothing should be associated with the default group for an
  // ACTIVATE_ON_STARTUP trial.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kDisabled));
  EXPECT_EQ(AssociatedStudyGroup(kDisabled), "");
  EXPECT_TRUE(base::FeatureList::IsEnabled(kEnabled));
  EXPECT_EQ(AssociatedStudyGroup(kEnabled), "");
  EXPECT_FALSE(base::FeatureList::IsEnabled(kRepeated));
  EXPECT_EQ(AssociatedStudyGroup(kRepeated), "");
}

TYPED_TEST(VariationsSeedProcessorTest, LowEntropyStudyTest) {
  const std::string kTrial1Name = "A";
  const std::string kTrial2Name = "B";
  const std::string kGroup1Name = "AA";
  const std::string kDefaultName = "Default";

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrial1Name);
  study1->set_consistency(Study::PERMANENT);
  study1->set_default_experiment_name(kDefaultName);
  AddExperiment(kGroup1Name, 50, study1);
  AddExperiment(kDefaultName, 50, study1);
  Study* study2 = seed.add_study();
  study2->set_name(kTrial2Name);
  study2->set_consistency(Study::PERMANENT);
  study2->set_default_experiment_name(kDefaultName);
  AddExperiment(kGroup1Name, 50, study2);
  AddExperiment(kDefaultName, 50, study2);
  study2->mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);

  this->CreateTrialsFromSeed(seed);

  // The environment will create a low entropy source that always picks the last
  // group, and if it creates a high entropy provider will create one that
  // always uses the first group.

  // Since no experiment in study1 sends experiment IDs, it will use the high
  // entropy provider when available, which selects the non-default group.
  if (this->env.HasHighEntropy()) {
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrial1Name));
  } else {
    EXPECT_EQ(kDefaultName, base::FieldTrialList::FindFullName(kTrial1Name));
  }

  // Since an experiment in study2 has google_web_experiment_id set, it will use
  // the low entropy provider, which selects the default group.
  EXPECT_EQ(kDefaultName, base::FieldTrialList::FindFullName(kTrial2Name));
}

TYPED_TEST(VariationsSeedProcessorTest, LimitedEntropyStudyTest) {
  VariationsSeed seed;
  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  Study* study = seed.add_study();
  study->set_name("MyStudy");
  study->set_consistency(Study::PERMANENT);
  study->set_default_experiment_name("Default");
  AddExperiment("Group1", 50, study);
  AddExperiment(study->default_experiment_name(), 50, study);
  LayerMemberReference* layer_member_reference = study->mutable_layer();
  layer_member_reference->set_layer_id(layer->id());
  layer_member_reference->add_layer_member_ids(member->id());

  this->CreateTrialsFromSeed(seed);

  if (this->env.HasLimitedEntropy()) {
    // Expect the first group to be selected when using the limited entropy
    // provider from the setup (`kAlwaysUseFirstGroup`).
    EXPECT_EQ("Group1", base::FieldTrialList::FindFullName(study->name()));
  } else {
    // The study should be dropped on clients without a limited entropy
    // provider.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithInvalidLayer) {
  VariationsSeed seed;

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer = study->mutable_layer();
  layer->set_layer_id(42);
  layer->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // Since the studies references a layer which doesn't exist, it should
  // select the default group.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithInvalidLayerMember) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(2);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(88);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // Since the studies references a layer member which doesn't exist, it should
  // not be active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerSelected) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer only has the single member, which is what should be chosen.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLegacyLayerMemberReference) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  // `layer_member_id` is a legacy field that should still be considered.
  // TODO(crbug.com/TBA): remove `layer_member_id` after it's fully deprecated.
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
}

// TODO(b/260609574): Add a test for handling layers with unknown fields.

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerMemberWithNoSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer member referenced by the study is missing slots, and should
  // never be chosen.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerMemberWithUnsetSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  // Add one SlotRange, with no start/end unset. This should be equivalent
  // to specifying start/end = 0, which includes slot 0 only.
  member->add_slots();

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  if (this->env.HasHighEntropy()) {
    // high entropy should select slot 0, which activates the study.
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  } else {
    // low entropy should select slot 9, which does not activate the study.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerWithDuplicateSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* first_slot = member->add_slots();
  first_slot->set_start(0);
  first_slot->set_end(0);

  // A second overlapping slot.
  Layer::LayerMember::SlotRange* second_slot = member->add_slots();
  second_slot->set_start(0);
  second_slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  // The layer should be rejected due to duplicated slot bounds.
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidSlotBounds,
                                      1);

  // The layer only has the single member, which is what should be chosen.
  // Having two duplicate slot ranges within that member should not crash.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudyWithLayerMemberWithOutOfRangeSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* overshooting_slot = member->add_slots();
  overshooting_slot->set_start(20);
  overshooting_slot->set_end(50);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  // The layer should be rejected due to invalid slot bounds.
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidSlotBounds,
                                      1);

  // The layer member referenced by the study is missing slots, and should
  // never be chosen.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerMemberWithReversedSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* overshooting_slot = member->add_slots();
  overshooting_slot->set_start(8);
  overshooting_slot->set_end(2);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  // The layer should be rejected due to invalid slot bounds.
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidSlotBounds,
                                      1);

  // The layer member referenced by the study is has its slots in the wrong
  // order (end < start) which should cause the slot to never be chosen
  // (and not crash).
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudyWithLayerMemberWithOutOfOrderSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  {
    Layer::LayerMember::SlotRange* range = member->add_slots();
    range->set_start(8);
    range->set_end(9);
  }
  // Add a second range that is not increasing from the first one.
  {
    Layer::LayerMember::SlotRange* range = member->add_slots();
    range->set_start(1);
    range->set_end(2);
  }

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  // The layer should be rejected due to out of order slots.
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidSlotBounds,
                                      1);

  // The layer should be rejected, so the study should not be active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithInterleavedLayerMember) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(82);
    {
      Layer::LayerMember::SlotRange* range = member->add_slots();
      range->set_start(0);
      range->set_end(2);
    }
    {
      Layer::LayerMember::SlotRange* range = member->add_slots();
      range->set_start(8);
      range->set_end(9);
    }
  }
  // Add a second member that is interleaved with the first one.
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(100);
    {
      Layer::LayerMember::SlotRange* range = member->add_slots();
      range->set_start(4);
      range->set_end(5);
    }
  }

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // high entropy should select slot 0, and low entropy should select
  // slot 9, which both activate the study.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyReferencingMultipleLayerMember) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member_1 = layer->add_members();
  member_1->set_id(82);
  {
    Layer::LayerMember::SlotRange* range = member_1->add_slots();
    range->set_start(0);
    range->set_end(4);
  }
  Layer::LayerMember* member_2 = layer->add_members();
  member_2->set_id(83);
  {
    Layer::LayerMember::SlotRange* range = member_2->add_slots();
    range->set_start(5);
    range->set_end(9);
  }

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(layer->id());
  layer_membership->add_layer_member_ids(member_1->id());
  layer_membership->add_layer_member_ids(member_2->id());
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer members with IDs 0 and 1 cover 100% of the population. By
  // referencing both of the layer members the study must be active all the
  // time.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest,
           MultipleLayerMember_NoChangeToExistingClients_RemainderEntropy) {
  VariationsSeed seed;

  // Add a low entropy layer into the seed:
  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  layer->set_entropy_mode(Layer::LOW);

  // Populate the layer with two members covering all of the 10 slots:
  Layer::LayerMember* member_1 = layer->add_members();
  Layer::LayerMember* member_2 = layer->add_members();
  member_1->set_id(82);
  member_2->set_id(83);
  {
    Layer::LayerMember::SlotRange* range = member_1->add_slots();
    range->set_start(0);
    range->set_end(4);
  }
  {
    Layer::LayerMember::SlotRange* range = member_2->add_slots();
    range->set_start(5);
    range->set_end(9);
  }

  // Add a permanently consistent, starts-active study, and constrained it to
  // the layer member #2 in the layer:
  Study* study = seed.add_study();
  study->set_name("MyStudy");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  study->set_consistency(Study_Consistency_PERMANENT);
  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(layer->id());
  layer_membership->add_layer_member_ids(member_2->id());

  // Add two experiments with google_web_experiment_id. This setup forces the
  // study to be randomized using remainder entropy from the slot randomization.
  // See VariationsLayers::SelectEntropyProviderForStudy().
  AddExperiment("A", 1, study);
  AddExperiment("B", 1, study);
  study->mutable_experiment(0)->set_google_web_experiment_id(1001);
  study->mutable_experiment(1)->set_google_web_experiment_id(1002);

  this->CreateTrialsFromSeed(seed);

  // Verify that the study is active, with group A selected:
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(study->name())->group_name(), "A");

  // Clear field trial states:
  testing::ClearAllVariationIDs();
  testing::ClearAllVariationParams();

  // Give this study a new layer member constraint, and randomize it again:
  layer_membership->add_layer_member_ids(member_1->id());
  this->CreateTrialsFromSeed(seed);

  // The randomization of this exact same client is not affected, and it will
  // still randomize to group A. This verifies that existing clients using
  // remainder entropy will not be re-shuffled if the study is constrained to
  // another layer member.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(study->name())->group_name(), "A");
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerNotSelected) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(8000);
  // Setting this forces the provided entropy provider to be used when
  // calling CreateTrialsFromSeed.
  layer->set_entropy_mode(Layer::LOW);

  // Member with most slots, but won't be chosen due to the entropy provided.
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(0xDEAD);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(0);
    slot->set_end(7900);
  }

  // Member with few slots, but will be chosen.
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(0xBEEF);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(7901);
    slot->set_end(7999);
  }

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(0xDEAD);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // Low entropy should select slot 7999, which should not select layer 0xDEAD,
  // and the study should not be activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithDefaultEntropy) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(8000);

  // Member which should get chosen by the default high entropy source
  // (which defaults to half of the num_slots in tests).
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(0xDEAD);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(0);
    slot->set_end(7900);
  }

  // Member with few slots,
  {
    Layer::LayerMember* member = layer->add_members();
    member->set_id(0xBEEF);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(7901);
    slot->set_end(7999);
  }

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(0xDEAD);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  if (this->env.HasHighEntropy()) {
    // The high entropy source should select slot 0, which should select
    // the member 0xDEAD and activate the study.
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  } else {
    // The low entropy source should select slot 7999, which should NOT select
    // the member 0xDEAD, so the study will be inactive.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithNoMembers) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_num_slots(1);
  layer->set_salt(0xBEEF);

  // Layer should be rejected and not crash.
  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kNoMembers, 1);
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithNoSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_salt(0xBEEF);

  // Layer should be rejected and not crash.
  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kNoSlots, 1);
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithNoID) {
  VariationsSeed seed;
  Layer* layer = seed.add_layers();
  layer->set_salt(0xBEEF);

  // Layer should be rejected and not crash.
  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidId, 1);
}

TYPED_TEST(VariationsSeedProcessorTest, EmptyLayer) {
  VariationsSeed seed;
  seed.add_layers();

  // Layer should be rejected and not crash.
  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidId, 1);
}

TYPED_TEST(VariationsSeedProcessorTest, LayersWithDuplicateID) {
  VariationsSeed seed;

  {
    Layer* layer = seed.add_layers();
    layer->set_id(1);
    layer->set_salt(0xBEEF);
    layer->set_num_slots(1);
    Layer::LayerMember* member = layer->add_members();
    member->set_id(82);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(0);
    slot->set_end(0);
  }

  {
    Layer* layer = seed.add_layers();
    layer->set_id(1);
    layer->set_salt(0xBEEF);
    layer->set_num_slots(1);
    Layer::LayerMember* member = layer->add_members();
    member->set_id(82);
    Layer::LayerMember::SlotRange* slot = member->add_slots();
    slot->set_start(0);
    slot->set_end(0);
  }

  // The duplicate layer should be rejected and not crash.
  this->CreateTrialsFromSeed(seed);
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerMemberWithoutID) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer only has the single member but that member has no
  // ID set. The LayerMembership also has no member_id set. The study
  // should then *not* be chosen (i.e. a default initialized ID of 0
  // should not be seen as valid.)
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLowerEntropyThanLayer) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(1);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0);

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);
  study->mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);

  this->CreateTrialsFromSeed(seed);

  // Since the study will use the low entropy source and the layer the default
  // one, the study should be rejected.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudiesWithOverlappingEnabledFeatures) {
  static BASE_FEATURE(kFeature, "FeatureName",
                      base::FEATURE_ENABLED_BY_DEFAULT);

  VariationsSeed seed;

  // Create two studies that enable |kFeature|.
  Study* flags_study = seed.add_study();
  flags_study->set_name("FlagsStudy");
  flags_study->set_default_experiment_name("A");
  flags_study->set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);
  Study::Experiment* experiment =
      AddExperiment("A", /*probability=*/1, flags_study);
  experiment->mutable_feature_association()->add_enable_feature(kFeature.name);

  Study* server_side_study = seed.add_study();
  server_side_study->set_name("ServerSideStudy");
  server_side_study->set_default_experiment_name("A");
  server_side_study->set_activation_type(
      Study_ActivationType_ACTIVATE_ON_STARTUP);
  AddGoogleGroupFilter(*server_side_study);
  Study::Experiment* experiment2 =
      AddExperiment("A", /*probability=*/1, server_side_study);
  experiment2->mutable_feature_association()->add_enable_feature(kFeature.name);

  this->CreateTrialsFromSeed(seed);

  // Verify that FlagsStudy was created and activated, and that the "A"
  // experiment group was selected.
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(flags_study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(flags_study->name())->group_name(), "A");

  // Verify that ServerSideStudy was created and activated, but that the
  // |kFeatureConflictGroupName| experiment group was forcibly selected due to
  // the study being associated with |kFeature| (which is already associated
  // with trial FlagsStudy).
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(server_side_study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(server_side_study->name())->group_name(),
            internal::kFeatureConflictGroupName);

  // Only one of the studies is returned by the default field trial list (as
  // the second is low-anonymity).
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(active_groups.size(), 1u);

  // Both studies are returned by in the full list including low anonymity.
  base::FieldTrial::ActiveGroups active_groups_including_low_anonymity;
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(
          &active_groups_including_low_anonymity);
  EXPECT_EQ(active_groups_including_low_anonymity.size(), 2u);
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudiesWithOverlappingDisabledFeatures) {
  static BASE_FEATURE(kFeature, "FeatureName",
                      base::FEATURE_ENABLED_BY_DEFAULT);

  VariationsSeed seed;

  // Create two studies that disable |kFeature|.
  Study* flags_study = seed.add_study();
  flags_study->set_name("FlagsStudy");
  flags_study->set_default_experiment_name("A");
  flags_study->set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);
  Study::Experiment* experiment =
      AddExperiment("A", /*probability=*/1, flags_study);
  experiment->mutable_feature_association()->add_disable_feature(kFeature.name);

  Study* server_side_study = seed.add_study();
  server_side_study->set_name("ServerSideStudy");
  server_side_study->set_default_experiment_name("A");
  server_side_study->set_activation_type(
      Study_ActivationType_ACTIVATE_ON_STARTUP);
  AddGoogleGroupFilter(*server_side_study);
  Study::Experiment* experiment2 =
      AddExperiment("A", /*probability=*/1, server_side_study);
  experiment2->mutable_feature_association()->add_disable_feature(
      kFeature.name);

  this->CreateTrialsFromSeed(seed);

  // Verify that FlagsStudy was created and activated, and that the "A"
  // experiment group was selected.
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(flags_study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(flags_study->name())->group_name(), "A");

  // Verify that ServerSideStudy was created and activated, but that the
  // |kFeatureConflictGroupName| experiment group was forcibly selected due to
  // the study being associated with |kFeature| (which is already associated
  // with trial FlagsStudy).
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(server_side_study->name()));
  EXPECT_EQ(base::FieldTrialList::Find(server_side_study->name())->group_name(),
            internal::kFeatureConflictGroupName);

  // Only one of the studies is returned by the default field trial list (as
  // the second is low-anonymity).
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(active_groups.size(), 1u);

  // Both studies are returned by in the full list including low anonymity.
  base::FieldTrial::ActiveGroups active_groups_including_low_anonymity;
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(
          &active_groups_including_low_anonymity);
  EXPECT_EQ(active_groups_including_low_anonymity.size(), 2u);
}

TYPED_TEST(VariationsSeedProcessorTest, OutOfBoundsLayer) {
  VariationsSeed seed;
  // Define an invalid layer with out of bounds slots.
  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(8000);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0);
  slot->set_end(0x7fffffff);

  // Add a study that uses it with remainder entropy.
  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->add_layer_member_ids(82);
  AddExperiment("A", 1, study);
  study->mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);
  AddExperiment("B", 1, study);

  // Layer should be rejected and not crash or timeout.
  base::HistogramTester histogram_tester;
  this->CreateTrialsFromSeed(seed);
  histogram_tester.ExpectUniqueSample("Variations.InvalidLayerReason",
                                      InvalidLayerReason::kInvalidSlotBounds,
                                      1);
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudyWithGoogleGroupFilterIsLowAnonymity) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("A");
  study->set_default_experiment_name("Default");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  AddExperiment("AA", 100, study);
  AddExperiment("Default", 0, study);
  AddGoogleGroupFilter(*study);

  this->CreateTrialsFromSeed(seed);

  // This study should be marked as low anonymity, and therefore only returned
  // by |FieldTrialListIncludingLowAnonymity|.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(active_groups.size(), 0u);

  base::FieldTrial::ActiveGroups active_groups_including_low_anonymity;
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(
          &active_groups_including_low_anonymity);
  EXPECT_EQ(active_groups_including_low_anonymity.size(), 1u);
}

// Tests that studies with filters with a `google_groups` parameter generate a
// field trial parameter that contains the Google Groups ids for that study.
TYPED_TEST(VariationsSeedProcessorTest,
           StudyWithGoogleGroupFilterGeneratesFieldTrialParam) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("A");
  study->set_default_experiment_name("Default");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  AddExperiment("AA", 100, study);
  AddExperiment("Default", 0, study);
  AddGoogleGroupFilter(*study);

  this->CreateTrialsFromSeed(seed);

  EXPECT_EQ(base::NumberToString(kExampleGoogleGroup),
            base::GetFieldTrialParamValue(
                "A", internal::kGoogleGroupFeatureParamName));
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudyWithExcludeGoogleGroupFilterIsNotLowAnonymity) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("A");
  study->set_default_experiment_name("Default");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  AddExperiment("AA", 100, study);
  AddExperiment("Default", 0, study);

  // Add a study filter that excludes a Google group, which this client is not
  // a member of (i.e. the client does select this study).
  Study::Filter* filter = study->mutable_filter();
  filter->add_exclude_google_group(987654);
  // Also add a platform filter that matches both the environments we're
  // testing in the typed tests.
  filter->add_platform(Study::PLATFORM_ANDROID);
  filter->add_platform(Study::PLATFORM_ANDROID_WEBVIEW);

  this->CreateTrialsFromSeed(seed);

  // This study should not be marked as low anonymity, and therefore is returned
  // by both APIs.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(active_groups.size(), 1u);

  base::FieldTrial::ActiveGroups active_groups_including_low_anonymity;
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(
          &active_groups_including_low_anonymity);
  EXPECT_EQ(active_groups_including_low_anonymity.size(), 1u);
}

}  // namespace variations
