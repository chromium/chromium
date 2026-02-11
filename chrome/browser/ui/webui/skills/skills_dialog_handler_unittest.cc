// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Optional;

class MockSkillsDialogDelegate : public SkillsDialogDelegate {
 public:
  MOCK_METHOD(void, CloseDialog, (), (override));
  MOCK_METHOD(void, OnSkillSaved, (const std::string&), (override));

  base::WeakPtr<MockSkillsDialogDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSkillsDialogDelegate> weak_factory_{this};
};

class TestSkillsDialogHandler : public SkillsDialogHandler {
 public:
  using SkillsDialogHandler::SkillsDialogHandler;

  // Member to hold the saved skill so we can return a valid pointer.
  skills::Skill fake_saved_skill_;

  const skills::Skill* SaveOrUpdateSkill(const skills::Skill& skill) override {
    fake_saved_skill_.id = skill.id.empty() ? "generated_fake_id" : skill.id;
    fake_saved_skill_.name = skill.name;
    fake_saved_skill_.icon = skill.icon;
    fake_saved_skill_.prompt = skill.prompt;
    return &fake_saved_skill_;
  }
};
class SkillsDialogHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    web_ui_.set_web_contents(web_contents_.get());
    handler_ = std::make_unique<TestSkillsDialogHandler>(
        receiver_.BindNewPipeAndPassReceiver(), web_ui_.GetWebContents(),
        &mock_opt_guide_service_, mock_skill_, mock_delegate_.GetWeakPtr());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  MockOptimizationGuideKeyedService mock_opt_guide_service_;
  Skill mock_skill_;
  NiceMock<MockSkillsDialogDelegate> mock_delegate_;
  mojo::Remote<mojom::DialogHandler> receiver_;
  std::unique_ptr<TestSkillsDialogHandler> handler_;
  base::HistogramTester histogram_tester_;
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
            suggestion->set_icon("🤖");

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
                         Field(&skills::Skill::name, "suggested name"),
                         Field(&skills::Skill::icon, "🤖")))));

  handler_->RefineSkill(std::move(skill), callback.Get());
}

TEST_F(SkillsDialogHandlerTest, CloseDialog_LogsCancelled) {
  EXPECT_CALL(mock_delegate_, CloseDialog()).Times(1);
  handler_->CloseDialog();

  histogram_tester_.ExpectBucketCount(
      "Skills.Actions", skills::SkillsActions::kClickedCancelInCreationDialog,
      1);
}

TEST_F(SkillsDialogHandlerTest, RefineSkill_LogsMetric) {
  skills::Skill skill;
  skill.prompt = "test";
  base::MockCallback<mojom::DialogHandler::RefineSkillCallback> callback;
  EXPECT_CALL(mock_opt_guide_service_, ExecuteModel(_, _, _, _));

  handler_->RefineSkill(std::move(skill), callback.Get());

  histogram_tester_.ExpectBucketCount(
      "Skills.Actions", skills::SkillsActions::kClickedRefineInCreationDialog,
      1);
}

TEST_F(SkillsDialogHandlerTest, SubmitSkill_LogsSaved) {
  skills::Skill skill;
  skill.name = "Test Skill";
  skill.prompt = "Test Prompt";

  EXPECT_CALL(mock_delegate_, OnSkillSaved("generated_fake_id")).Times(1);
  EXPECT_CALL(mock_delegate_, CloseDialog()).Times(1);

  handler_->SubmitSkill(skill);

  // Metric Check
  histogram_tester_.ExpectBucketCount("Skills.Actions",
                                      skills::SkillsActions::kSavedSkill, 1);
}

// Tests that GetInitialSkill returns the skill passed in during construction.
TEST_F(SkillsDialogHandlerTest, GetInitialSkillReturnsCorrectData) {
  skills::Skill test_skill;
  test_skill.name = "Unit Test Skill";
  test_skill.prompt = "Unit Test Prompt";
  test_skill.icon = "🧪";

  // Re-initialize the handler with this data.
  receiver_.reset();
  handler_ = std::make_unique<TestSkillsDialogHandler>(
      receiver_.BindNewPipeAndPassReceiver(), web_contents_.get(),
      &mock_opt_guide_service_, test_skill, mock_delegate_.GetWeakPtr());

  // Call the API using a lambda as the callback.
  skills::Skill received_skill;
  handler_->GetInitialSkill(
      base::BindOnce([](skills::Skill* out_skill,
                        const skills::Skill& result) { *out_skill = result; },
                     &received_skill));

  EXPECT_EQ(received_skill.name, "Unit Test Skill");
  EXPECT_EQ(received_skill.prompt, "Unit Test Prompt");
  EXPECT_EQ(received_skill.icon, "🧪");
}

}  // namespace
}  // namespace skills
