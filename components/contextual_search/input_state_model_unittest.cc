// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rule_set.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
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
    pref_service_.registry()->RegisterIntegerPref(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));
    input_state_model_ = std::make_unique<InputStateModel>(
        session_handle_, config_, /*is_off_the_record=*/false);
    input_state_model_->SetPrefService(&pref_service_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<InputStateModel> input_state_model_;
  MockContextualSearchSessionHandle session_handle_;
  std::unique_ptr<MockContextualSearchContextController> mock_controller_;
  omnibox::SearchboxConfig config_;
  InputState state_;
  const std::vector<const contextual_search::FileInfo*> empty_file_info_list_;
};

TEST_F(InputStateModelTest, TestInitialization) {
  EXPECT_TRUE(input_state_model_);
  const auto& state = input_state_model_->get_state_for_testing();

  // All values should be default since we are using a default
  // `SearchboxConfig`.
  EXPECT_TRUE(state.allowed_tools.empty());
  EXPECT_TRUE(state.allowed_models.empty());
  EXPECT_TRUE(state.allowed_input_types.empty());
  EXPECT_EQ(state.active_tool, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);
  EXPECT_EQ(state.active_model, omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  EXPECT_TRUE(state.disabled_tools.empty());
  EXPECT_TRUE(state.disabled_models.empty());
  EXPECT_TRUE(state.disabled_input_types.empty());
}

TEST_F(InputStateModelTest,
       AddsBrowserTabWhenLensImageAndFileInputsAreAllowed) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);

  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/false);
  const auto& state = input_state_model_->get_state_for_testing();

  EXPECT_THAT(state.allowed_input_types,
              testing::UnorderedElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                            omnibox::INPUT_TYPE_LENS_FILE,
                                            omnibox::INPUT_TYPE_BROWSER_TAB));
}

TEST_F(InputStateModelTest, TestSubscribeAndNotify) {
  base::MockCallback<InputStateModel::Subscriber> mock_subscriber;
  base::CallbackListSubscription subscription =
      input_state_model_->subscribe(mock_subscriber.Get());

  EXPECT_CALL(mock_subscriber, Run(testing::_)).Times(1);
  // Setting a tool notifies subscribers.
  input_state_model_->setActiveTool(ToolMode::TOOL_MODE_UNSPECIFIED);
}

TEST_F(InputStateModelTest, DefaultToFirstAllowedModel) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();

  // Setup Allowed Models.
  // Add Gemini Regular first. It becomes allowed_models[0].
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  // Add Gemini Pro as the second option.
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  auto* auto_rule = rule_set->add_model_rules();
  auto_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  auto* pro_rule = rule_set->add_model_rules();
  pro_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  // Initialize Model.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/false);
  const auto& state = input_state_model_->get_state_for_testing();

  // Verify Initialization Logic.
  // Active model defaults to allowed_models[0].
  EXPECT_EQ(state.active_model, omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  // Verify: Even though Regular is active, Pro should remain enabled.
  EXPECT_TRUE(state.disabled_models.empty());
}

TEST_F(InputStateModelTest, RegularModelAllowsAllToolsAndInputsWithEmptyLists) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();

  // 1. Prepare data: Add some Tools and Inputs to the global allowed list.
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);

  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);

  auto* deep_search_rule = rule_set->add_tool_rules();
  deep_search_rule->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  deep_search_rule->set_allow_all_input_types(true);

  auto* image_gen_rule = rule_set->add_tool_rules();
  image_gen_rule->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  image_gen_rule->set_allow_all_input_types(true);

  // 2.Configure rules for the Regular model.
  // We set `allow_all_*` to true, but intentionally keep the `allowed_tools`
  // and `allowed_input_types` lists empty.
  auto* model_rule = rule_set->add_model_rules();
  model_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  model_rule->set_allow_all_tools(true);
  model_rule->set_allow_all_input_types(true);

  // 3. Initialize the model.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);

  const auto& state = input_state_model_->get_state_for_testing();

  // 4. Verify the Active Model is Regular.
  EXPECT_EQ(state.active_model, omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  // 5. Verify core logic: Even though the specific allowed list for Regular is
  // empty, no Tools or Inputs should be disabled due to the presence of the
  // `allow_all_*` flags.
  EXPECT_TRUE(state.disabled_tools.empty());
  EXPECT_TRUE(state.disabled_input_types.empty());
}

TEST_F(InputStateModelTest, ModelWithAllowAllToolsIsNotDisabled) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();

  // Regular model allows everything.
  auto* model_gemini_rule = rule_set->add_model_rules();
  model_gemini_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  model_gemini_rule->set_allow_all_tools(true);
  model_gemini_rule->set_allow_all_input_types(true);

  // Pro model only allows Image Gen tool.
  auto* model_pro_rule = rule_set->add_model_rules();
  model_pro_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  model_pro_rule->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);

  // Add allowed models and tools.
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);

  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);

  // Select Deep Search tool.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  const auto& state = input_state_model_->get_state_for_testing();

  // Pro model should be disabled as it doesn't support Deep Search.
  // Regular model should not be disabled as it allows all tools.
  EXPECT_THAT(state.disabled_models,
              UnorderedElementsAre(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO));
}

TEST_F(InputStateModelTest, ModelWithAllowAllInputsIsNotDisabled) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();

  // Regular model allows everything.
  auto* model_gemini_rule = rule_set->add_model_rules();
  model_gemini_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  model_gemini_rule->set_allow_all_tools(true);
  model_gemini_rule->set_allow_all_input_types(true);

  // Pro model only allows image input.
  auto* model_pro_rule = rule_set->add_model_rules();
  model_pro_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  model_pro_rule->add_allowed_input_types(
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

  // Globally allowed models and inputs.
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);

  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);

  // Simulate adding a file.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kPdf;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& state = input_state_model_->get_state_for_testing();

  // Pro model should be disabled as it doesn't support file input.
  // Regular model should not be disabled as it allows all input types.
  EXPECT_THAT(state.disabled_models,
              UnorderedElementsAre(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO));
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
    state_.max_total_inputs = 2;
    state_.max_instances[omnibox::InputType::INPUT_TYPE_LENS_IMAGE] = 1;

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

  // All other tools should be disabled (including Deep Search itself).
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
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

  // No models should be disabled.
  EXPECT_TRUE(new_state.disabled_models.empty());

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

  // Only image input should be disabled since max_instance is 1.
  EXPECT_THAT(new_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE));
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
  // Add tool and model configs.
  auto* deep_search_config = config_.add_tool_configs();
  deep_search_config->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  auto* ds_param = deep_search_config->add_aim_url_params();
  ds_param->set_param_key("dr");
  ds_param->set_param_value("1");

  auto* canvas_config = config_.add_tool_configs();
  canvas_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  auto* canvas_param = canvas_config->add_aim_url_params();
  canvas_param->set_param_key("rc");
  canvas_param->set_param_value("1");

  auto* image_gen_config = config_.add_tool_configs();
  image_gen_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  auto* imgn_param = image_gen_config->add_aim_url_params();
  imgn_param->set_param_key("imgn");
  imgn_param->set_param_value("1");

  auto* gemini_pro_config = config_.add_model_configs();
  gemini_pro_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  auto* gp_param = gemini_pro_config->add_aim_url_params();
  gp_param->set_param_key("nem");
  gp_param->set_param_value("143");

  // Recreate the model with the new config.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config_, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);

  // No tool or model added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("udm", "50")));

  // Deep Search added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("dr", "1"),
                                            testing::Pair("udm", "50")));

  // Canvas added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("rc", "1"),
                                            testing::Pair("udm", "50")));

  // Image Gen added.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("imgn", "1"),
                                            testing::Pair("udm", "50")));

  // Reset all tools.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);

  // Set a model, should have query params.
  input_state_model_->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("nem", "143")));

  // Deep Search and Gemini Pro added. Both tool and model should be in params.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  input_state_model_->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_THAT(input_state_model_->GetAdditionalQueryParams(),
              testing::UnorderedElementsAre(testing::Pair("dr", "1"),
                                            testing::Pair("nem", "143")));
}

TEST_F(InputStateModelCompatibilityTest, PolicyDisablesInputs) {
  // 1. Initial Setup: Explicitly allow restricted inputs.
  state_.allowed_input_types = {
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
      omnibox::InputType::INPUT_TYPE_LENS_FILE,
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB,
  };

  // Enable content sharing policy.
  pref_service_.SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  input_state_model_->set_state_for_testing(state_);
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  auto new_state = input_state_model_->get_state_for_testing();

  // Verify: Inputs remain allowed and are not disabled.
  EXPECT_FALSE(new_state.allowed_input_types.empty());
  EXPECT_TRUE(new_state.disabled_input_types.empty());

  // 2. Disable content sharing policy.
  pref_service_.SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  // Trigger update.
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  new_state = input_state_model_->get_state_for_testing();

  // Verify: Restricted inputs are removed from the allowed list entirely.
  // Consequently, the disabled list remains empty as the inputs no longer
  // exist.
  EXPECT_TRUE(new_state.allowed_input_types.empty());
  EXPECT_TRUE(new_state.disabled_input_types.empty());

  // 3. Re-enable content sharing policy.
  pref_service_.SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // Reset the state with the original inputs, as they were erased in step 2.
  state_.allowed_input_types = {
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
      omnibox::InputType::INPUT_TYPE_LENS_FILE,
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB,
  };
  input_state_model_->set_state_for_testing(state_);

  // Trigger update.
  input_state_model_->setActiveModel(
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
  new_state = input_state_model_->get_state_for_testing();

  // Verify: Inputs are restored and enabled.
  EXPECT_THAT(new_state.allowed_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                   omnibox::InputType::INPUT_TYPE_BROWSER_TAB));
  EXPECT_TRUE(new_state.disabled_input_types.empty());
}

TEST_F(InputStateModelCompatibilityTest, MaxTotalInputsDisablesInputs) {
  // Set max_total_inputs to 2.
  config_.mutable_rule_set()->set_max_total_inputs(2);
  // Recreate the model with the new config.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config_, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);
  input_state_model_->set_state_for_testing(state_);

  // Simulate adding two images.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& new_state = input_state_model_->get_state_for_testing();

  // All input types should be disabled.
  EXPECT_THAT(new_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                   omnibox::InputType::INPUT_TYPE_BROWSER_TAB));

  // Simulate removing one image.
  file_infos.pop_back();
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& final_state = input_state_model_->get_state_for_testing();

  // Image input should still be disabled due to max_instance.
  EXPECT_THAT(final_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE));
}

TEST_F(InputStateModelCompatibilityTest,
       MaxInstancesAndMaxTotalInputsDisablesInputs) {
  // Simulate adding one image.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& new_state = input_state_model_->get_state_for_testing();

  // Only image input should be disabled.
  EXPECT_THAT(new_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE));

  // Simulate adding a file.
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kPdf;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& final_state = input_state_model_->get_state_for_testing();

  // All input types should be disabled.
  EXPECT_THAT(final_state.disabled_input_types,
              UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::InputType::INPUT_TYPE_LENS_FILE,
                                   omnibox::InputType::INPUT_TYPE_BROWSER_TAB));
}

TEST_F(InputStateModelCompatibilityTest, ToolWithAllowAllInputs) {
  // Set Canvas tool rule `allow_all_input_types` to true.
  auto* tool_canvas_rule = config_.mutable_rule_set()->add_tool_rules();
  tool_canvas_rule->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  tool_canvas_rule->set_allow_all_input_types(true);

  // Re-create the model with the modified config.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config_, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);
  input_state_model_->set_state_for_testing(state_);

  // Simulate adding a file.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kPdf;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  const auto& new_state = input_state_model_->get_state_for_testing();

  // With allow_all_input_types, Canvas should not be disabled.
  // Deep search is disabled as it does not support any inputs.
  // Image Gen is disabled as it has no rule.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));
}

TEST_F(InputStateModelCompatibilityTest, ToolWithSpecificInputs) {
  // Set up a rule for Canvas tool to only allow images.
  auto* tool_canvas_rule = config_.mutable_rule_set()->add_tool_rules();
  tool_canvas_rule->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  tool_canvas_rule->set_allow_all_input_types(false);
  tool_canvas_rule->add_allowed_input_types(
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

  // Re-create the model with the modified config.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config_, /*is_off_the_record=*/false);
  input_state_model_->SetPrefService(&pref_service_);
  input_state_model_->set_state_for_testing(state_);

  // Simulate adding a file, which is not allowed by Canvas.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kPdf;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  auto new_state = input_state_model_->get_state_for_testing();

  // Canvas tool should be disabled because it doesn't support file inputs.
  // Deep search is disabled as it does not support any inputs.
  // Image Gen is disabled as it has no rule.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));

  // Now simulate adding an image, which is allowed.
  file_infos.clear();
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // Trigger an update.
  input_state_model_->OnContextChanged();
  new_state = input_state_model_->get_state_for_testing();

  // Canvas tool should now be enabled.
  // Deep search is still disabled because it has no allowed inputs.
  // Image Gen is still disabled as it has no rule.
  EXPECT_THAT(new_state.disabled_tools,
              UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));
}

TEST_F(InputStateModelTest, ImageGenUploadActive) {
  // 1. Set active tool to IMAGE_GEN without any image input.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  auto state = input_state_model_->get_state_for_testing();
  EXPECT_EQ(state.active_tool, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  EXPECT_FALSE(state.image_gen_upload_active);

  // 2. Simulate adding an image.
  std::vector<FileInfo> file_infos;
  file_infos.emplace_back();
  file_infos.back().mime_type = lens::MimeType::kImage;
  ON_CALL(session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(file_infos));

  // 3. Set active tool to IMAGE_GEN again, now with an image input.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  state = input_state_model_->get_state_for_testing();
  EXPECT_EQ(state.active_tool, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  EXPECT_TRUE(state.image_gen_upload_active);

  // 4. Set a different tool and verify image_gen_upload_active is reset.
  input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  state = input_state_model_->get_state_for_testing();
  EXPECT_EQ(state.active_tool, omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  EXPECT_FALSE(state.image_gen_upload_active);
}

TEST_F(InputStateModelTest, FiltersImageGenInIncognito) {
  omnibox::SearchboxConfig config;
  auto* rule_set = config.mutable_rule_set();
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

  // Initialize with is_off_the_record = true.
  input_state_model_ = std::make_unique<InputStateModel>(
      session_handle_, config, /*is_off_the_record=*/true);
  const auto& state = input_state_model_->get_state_for_testing();

  // Verify that IMAGE_GEN is filtered out but DEEP_SEARCH remains.
  EXPECT_THAT(
      state.allowed_tools,
      testing::UnorderedElementsAre(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH));
}

// Regression test for crbug.com/487677445. Ensures that the model doesn't crash
// if it's updated after the session handle has been destroyed.
TEST_F(InputStateModelTest,
       Regression_Bug487677445_CrashOnDanglingSessionHandle) {
  auto local_session = std::make_unique<MockContextualSearchSessionHandle>();
  omnibox::SearchboxConfig config;

  auto local_model = std::make_unique<InputStateModel>(
      *local_session, config, /*is_off_the_record=*/false);

  local_session.reset();  // Destroy session.

  // This should not crash.
  local_model->OnContextChanged();
}

}  // namespace contextual_search
