// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations_layers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

const char kInvalidStudyReasonHistogram[] = "Variations.InvalidStudyReason";
const uint32_t kMaxProbabilityValue =
    std::numeric_limits<base::FieldTrial::Probability>::max();

// Adds an experiment with the given name and probability to a study.
Study::Experiment* AddExperiment(const std::string& name,
                                 uint32_t probability,
                                 Study* study) {
  Study::Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

// Creates a study with the given name.
Study CreateStudy(const std::string& name) {
  Study study;
  study.set_name(name);
  return study;
}

// Creates a valid study named "Study". This study has min and max version
// filters, min and max OS version filters, and three groups: Default, Enabled,
// and Disabled. The Enabled and Disabled groups have GWS IDs. The study's
// default experiment is the Default group.
Study CreateValidStudy() {
  Study study = CreateStudy("Study");

  Study::Filter* filter = study.mutable_filter();
  filter->set_min_version("1.1.*");
  filter->set_max_version("2.2.2.2");
  filter->set_min_os_version("1");
  filter->set_max_os_version("2.*");

  Study::Experiment* default_experiment = AddExperiment("Default", 0, &study);

  Study::Experiment* enabled_experiment = AddExperiment("Enabled", 50, &study);
  enabled_experiment->set_google_web_experiment_id(1);

  Study::Experiment* disabled_experiment =
      AddExperiment("Disabled", 50, &study);
  disabled_experiment->set_google_web_experiment_id(2);

  study.set_default_experiment_name(default_experiment->name());

  return study;
}

}  // namespace

TEST(ProcessedStudyTest, InitValidStudy) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  histogram_tester.ExpectTotalCount(kInvalidStudyReasonHistogram, 0);
}

TEST(ProcessedStudyTest, InitInvalidStudyName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.set_name("Not,Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(kInvalidStudyReasonHistogram,
                                      InvalidStudyReason::kInvalidStudyName, 1);
}

TEST(ProcessedStudyTest, InitInvalidExperimentName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_experiment(0)->set_name("Not<Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidExperimentName,
      1);
}

TEST(ProcessedStudyTest, InitInvalidEnableFeatureName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_experiment(0)
      ->mutable_feature_association()
      ->add_enable_feature("Not,Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidFeatureName, 1);
}

TEST(ProcessedStudyTest, InitInvalidDisableFeatureName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_experiment(0)
      ->mutable_feature_association()
      ->add_disable_feature("Not\252Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidFeatureName, 1);
}

TEST(ProcessedStudyTest, InitInvalidForcingFeatureOnName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  auto* experiment = study.add_experiment();
  experiment->set_name("Forced");
  experiment->mutable_feature_association()->set_forcing_feature_on(
      "Not*Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidFeatureName, 1);
}

TEST(ProcessedStudyTest, InitInvalidForcingFeatureOffName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  auto* experiment = study.add_experiment();
  experiment->set_name("Forced");
  experiment->mutable_feature_association()->set_forcing_feature_off(
      "Not,Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidFeatureName, 1);
}

TEST(ProcessedStudyTest, InitInvalidForcingFlag) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  auto* experiment = study.add_experiment();
  experiment->set_name("Forced");
  experiment->set_forcing_flag("Not,Valid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidForcingFlag, 1);
}

// Verifies that a study with an invalid min version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMinVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_min_version("invalid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidMinVersion, 1);
}

// Verifies that a study with an invalid max version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMaxVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_max_version("1.invalid.1");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidMaxVersion, 1);
}

// Verifies that a study with an invalid min OS version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMinOsVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_min_os_version("0.*.0");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(kInvalidStudyReasonHistogram,
                                      InvalidStudyReason::kInvalidMinOsVersion,
                                      1);
}

// Verifies that a study with an invalid max OS version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMaxOsVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_max_os_version("\001\000\000\003");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(kInvalidStudyReasonHistogram,
                                      InvalidStudyReason::kInvalidMaxOsVersion,
                                      1);
}

// Verifies that a study with a blank study name is invalid.
TEST(ProcessedStudyTest, InitBlankStudyName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.set_name("");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(kInvalidStudyReasonHistogram,
                                      InvalidStudyReason::kBlankStudyName, 1);
}

// Verifies that a study with an experiment that has no name is invalid.
TEST(ProcessedStudyTest, InitMissingExperimentName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  AddExperiment("", 0, &study);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kMissingExperimentName,
      1);
}

// Verifies that a study with multiple experiments that are named the same is
// invalid.
TEST(ProcessedStudyTest, InitRepeatedExperimentName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  AddExperiment("Group", 0, &study);
  AddExperiment("Group", 0, &study);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kRepeatedExperimentName,
      1);
}

// Verifies that a study with an experiment that specified both a trigger and
// non-trigger GWS id is invalid.
TEST(ProcessedStudyTest, InitTriggerAndNonTriggerExperimentId) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  Study::Experiment* experiment = AddExperiment("Group", 0, &study);
  experiment->set_google_web_experiment_id(123);
  experiment->set_google_web_trigger_experiment_id(123);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kTriggerAndNonTriggerExperimentId, 1);
}

// Verifies that a study with an experiment that has a probability over the
// maximum is invalid.
TEST(ProcessedStudyTest, InitExperimentProbabilityOverflow) {
  base::HistogramTester histogram_tester;

  Study study = CreateStudy("Study");

  AddExperiment("Group", kMaxProbabilityValue + 1, &study);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kExperimentProbabilityOverflow, 1);
}

// Verifies that a study with groups whose total probability is over the maximum
// is invalid.
TEST(ProcessedStudyTest, InitTotalProbabilityOverflow) {
  base::HistogramTester histogram_tester;

  Study study = CreateStudy("Study");

  AddExperiment("Group1", kMaxProbabilityValue, &study);
  AddExperiment("Group2", 1, &study);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kTotalProbabilityOverflow, 1);
}

// Verifies that a study that specifies a default experiment name but does not
// contain an experiment with that name is invalid.
TEST(ProcessedStudyTest, InitMissingDefaultExperimentInList) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  study.set_default_experiment_name("NonExistentGroup");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kMissingDefaultExperimentInList, 1);
}

TEST(ProcessedStudyTest, ValidateStudy) {
  Study study;
  study.set_name("study");
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study::Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  EXPECT_EQ(300, processed_study.total_probability());
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study));

  // A blank default study is allowed.
  study.clear_default_experiment_name();
  EXPECT_TRUE(processed_study.Init(&study));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study));
  Study::Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study));
}

TEST(ProcessedStudyTest, ProcessedStudyAllAssignmentsToOneGroup) {
  Study study;   // Must outlive `processed_study`
  Study study2;  // Must outlive `processed_study`

  study.set_name("study1");
  study.set_default_experiment_name("def");
  AddExperiment("def", 100, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());

  AddExperiment("abc", 0, &study);
  AddExperiment("flag", 0, &study)->set_forcing_flag("flag_test1");
  EXPECT_TRUE(processed_study.Init(&study));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());

  AddExperiment("xyz", 1, &study);
  EXPECT_TRUE(processed_study.Init(&study));
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());

  // Try with default group and first group being at 0.
  study2.set_name("study2");
  study2.set_default_experiment_name("def");
  AddExperiment("def", 0, &study2);
  AddExperiment("xyz", 34, &study2);
  EXPECT_TRUE(processed_study.Init(&study2));
  EXPECT_TRUE(processed_study.all_assignments_to_one_group());
  AddExperiment("abc", 12, &study2);
  EXPECT_TRUE(processed_study.Init(&study2));
  EXPECT_FALSE(processed_study.all_assignments_to_one_group());
}

}  // namespace variations
