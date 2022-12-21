// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/visual_annotator_proto_convertor.h"

#include <string>

#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai {

using ScreenAIVisualAnnotatorProtoConvertorTest = testing::Test;

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_LayoutExtractionResults) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::UIComponent* component_0 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_0 =
        component_0->mutable_predicted_type();
    type_0->set_enum_type(chrome_screen_ai::UIComponent::BUTTON);
    chrome_screen_ai::Rect* box_0 = component_0->mutable_bounding_box();
    box_0->set_x(0);
    box_0->set_y(1);
    box_0->set_width(2);
    box_0->set_height(3);
    box_0->set_angle(90.0f);

    chrome_screen_ai::UIComponent* component_1 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_1 =
        component_1->mutable_predicted_type();
    type_1->set_string_type("Signature");
    chrome_screen_ai::Rect* box_1 = component_1->mutable_bounding_box();
    // `x`, `y`, and `angle` should be defaulted to 0 since they are singular
    // proto3 fields, not proto2.
    box_1->set_width(5);
    box_1->set_height(5);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=1 dialog child_ids=2,3 (0, 0)-(800, 900)\n"
        "  id=2 button offset_container_id=1 (0, 1)-(2, 3)"
        " transform=[ 0 -1 0 0\n  1 0 0 0\n  0 0 1 0\n  0 0 0 1 ]\n"
        "\n"
        "  id=3 genericContainer offset_container_id=1 (0, 0)-(5, 5) "
        "role_description=Signature\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_OcrResults) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    line_0->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_0 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_0 = word_0_0->mutable_bounding_box();
    box_0_0->set_x(100);
    box_0_0->set_y(100);
    box_0_0->set_width(250);
    box_0_0->set_height(20);
    word_0_0->set_utf8_string("Hello");
    word_0_0->set_language("en");
    word_0_0->set_has_space_after(true);
    word_0_0->set_estimate_color_success(true);
    word_0_0->set_background_rgb_value(50000);
    word_0_0->set_foreground_rgb_value(25000);
    word_0_0->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_1 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_1 = word_0_1->mutable_bounding_box();
    box_0_1->set_x(350);
    box_0_1->set_y(100);
    box_0_1->set_width(250);
    box_0_1->set_height(20);
    word_0_1->set_utf8_string("world");
    word_0_1->set_language("en");
    // `word_0_1.has_space_after()` should be defaulted to false.
    word_0_1->set_estimate_color_success(true);
    word_0_1->set_background_rgb_value(50000);
    word_0_1->set_foreground_rgb_value(25000);
    word_0_1->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::Rect* box_0 = line_0->mutable_bounding_box();
    box_0->set_x(100);
    box_0->set_y(100);
    box_0->set_width(500);
    box_0->set_height(20);
    line_0->set_utf8_string("Hello world");
    line_0->set_language("en");
    line_0->set_block_id(1);
    line_0->set_order_within_block(1);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=1 region child_ids=2 (0, 0)-(800, 900) "
        "is_page_breaking_object=true\n"
        "  id=2 staticText name=Hello world child_ids=3 offset_container_id=1 "
        "(100, 100)-(500, 20) "
        "text_direction=rtl language=en\n"
        "    id=3 inlineTextBox name=Hello world (100, 100)-(500, 20) "
        "background_color=&C350 color=&61A8 text_direction=rtl language=en "
        "word_starts=0,6 word_ends=6,11\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

}  // namespace screen_ai
