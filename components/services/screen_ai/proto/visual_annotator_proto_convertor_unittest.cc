// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/visual_annotator_proto_convertor.h"

#include <string>

#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

void InitLineBox(chrome_screen_ai::LineBox* line_box,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 int32_t block_id,
                 int32_t order_within_block) {
  chrome_screen_ai::Rect* rect = line_box->mutable_bounding_box();
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  line_box->set_utf8_string(text);
  line_box->set_language(language);
  line_box->set_direction(direction);
  line_box->set_block_id(block_id);
  line_box->set_order_within_block(order_within_block);
}

void InitWordBox(chrome_screen_ai::WordBox* word_box,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 bool has_space_after,
                 int32_t background_rgb_value,
                 int32_t foreground_rgb_value) {
  chrome_screen_ai::Rect* rect = word_box->mutable_bounding_box();
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  word_box->set_utf8_string(text);
  word_box->set_language(language);
  word_box->set_direction(direction);
  word_box->set_has_space_after(has_space_after);
  word_box->set_estimate_color_success(true);
  word_box->set_background_rgb_value(background_rgb_value);
  word_box->set_foreground_rgb_value(foreground_rgb_value);
}

}  // namespace

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
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "id=-2 dialog child_ids=-3,-4 (0, 0)-(800, 900)\n"
        "  id=-3 button offset_container_id=-2 (0, 1)-(2, 3)"
        " transform=[ 0 -1 0 0\n  1 0 0 0\n  0 0 1 0\n  0 0 0 1 ]\n"
        "\n"
        "  id=-4 genericContainer offset_container_id=-2 (0, 0)-(5, 5) "
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

    InitWordBox(line_0->add_words(),
                /*x=*/100,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"Hello",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000);  // Black on white.

    InitWordBox(line_0->add_words(),
                /*x=*/350,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000);  // Blue on white.

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/500,
                /*height=*/20,
                /*text=*/"Hello world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*block_id=*/1,
                /*order_within_block=*/1);
  }

  {
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region child_ids=-3,-5,-7 (0, 0)-(800, 900) "
        "is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 staticText name=Hello world child_ids=-6 "
        "offset_container_id=-2 (100, 100)-(500, 20) "
        "text_direction=rtl language=en\n"
        "    id=-6 inlineTextBox name=Hello world (100, 100)-(500, 20) "
        "background_color=&FFFFFF00 color=&0 text_direction=rtl language=en "
        "word_starts=0,6 word_ends=6,11\n"
        "  id=-7 contentInfo child_ids=-8 (800, 900)-(1, 1)\n"
        "    id=-8 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

}  // namespace screen_ai
