// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/post_processor/post_processor.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

// Labels for BinaryClassifier.
const char kNotShowShare[] = "Not Show Share";
const char kShowShare[] = "Show Share";

// TTL for BinaryClassifier labels.
const int kShowShareTTL = 3;
const int kDefaultTTL = 5;

// Labels for MultiClassClassifier.
const char kNewTabUser[] = "NewTab";
const char kShareUser[] = "Share";
const char kShoppingUser[] = "Shopping";
const char kVoiceUser[] = "Voice";

// Labels for BinnedClassifier.
const char kLowUsed[] = "Low";
const char kMediumUsed[] = "Medium";
const char kHighUsed[] = "High";
const char kUnderflowLabel[] = "Underflow";

proto::OutputConfig GetTestOutputConfigForBinaryClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5, /*positive_label=*/kShowShare,
      /*negative_label=*/kNotShowShare);

  writer.AddPredictedResultTTLInOutputConfig({{kShowShare, kShowShareTTL}},
                                             kDefaultTTL, proto::TimeUnit::DAY);

  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForMultiClassClassifier(
    int top_k_outputs,
    absl::optional<float> threshold) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  std::array<const char*, 4> labels{kShareUser, kNewTabUser, kVoiceUser,
                                    kShoppingUser};
  writer.AddOutputConfigForMultiClassClassifier(labels.begin(), labels.size(),
                                                top_k_outputs, threshold);
  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForBinnedClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{0.2, kLowUsed}, {0.3, kMediumUsed}, {0.5, kHighUsed}},
      kUnderflowLabel);
  return model_metadata.output_config();
}

}  // namespace

TEST(PostProcessorTest, BinaryClassifierScoreGreaterThanThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> selected_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.6}, GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(selected_label, testing::ElementsAre(kShowShare));
}

TEST(PostProcessorTest, BinaryClassifierScoreGreaterEqualToThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> selected_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5}, GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(selected_label, testing::ElementsAre(kShowShare));
}

TEST(PostProcessorTest, BinaryClassifierScoreGreaterLessThanThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> selected_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.4}, GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(selected_label, testing::ElementsAre(kNotShowShare));
}

TEST(PostProcessorTest, MultiClassClassifierWithTopKLessThanElements) {
  PostProcessor post_processor;
  std::vector<std::string> top_k_labels = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
          GetTestOutputConfigForMultiClassClassifier(
              /*top_k-outputs=*/2,
              /*threshold=*/absl::nullopt),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(top_k_labels, testing::ElementsAre(kShoppingUser, kShareUser));
}

TEST(PostProcessorTest, MultiClassClassifierWithTopKEqualToElements) {
  PostProcessor post_processor;
  std::vector<std::string> top_k_labels = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
          GetTestOutputConfigForMultiClassClassifier(
              /*top_k-outputs=*/4,
              /*threshold=*/absl::nullopt),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(top_k_labels, testing::ElementsAre(kShoppingUser, kShareUser,
                                                 kVoiceUser, kNewTabUser));
}

TEST(PostProcessorTest, MultiClassClassifierWithThresholdBetweenModelResult) {
  PostProcessor post_processor;
  std::vector<std::string> top_k_labels = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
          GetTestOutputConfigForMultiClassClassifier(/*top_k-outputs=*/4,
                                                     /*threshold=*/0.4),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(top_k_labels,
              testing::ElementsAre(kShoppingUser, kShareUser, kVoiceUser));
}

TEST(PostProcessorTest,
     MultiClassClassifierWithThresholdGreaterThanModelResult) {
  PostProcessor post_processor;
  std::vector<std::string> top_k_labels = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
          GetTestOutputConfigForMultiClassClassifier(/*top_k-outputs=*/4,
                                                     /*threshold=*/0.8),
          /*timestamp=*/base::Time::Now()));
  EXPECT_TRUE(top_k_labels.empty());
}

TEST(PostProcessorTest,
     MultiClassClassifierWithThresholdLesserThanModelResult) {
  PostProcessor post_processor;
  std::vector<std::string> top_k_labels = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
          GetTestOutputConfigForMultiClassClassifier(/*top_k-outputs=*/2,
                                                     /*threshold=*/0.1),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(top_k_labels, testing::ElementsAre(kShoppingUser, kShareUser));
}

TEST(PostProcessorTest, BinnedClassifierScoreGreaterThanHighUserThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> winning_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.6}, GetTestOutputConfigForBinnedClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(winning_label, testing::ElementsAre(kHighUsed));
}

TEST(PostProcessorTest, BinnedClassifierScoreGreaterThanMediumUserThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> winning_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.4}, GetTestOutputConfigForBinnedClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(winning_label, testing::ElementsAre(kMediumUsed));
}

TEST(PostProcessorTest, BinnedClassifierScoreGreaterThanLowUserThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> winning_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.24}, GetTestOutputConfigForBinnedClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(winning_label, testing::ElementsAre(kLowUsed));
}

TEST(PostProcessorTest, BinnedClassifierScoreEqualToLowUserThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> winning_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.2}, GetTestOutputConfigForBinnedClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(winning_label, testing::ElementsAre(kLowUsed));
}

TEST(PostProcessorTest, BinnedClassifierScoreLessThanLowUserThreshold) {
  PostProcessor post_processor;
  std::vector<std::string> winning_label = post_processor.GetClassifierResults(
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.1}, GetTestOutputConfigForBinnedClassifier(),
          /*timestamp=*/base::Time::Now()));
  EXPECT_THAT(winning_label, testing::ElementsAre(kUnderflowLabel));
}

TEST(PostProcessorTest,
     GetPostProcessedClassificationResultWithEmptyPredResult) {
  PostProcessor post_processor;
  proto::PredictionResult pred_result;
  ClassificationResult classification_result =
      post_processor.GetPostProcessedClassificationResult(
          pred_result, PredictionStatus::kFailed);
  EXPECT_THAT(classification_result.status, PredictionStatus::kFailed);
  EXPECT_TRUE(classification_result.ordered_labels.empty());
}

TEST(PostProcessorTest,
     GetPostProcessedClassificationResultWithNonEmptyPredResult) {
  PostProcessor post_processor;
  proto::PredictionResult pred_result = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.5, 0.2, 0.4, 0.7},
      GetTestOutputConfigForMultiClassClassifier(
          /*top_k-outputs=*/2,
          /*threshold=*/absl::nullopt),
      /*timestamp=*/base::Time::Now());
  ClassificationResult classification_result =
      post_processor.GetPostProcessedClassificationResult(
          pred_result, PredictionStatus::kSucceeded);
  EXPECT_THAT(classification_result.status, PredictionStatus::kSucceeded);
  EXPECT_THAT(classification_result.ordered_labels,
              testing::ElementsAre(kShoppingUser, kShareUser));
}

TEST(PostProcessorTest, GetTTLWhenLabelTTLPresentInMap) {
  PostProcessor post_processor;
  proto::PredictionResult pred_result = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.5}, GetTestOutputConfigForBinaryClassifier(),
      /*timestamp=*/base::Time::Now());
  // ShowShare is selected based on score.
  EXPECT_EQ(base::Days(1) * kShowShareTTL,
            post_processor.GetTTLForPredictedResult(pred_result));
}

TEST(PostProcessorTest, GetTTLWhenLabelTTLNotPresentInMap) {
  PostProcessor post_processor;
  proto::PredictionResult pred_result = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.1}, GetTestOutputConfigForBinaryClassifier(),
      /*timestamp=*/base::Time::Now());
  // NotShowShare is selected based on score.
  EXPECT_EQ(base::Days(1) * kDefaultTTL,
            post_processor.GetTTLForPredictedResult(pred_result));
}

}  // namespace segmentation_platform