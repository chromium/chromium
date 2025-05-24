// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/rank_fetcher_helper.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::SizeIs;

namespace segmentation_platform::home_modules {
namespace {

constexpr char kInput1[] = "input1";
constexpr char kInput2[] = "input_freshness_2";
constexpr char kInput3[] = "input_freshness_3";
constexpr char kInput4[] = "input4";

class RankFetcherHelperTest : public testing::Test {
 public:
  RankFetcherHelperTest() = default;
  ~RankFetcherHelperTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kSegmentationPlatformAndroidHomeModuleRanker,
         features::kSegmentationPlatformIosModuleRanker,
         features::kSegmentationPlatformEphemeralCardRanker,
         features::kSegmentationPlatformIosModuleRanker},
        {});
  }

  void TearDown() override {}

  ClassificationResult FetchRank(RankFetcherHelper& rank_fetcher_helper,
                                 scoped_refptr<InputContext> inputs =
                                     base::MakeRefCounted<InputContext>()) {
    base::test::TestFuture<const ClassificationResult&> result_future;

    rank_fetcher_helper.GetHomeModulesRank(
        &segmentation_service_, PredictionOptions(false, false, false),
        /*input_context=*/inputs, result_future.GetCallback());
    return result_future.Get();
  }

  // Warning: Keep `modules_rank_result` alive till end of the test.
  void ReturnHomeModulesResult(
      const std::vector<std::string>& modules_rank_result) {
    EXPECT_CALL(segmentation_service_, GetClassificationResult(_, _, _, _))
        .Times(1)
        .WillOnce(Invoke([&](const std::string& segmentation_key,
                             const PredictionOptions& prediction_options,
                             scoped_refptr<InputContext> input_context,
                             ClassificationResultCallback callback) {
          ClassificationResult result(PredictionStatus::kSucceeded);
          result.ordered_labels = modules_rank_result;
          std::move(callback).Run(result);
        }));
  }

  // Warning: Keep `card_rank_result` alive till end of the test.
  void ReturnEphermeralResult(
      const std::map<std::string, float>& card_rank_result) {
    EXPECT_CALL(
        segmentation_service_,
        GetAnnotatedNumericResult(kEphemeralHomeModuleBackendKey, _, _, _))
        .Times(1)
        .WillOnce(Invoke([&](const std::string& segmentation_key,
                             const PredictionOptions& prediction_options,
                             scoped_refptr<InputContext> input_context,
                             AnnotatedNumericResultCallback callback) {
          AnnotatedNumericResult result(PredictionStatus::kSucceeded);
          for (const auto& it : card_rank_result) {
            result.result.mutable_output_config()
                ->mutable_predictor()
                ->mutable_generic_predictor()
                ->add_output_labels(it.first);
            result.result.add_result(it.second);
          }
          std::move(callback).Run(result);
        }));
  }

  void ReturnInputKeys(int count) {
    EXPECT_CALL(segmentation_service_, GetInputKeysForModel(_, _))
        .Times(count)
        .WillRepeatedly(Invoke([&](const std::string& segmentation_key,
                                   InputContextKeysCallback callback) {
          std::move(callback).Run({kInput1, kInput2, kInput3, kInput4});
        }));
  }

  void ReturnEphermeralFailure() {
    EXPECT_CALL(
        segmentation_service_,
        GetAnnotatedNumericResult(kEphemeralHomeModuleBackendKey, _, _, _))
        .Times(1)
        .WillOnce(Invoke([&](const std::string& segmentation_key,
                             const PredictionOptions& prediction_options,
                             scoped_refptr<InputContext> input_context,
                             AnnotatedNumericResultCallback callback) {
          AnnotatedNumericResult result(PredictionStatus::kFailed);
          std::move(callback).Run(result);
        }));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  MockSegmentationPlatformService segmentation_service_;
};

TEST_F(RankFetcherHelperTest, GetHomeModulesRankDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {}, {features::kSegmentationPlatformAndroidHomeModuleRanker,
           features::kSegmentationPlatformIosModuleRanker,
           features::kSegmentationPlatformIosModuleRanker});
  RankFetcherHelper rank_fetcher_helper;

  ClassificationResult result = FetchRank(rank_fetcher_helper);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(result.ordered_labels, SizeIs(4));  // Fixed modules size.
#endif
}

TEST_F(RankFetcherHelperTest, GetHomeModulesRankEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {features::kSegmentationPlatformAndroidHomeModuleRanker,
       features::kSegmentationPlatformIosModuleRanker,
       features::kSegmentationPlatformIosModuleRanker},
      {features::kSegmentationPlatformEphemeralCardRanker});
  std::vector<std::string> module_ranks = {"module1", "module3", "module2"};
  RankFetcherHelper rank_fetcher_helper;

  ReturnHomeModulesResult(module_ranks);
  ReturnInputKeys(1);

  ClassificationResult result = FetchRank(rank_fetcher_helper);

  EXPECT_EQ(result.status, PredictionStatus::kSucceeded);
  EXPECT_EQ(result.ordered_labels, module_ranks);
}

TEST_F(RankFetcherHelperTest, MergeResultsAndRunCallback) {
  std::vector<std::string> module_ranks = {"module1", "module3", "module2"};
  std::map<std::string, float> ephemeral_ranks{{"EphemeralModule1", 1.0},
                                               {"EphemeralModule2", 0.5}};
  RankFetcherHelper rank_fetcher_helper;

  ReturnInputKeys(2);
  ReturnHomeModulesResult(module_ranks);
  ReturnEphermeralResult(ephemeral_ranks);

  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      kInput1, processing::ProcessedValue::FromFloat(1));
  input_context->metadata_args.emplace(
      kInput3, processing::ProcessedValue::FromFloat(3));
  ClassificationResult result = FetchRank(rank_fetcher_helper, input_context);

  EXPECT_EQ(result.status, PredictionStatus::kSucceeded);
  std::vector<std::string> expected_result = {
      "EphemeralModule1", "module1", "module3", "module2", "EphemeralModule2"};
  EXPECT_EQ(result.ordered_labels, expected_result);

  EXPECT_NEAR(input_context->GetMetadataArgument(kInput1)->float_val, 1, 0.01);
  EXPECT_NEAR(input_context->GetMetadataArgument(kInput3)->float_val, 3, 0.01);

#if BUILDFLAG(IS_ANDROID)
  // On Android the freshness inputs get backfilled if missing since modules can
  // be disabled.
  EXPECT_NEAR(input_context->GetMetadataArgument(kInput2)->float_val, -1, 0.01);
  EXPECT_FALSE(input_context->GetMetadataArgument(kInput4));
#endif
}

TEST_F(RankFetcherHelperTest, EphemeralCardsEmpty) {
  std::vector<std::string> module_ranks = {"module1", "module3", "module2"};
  std::map<std::string, float> ephemeral_ranks;
  RankFetcherHelper rank_fetcher_helper;

  ReturnInputKeys(2);
  ReturnHomeModulesResult(module_ranks);
  ReturnEphermeralResult(ephemeral_ranks);

  ClassificationResult result = FetchRank(rank_fetcher_helper);

  EXPECT_EQ(result.status, PredictionStatus::kSucceeded);
  EXPECT_EQ(result.ordered_labels, module_ranks);
}

TEST_F(RankFetcherHelperTest, MergeResultsAndRunCallback_Failed) {
  std::vector<std::string> module_ranks = {"module1", "module3", "module2"};
  RankFetcherHelper rank_fetcher_helper;

  ReturnInputKeys(2);
  ReturnHomeModulesResult(module_ranks);
  ReturnEphermeralFailure();

  ClassificationResult result = FetchRank(rank_fetcher_helper);

  EXPECT_EQ(result.status, PredictionStatus::kSucceeded);
  EXPECT_EQ(result.ordered_labels, module_ranks);
}

}  // namespace

}  // namespace segmentation_platform::home_modules
