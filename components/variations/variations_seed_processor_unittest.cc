// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"
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

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}

  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() {}

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

// Create a ClientFilterableState with reasonable default values for Chrome.
std::unique_ptr<ClientFilterableState> CreateChromeClientFilterableState() {
  auto client_state = std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }));
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_ANDROID;
  return client_state;
}

// ChromeEnvironment calls CreateTrialsFromSeed with arguments similar to
// chrome. In particular, it passes a non-nullptr as low_entropy_source.
class ChromeEnvironment {
 public:
  void CreateTrialsFromSeed(
      const VariationsSeed& seed,
      double low_entropy,
      base::FeatureList* feature_list,
      const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
    auto client_state = CreateChromeClientFilterableState();
    client_state->platform = Study::PLATFORM_ANDROID;

    base::MockEntropyProvider mock_low_entropy_provider(low_entropy);
    VariationsSeedProcessor seed_processor;
    // This should mimic the call through SetUpFieldTrials from
    // components/variations/service/variations_service.cc
    seed_processor.CreateTrialsFromSeed(seed, *client_state, callback,
                                        &mock_low_entropy_provider,
                                        feature_list);
  }

  bool SupportsLayers() { return true; }
};

// WebViewEnvironment calls CreateTrialsFromSeed with arguments similar to
// WebView. In particular, it passes a nullptr as low_entropy_source.
class WebViewEnvironment {
 public:
  void CreateTrialsFromSeed(
      const VariationsSeed& seed,
      double low_entropy,
      base::FeatureList* feature_list,
      const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
    auto client_state = CreateChromeClientFilterableState();
    client_state->platform = Study::PLATFORM_ANDROID_WEBVIEW;

    VariationsSeedProcessor seed_processor;
    // This should mimic the call through SetUpFieldTrials from
    // android_webview/browser/aw_feature_list_creator.cc
    seed_processor.CreateTrialsFromSeed(seed, *client_state, callback, nullptr,
                                        feature_list);
  }

  bool SupportsLayers() { return false; }
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

  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            double low_entropy = 0.9) {
    base::FeatureList feature_list;
    env.CreateTrialsFromSeed(seed, low_entropy, &feature_list,
                             override_callback_.callback());
  }

  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            base::FeatureList* feature_list) {
    env.CreateTrialsFromSeed(seed, 0.9, feature_list,
                             override_callback_.callback());
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

// Test that the group for kForcingFlag2 is forced.
TYPED_TEST(VariationsSeedProcessorTest, ForceGroupWithFlag2) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  VariationsSeed seed;
  CreateStudyWithFlagGroups(100, 0, 0, &seed);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(kFlagGroup2Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
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
  EXPECT_EQ("y", GetVariationParamValue("Study1", "x"));

  study->set_name("Study2");
  experiment1->set_probability_weight(0);
  experiment2->set_probability_weight(1);
  this->CreateTrialsFromSeed(seed);
  EXPECT_EQ(std::string(), GetVariationParamValue("Study2", "x"));
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
  EXPECT_EQ("y", GetVariationParamValue(study->name(), "x"));
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

  ClientFilterableState client_state(base::BindOnce([] { return false; }));
  client_state.locale = "en-CA";
  client_state.reference_date = base::Time::Now();
  client_state.version = base::Version("20.0.0.0");
  client_state.channel = Study::STABLE;
  client_state.form_factor = Study::DESKTOP;
  client_state.platform = Study::PLATFORM_ANDROID;

  VariationsSeedProcessor seed_processor;
  base::MockEntropyProvider mock_low_entropy_provider(0.9);
  seed_processor.CreateTrialsFromSeed(
      seed, client_state, this->override_callback_.callback(),
      &mock_low_entropy_provider, base::FeatureList::GetInstance());

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
  EXPECT_EQ("y", GetVariationParamValue(study->name(), "x"));
  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                        kFlagStudyName, kNonFlagGroupName);
  EXPECT_EQ(kExperimentId, id);
}

TYPED_TEST(VariationsSeedProcessorTest, FeatureEnabledOrDisableByTrial) {
  struct base::Feature kFeatureOffByDefault {
    "kOff", base::FEATURE_DISABLED_BY_DEFAULT
  };
  struct base::Feature kFeatureOnByDefault {
    "kOn", base::FEATURE_ENABLED_BY_DEFAULT
  };
  struct base::Feature kUnrelatedFeature {
    "kUnrelated", base::FEATURE_DISABLED_BY_DEFAULT
  };

  struct {
    const char* enable_feature;
    const char* disable_feature;
    bool expected_feature_off_state;
    bool expected_feature_on_state;
  } test_cases[] = {
      {nullptr, nullptr, false, true},
      {kFeatureOnByDefault.name, nullptr, false, true},
      {kFeatureOffByDefault.name, nullptr, true, true},
      {nullptr, kFeatureOnByDefault.name, false, false},
      {nullptr, kFeatureOffByDefault.name, false, true},
  };

  for (size_t i = 0; i < std::size(test_cases); i++) {
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
  struct base::Feature kFeatureOffByDefault {
    "kFeatureOffByDefault", base::FEATURE_DISABLED_BY_DEFAULT
  };
  struct base::Feature kFeatureOnByDefault {
    "kFeatureOnByDefault", base::FEATURE_ENABLED_BY_DEFAULT
  };

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
    const base::Feature& feature;
    const char* enable_features_command_line;
    const char* disable_features_command_line;
    OneHundredPercentGroup one_hundred_percent_group;

    const char* expected_group;
    bool expected_feature_state;
    bool expected_trial_activated;
  } test_cases[] = {
      // Check what happens without and command-line forcing flags - that the
      // |one_hundred_percent_group| gets correctly selected and does the right
      // thing w.r.t. to affecting the feature / activating the trial.
      {kFeatureOffByDefault, "", "", DEFAULT_GROUP, kDefaultGroup, false, true},
      {kFeatureOffByDefault, "", "", ENABLE_GROUP, kEnabledGroup, true, true},
      {kFeatureOffByDefault, "", "", DISABLE_GROUP, kDisabledGroup, false,
       true},

      // Do the same as above, but for kFeatureOnByDefault feature.
      {kFeatureOnByDefault, "", "", DEFAULT_GROUP, kDefaultGroup, true, true},
      {kFeatureOnByDefault, "", "", ENABLE_GROUP, kEnabledGroup, true, true},
      {kFeatureOnByDefault, "", "", DISABLE_GROUP, kDisabledGroup, false, true},

      // Test forcing each feature on and off through the command-line and that
      // the correct associated experiment gets chosen.
      {kFeatureOffByDefault, kFeatureOffByDefault.name, "", DEFAULT_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOffByDefault, "", kFeatureOffByDefault.name, DEFAULT_GROUP,
       kForcedOffGroup, false, true},
      {kFeatureOnByDefault, kFeatureOnByDefault.name, "", DEFAULT_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOnByDefault, "", kFeatureOnByDefault.name, DEFAULT_GROUP,
       kForcedOffGroup, false, true},

      // Check that even if a feature should be enabled or disabled based on the
      // the experiment probability weights, the forcing flag association still
      // takes precedence. This is 4 cases as above, but with different values
      // for |one_hundred_percent_group|.
      {kFeatureOffByDefault, kFeatureOffByDefault.name, "", ENABLE_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOffByDefault, "", kFeatureOffByDefault.name, ENABLE_GROUP,
       kForcedOffGroup, false, true},
      {kFeatureOnByDefault, kFeatureOnByDefault.name, "", ENABLE_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOnByDefault, "", kFeatureOnByDefault.name, ENABLE_GROUP,
       kForcedOffGroup, false, true},
      {kFeatureOffByDefault, kFeatureOffByDefault.name, "", DISABLE_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOffByDefault, "", kFeatureOffByDefault.name, DISABLE_GROUP,
       kForcedOffGroup, false, true},
      {kFeatureOnByDefault, kFeatureOnByDefault.name, "", DISABLE_GROUP,
       kForcedOnGroup, true, true},
      {kFeatureOnByDefault, "", kFeatureOnByDefault.name, DISABLE_GROUP,
       kForcedOffGroup, false, true},
  };

  for (size_t i = 0; i < std::size(test_cases); i++) {
    const auto& test_case = test_cases[i];
    const int group = test_case.one_hundred_percent_group;
    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "]: %s [%s] [%s] %d", i, test_case.feature.name,
        test_case.enable_features_command_line,
        test_case.disable_features_command_line, static_cast<int>(group)));

    // Needed for base::FeatureList::GetInstance() when creating field trials.
    base::test::ScopedFeatureList base_scoped_feature_list;
    base_scoped_feature_list.Init();

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        test_case.enable_features_command_line,
        test_case.disable_features_command_line);

    VariationsSeed seed;
    Study* study = seed.add_study();
    study->set_name("Study1");
    study->set_default_experiment_name(kDefaultGroup);
    AddExperiment(kDefaultGroup, group == DEFAULT_GROUP ? 1 : 0, study);

    Study::Experiment* feature_enable =
        AddExperiment(kEnabledGroup, group == ENABLE_GROUP ? 1 : 0, study);
    feature_enable->mutable_feature_association()->add_enable_feature(
        test_case.feature.name);

    Study::Experiment* feature_disable =
        AddExperiment(kDisabledGroup, group == DISABLE_GROUP ? 1 : 0, study);
    feature_disable->mutable_feature_association()->add_disable_feature(
        test_case.feature.name);

    AddExperiment(kForcedOnGroup, 0, study)
        ->mutable_feature_association()
        ->set_forcing_feature_on(test_case.feature.name);
    AddExperiment(kForcedOffGroup, 0, study)
        ->mutable_feature_association()
        ->set_forcing_feature_off(test_case.feature.name);

    this->CreateTrialsFromSeed(seed, feature_list.get());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // Trial should not be activated initially, but later might get activated
    // depending on the expected values.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
    EXPECT_EQ(test_case.expected_feature_state,
              base::FeatureList::IsEnabled(test_case.feature));
    EXPECT_EQ(test_case.expected_trial_activated,
              base::FieldTrialList::IsTrialActive(study->name()));
  }
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

  // An entropy value of 0.1 will cause the AA group to be chosen, since AA is
  // the only non-default group, and has a probability percent above 0.1.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  base::FieldTrialList field_trial_list(
      std::make_unique<base::MockEntropyProvider>(0.1));

  // This entropy value will cause the default group to be chosen since it's a
  // 50/50 trial.
  this->CreateTrialsFromSeed(seed, 0.9);

  // Since no experiment in study1 sends experiment IDs, it will use the high
  // entropy provider, which selects the non-default group.
  EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrial1Name));

  // Since an experiment in study2 has google_web_experiment_id set, it will use
  // the low entropy provider, which selects the default group.
  if (this->env.SupportsLayers()) {
    EXPECT_EQ(kDefaultName, base::FieldTrialList::FindFullName(kTrial2Name));
  } else {
    // On WebView we always use the default entropy provider.
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrial1Name));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithInvalidLayer) {
  VariationsSeed seed;

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer = study->mutable_layer();
  layer->set_layer_id(42);
  layer->set_layer_member_id(82);
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
  layer_membership->set_layer_member_id(88);
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
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer only has the single member, which is what should be chosen.
  if (this->env.SupportsLayers()) {
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  } else {
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
  }
}

TYPED_TEST(VariationsSeedProcessorTest, StudyWithLayerMemberWithNoSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(42);
  layer->set_num_slots(10);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  // Add one SlotRange, with no slots actually defined.
  member->add_slots();

  Study* study = seed.add_study();
  study->set_name("Study1");
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);

  LayerMemberReference* layer_membership = study->mutable_layer();
  layer_membership->set_layer_id(42);
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer member referenced by the study is missing slots, and should
  // never be chosen.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
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
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer only has the single member, which is what should be chosen.
  // Having two duplicate slot ranges within that member should not crash.
  if (this->env.SupportsLayers()) {
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  } else {
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
  }
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
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

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
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);

  this->CreateTrialsFromSeed(seed);

  // The layer member referenced by the study is has its slots in the wrong
  // order (end < start) which should cause the slot to never be chosen
  // (and not crash).
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
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
  layer_membership->set_layer_member_id(0xDEAD);
  AddExperiment("A", 1, study);

  // Entropy 0.99 Should cause slot 7920 to be chosen.
  this->CreateTrialsFromSeed(seed, /*low_entropy=*/0.99);

  // The study is a member of the 0xDEAD layer member and should be inactive
  // (or layers are not supported by the environment).
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
  layer_membership->set_layer_member_id(0xDEAD);
  AddExperiment("A", 1, study);

  // Since we're *not* setting the entropy_mode to LOW, |low_entropy| should
  // be ignored and the default high entropy should be used, which in
  // this case is slot 4000 and hence the first layer member is chosen.
  this->CreateTrialsFromSeed(seed, /*low_entropy=*/0.99);

  // The study is a member of the 0xDEAD layer member and should be active.
  if (this->env.SupportsLayers()) {
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(study->name()));
  } else {
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
  this->CreateTrialsFromSeed(seed);
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithNoSlots) {
  VariationsSeed seed;

  Layer* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_salt(0xBEEF);

  // Layer should be rejected and not crash.
  this->CreateTrialsFromSeed(seed);
}

TYPED_TEST(VariationsSeedProcessorTest, LayerWithNoID) {
  VariationsSeed seed;
  Layer* layer = seed.add_layers();
  layer->set_salt(0xBEEF);

  // Layer should be rejected and not crash.
  this->CreateTrialsFromSeed(seed);
}

TYPED_TEST(VariationsSeedProcessorTest, EmptyLayer) {
  VariationsSeed seed;
  seed.add_layers();

  // Layer should be rejected and not crash.
  this->CreateTrialsFromSeed(seed);
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
  layer_membership->set_layer_member_id(82);
  AddExperiment("A", 1, study);
  study->mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);

  this->CreateTrialsFromSeed(seed);

  // Since the study will use the low entropy source and the layer the default
  // one, the study should be rejected.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study->name()));
}

TYPED_TEST(VariationsSeedProcessorTest, StudiesWithOverlappingEnabledFeatures) {
  static struct base::Feature kFeature {
    "FeatureName", base::FEATURE_ENABLED_BY_DEFAULT
  };

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
}

TYPED_TEST(VariationsSeedProcessorTest,
           StudiesWithOverlappingDisabledFeatures) {
  static struct base::Feature kFeature {
    "FeatureName", base::FEATURE_ENABLED_BY_DEFAULT
  };

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
}

}  // namespace variations
