// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/skills/public/skill.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Optional;

class SkillsDialogHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<SkillsDialogHandler>(
        receiver_.BindNewPipeAndPassReceiver(), web_ui_.GetWebContents(),
        &mock_opt_guide_service_, mock_delegate_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  MockOptimizationGuideKeyedService mock_opt_guide_service_;
  base::WeakPtr<SkillsDialogDelegate> mock_delegate_;
  mojo::Remote<mojom::DialogHandler> receiver_;
  std::unique_ptr<SkillsDialogHandler> handler_;
};

// Tests that a successful response from MES is correctly mapped to Mojo.
TEST_F(SkillsDialogHandlerTest, RefineSkillSuccess) {
  skills::Skill skill;
  skill.prompt = "test prompt";
  base::MockCallback<mojom::DialogHandler::RefineSkillCallback> callback;

  EXPECT_CALL(mock_opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](auto capability, const google::protobuf::MessageLite& request,
             std::optional<optimization_guide::ModelExecutionOptions> options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 cb) {
            // Simulate the server returning one suggestion.
            optimization_guide::proto::SkillsResponse response;
            auto* suggestion = response.add_suggestions();
            suggestion->set_prompt("refined prompt");
            suggestion->set_name("suggested name");

            std::string serialized;
            response.SerializeToString(&serialized);
            optimization_guide::proto::Any any;
            any.set_type_url("type.googleapis.com/SkillsResponse");
            any.set_value(serialized);

            std::move(cb).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), /*execution_info=*/nullptr),
                nullptr);
          });

  EXPECT_CALL(
      callback,
      Run(Optional(AllOf(Field(&skills::Skill::prompt, "refined prompt"),
                         Field(&skills::Skill::name, "suggested name")))));

  handler_->RefineSkill(std::move(skill), callback.Get());
}

}  // namespace
}  // namespace skills
