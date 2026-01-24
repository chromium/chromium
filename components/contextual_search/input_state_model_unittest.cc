// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/searchbox_config_constraints.pb.h"
#include "url/gurl.h"

namespace contextual_search {

using ::testing::UnorderedElementsAre;

class InputStateModelTest : public testing::Test {
 public:
  InputStateModelTest() = default;
  ~InputStateModelTest() override = default;

  void SetUp() override {
    mock_controller_ =
        std::make_unique<MockContextualSearchContextController>();
    ON_CALL(session_handle_, GetController())
        .WillByDefault(testing::Return(mock_controller_.get()));
    ON_CALL(*mock_controller_, GetFileInfoList())
        .WillByDefault(testing::Return(empty_file_info_list_));
    input_state_model_ =
        std::make_unique<InputStateModel>(session_handle_, config_);
  }

 protected:
  std::unique_ptr<InputStateModel> input_state_model_;
  MockContextualSearchSessionHandle session_handle_;
  std::unique_ptr<MockContextualSearchContextController> mock_controller_;
  omnibox::SearchboxConfig config_;
  InputState state_;
  const std::vector<const contextual_search::FileInfo*> empty_file_info_list_;
};

TEST_F(InputStateModelTest, TestInitialization) {
  EXPECT_TRUE(input_state_model_);
}

TEST_F(InputStateModelTest, TestSubscribeAndNotify) {
  base::MockCallback<InputStateModel::Subscriber> mock_subscriber;
  base::CallbackListSubscription subscription =
      input_state_model_->subscribe(mock_subscriber.Get());

  EXPECT_CALL(mock_subscriber, Run(testing::_)).Times(1);
  // Setting a tool notifies subscribers.
  input_state_model_->setActiveTool(ToolMode::TOOL_MODE_UNSPECIFIED);
}

class InputStateModelCompatibilityTest : public InputStateModelTest {
 public:
  void SetUp() override {
    auto* rule_set = config_.mutable_rule_set();

    // Gemini only allows image input.
    auto* model_gemini_rule = rule_set->add_model_rules();
    model_gemini_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
    model_gemini_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

    // Pro allows Create Images and image and file input.
    auto* model_pro_rule = rule_set->add_model_rules();
    model_pro_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
    model_pro_rule->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    model_pro_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
    model_pro_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_FILE);
    model_pro_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_BROWSER_TAB);

    auto* tool_ds_rule = rule_set->add_tool_rules();
    tool_ds_rule->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

    // Image input is only compatible with other image inputs.
    auto* input_image_rule = rule_set->add_input_type_rules();
    input_image_rule->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
    input_image_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

    // By default, all models and tools are allowed.
    state_.allowed_models = {omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR,
                             omnibox::ModelMode::MODEL_MODE_GEMINI_PRO};
    state_.allowed_tools = {omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                            omnibox::ToolMode::TOOL_MODE_CANVAS,
                            omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
    state_.allowed_input_types = {omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                  omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                  omnibox::InputType::INPUT_TYPE_BROWSER_TAB};
    state_.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
    state_.active_model = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
    state_.disabled_tools = {};
    state_.disabled_models = {};
    state_.disabled_input_types = {};

    // Create the model *after* the config is set up.
    InputStateModelTest::SetUp();
    input_state_model_->set_state_for_testing(state_);
  }
};

TEST_F(InputStateModelCompatibilityTest, SelectTool) {
  // Select Deep Search.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  const auto& new_state = input_state_model_->get_state_for_testing();

  // Both models should be disabled because neither supports Deep Search.
  EXPECT_THAT(
      new_state.disabled_models,
      UnorderedElementsAre(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR,
                           omnibox::ModelMode::MODEL_MODE_GEMINI_PRO));

  // All other tools should be disabled.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS));

  // All inputs disabled.
  EXPECT_THAT(new_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                   omnibox::InputType::INPUT_TYPE_BROWSER_TAB));
}

TEST_F(InputStateModelCompatibilityTest, SelectModel) {
  // Select Gemini.
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  const auto& new_state = input_state_model_->get_state_for_testing();

  // All other models disabled when a model is selected.
  EXPECT_THAT(new_state.disabled_models,
              UnorderedElementsAre(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO));

  // All tools disabled when a model is selected.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS));

  // Only images can be uploaded with Gemini so file and tab are disabled.
  EXPECT_THAT(new_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                   omnibox::InputType::INPUT_TYPE_BROWSER_TAB));
}

TEST_F(InputStateModelCompatibilityTest, SelectImageInput) {
  // Simulate adding an image.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);
  const auto& new_state = input_state_model_->get_state_for_testing();

  // With an image, tools that don't support images are disabled.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));

  // Models are not disabled, since they both support images.
  EXPECT_TRUE(new_state.disabled_models.empty());

  // No input types are disabled based on other inputs.
  EXPECT_TRUE(new_state.disabled_input_types.empty());
}

TEST_F(InputStateModelCompatibilityTest, SelectTabInput) {
  // Simulate adding a tab.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().tab_url = GURL("https://www.google.com");
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Pro supports tabs, but regular Gemini does not.
  input_state_model_->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  const auto& new_state = input_state_model_->get_state_for_testing();

  // Gemini regular should be disabled.
  EXPECT_THAT(
      new_state.disabled_models,
      UnorderedElementsAre(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR));
}

TEST_F(InputStateModelTest, GetAdditionalQueryParams) {
  // No tool or model added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  EXPECT_TRUE(input_state_model_->GetAdditionalQueryParams().empty());

  // Deep Search added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("dr", "1")));

  // Canvas added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("rc", "1")));

  // Image Gen added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("imgn", "1")));

  // Image Gen Upload added.
  input_state_model_->setActiveTool(
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("imgn", "1")));

  // Reset all tools.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);

  // Gemini Pro added.
  input_state_model_->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("m", "1")));

  // Gemini Pro Autoroute added.
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("m", "2")));

  // Deep Search and Gemini Pro added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  input_state_model_->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("dr", "1"),
                                            testing::Pair("m", "1")));
}

}  // namespace contextual_search
