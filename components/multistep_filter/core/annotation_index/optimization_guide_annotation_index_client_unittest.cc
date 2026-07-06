// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockOptimizationGuideDecider;
using ::optimization_guide::OptimizationGuideDecision;
using ::optimization_guide::OptimizationGuideDecisionCallback;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::proto::Any;
using ::optimization_guide::proto::OptimizationType;
using ::testing::_;
using ::testing::A;
using ::testing::NiceMock;
using ::testing::WithArgs;

constexpr int64_t kTestNavigationId = 12345;
constexpr char kTestUrl[] = "https://example.com/test";
constexpr char kTestTaskType1[] = "TASK_TYPE_1";
constexpr char kTestTaskType2[] = "TASK_TYPE_2";

class OptimizationGuideAnnotationIndexClientTest : public testing::Test {
 public:
  OptimizationGuideAnnotationIndexClientTest() {
    client_ = std::make_unique<OptimizationGuideAnnotationIndexClient>(
        &mock_decider_, /*log_router=*/nullptr);
  }
  ~OptimizationGuideAnnotationIndexClientTest() override = default;

 protected:
  void SetupFilterTasksSupportedDeciderResponse(
      OptimizationGuideDecision decision,
      const OptimizationMetadata& metadata) {
    EXPECT_CALL(mock_decider_,
                CanApplyOptimization(GURL(kTestUrl),
                                     OptimizationType::FILTER_TASKS_SUPPORTED,
                                     A<OptimizationGuideDecisionCallback>()))
        .WillOnce(WithArgs<2>(
            [decision, metadata](OptimizationGuideDecisionCallback callback) {
              std::move(callback).Run(decision, metadata);
            }));
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockOptimizationGuideDecider> mock_decider_;
  std::unique_ptr<OptimizationGuideAnnotationIndexClient> client_;
};

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidatesTriggersCallback) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), annotations, future.GetCallback(), kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_ValidMetadata_ReturnsSupportedTasks) {
  std::vector<std::string> expected_tasks = {kTestTaskType1, kTestTaskType2};
  GetSupportedTasksResponse response_proto =
      CreateSupportedTasksResponse(expected_tasks);
  OptimizationMetadata metadata;
  metadata.set_any_metadata(AnyWrapProto(response_proto));
  SetupFilterTasksSupportedDeciderResponse(OptimizationGuideDecision::kTrue,
                                           metadata);

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_EQ(future.Take(), expected_tasks);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_FalseDecision_ReturnsEmptyVector) {
  SetupFilterTasksSupportedDeciderResponse(OptimizationGuideDecision::kFalse,
                                           OptimizationMetadata());

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_NoMetadata_ReturnsEmptyVector) {
  SetupFilterTasksSupportedDeciderResponse(OptimizationGuideDecision::kTrue,
                                           OptimizationMetadata());

  base::test::TestFuture<std::vector<std::string>> future;
  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasks_MalformedMetadata_ReturnsEmptyVector) {
  OptimizationMetadata metadata;
  Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/multistep_filter.GetSupportedTasksResponse");
  any_metadata.set_value("invalid_protobuf_bytes");
  metadata.set_any_metadata(any_metadata);
  SetupFilterTasksSupportedDeciderResponse(OptimizationGuideDecision::kTrue,
                                           metadata);

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
       ExtractFilterAnnotationTriggersCallback) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

}  // namespace

}  // namespace multistep_filter
