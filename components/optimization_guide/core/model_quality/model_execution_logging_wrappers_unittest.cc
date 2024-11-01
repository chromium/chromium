// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"

#include <algorithm>

#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {
using base::test::EqualsProto;
using ::testing::_;
using ::testing::An;

class ModelExecutionLoggingWrappersTest : public testing::Test {
 public:
  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

 private:
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
};

TEST_F(ModelExecutionLoggingWrappersTest, ExecuteModelWithLogging) {
  proto::TabOrganizationResponse response;
  response.add_tab_groups()->set_label("foo");
  proto::ModelExecutionInfo model_execution_info;
  model_execution_info.set_execution_id("id");
  EXPECT_CALL(*model_executor(),
              ExecuteModel(ModelBasedCapabilityKey::kTabOrganization, _, _,
                           An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(testing::Invoke(
          [&response, &model_execution_info](
              ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const std::optional<base::TimeDelta>& execution_timeout,
              OptimizationGuideModelExecutionResultCallback callback) {
            std::move(callback).Run(
                OptimizationGuideModelExecutionResult(
                    AnyWrapProto(response),
                    std::make_unique<proto::ModelExecutionInfo>(model_execution_info)),
                /*log_entry=*/nullptr);
          }));
  proto::TabOrganizationRequest request;
  auto* tabs = request.mutable_tabs();
  auto* tab = tabs->Add();
  tab->set_title("tab");
  tab->set_tab_id(1);
  ModelExecutionCallbackWithLogging<proto::TabOrganizationLoggingData>
      callback = base::BindLambdaForTesting(
          [&request, &response, &model_execution_info](
              OptimizationGuideModelExecutionResult result,
              std::unique_ptr<proto::TabOrganizationLoggingData>
                  model_execution_proto) {
            ASSERT_TRUE(model_execution_proto);
            EXPECT_THAT(model_execution_proto->request(), EqualsProto(request));
            EXPECT_THAT(model_execution_proto->response(),
                        EqualsProto(response));
            EXPECT_THAT(model_execution_proto->model_execution_info(),
                        EqualsProto(model_execution_info));
          });
  ExecuteModelWithLogging(model_executor(),
                          ModelBasedCapabilityKey::kTabOrganization, request,
                          std::nullopt, std::move(callback));
}

TEST_F(ModelExecutionLoggingWrappersTest, ExecuteModelSessionWithLogging) {
  testing::NiceMock<MockSession> session;

  proto::ComposeResponse response;
  response.set_output("foo");
  proto::ModelExecutionInfo model_execution_info;
  model_execution_info.set_execution_id("id");
  EXPECT_CALL(
      session,
      ExecuteModel(
          _, An<OptimizationGuideModelExecutionResultStreamingCallback>()))
      .WillOnce(testing::Invoke(
          [&response, &model_execution_info](
              const google::protobuf::MessageLite& request_metadata,
              OptimizationGuideModelExecutionResultStreamingCallback callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    StreamingResponse{.response = AnyWrapProto(response),
                                      .is_complete = true},
                    /*provided_by_on_device=*/true,
                    /*log_entry=*/nullptr,
                    std::make_unique<proto::ModelExecutionInfo>(
                        model_execution_info)));
          }));
  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  ModelExecutionSessionCallbackWithLogging<proto::ComposeLoggingData> callback =
      base::BindLambdaForTesting(
          [&request, &response, &model_execution_info](
              OptimizationGuideModelStreamingExecutionResult result,
              std::unique_ptr<proto::ComposeLoggingData>
                  model_execution_proto) {
            ASSERT_TRUE(model_execution_proto);
            EXPECT_THAT(model_execution_proto->request(), EqualsProto(request));
            EXPECT_THAT(model_execution_proto->response(),
                        EqualsProto(response));
            EXPECT_THAT(model_execution_proto->model_execution_info(),
                        EqualsProto(model_execution_info));
          });
  ExecuteModelSessionWithLogging(&session, request, std::move(callback));
}

TEST_F(ModelExecutionLoggingWrappersTest,
       ExecuteModelSessionWithLogging_IncompleteResponse) {
  testing::NiceMock<MockSession> session;

  proto::ComposeResponse response;
  response.set_output("foo");
  EXPECT_CALL(
      session,
      ExecuteModel(
          _, An<OptimizationGuideModelExecutionResultStreamingCallback>()))
      .WillOnce(testing::Invoke(
          [&response](
              const google::protobuf::MessageLite& request_metadata,
              OptimizationGuideModelExecutionResultStreamingCallback callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    StreamingResponse{.response = AnyWrapProto(response),
                                      .is_complete = false},
                    /*provided_by_on_device=*/true,
                    /*log_entry=*/nullptr,
                    // execution_info is not set for incomplete responses.
                    /*execution_info=*/nullptr));
          }));
  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  ModelExecutionSessionCallbackWithLogging callback =
      base::BindLambdaForTesting(
          [](OptimizationGuideModelStreamingExecutionResult result,
             std::unique_ptr<proto::ComposeLoggingData> model_execution_proto) {
            ASSERT_FALSE(model_execution_proto);
          });
  ExecuteModelSessionWithLogging(&session, request, std::move(callback));
}

}  // namespace
}  // namespace optimization_guide
