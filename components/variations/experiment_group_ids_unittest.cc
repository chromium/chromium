// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_group_ids.h"

#include <cstdint>

#include "base/containers/span.h"
#include "components/variations/proto/study.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

constexpr uint64_t kGenericId = 1234;

struct ExperimentOptions {
  bool set_google_web_experiment_id = false;
  bool set_google_web_trigger_experiment_id = false;
  bool set_google_app_experiment_id = false;
};

Study::Experiment CreateExperiment(uint32_t weight,
                                   const ExperimentOptions& options = {}) {
  Study::Experiment group;
  group.set_name("Group");
  group.set_probability_weight(weight);
  if (options.set_google_web_experiment_id) {
    group.set_google_web_experiment_id(kGenericId);
  }
  if (options.set_google_web_trigger_experiment_id) {
    group.set_google_web_trigger_experiment_id(kGenericId);
  }
  if (options.set_google_app_experiment_id) {
    group.set_google_app_experiment_id(kGenericId);
  }
  return group;
}

Study CreateStudy(base::span<const Study::Experiment> groups,
                  Study::Consistency consistency = Study::PERMANENT) {
  Study study;
  study.set_consistency(consistency);
  for (const Study::Experiment& group : groups) {
    *study.add_experiment() = group;
  }
  return study;
}

TEST(ExperimentGroupIdsTest, WeightedGroupWithoutId) {
  Study::Experiment group = CreateExperiment(/*weight=*/100);
  Study study = CreateStudy({group});

  EXPECT_FALSE(HasGoogleWebExperimentId(group));
  EXPECT_FALSE(HasGoogleWebExperimentId(study));
  EXPECT_FALSE(HasExperimentId(group));
  EXPECT_FALSE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_FALSE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, WeightedGroupWithGoogleWebExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/100, {.set_google_web_experiment_id = true});
  Study study = CreateStudy({group});

  EXPECT_TRUE(HasGoogleWebExperimentId(group));
  EXPECT_TRUE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_TRUE(IsWeightedGroupWithExperimentId(group));
  EXPECT_TRUE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_TRUE(HasWeightedGroupWithExperimentId(study));
  EXPECT_TRUE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, WeightedGroupWithGoogleWebTriggerExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/100, {.set_google_web_trigger_experiment_id = true});
  Study study = CreateStudy({group});

  EXPECT_TRUE(HasGoogleWebExperimentId(group));
  EXPECT_TRUE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_TRUE(IsWeightedGroupWithExperimentId(group));
  EXPECT_TRUE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_TRUE(HasWeightedGroupWithExperimentId(study));
  EXPECT_TRUE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, WeightedGroupWithGoogleAppExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/100, {.set_google_app_experiment_id = true});
  Study study = CreateStudy({group});

  EXPECT_FALSE(HasGoogleWebExperimentId(group));
  EXPECT_FALSE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_TRUE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_TRUE(HasWeightedGroupWithExperimentId(study));
  EXPECT_TRUE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, ZeroWeightGroupWithoutId) {
  Study::Experiment group = CreateExperiment(/*weight=*/0);
  Study study = CreateStudy({group, CreateExperiment(/*weight=*/100)});

  EXPECT_FALSE(HasGoogleWebExperimentId(group));
  EXPECT_FALSE(HasGoogleWebExperimentId(study));
  EXPECT_FALSE(HasExperimentId(group));
  EXPECT_FALSE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_FALSE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, ZeroWeightGroupWithGoogleWebExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/0, {.set_google_web_experiment_id = true});
  Study study = CreateStudy({group, CreateExperiment(/*weight=*/100)});

  EXPECT_TRUE(HasGoogleWebExperimentId(group));
  EXPECT_TRUE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_FALSE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_FALSE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, ZeroWeightGroupWithGoogleWebTriggerExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/0, {.set_google_web_trigger_experiment_id = true});
  Study study = CreateStudy({group, CreateExperiment(/*weight=*/100)});

  EXPECT_TRUE(HasGoogleWebExperimentId(group));
  EXPECT_TRUE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_FALSE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_FALSE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, ZeroWeightGroupWithGoogleAppExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/0, {.set_google_app_experiment_id = true});
  Study study = CreateStudy({group, CreateExperiment(/*weight=*/100)});

  EXPECT_FALSE(HasGoogleWebExperimentId(group));
  EXPECT_FALSE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_FALSE(IsWeightedGroupWithExperimentId(group));
  EXPECT_FALSE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_FALSE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

TEST(ExperimentGroupIdsTest, SessionStudyWithGoogleWebExperimentId) {
  Study::Experiment group = CreateExperiment(
      /*weight=*/100, {.set_google_web_experiment_id = true});
  Study study = CreateStudy({group}, Study::SESSION);

  EXPECT_TRUE(HasGoogleWebExperimentId(group));
  EXPECT_TRUE(HasGoogleWebExperimentId(study));
  EXPECT_TRUE(HasExperimentId(group));
  EXPECT_TRUE(IsWeightedGroupWithExperimentId(group));
  EXPECT_TRUE(HasWeightedGroupWithGoogleWebExperimentId(study));
  EXPECT_TRUE(HasWeightedGroupWithExperimentId(study));
  EXPECT_FALSE(ConsumesEntropy(study));
}

}  // namespace
}  // namespace variations
