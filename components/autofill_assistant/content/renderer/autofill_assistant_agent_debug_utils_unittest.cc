// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent_debug_utils.h"

#include "base/base64.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

class SemanticLabelsJsonParsingTest : public ::testing::Test {
 protected:
  ModelExecutorResult model_executor_result_ =
      ModelExecutorResult(47, 7, false);
};

TEST_F(SemanticLabelsJsonParsingTest, ValidJson) {
  std::string json_input = R"({
      "roles": [{"id": 47, "name": "ADDRESS_LINE1"}],
      "objectives": [{"id": 7, "name": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: ADDRESS_LINE1, objective: FILL_DELIVERY_ADDRESS}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, UseOverrideField) {
  std::string json_input = R"({
      "roles": [{"id": 47, "name": "ADDRESS_LINE1"}],
      "objectives": [{"id": 7, "name": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: ADDRESS_LINE1, objective: FILL_DELIVERY_ADDRESS}[override]";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, ModelExecutorResult(47, 7, true), false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, ValidJson_MoreThanOneObjectPerList) {
  std::string json_input = R"({
      "roles": [
        {"id": 0, "name": "UNKNOWN_ROLE"},
        {"id": 47, "name": "ADDRESS_LINE1"}
      ],
      "objectives": [
        {"id": 0, "name": "UNKNOWN_OBJECTIVE"},
        {"id": 7, "name": "FILL_DELIVERY_ADDRESS"}
      ]
    })";
  std::string expected_output =
      "{role: ADDRESS_LINE1, objective: FILL_DELIVERY_ADDRESS}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_NotAnObject) {
  std::string json_input = R"( [{"id": 47, "name": "ADDRESS_LINE1"}] )";
  std::string expected_output = "{role: 47, objective: 7}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_RolesNotPresent) {
  std::string json_input = R"({
      "not_roles": [{"id": 47, "name": "ADDRESS_LINE1"}],
      "objectives": [{"id": 7, "name": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: (missing-label) 47, objective: FILL_DELIVERY_ADDRESS}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_ObjectivesNotPresent) {
  std::string json_input = R"({
      "roles": [{"id": 47, "name": "ADDRESS_LINE1"}],
      "not_objectives": [{"id": 7, "name": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: ADDRESS_LINE1, objective: (missing-label) 7}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_EnumsNotAList) {
  std::string json_input = R"({
      "roles": {"id": 47, "name": "ADDRESS_LINE1"},
      "objectives": {"id": 7, "name": "FILL_DELIVERY_ADDRESS"}
    })";
  std::string expected_output = "{role: 47, objective: 7}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_IndexFieldNotNamedId) {
  std::string json_input = R"({
      "roles": [{"index": 47, "name": "ADDRESS_LINE1"}],
      "objectives": [{"id": 7, "name": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: (missing-label) 47, objective: FILL_DELIVERY_ADDRESS}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_LabelValueFieldNotNamedName) {
  std::string json_input = R"({
      "roles": [{"id": 47, "name": "ADDRESS_LINE1"}],
      "objectives": [{"id": 7, "label": "FILL_DELIVERY_ADDRESS"}]
    })";
  std::string expected_output =
      "{role: ADDRESS_LINE1, objective: (missing-label) 7}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

TEST_F(SemanticLabelsJsonParsingTest, InvalidJson_Empty) {
  std::string json_input = "";
  std::string expected_output = "{role: 47, objective: 7}";

  // Encode the JSON and add it to the debug DOM annotations switch
  std::string base64_json;
  base::Base64Encode(json_input, &base64_json);

  SemanticLabelsPair labels = DecodeSemanticPredictionLabelsJson(base64_json);

  std::u16string debug_string = SemanticPredictionResultToDebugString(
      labels.first, labels.second, model_executor_result_, false);
  EXPECT_EQ(debug_string,
            std::u16string(expected_output.begin(), expected_output.end()));
}

}  // namespace
}  // namespace autofill_assistant
