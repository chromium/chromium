// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <cstdint>

#include "base/test/metrics/histogram_tester.h"
#include "components/variations/proto/study.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

const char kInvalidStudyReasonHistogram[] = "Variations.InvalidStudyReason";
const uint32_t kMaxProbabilityValue =
    std::numeric_limits<base::FieldTrial::Probability>::max();

// Adds an experiment with the given name and probability to a study.
Study::Experiment* AddExperiment(Study& study,
                                 const std::string& name,
                                 uint32_t probability) {
  Study::Experiment* experiment = study.add_experiment();
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

  Study::Experiment* default_experiment = AddExperiment(study, "Default", 0);

  Study::Experiment* enabled_experiment = AddExperiment(study, "Enabled", 50);
  enabled_experiment->set_google_web_experiment_id(1);

  Study::Experiment* disabled_experiment = AddExperiment(study, "Disabled", 50);
  disabled_experiment->set_google_web_experiment_id(2);

  study.set_default_experiment_name(default_experiment->name());

  return study;
}

}  // namespace

TEST(ProcessedStudyTest, InitValidStudy) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  histogram_tester.ExpectTotalCount(kInvalidStudyReasonHistogram, 0);
}

// Verifies that a study with an invalid min version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMinVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_min_version("invalid");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidMinVersion, 1);
}

// Verifies that a study with an invalid max version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMaxVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_max_version("1.invalid.1");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kInvalidMaxVersion, 1);
}

// Verifies that a study with an invalid min OS version filter is invalid.
TEST(ProcessedStudyTest, InitInvalidMinOsVersion) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();
  study.mutable_filter()->set_min_os_version("0.*.0");

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
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
  EXPECT_FALSE(processed_study.Init(&study, false));
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
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(kInvalidStudyReasonHistogram,
                                      InvalidStudyReason::kBlankStudyName, 1);
}

// Verifies that a study with an experiment that has no name is invalid.
TEST(ProcessedStudyTest, InitMissingExperimentName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  AddExperiment(study, "", 0);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kMissingExperimentName,
      1);
}

// Verifies that a study with multiple experiments that are named the same is
// invalid.
TEST(ProcessedStudyTest, InitRepeatedExperimentName) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  AddExperiment(study, "Group", 0);
  AddExperiment(study, "Group", 0);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram, InvalidStudyReason::kRepeatedExperimentName,
      1);
}

// Verifies that a study with an experiment that specified both a trigger and
// non-trigger GWS id is invalid.
TEST(ProcessedStudyTest, InitTriggerAndNonTriggerExperimentId) {
  base::HistogramTester histogram_tester;

  Study study = CreateValidStudy();

  Study::Experiment* experiment = AddExperiment(study, "Group", 0);
  experiment->set_google_web_experiment_id(123);
  experiment->set_google_web_trigger_experiment_id(123);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kTriggerAndNonTriggerExperimentId, 1);
}

// Verifies that a study with an experiment that has a probability over the
// maximum is invalid.
TEST(ProcessedStudyTest, InitExperimentProbabilityOverflow) {
  base::HistogramTester histogram_tester;

  Study study = CreateStudy("Study");

  AddExperiment(study, "Group", kMaxProbabilityValue + 1);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kExperimentProbabilityOverflow, 1);
}

// Verifies that a study with groups whose total probability is over the maximum
// is invalid.
TEST(ProcessedStudyTest, InitTotalProbabilityOverflow) {
  base::HistogramTester histogram_tester;

  Study study = CreateStudy("Study");

  AddExperiment(study, "Group1", kMaxProbabilityValue);
  AddExperiment(study, "Group2", 1);

  ProcessedStudy processed_study;
  EXPECT_FALSE(processed_study.Init(&study, false));
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
  EXPECT_FALSE(processed_study.Init(&study, false));
  histogram_tester.ExpectUniqueSample(
      kInvalidStudyReasonHistogram,
      InvalidStudyReason::kMissingDefaultExperimentInList, 1);
}

}  // namespace variations