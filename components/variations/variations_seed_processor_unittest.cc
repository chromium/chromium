// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace variations {
namespace {

// Converts |time| to Study proto format.
int64_t TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

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
Study_Experiment* AddExperiment(const std::string& name, int probability,
                                Study* study) {
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

// Populates |study| with test data used for testing associating command line
// flags with trials groups. The study will contain three groups, a default
// group that isn't associated with a flag, and two other groups, both
// associated with different flags.
Study CreateStudyWithFlagGroups(int default_group_probability,
                                int flag_group1_probability,
                                int flag_group2_probability) {
  DCHECK_GE(default_group_probability, 0);
  DCHECK_GE(flag_group1_probability, 0);
  DCHECK_GE(flag_group2_probability, 0);
  Study study;
  study.set_name(kFlagStudyName);
  study.set_default_experiment_name(kNonFlagGroupName);

  AddExperiment(kNonFlagGroupName, default_group_probability, &study);
  AddExperiment(kFlagGroup1Name, flag_group1_probability, &study)
      ->set_forcing_flag(kForcingFlag1);
  AddExperiment(kFlagGroup2Name, flag_group2_probability, &study)
      ->set_forcing_flag(kForcingFlag2);

  return study;
}

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, base::string16> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::Bind(&TestOverrideStringCallback::Override,
                             base::Unretained(this))) {}

  virtual ~TestOverrideStringCallback() {}

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const base::string16& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;

  DISALLOW_COPY_AND_ASSIGN(TestOverrideStringCallback);
};

}  // namespace

class VariationsSeedProcessorTest : public ::testing::Test {
 public:
  VariationsSeedProcessorTest() {
  }

  ~VariationsSeedProcessorTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

  bool CreateTrialFromStudy(const Study& study) {
    return CreateTrialFromStudyWithFeatureListAndEntropyOverride(
        study, nullptr, base::FeatureList::GetInstance());
  }

  bool CreateTrialFromStudyWithEntropyOverride(
      const Study& study,
      const base::FieldTrial::EntropyProvider* override_entropy_provider) {
    return CreateTrialFromStudyWithFeatureListAndEntropyOverride(
        study, override_entropy_provider, base::FeatureList::GetInstance());
  }

  bool CreateTrialFromStudyWithFeatureList(const Study& study,
                                           base::FeatureList* feature_list) {
    return CreateTrialFromStudyWithFeatureListAndEntropyOverride(study, nullptr,
                                                                 feature_list);
  }

  bool CreateTrialFromStudyWithFeatureListAndEntropyOverride(
      const Study& study,
      const base::FieldTrial::EntropyProvider* override_entropy_provider,
      base::FeatureList* feature_list) {
    ProcessedStudy processed_study;
    const bool is_expired = internal::IsStudyExpired(study, base::Time::Now());
    if (processed_study.Init(&study, is_expired)) {
      VariationsSeedProcessor().CreateTrialFromStudy(
          processed_study, override_callback_.callback(),
          override_entropy_provider, feature_list);
      return true;
    }
    return false;
  }

 protected:
  TestOverrideStringCallback override_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessorTest);
};

TEST_F(VariationsSeedProcessorTest, AllowForceGroupAndVariationId) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  study.mutable_experiment(1)->set_google_web_experiment_id(kExperimentId);

  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));

  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, kFlagStudyName,
                                        kFlagGroup1Name);
  EXPECT_EQ(kExperimentId, id);
}

// Test that the group for kForcingFlag1 is forced.
TEST_F(VariationsSeedProcessorTest, ForceGroupWithFlag1) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

// Test that the group for kForcingFlag2 is forced.
TEST_F(VariationsSeedProcessorTest, ForceGroupWithFlag2) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup2Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, ForceGroup_ChooseFirstGroupWithFlag) {
  // Add the flag to the command line arguments so the flag group is forced.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, ForceGroup_DontChooseGroupWithFlag) {
  // The two flag groups are given high probability, which would normally make
  // them very likely to be chosen. They won't be chosen since flag groups are
  // never chosen when their flag isn't present.
  Study study = CreateStudyWithFlagGroups(1, 999, 999);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, CreateTrialForRegisteredGroup) {
  base::FieldTrialList::CreateFieldTrial(kFlagStudyName, kOtherGroupName);

  // Create an arbitrary study that does not have group named |kOtherGroupName|.
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  // Creating the trial should not crash.
  EXPECT_TRUE(CreateTrialFromStudy(study));
  // And the previous group should still be selected.
  EXPECT_EQ(kOtherGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest,
       NonExpiredStudyPrioritizedOverExpiredStudy) {
  VariationsSeedProcessor seed_processor;

  const std::string kTrialName = "A";
  const std::string kGroup1Name = "Group1";

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrialName);
  study1->set_default_experiment_name("Default");
  AddExperiment(kGroup1Name, 100, study1);
  AddExperiment("Default", 0, study1);
  Study* study2 = seed.add_study();
  *study2 = *study1;
  ASSERT_EQ(seed.study(0).name(), seed.study(1).name());

  const base::Time year_ago =
      base::Time::Now() - base::TimeDelta::FromDays(365);

  ClientFilterableState client_state({});
  client_state.locale = "en-CA";
  client_state.reference_date = base::Time::Now();
  client_state.version = base::Version("20.0.0.0");
  client_state.channel = Study::STABLE;
  client_state.form_factor = Study::DESKTOP;
  client_state.platform = Study::PLATFORM_ANDROID;

  // Check that adding [expired, non-expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    base::FeatureList feature_list;
    study1->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed, client_state,
                                        override_callback_.callback(), nullptr,
                                        &feature_list);
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }

  // Check that adding [non-expired, expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    base::FeatureList feature_list;
    study1->clear_expiry_date();
    study2->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed, client_state,
                                        override_callback_.callback(), nullptr,
                                        &feature_list);
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }
}

TEST_F(VariationsSeedProcessorTest, OverrideUIStrings) {
  Study study;
  study.set_name("Study1");
  study.set_default_experiment_name("B");
  study.set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);

  Study_Experiment* experiment1 = AddExperiment("A", 0, &study);
  Study_Experiment_OverrideUIString* override =
      experiment1->add_override_ui_string();

  override->set_name_hash(1234);
  override->set_value("test");

  Study_Experiment* experiment2 = AddExperiment("B", 1, &study);

  EXPECT_TRUE(CreateTrialFromStudy(study));

  const TestOverrideStringCallback::OverrideMap& overrides =
      override_callback_.overrides();

  EXPECT_TRUE(overrides.empty());

  study.set_name("Study2");
  experiment1->set_probability_weight(1);
  experiment2->set_probability_weight(0);

  EXPECT_TRUE(CreateTrialFromStudy(study));

  EXPECT_EQ(1u, overrides.size());
  auto it = overrides.find(1234);
  EXPECT_EQ(base::ASCIIToUTF16("test"), it->second);
}

TEST_F(VariationsSeedProcessorTest, OverrideUIStringsWithForcingFlag) {
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  ASSERT_EQ(kForcingFlag1, study.experiment(1).forcing_flag());

  study.set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);
  Study_Experiment_OverrideUIString* override =
      study.mutable_experiment(1)->add_override_ui_string();
  override->set_name_hash(1234);
  override->set_value("test");

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study.name()));

  const TestOverrideStringCallback::OverrideMap& overrides =
      override_callback_.overrides();
  EXPECT_EQ(1u, overrides.size());
  auto it = overrides.find(1234);
  EXPECT_EQ(base::ASCIIToUTF16("test"), it->second);
}

TEST_F(VariationsSeedProcessorTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study_Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_EQ(300, processed_study.total_probability());
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // A blank default study is allowed.
  study.clear_default_experiment_name();
  EXPECT_TRUE(processed_study.Init(&study, false));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study, false));
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study, false));
}

TEST_F(VariationsSeedProcessorTest, ValidateStudyWithAssociatedFeatures) {
  Study study;
  study.set_default_experiment_name("def");
  Study_Experiment* exp1 = AddExperiment("exp1", 100, &study);
  Study_Experiment* exp2 = AddExperiment("exp2", 100, &study);
  Study_Experiment* exp3 = AddExperiment("exp3", 100, &study);
  AddExperiment("def", 100, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_EQ(400, processed_study.total_probability());

  EXPECT_THAT(processed_study.associated_features(), IsEmpty());

  const char kFeature1Name[] = "Feature1";
  const char kFeature2Name[] = "Feature2";

  exp1->mutable_feature_association()->add_enable_feature(kFeature1Name);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_THAT(processed_study.associated_features(),
              ElementsAre(kFeature1Name));

  exp1->clear_feature_association();
  exp1->mutable_feature_association()->add_enable_feature(kFeature1Name);
  exp1->mutable_feature_association()->add_enable_feature(kFeature2Name);
  EXPECT_TRUE(processed_study.Init(&study, false));
  // Since there's multiple different features, |associated_features| should now
  // contain them all.
  EXPECT_THAT(processed_study.associated_features(),
              ElementsAre(kFeature1Name, kFeature2Name));

  exp1->clear_feature_association();
  exp1->mutable_feature_association()->add_enable_feature(kFeature1Name);
  exp2->mutable_feature_association()->add_enable_feature(kFeature1Name);
  exp3->mutable_feature_association()->add_disable_feature(kFeature1Name);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_THAT(processed_study.associated_features(),
              ElementsAre(kFeature1Name));

  // Setting a different feature name on exp2 should cause |associated_features|
  // to contain both feature names.
  exp2->mutable_feature_association()->set_enable_feature(0, kFeature2Name);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_THAT(processed_study.associated_features(),
              ElementsAre(kFeature1Name, kFeature2Name));

  // Setting a different activation type should result in empty
  // |associated_features|.
  study.set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_THAT(processed_study.associated_features(), IsEmpty());
}

TEST_F(VariationsSeedProcessorTest, ProcessedStudyAllAssignmentsToOneGroup) {
  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("def", 100, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());

  AddExperiment("abc", 0, &study);
  AddExperiment("flag", 0, &study)->set_forcing_flag(kForcingFlag1);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());

  AddExperiment("xyz", 1, &study);
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());

  // Try with default group and first group being at 0.
  Study study2;
  study2.set_default_experiment_name("def");
  AddExperiment("def", 0, &study2);
  AddExperiment("xyz", 34, &study2);
  EXPECT_TRUE(processed_study.Init(&study2, false));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());
  AddExperiment("abc", 12, &study2);
  EXPECT_TRUE(processed_study.Init(&study2, false));
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());
}

TEST_F(VariationsSeedProcessorTest, VariationParams) {
  Study study;
  study.set_name("Study1");
  study.set_default_experiment_name("B");

  Study_Experiment* experiment1 = AddExperiment("A", 1, &study);
  Study_Experiment_Param* param = experiment1->add_param();
  param->set_name("x");
  param->set_value("y");

  Study_Experiment* experiment2 = AddExperiment("B", 0, &study);

  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ("y", GetVariationParamValue("Study1", "x"));

  study.set_name("Study2");
  experiment1->set_probability_weight(0);
  experiment2->set_probability_weight(1);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(std::string(), GetVariationParamValue("Study2", "x"));
}

TEST_F(VariationsSeedProcessorTest, VariationParamsWithForcingFlag) {
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  ASSERT_EQ(kForcingFlag1, study.experiment(1).forcing_flag());
  Study_Experiment_Param* param = study.mutable_experiment(1)->add_param();
  param->set_name("x");
  param->set_value("y");

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study.name()));
  EXPECT_EQ("y", GetVariationParamValue(study.name(), "x"));
}

TEST_F(VariationsSeedProcessorTest, StartsActive) {
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
  study2->set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);

  Study* study3 = seed.add_study();
  study3->set_name("C");
  study3->set_default_experiment_name("Default");
  AddExperiment("CC", 100, study3);
  AddExperiment("Default", 0, study3);
  study3->set_activation_type(Study_ActivationType_ACTIVATE_ON_QUERY);

  ClientFilterableState client_state({});
  client_state.locale = "en-CA";
  client_state.reference_date = base::Time::Now();
  client_state.version = base::Version("20.0.0.0");
  client_state.channel = Study::STABLE;
  client_state.form_factor = Study::DESKTOP;
  client_state.platform = Study::PLATFORM_ANDROID;

  VariationsSeedProcessor seed_processor;
  seed_processor.CreateTrialsFromSeed(seed, client_state,
                                      override_callback_.callback(), nullptr,
                                      base::FeatureList::GetInstance());

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

TEST_F(VariationsSeedProcessorTest, StartsActiveWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  study.set_activation_type(Study_ActivationType_ACTIVATE_ON_STARTUP);

  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kFlagStudyName));

  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, ForcingFlagAlreadyForced) {
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  ASSERT_EQ(kNonFlagGroupName, study.experiment(0).name());
  Study_Experiment_Param* param = study.mutable_experiment(0)->add_param();
  param->set_name("x");
  param->set_value("y");
  study.mutable_experiment(0)->set_google_web_experiment_id(kExperimentId);

  base::FieldTrialList::CreateFieldTrial(kFlagStudyName, kNonFlagGroupName);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  EXPECT_TRUE(CreateTrialFromStudy(study));
  // The previously forced experiment should still hold.
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(study.name()));

  // Check that params and experiment ids correspond.
  EXPECT_EQ("y", GetVariationParamValue(study.name(), "x"));
  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, kFlagStudyName,
                                        kNonFlagGroupName);
  EXPECT_EQ(kExperimentId, id);
}

TEST_F(VariationsSeedProcessorTest, FeatureEnabledOrDisableByTrial) {
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

  for (size_t i = 0; i < base::size(test_cases); i++) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    // Needed for base::FeatureList::GetInstance() when creating field trials.
    base::test::ScopedFeatureList base_scoped_feature_list;
    base_scoped_feature_list.Init();

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

    Study study;
    study.set_name("Study1");
    study.set_default_experiment_name("B");
    AddExperiment("B", 0, &study);

    Study_Experiment* experiment = AddExperiment("A", 1, &study);
    Study_Experiment_FeatureAssociation* association =
        experiment->mutable_feature_association();
    if (test_case.enable_feature)
      association->add_enable_feature(test_case.enable_feature);
    else if (test_case.disable_feature)
      association->add_disable_feature(test_case.disable_feature);

    EXPECT_TRUE(CreateTrialFromStudyWithFeatureList(study, feature_list.get()));
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // |kUnrelatedFeature| should not be affected.
    EXPECT_FALSE(base::FeatureList::IsEnabled(kUnrelatedFeature));

    // Before the associated feature is queried, the trial shouldn't be active.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study.name()));

    EXPECT_EQ(test_case.expected_feature_off_state,
              base::FeatureList::IsEnabled(kFeatureOffByDefault));
    EXPECT_EQ(test_case.expected_feature_on_state,
              base::FeatureList::IsEnabled(kFeatureOnByDefault));

    // The field trial should get activated if it had a feature association.
    const bool expected_field_trial_active =
        test_case.enable_feature || test_case.disable_feature;
    EXPECT_EQ(expected_field_trial_active,
              base::FieldTrialList::IsTrialActive(study.name()));
  }
}

TEST_F(VariationsSeedProcessorTest, FeatureAssociationAndForcing) {
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

  for (size_t i = 0; i < base::size(test_cases); i++) {
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

    Study study;
    study.set_name("Study1");
    study.set_default_experiment_name(kDefaultGroup);
    AddExperiment(kDefaultGroup, group == DEFAULT_GROUP ? 1 : 0, &study);

    Study_Experiment* feature_enable =
        AddExperiment(kEnabledGroup, group == ENABLE_GROUP ? 1 : 0, &study);
    feature_enable->mutable_feature_association()->add_enable_feature(
        test_case.feature.name);

    Study_Experiment* feature_disable =
        AddExperiment(kDisabledGroup, group == DISABLE_GROUP ? 1 : 0, &study);
    feature_disable->mutable_feature_association()->add_disable_feature(
        test_case.feature.name);

    AddExperiment(kForcedOnGroup, 0, &study)
        ->mutable_feature_association()
        ->set_forcing_feature_on(test_case.feature.name);
    AddExperiment(kForcedOffGroup, 0, &study)
        ->mutable_feature_association()
        ->set_forcing_feature_off(test_case.feature.name);

    EXPECT_TRUE(CreateTrialFromStudyWithFeatureList(study, feature_list.get()));
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // Trial should not be activated initially, but later might get activated
    // depending on the expected values.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(study.name()));
    EXPECT_EQ(test_case.expected_feature_state,
              base::FeatureList::IsEnabled(test_case.feature));
    EXPECT_EQ(test_case.expected_trial_activated,
              base::FieldTrialList::IsTrialActive(study.name()));
  }
}

TEST_F(VariationsSeedProcessorTest, FeaturesInExpiredStudies) {
  struct base::Feature kDisabledFeature {
    "kDisabledFeature", base::FEATURE_DISABLED_BY_DEFAULT
  };
  struct base::Feature kEnabledFeature {
    "kEnabledFeature", base::FEATURE_ENABLED_BY_DEFAULT
  };
  const base::Time now = base::Time::Now();
  const base::Time year_ago = now - base::TimeDelta::FromDays(365);
  const base::Time year_later = now + base::TimeDelta::FromDays(365);

  struct {
    const base::Feature& feature;
    bool study_force_feature_state;
    base::Time expiry_date;
    bool expected_feature_enabled;
  } test_cases[] = {
      {kDisabledFeature, true, year_ago, false},
      {kDisabledFeature, true, year_later, true},
      {kEnabledFeature, false, year_ago, true},
      {kEnabledFeature, false, year_later, false},
  };

  for (size_t i = 0; i < base::size(test_cases); i++) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(
        base::StringPrintf("Test[%" PRIuS "]: %s", i, test_case.feature.name));

    // Needed for base::FeatureList::GetInstance() when creating field trials.
    base::test::ScopedFeatureList base_scoped_feature_list;
    base_scoped_feature_list.Init();

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(std::string(), std::string());

    // Expired study with a 100% feature group and a default group that has no
    // feature association.
    Study study;
    study.set_name("Study1");
    study.set_default_experiment_name("Default");

    study.set_expiry_date(TimeToProtoTime(test_case.expiry_date));

    AddExperiment("Default", 0, &study);
    Study_Experiment* feature_experiment = AddExperiment("Feature", 1, &study);
    if (test_case.study_force_feature_state) {
      feature_experiment->mutable_feature_association()->add_enable_feature(
          test_case.feature.name);
    } else {
      feature_experiment->mutable_feature_association()->add_disable_feature(
          test_case.feature.name);
    }

    EXPECT_TRUE(CreateTrialFromStudyWithFeatureList(study, feature_list.get()));
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    // The feature should not be enabled, because the study is expired.
    EXPECT_EQ(test_case.expected_feature_enabled,
              base::FeatureList::IsEnabled(test_case.feature));
  }
}

TEST_F(VariationsSeedProcessorTest, NoDefaultExperiment) {
  Study study;
  study.set_name("Study1");

  AddExperiment("A", 1, &study);

  EXPECT_TRUE(CreateTrialFromStudy(study));

  base::FieldTrial* trial = base::FieldTrialList::Find("Study1");
  trial->Disable();

  EXPECT_EQ(ProcessedStudy::kGenericDefaultExperimentName,
            base::FieldTrialList::FindFullName("Study1"));
}

TEST_F(VariationsSeedProcessorTest, ExistingFieldTrial_ExpiredByConfig) {
  static struct base::Feature kFeature {
    "FeatureName", base::FEATURE_ENABLED_BY_DEFAULT
  };

  // In this case, an existing forced trial exists with a different default
  // group than the study config, which is expired. This tests that we don't
  // crash in such a case.
  auto* trial = base::FieldTrialList::FactoryGetFieldTrial(
      "Study1", 100, "ExistingDefault", base::FieldTrial::SESSION_RANDOMIZED,
      nullptr);
  trial->AppendGroup("A", 100);
  trial->SetForced();

  Study study;
  study.set_name("Study1");
  const base::Time year_ago =
      base::Time::Now() - base::TimeDelta::FromDays(365);
  study.set_expiry_date(TimeToProtoTime(year_ago));
  auto* exp1 = AddExperiment("A", 1, &study);
  exp1->mutable_feature_association()->add_enable_feature(kFeature.name);
  AddExperiment("Default", 1, &study);
  study.set_default_experiment_name("Default");

  EXPECT_TRUE(CreateTrialFromStudy(study));

  // The expected effect is that processing the server config will expire
  // the existing trial.
  EXPECT_EQ("ExistingDefault", trial->group_name());
}

TEST_F(VariationsSeedProcessorTest, ExpiredStudy_NoDefaultGroup) {
  static struct base::Feature kFeature {
    "FeatureName", base::FEATURE_ENABLED_BY_DEFAULT
  };

  // Although it's not expected for the server to provide a study with an expiry
  // date set, but not default experiment, this tests that we don't crash if
  // that happens.
  Study study;
  study.set_name("Study1");
  const base::Time year_ago =
      base::Time::Now() - base::TimeDelta::FromDays(365);
  study.set_expiry_date(TimeToProtoTime(year_ago));
  auto* exp1 = AddExperiment("A", 1, &study);
  exp1->mutable_feature_association()->add_enable_feature(kFeature.name);

  EXPECT_FALSE(study.has_default_experiment_name());
  EXPECT_TRUE(CreateTrialFromStudy(study));
  EXPECT_EQ("VariationsDefaultExperiment",
            base::FieldTrialList::FindFullName("Study1"));
}

TEST_F(VariationsSeedProcessorTest, LowEntropyStudyTest) {
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

  // An entorpy value of 0.1 will cause the AA group to be chosen, since AA is
  // the only non-default group, and has a probability percent above 0.1.
  base::test::ScopedFieldTrialListResetter resetter;
  base::FieldTrialList field_trial_list(
      std::make_unique<base::MockEntropyProvider>(0.1));

  // Use a stack instance, since nothing takes ownership of this provider.
  // This entropy value will cause the default group to be chosen since it's a
  // 50/50 trial.
  base::MockEntropyProvider mock_low_entropy_provider(0.9);

  EXPECT_TRUE(CreateTrialFromStudyWithEntropyOverride(
      *study1, &mock_low_entropy_provider));
  EXPECT_TRUE(CreateTrialFromStudyWithEntropyOverride(
      *study2, &mock_low_entropy_provider));

  // Since no experiment in study1 sends experiment IDs, it will use the high
  // entropy provider, which selects the non-default group.
  EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrial1Name));

  // Since an experiment in study2 has google_web_experiment_id set, it will use
  // the low entropy provider, which selects the default group.
  EXPECT_EQ(kDefaultName, base::FieldTrialList::FindFullName(kTrial2Name));
}

}  // namespace variations
