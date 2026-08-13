// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fusebox_action_mojo_utils.h"

#include <sstream>

#include "components/omnibox/browser/fusebox_action_mojo_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace fusebox_action {
namespace {

TEST(FuseboxActionMojoUtilsTest, ConvertsAllProtoFieldsToMojo) {
  omnibox::SuggestTemplateInfo::FuseboxAction proto;
  proto.set_preselected_tool(omnibox::TOOL_MODE_CANVAS);
  proto.set_preferred_inventory(
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT);
  proto.set_preselected_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  proto.set_preselected_input_source(
      omnibox::InputSource::INPUT_SOURCE_GALLERY);
  proto.set_query_action_override(
      omnibox::SuggestTemplateInfo_FuseboxAction::QUERY_ACTION_PASTE);
  proto.set_searchbox_override(omnibox::SuggestTemplateInfo_FuseboxAction::
                                   SEARCHBOX_OVERRIDE_COMPOSEBOX);

  mojom::FuseboxActionPtr mojo_action = SyncFuseboxActionProtoToMojo(proto);
  ASSERT_TRUE(mojo_action);

  EXPECT_EQ(mojo_action->preselected_tool, omnibox::TOOL_MODE_CANVAS);
  EXPECT_EQ(mojo_action->preferred_inventory,
            omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT);
  EXPECT_EQ(mojo_action->preselected_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  EXPECT_EQ(mojo_action->preselected_input_source,
            mojom::InputSource::kInputSourceGallery);
  EXPECT_EQ(mojo_action->query_action_override,
            mojom::QueryActionOverride::kPaste);
  EXPECT_EQ(mojo_action->searchbox_override,
            mojom::SearchboxOverride::kComposebox);
}

TEST(FuseboxActionMojoUtilsTest, MissingProtoFieldsStayNull) {
  omnibox::SuggestTemplateInfo::FuseboxAction proto;

  mojom::FuseboxActionPtr mojo_action = SyncFuseboxActionProtoToMojo(proto);
  ASSERT_TRUE(mojo_action);
  EXPECT_FALSE(mojo_action->preselected_tool);
  EXPECT_FALSE(mojo_action->preferred_inventory);
  EXPECT_FALSE(mojo_action->preselected_model);
  EXPECT_FALSE(mojo_action->preselected_input_source);
  EXPECT_FALSE(mojo_action->query_action_override);
  EXPECT_FALSE(mojo_action->searchbox_override);
}

TEST(FuseboxActionMojoUtilsTest, MapsInputSourceValuesToMojo) {
  struct {
    omnibox::InputSource proto_value;
    mojom::InputSource mojo_value;
  } kCases[] = {
      {omnibox::INPUT_SOURCE_UNSPECIFIED,
       mojom::InputSource::kInputSourceUnspecified},
      {omnibox::INPUT_SOURCE_GALLERY, mojom::InputSource::kInputSourceGallery},
      {omnibox::INPUT_SOURCE_CAMERA, mojom::InputSource::kInputSourceCamera},
      {omnibox::INPUT_SOURCE_FILE_PICKER,
       mojom::InputSource::kInputSourceFilePicker},
      {omnibox::INPUT_SOURCE_DRIVE, mojom::InputSource::kInputSourceDrive},
      {omnibox::INPUT_SOURCE_TAB_PICKER,
       mojom::InputSource::kInputSourceTabPicker},
      {omnibox::INPUT_SOURCE_VOICE, mojom::InputSource::kInputSourceVoice},
  };
  for (const auto& test_case : kCases) {
    omnibox::SuggestTemplateInfo::FuseboxAction proto;
    proto.set_preselected_input_source(test_case.proto_value);

    mojom::FuseboxActionPtr mojo_action = SyncFuseboxActionProtoToMojo(proto);
    ASSERT_TRUE(mojo_action);
    EXPECT_EQ(mojo_action->preselected_input_source, test_case.mojo_value);
  }
}

TEST(FuseboxActionMojoUtilsTest, MapsSearchboxOverrideValuesToMojo) {
  struct {
    omnibox::SuggestTemplateInfo_FuseboxAction_SearchboxOverride proto_value;
    mojom::SearchboxOverride mojo_value;
  } kCases[] = {
      {omnibox::SuggestTemplateInfo_FuseboxAction::
           SEARCHBOX_OVERRIDE_UNSPECIFIED,
       mojom::SearchboxOverride::kUnspecified},
      {omnibox::SuggestTemplateInfo_FuseboxAction::SEARCHBOX_OVERRIDE_REALBOX,
       mojom::SearchboxOverride::kRealbox},
      {omnibox::SuggestTemplateInfo_FuseboxAction::
           SEARCHBOX_OVERRIDE_COMPOSEBOX,
       mojom::SearchboxOverride::kComposebox},
  };
  for (const auto& test_case : kCases) {
    omnibox::SuggestTemplateInfo::FuseboxAction proto;
    proto.set_searchbox_override(test_case.proto_value);

    mojom::FuseboxActionPtr mojo_action = SyncFuseboxActionProtoToMojo(proto);
    ASSERT_TRUE(mojo_action);
    EXPECT_EQ(mojo_action->searchbox_override, test_case.mojo_value);
  }
}

TEST(FuseboxActionMojoUtilsTest, DebugPrintFuseboxAction) {
  auto action = mojom::FuseboxAction::New();
  action->preselected_tool = omnibox::TOOL_MODE_CANVAS;
  action->preferred_inventory =
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT;
  action->searchbox_override = mojom::SearchboxOverride::kComposebox;

  std::ostringstream os;
  mojom::PrintTo(*action, &os);
  EXPECT_EQ(os.str(),
            "FuseboxAction{\n"
            "  preselected_tool: 2,\n"
            "  preferred_inventory: 0,\n"
            "  preselected_model: null,\n"
            "  query_action_override: null,\n"
            "  preselected_input_source: null,\n"
            "  searchbox_override: 2,\n"
            "}");
}

}  // namespace
}  // namespace fusebox_action
