// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"

#include <string>

#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

constexpr int kMaxChildInTemplate = 3;

// A dummy tree node definition.
struct NodeTemplate {
  ui::AXNodeID node_id;
  int child_count;
  ui::AXNodeID child_ids[kMaxChildInTemplate];
};

ui::AXTreeUpdate CreateAXTreeUpdateFromTemplate(int root_id,
                                                NodeTemplate* nodes_template,
                                                int nodes_count) {
  ui::AXTreeUpdate update;
  update.root_id = root_id;

  for (int i = 0; i < nodes_count; i++) {
    ui::AXNodeData node;
    node.id = nodes_template[i].node_id;
    for (int j = 0; j < nodes_template[i].child_count; j++)
      node.child_ids.push_back(nodes_template[i].child_ids[j]);
    update.nodes.push_back(node);
  }
  return update;
}

int GetAxNodeID(const ::screenai::UiElement& ui_element) {
  for (const auto& attribute : ui_element.attributes()) {
    if (attribute.name() == "/axnode/node_id")
      return attribute.int_value();
  }
  return static_cast<int>(ui::kInvalidAXNodeID);
}

}  // namespace

namespace screen_ai {

using ProtoConvertorTest = testing::Test;

TEST_F(ProtoConvertorTest, ScreenAIVisualAnnotationToAXTreeUpdate) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  {
    chrome_screen_ai::UIComponent* component_0 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_0 =
        component_0->mutable_predicted_type();
    type_0->set_enum_type(chrome_screen_ai::UIComponent::BUTTON);
    type_0->set_confidence(0.8f);
    chrome_screen_ai::Rect* box_0 = component_0->mutable_bounding_box();
    box_0->set_x(0);
    box_0->set_y(1);
    box_0->set_width(2);
    box_0->set_height(3);
    box_0->set_angle(90.0f);

    chrome_screen_ai::UIComponent* component_1 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_1 =
        component_1->mutable_predicted_type();
    type_1->set_string_type("Presentational");
    // If the confidence is low, this component together with all its fields
    // should be ignored.
    type_1->set_confidence(0.05f);
    chrome_screen_ai::Rect* box_1 = component_1->mutable_bounding_box();
    box_1->set_x(0);
    box_1->set_y(0);
    box_1->set_width(5);
    box_1->set_height(5);

    chrome_screen_ai::UIComponent* component_2 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_2 =
        component_2->mutable_predicted_type();
    type_2->set_string_type("Signature");
    type_2->set_confidence(0.6f);
    chrome_screen_ai::Rect* box_2 = component_2->mutable_bounding_box();
    // `x`, `y`, and `angle` should be defaulted to 0 since they are singular
    // proto3 fields, not proto2.
    box_2->set_width(5);
    box_2->set_height(5);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update = ScreenAIVisualAnnotationToAXTreeUpdate(
        serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=1 dialog (0, 0)-(800, 900) child_ids=2,3\n"
        "  id=2 button offset_container_id=1 (0, 1)-(2, 3) transform=[ +0.0000 "
        "-1.0000 +0.0000 +0.0000  \n"
        "  +1.0000 +0.0000 +0.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +1.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +0.0000 +1.0000 ]\n"
        "\n"
        "  id=3 genericContainer offset_container_id=1 (0, 0)-(5, 5) "
        "role_description=Signature\n");
    EXPECT_EQ(expected_update, update.ToString());
  }

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    line_0->set_direction(chrome_screen_ai::Direction::RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_0 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_0 = word_0_0->mutable_bounding_box();
    box_0_0->set_x(100);
    box_0_0->set_y(100);
    box_0_0->set_width(250);
    box_0_0->set_height(20);
    word_0_0->set_utf8_string("Hello");
    word_0_0->set_has_space_after(true);
    word_0_0->set_confidence(0.9f);
    word_0_0->set_estimate_color_success(true);
    word_0_0->set_background_rgb_value(50000);
    word_0_0->set_foreground_rgb_value(25000);
    word_0_0->set_direction(chrome_screen_ai::Direction::RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_1 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_1 = word_0_1->mutable_bounding_box();
    box_0_1->set_x(350);
    box_0_1->set_y(100);
    box_0_1->set_width(250);
    box_0_1->set_height(20);
    word_0_1->set_utf8_string("world");
    // `word_0_1.has_space_after()` should be defaulted to false.
    word_0_1->set_confidence(0.9f);
    word_0_1->set_estimate_color_success(true);
    word_0_1->set_background_rgb_value(50000);
    word_0_1->set_foreground_rgb_value(25000);
    word_0_1->set_direction(chrome_screen_ai::Direction::RIGHT_TO_LEFT);

    chrome_screen_ai::Rect* box_0 = line_0->mutable_bounding_box();
    box_0->set_x(100);
    box_0->set_y(100);
    box_0->set_width(500);
    box_0->set_height(20);
    line_0->set_utf8_string("Hello world");
    line_0->set_confidence(0.9f);
    line_0->set_language("en");
    line_0->set_block_id(2);
    line_0->set_order_within_block(1);

    chrome_screen_ai::LineBox* line_1 = annotation.add_lines();
    line_1->set_confidence(0.0f);
    // Language, and the line as a whole,  should be ignored since the
    // confidence is zero.
    line_1->set_language("en");
    line_1->set_block_id(1);
    line_1->set_order_within_block(0);

    chrome_screen_ai::LineBox* line_2 = annotation.add_lines();
    line_2->set_confidence(0.7f);
    chrome_screen_ai::Rect* box_2 = line_2->mutable_bounding_box();
    // No bounding box should be created in the AXTree because the height is -5.
    box_2->set_width(5);
    box_2->set_height(-5);
    line_2->set_block_id(2);
    line_2->set_order_within_block(0);
    line_2->set_direction(chrome_screen_ai::Direction::UNSPECIFIED);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update = ScreenAIVisualAnnotationToAXTreeUpdate(
        serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=4 dialog (0, 0)-(800, 900) child_ids=5,6\n"
        "  id=5 button offset_container_id=4 (0, 1)-(2, 3) transform=[ +0.0000 "
        "-1.0000 +0.0000 +0.0000  \n"
        "  +1.0000 +0.0000 +0.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +1.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +0.0000 +1.0000 ]\n"
        "\n"
        "  id=6 genericContainer offset_container_id=4 (0, 0)-(5, 5) "
        "role_description=Signature\n"
        "id=7 region (0, 0)-(800, 900) is_page_breaking_object=true "
        "child_ids=8,9\n"
        "  id=8 staticText (0, 0)-(5, 0) name_from=contents text_direction=ltr "
        "name=\n"
        "  id=9 staticText offset_container_id=7 (100, 100)-(500, 20) "
        "name_from=contents text_direction=rtl name=Hello world language=en "
        "child_ids=10,11\n"
        "    id=10 inlineTextBox offset_container_id=9 (100, 100)-(250, 20) "
        "name_from=contents background_color=&C350 color=&61A8 "
        "text_direction=rtl name=Hello \n"
        "    id=11 inlineTextBox offset_container_id=9 (350, 100)-(250, 20) "
        "name_from=contents background_color=&C350 color=&61A8 "
        "text_direction=rtl name=world\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

// Tests if the given tree is properly traversed and new ids are assigned.
TEST_F(ProtoConvertorTest, PreOrderTreeGeneration) {
  // Input Tree:
  // +-- 1
  //     +-- 2
  //         +-- 7
  //         +-- 8
  //             +-- 3
  //     +-- 4
  //         +-- 5
  //         +-- 6
  //         +-- 9
  //     +-- -20

  // Input tree is added in shuffled order to avoid order assumption.
  NodeTemplate input_tree[] = {
      {4, 3, {5, 6, 9}}, {1, 3, {2, 4, -20}}, {7, 0, {}}, {8, 1, {3}},
      {6, 0, {}},        {5, 0, {}},          {3, 0, {}}, {2, 2, {7, 8}},
      {9, 0, {}},        {-20, 0, {}}};
  const int nodes_count = sizeof(input_tree) / sizeof(NodeTemplate);

  // Expected order of nodes in the output.
  int expected_order[] = {1, 2, 7, 8, 3, 4, 5, 6, 9, -20};

  // Create the tree, convert it, and decode from proto.
  ui::AXTreeUpdate tree_update =
      CreateAXTreeUpdateFromTemplate(1, input_tree, nodes_count);
  std::string serialized_proto = Screen2xSnapshotToViewHierarchy(tree_update);
  screenai::ViewHierarchy view_hierarchy;
  ASSERT_TRUE(view_hierarchy.ParseFromString(serialized_proto));

  // Verify.
  EXPECT_EQ(view_hierarchy.ui_elements().size(), nodes_count);
  for (int i = 0; i < nodes_count; i++) {
    const screenai::UiElement& ui_element = view_hierarchy.ui_elements(i);

    // Expect node to be correctly re-ordered.
    EXPECT_EQ(expected_order[i], GetAxNodeID(ui_element));

    // Expect node at index 'i' has id 'i'
    EXPECT_EQ(ui_element.id(), i);
  }
}

}  // namespace screen_ai
