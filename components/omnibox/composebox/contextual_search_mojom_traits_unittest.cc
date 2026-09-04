// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_search_mojom_traits.h"

#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/icon_resource_ids.pb.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace composebox_query {

TEST(ContextualSearchMojomTraitsTest, ModelModeUnknownValue) {
  omnibox::ModelMode input = static_cast<omnibox::ModelMode>(999);
  ASSERT_FALSE(omnibox::ModelMode_IsValid(static_cast<int>(input)));

  omnibox::ModelMode output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ModelMode>(input, output));

  // This should not crash and should return the default value.
  EXPECT_EQ(output, omnibox::ModelMode::MODEL_MODE_UNSPECIFIED);
}

TEST(ContextualSearchMojomTraitsTest, ToolModeUnknownValue) {
  omnibox::ToolMode input = static_cast<omnibox::ToolMode>(999);
  ASSERT_FALSE(omnibox::ToolMode_IsValid(static_cast<int>(input)));

  omnibox::ToolMode output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ToolMode>(input, output));

  EXPECT_EQ(output, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);
}

TEST(ContextualSearchMojomTraitsTest, InputTypeUnknownValue) {
  omnibox::InputType input = static_cast<omnibox::InputType>(999);
  ASSERT_FALSE(omnibox::InputType_IsValid(static_cast<int>(input)));

  omnibox::InputType output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::InputType>(input, output));

  EXPECT_EQ(output, omnibox::InputType::INPUT_TYPE_UNSPECIFIED);
}

TEST(ContextualSearchMojomTraitsTest, InputTypeInvalidMojomValue) {
  mojom::InputType input = static_cast<mojom::InputType>(999);
  omnibox::InputType result =
      mojo::EnumTraits<mojom::InputType, omnibox::InputType>::FromMojom(input);
  EXPECT_EQ(result, omnibox::InputType::INPUT_TYPE_UNSPECIFIED);
}

TEST(ContextualSearchMojomTraitsTest, ModelConfigWithIcon) {
  omnibox::ModelConfig input;
  input.set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  input.set_menu_label("Test Model");
  input.mutable_icon()->set_icon_id(omnibox::IconResourceIds::ACUTE);

  omnibox::ModelConfig output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ModelConfig>(input, output));

  EXPECT_EQ(output.model(), omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  EXPECT_EQ(output.menu_label(), "Test Model");
  EXPECT_TRUE(output.has_icon());
  EXPECT_EQ(output.icon().icon_id(), omnibox::IconResourceIds::ACUTE);
}

TEST(ContextualSearchMojomTraitsTest, ModelConfigWithInvalidIcon) {
  omnibox::ModelConfig input;
  input.set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  input.set_menu_label("Test Model");
  input.mutable_icon()->set_icon_id(static_cast<omnibox::IconResourceIds>(999));

  omnibox::ModelConfig output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ModelConfig>(input, output));

  EXPECT_EQ(output.model(), omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  EXPECT_EQ(output.menu_label(), "Test Model");
  EXPECT_FALSE(output.has_icon());
}

TEST(ContextualSearchMojomTraitsTest, ToolConfigWithIcon) {
  omnibox::ToolConfig input;
  input.set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  input.set_menu_label("Test Tool");
  input.mutable_icon()->set_icon_id(omnibox::IconResourceIds::TRAVEL_EXPLORE);

  omnibox::ToolConfig output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ToolConfig>(input, output));

  EXPECT_EQ(output.tool(), omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  EXPECT_EQ(output.menu_label(), "Test Tool");
  EXPECT_TRUE(output.has_icon());
  EXPECT_EQ(output.icon().icon_id(), omnibox::IconResourceIds::TRAVEL_EXPLORE);
}

TEST(ContextualSearchMojomTraitsTest, ToolConfigWithInvalidIcon) {
  omnibox::ToolConfig input;
  input.set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  input.set_menu_label("Test Tool");
  input.mutable_icon()->set_icon_id(static_cast<omnibox::IconResourceIds>(999));

  omnibox::ToolConfig output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ToolConfig>(input, output));

  EXPECT_EQ(output.tool(), omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  EXPECT_EQ(output.menu_label(), "Test Tool");
  EXPECT_FALSE(output.has_icon());
}

TEST(ContextualSearchMojomTraitsTest, ContextUploadStatusUnknownValue) {
  contextual_search::ContextUploadStatus input =
      static_cast<contextual_search::ContextUploadStatus>(999);
  contextual_search::ContextUploadStatus output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ContextUploadStatus>(
      input, output));

  EXPECT_EQ(output, contextual_search::ContextUploadStatus::kNotUploaded);
}

TEST(ContextualSearchMojomTraitsTest, ContextUploadErrorTypeUnknownValue) {
  contextual_search::ContextUploadErrorType input =
      static_cast<contextual_search::ContextUploadErrorType>(999);
  contextual_search::ContextUploadErrorType output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ContextUploadErrorType>(
          input, output));

  EXPECT_EQ(output, contextual_search::ContextUploadErrorType::kUnknown);
}

}  // namespace composebox_query
