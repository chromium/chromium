// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_test_utils.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockOptimizationGuideDecider;
using ::optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback;
using ::optimization_guide::OptimizationGuideDecision;
using ::optimization_guide::OptimizationGuideDecisionCallback;
using ::optimization_guide::OptimizationGuideDecisionWithMetadata;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::proto::Any;
using ::optimization_guide::proto::OptimizationType;
using ::optimization_guide::proto::RequestContext;
using ::optimization_guide::proto::RequestContextMetadata;
using ::testing::_;
using ::testing::A;
using ::testing::NiceMock;
using ::testing::WithArgs;

constexpr int64_t kTestNavigationId = 12345;
constexpr char kTestUrl[] = "https://example.com/test";
constexpr char kTestCandidateId[] = "12345678-1234-5678-1234-567812345678";
constexpr char kTestTaskType1[] = "TASK_TYPE_1";
constexpr char kTestTaskType2[] = "TASK_TYPE_2";
constexpr char kTestTaskTypeShopping[] = "SHOPPING";
constexpr char kTestAttributeKey[] = "key";
constexpr char kTestAttributeValue[] = "value";
constexpr char kGetSupportedTasksResponseUrl[] =
    "type.googleapis.com/multistep_filter.GetSupportedTasksResponse";
constexpr char kExtractTaskAttributesResponseUrl[] =
    "type.googleapis.com/multistep_filter.ExtractTaskAttributesResponse";
constexpr char kGetTaskExecutionStrategiesResponseUrl[] =
    "type.googleapis.com/multistep_filter.GetTaskExecutionStrategiesResponse";

OptimizationMetadata CreateMalformedOptimizationMetadata(
    std::string_view type_url) {
  Any any_metadata;
  any_metadata.set_type_url(type_url);
  any_metadata.set_value("kInvalidProtobufBytes");
  return CreateOptimizationMetadata(any_metadata);
}

class OptimizationGuideAnnotationIndexClientTest : public testing::Test {
 public:
  OptimizationGuideAnnotationIndexClientTest() {
    client_ = std::make_unique<OptimizationGuideAnnotationIndexClient>(
        &mock_decider_, /*log_router=*/nullptr);
  }
  ~OptimizationGuideAnnotationIndexClientTest() override = default;

 protected:
  void SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision decision,
      OptimizationType optimization_type,
      const OptimizationMetadata& metadata) {
    EXPECT_CALL(mock_decider_,
                CanApplyOptimization(GURL(kTestUrl), optimization_type,
                                     A<OptimizationGuideDecisionCallback>()))
        .WillOnce(WithArgs<2>(
            [decision, metadata](OptimizationGuideDecisionCallback callback) {
              std::move(callback).Run(decision, metadata);
            }));
  }

  void SetupOptimizationGuideDeciderOnDemandResponse(
      OptimizationGuideDecision decision,
      OptimizationType optimization_type,
      RequestContext request_context,
      const OptimizationMetadata& metadata) {
    EXPECT_CALL(mock_decider_,
                CanApplyOptimizationOnDemand(
                    std::vector<GURL>{GURL(kTestUrl)},
                    base::flat_set<OptimizationType>{optimization_type},
                    request_context,
                    A<OnDemandOptimizationGuideDecisionRepeatingCallback>(),
                    A<std::optional<RequestContextMetadata>>()))
        .WillOnce(WithArgs<3>(
            [decision, optimization_type, metadata](
                OnDemandOptimizationGuideDecisionRepeatingCallback callback) {
              base::flat_map<OptimizationType,
                             OptimizationGuideDecisionWithMetadata>
                  decisions = {{optimization_type, CreateDecisionWithMetadata(
                                                       decision, metadata)}};
              callback.Run(GURL(kTestUrl), decisions);
            }));
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockOptimizationGuideDecider> mock_decider_;
  std::unique_ptr<OptimizationGuideAnnotationIndexClient> client_;
};

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_ValidMetadata_ReturnsSupportedTasks) {
  std::vector<std::string> expected_tasks = {kTestTaskType1, kTestTaskType2};
  GetSupportedTasksResponse response_proto =
      CreateSupportedTasksResponse(expected_tasks);
  OptimizationMetadata metadata =
      CreateOptimizationMetadata(AnyWrapProto(response_proto));
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_TASKS_SUPPORTED, metadata);

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_EQ(future.Take(), expected_tasks);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_FalseDecision_ReturnsEmptyVector) {
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kFalse,
      OptimizationType::FILTER_TASKS_SUPPORTED, OptimizationMetadata());

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_NoMetadata_ReturnsEmptyVector) {
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_TASKS_SUPPORTED, OptimizationMetadata());

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_MalformedMetadata_ReturnsEmptyVector) {
  OptimizationMetadata metadata =
      CreateMalformedOptimizationMetadata(kGetSupportedTasksResponseUrl);
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_TASKS_SUPPORTED, metadata);

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_NullDecider_ReturnsEmptyVector) {
  auto client = std::make_unique<OptimizationGuideAnnotationIndexClient>(
      /*optimization_guide_decider=*/nullptr, /*log_router=*/nullptr);

  base::test::TestFuture<std::vector<std::string>> future;
  client->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                            kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotation_ValidMetadata_ReturnsAnnotation) {
  ExtractTaskAttributesResponse response_proto =
      CreateExtractTaskAttributesResponse(
          kTestTaskTypeShopping, {{kTestAttributeKey, kTestAttributeValue}});
  OptimizationMetadata metadata =
      CreateOptimizationMetadata(AnyWrapProto(response_proto));
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXTRACT_ATTRIBUTES, metadata);

  base::test::TestFuture<std::optional<FilterAnnotation>> future;
  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  std::optional<FilterAnnotation> annotation = future.Take();
  ASSERT_TRUE(annotation.has_value());
  EXPECT_EQ(annotation->task_type, kTestTaskTypeShopping);
  ASSERT_EQ(annotation->attributes.size(), 1u);
  EXPECT_EQ(annotation->attributes[0].key, kTestAttributeKey);
  EXPECT_EQ(annotation->attributes[0].value, kTestAttributeValue);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotation_FalseDecision_ReturnsNullopt) {
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kFalse,
      OptimizationType::FILTER_EXTRACT_ATTRIBUTES, OptimizationMetadata());

  base::test::TestFuture<std::optional<FilterAnnotation>> future;
  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotation_NoMetadata_ReturnsNullopt) {
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXTRACT_ATTRIBUTES, OptimizationMetadata());

  base::test::TestFuture<std::optional<FilterAnnotation>> future;
  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotation_MalformedMetadata_ReturnsNullopt) {
  OptimizationMetadata metadata =
      CreateMalformedOptimizationMetadata(kExtractTaskAttributesResponseUrl);
  SetupOptimizationGuideDeciderResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXTRACT_ATTRIBUTES, metadata);

  base::test::TestFuture<std::optional<FilterAnnotation>> future;
  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotation_NullDecider_ReturnsNullopt) {
  auto client = std::make_unique<OptimizationGuideAnnotationIndexClient>(
      /*optimization_guide_decider=*/nullptr, /*log_router=*/nullptr);

  base::test::TestFuture<std::optional<FilterAnnotation>> future;
  client->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                  kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidates_ValidMetadata_ReturnsCandidates) {
  GetTaskExecutionStrategiesResponse response_proto =
      CreateTaskExecutionStrategiesResponse(
          GURL(kTestUrl), {{kTestAttributeKey, kTestAttributeValue}});
  response_proto.mutable_execution_strategies(0)->set_candidate_id(
      kTestCandidateId);
  OptimizationMetadata metadata =
      CreateOptimizationMetadata(AnyWrapProto(response_proto));
  SetupOptimizationGuideDeciderOnDemandResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXECUTION_STRATEGY,
      RequestContext::CONTEXT_FILTER_EXECUTION, metadata);

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), std::vector<FilterAnnotation>(), future.GetCallback(),
      kTestNavigationId);

  std::optional<std::vector<FilterSuggestionCandidate>> candidates =
      future.Take();
  ASSERT_TRUE(candidates.has_value());
  ASSERT_EQ(candidates->size(), 1u);
  EXPECT_EQ((*candidates)[0].filter_annotation_id.AsLowercaseString(),
            kTestCandidateId);
  EXPECT_EQ((*candidates)[0].navigation_url.spec(), kTestUrl);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidates_FalseDecision_ReturnsNullopt) {
  SetupOptimizationGuideDeciderOnDemandResponse(
      OptimizationGuideDecision::kFalse,
      OptimizationType::FILTER_EXECUTION_STRATEGY,
      RequestContext::CONTEXT_FILTER_EXECUTION, OptimizationMetadata());

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), std::vector<FilterAnnotation>(), future.GetCallback(),
      kTestNavigationId);

  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidates_NoMetadata_ReturnsNullopt) {
  SetupOptimizationGuideDeciderOnDemandResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXECUTION_STRATEGY,
      RequestContext::CONTEXT_FILTER_EXECUTION, OptimizationMetadata());

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), std::vector<FilterAnnotation>(), future.GetCallback(),
      kTestNavigationId);

  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidates_MalformedMetadata_ReturnsNullopt) {
  OptimizationMetadata metadata = CreateMalformedOptimizationMetadata(
      kGetTaskExecutionStrategiesResponseUrl);
  SetupOptimizationGuideDeciderOnDemandResponse(
      OptimizationGuideDecision::kTrue,
      OptimizationType::FILTER_EXECUTION_STRATEGY,
      RequestContext::CONTEXT_FILTER_EXECUTION, metadata);

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), std::vector<FilterAnnotation>(), future.GetCallback(),
      kTestNavigationId);

  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidates_NullDecider_ReturnsNullopt) {
  auto client = std::make_unique<OptimizationGuideAnnotationIndexClient>(
      /*optimization_guide_decider=*/nullptr, /*log_router=*/nullptr);

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  client->GetFilterSuggestionCandidates(
      GURL(kTestUrl), std::vector<FilterAnnotation>(), future.GetCallback(),
      kTestNavigationId);

  EXPECT_FALSE(future.Take().has_value());
}

}  // namespace

}  // namespace multistep_filter
