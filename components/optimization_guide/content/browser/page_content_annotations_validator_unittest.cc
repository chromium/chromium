// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_validator.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(PageContentAnnotationsValidatorTest, DoesNothing) {
  TestPageContentAnnotator annotator;
  EXPECT_EQ(nullptr, PageContentAnnotationsValidator::MaybeCreateAndStartTimer(
                         &annotator));
}

TEST(PageContentAnnotationsValidatorTest, NoAnnotator) {
  TestPageContentAnnotator annotator;
  EXPECT_EQ(nullptr,
            PageContentAnnotationsValidator::MaybeCreateAndStartTimer(nullptr));
}

TEST(PageContentAnnotationsValidatorTest,
     DoesNothing_FeatureEnabledButNoTypes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPageContentAnnotationsValidation);

  TestPageContentAnnotator annotator;
  EXPECT_EQ(nullptr, PageContentAnnotationsValidator::MaybeCreateAndStartTimer(
                         &annotator));
}

TEST(PageContentAnnotationsValidatorTest, AllEnabledByExperiment) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {
          {"PageEntities", "true"},
          {"ContentVisibility", "true"},
          {"TextEmbedding", "true"},
      });

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  task_env.FastForwardBy(base::Seconds(30));

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(3U, annotation_requests.size());
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kPageEntities);
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kContentVisibility);
  EXPECT_EQ(annotation_requests[2].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, AllEnabledByCommandLine) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationPageEntities,
      "page entities,pe input2, pe keeps whitespace  ");
  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationContentVisibility,
      "content viz,cv input2, cv keeps whitespace  ");
  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationTextEmbedding,
      "text embedding,te input2, te keeps whitespace  ");

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  task_env.FastForwardBy(base::Seconds(30));

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(3U, annotation_requests.size());

  EXPECT_THAT(annotation_requests[0].first, testing::ElementsAreArray({
                                                "page entities",
                                                "pe input2",
                                                " pe keeps whitespace  ",
                                            }));
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kPageEntities);

  EXPECT_THAT(annotation_requests[1].first, testing::ElementsAreArray({
                                                "content viz",
                                                "cv input2",
                                                " cv keeps whitespace  ",
                                            }));
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kContentVisibility);

  EXPECT_THAT(annotation_requests[2].first, testing::ElementsAreArray({
                                                "text embedding",
                                                "te input2",
                                                " te keeps whitespace  ",
                                            }));
  EXPECT_EQ(annotation_requests[2].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, OnlyOneEnabled_Cmd) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  for (AnnotationType type : {
           AnnotationType::kPageEntities,
           AnnotationType::kContentVisibility,
           AnnotationType::kTextEmbedding,
       }) {
    SCOPED_TRACE(AnnotationTypeToString(type));
    base::test::ScopedCommandLine scoped_cmd;
    base::CommandLine* cmd = scoped_cmd.GetProcessCommandLine();

    switch (type) {
      case AnnotationType::kPageEntities:
        cmd->AppendSwitch(
            switches::kPageContentAnnotationsValidationPageEntities);
        break;
      case AnnotationType::kContentVisibility:
        cmd->AppendSwitch(
            switches::kPageContentAnnotationsValidationContentVisibility);
        break;
      case AnnotationType::kTextEmbedding:
        cmd->AppendSwitch(
            switches::kPageContentAnnotationsValidationTextEmbedding);
        break;
      default:
        break;
    }

    TestPageContentAnnotator annotator;
    auto validator =
        PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
    ASSERT_TRUE(validator);
    task_env.FastForwardBy(base::Seconds(30));

    const auto& annotation_requests = annotator.annotation_requests();
    ASSERT_EQ(1U, annotation_requests.size());

    EXPECT_EQ(annotation_requests[0].second, type);
  }
}

TEST(PageContentAnnotationsValidatorTest, OnlyOneEnabled_Feature) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  for (AnnotationType type : {
           AnnotationType::kPageEntities,
           AnnotationType::kContentVisibility,
           AnnotationType::kTextEmbedding,
       }) {
    SCOPED_TRACE(AnnotationTypeToString(type));
    base::test::ScopedFeatureList scoped_feature_list;

    switch (type) {
      case AnnotationType::kPageEntities:
        scoped_feature_list.InitAndEnableFeatureWithParameters(
            features::kPageContentAnnotationsValidation,
            {{"PageEntities", "true"}});
        break;
      case AnnotationType::kContentVisibility:
        scoped_feature_list.InitAndEnableFeatureWithParameters(
            features::kPageContentAnnotationsValidation,
            {{"ContentVisibility", "true"}});
        break;
      case AnnotationType::kTextEmbedding:
        scoped_feature_list.InitAndEnableFeatureWithParameters(
            features::kPageContentAnnotationsValidation,
            {{"TextEmbedding", "true"}});
        break;
      default:
        break;
    }

    TestPageContentAnnotator annotator;
    auto validator =
        PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
    ASSERT_TRUE(validator);
    task_env.FastForwardBy(base::Seconds(30));

    const auto& annotation_requests = annotator.annotation_requests();
    ASSERT_EQ(1U, annotation_requests.size());
    EXPECT_EQ(annotation_requests[0].second, type);
  }
}

TEST(PageContentAnnotationsValidatorTest, TimerDelayByCmd) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  cmd->AppendSwitch(
      switches::kPageContentAnnotationsValidationContentVisibility);
  cmd->AppendSwitch(switches::kPageContentAnnotationsValidationTextEmbedding);
  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationStartupDelaySeconds, "5");

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  EXPECT_TRUE(annotator.annotation_requests().empty());

  task_env.FastForwardBy(base::Milliseconds(4999));
  EXPECT_TRUE(annotator.annotation_requests().empty());

  task_env.FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(annotator.annotation_requests().empty());

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(2U, annotation_requests.size());
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kContentVisibility);
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, TimerDelayByFeature) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {
          {"ContentVisibility", "true"},
          {"TextEmbedding", "true"},
          {"startup_delay", "5"},
      });

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  EXPECT_TRUE(annotator.annotation_requests().empty());

  task_env.FastForwardBy(base::Milliseconds(4999));
  EXPECT_TRUE(annotator.annotation_requests().empty());

  task_env.FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(annotator.annotation_requests().empty());

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(2U, annotation_requests.size());
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kContentVisibility);
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, BatchSizeByCmd) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  cmd->AppendSwitch(
      switches::kPageContentAnnotationsValidationContentVisibility);
  cmd->AppendSwitch(switches::kPageContentAnnotationsValidationTextEmbedding);
  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationBatchSizeOverride, "5");

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  task_env.FastForwardBy(base::Seconds(30));

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(2U, annotation_requests.size());
  EXPECT_EQ(annotation_requests[0].first.size(), 5U);
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kContentVisibility);
  EXPECT_EQ(annotation_requests[1].first.size(), 5U);
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, BatchSizeByFeature) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {
          {"ContentVisibility", "true"},
          {"TextEmbedding", "true"},
          {"batch_size", "5"},
      });

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  task_env.FastForwardBy(base::Seconds(30));

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(2U, annotation_requests.size());
  EXPECT_EQ(annotation_requests[0].first.size(), 5U);
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kContentVisibility);
  EXPECT_EQ(annotation_requests[1].first.size(), 5U);
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kTextEmbedding);
}

TEST(PageContentAnnotationsValidatorTest, CommandOverridesFeature) {
  base::test::TaskEnvironment task_env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotationsValidation,
      {
          {"PageEntities", "true"},
          {"ContentVisibility", "true"},
          {"TextEmbedding", "true"},
          {"batch_size", "3"},
      });

  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationContentVisibility,
      "content visibility");
  cmd->AppendSwitchASCII(
      switches::kPageContentAnnotationsValidationBatchSizeOverride, "5");

  TestPageContentAnnotator annotator;
  auto validator =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(&annotator);
  ASSERT_TRUE(validator);
  task_env.FastForwardBy(base::Seconds(30));

  const auto& annotation_requests = annotator.annotation_requests();
  ASSERT_EQ(3U, annotation_requests.size());

  EXPECT_EQ(annotation_requests[0].first.size(), 5U);
  EXPECT_EQ(annotation_requests[0].second, AnnotationType::kPageEntities);

  EXPECT_THAT(annotation_requests[1].first,
              testing::ElementsAre("content visibility"));
  EXPECT_EQ(annotation_requests[1].second, AnnotationType::kContentVisibility);

  EXPECT_EQ(annotation_requests[2].first.size(), 5U);
  EXPECT_EQ(annotation_requests[2].second, AnnotationType::kTextEmbedding);
}

}  // namespace optimization_guide