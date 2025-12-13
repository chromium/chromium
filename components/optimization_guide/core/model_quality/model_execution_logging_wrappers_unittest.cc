// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"

#include <algorithm>

#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
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
  optimization_guide::MockRemoteModelExecutor* model_executor() {
    return &model_executor_;
  }

 private:
  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>
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
      .WillOnce([&response, &model_execution_info](
                    ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const ModelExecutionOptions& options,
                    OptimizationGuideModelExecutionResultCallback callback) {
        std::move(callback).Run(OptimizationGuideModelExecutionResult(
                                    AnyWrapProto(response),
                                    std::make_unique<proto::ModelExecutionInfo>(
                                        model_execution_info)),
                                /*log_entry=*/nullptr);
      });
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

TEST_F(ModelExecutionLoggingWrappersTest, ExecuteModelWithLogging_Error) {
  EXPECT_CALL(*model_executor(),
              ExecuteModel(ModelBasedCapabilityKey::kTabOrganization, _, _,
                           An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce([](ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const ModelExecutionOptions& options,
                   OptimizationGuideModelExecutionResultCallback callback) {
        std::move(callback).Run(
            OptimizationGuideModelExecutionResult(
                base::unexpected(OptimizationGuideModelExecutionError::
                                     FromModelExecutionError(
                                         OptimizationGuideModelExecutionError::
                                             ModelExecutionError::kDisabled)),
                // Errors that don't end up making a request to the model
                // won't have a ModelExecutionInfo.
                nullptr),
            /*log_entry=*/nullptr);
      });
  proto::TabOrganizationRequest request;
  auto* tabs = request.mutable_tabs();
  auto* tab = tabs->Add();
  tab->set_title("tab");
  tab->set_tab_id(1);
  ModelExecutionCallbackWithLogging<proto::TabOrganizationLoggingData>
      callback = base::BindLambdaForTesting(
          [&request](OptimizationGuideModelExecutionResult result,
                     std::unique_ptr<proto::TabOrganizationLoggingData>
                         model_execution_proto) {
            ASSERT_TRUE(model_execution_proto);
            EXPECT_THAT(model_execution_proto->request(), EqualsProto(request));
            EXPECT_EQ(
                model_execution_proto->model_execution_info()
                    .model_execution_error_enum(),
                static_cast<uint32_t>(OptimizationGuideModelExecutionError::
                                          ModelExecutionError::kDisabled));
          });
  ExecuteModelWithLogging(model_executor(),
                          ModelBasedCapabilityKey::kTabOrganization, request,
                          std::nullopt, std::move(callback));
}

}  // namespace
}  // namespace optimization_guide
