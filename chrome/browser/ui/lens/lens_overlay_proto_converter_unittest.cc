// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_deep_gleam_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_geometry.pb.h"
#include "third_party/lens_server_proto/lens_overlay_overlay_object.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_text.pb.h"
#include "ui/gfx/geometry/size_f.h"

namespace lens {

// Struct for creating fake geometry data.
struct BoundingBoxStruct {
  std::string id;
  float center_x;
  float center_y;
  float width;
  float height;
  float rotation_z;
  lens::CoordinateType coordinate_type;
};

// Struct for creating fake word text data.
struct WordStruct {
  BoundingBoxStruct geometry;
  std::string formula_metadata_latex;
  std::string plain_text;
  std::string text_separator;
  lens::TextLayout_Word_Type word_type;
};

// Struct for creating fake line text data.
struct LineStruct {
  BoundingBoxStruct geometry;
  std::vector<WordStruct> words;
};

// Struct for creating fake text data.
struct TextStruct {
  BoundingBoxStruct paragraph_geometry;
  std::vector<LineStruct> lines;
  lens::WritingDirection writing_direction;
  std::string content_language;
};

// Struct for creating fake translation word indices.
struct TranslationWordStruct {
  int start_index;
  int end_index;
  std::string_view expected_translation;
  std::string_view expected_separator;
};

// Struct for creating fake translation lines.
struct TranslationLineStruct {
  int start_index;
  int end_index;
  uint32_t background_color;
  std::string_view background_image_bytes;
  int background_image_height;
  int background_image_width;
  int background_image_vertical_padding;
  int background_image_horizontal_padding;
  uint32_t text_color;
  std::string_view text_mask_bytes;
  std::vector<TranslationWordStruct> words;
};

// Struct for creating fake translation text data.
struct TranslationTextStruct {
  BoundingBoxStruct line_geometry;
  std::string_view translation_text;
  std::string_view source_language;
  std::string_view target_language;
  std::vector<TranslationLineStruct> lines;
  lens::Alignment alignment;
  lens::WritingDirection writing_direction;
  lens::TranslationData::Status::Code status_code;
};

inline constexpr BoundingBoxStruct kTestBoundingBox1 = {
    .id = "0",
    .center_x = 0.5,
    .center_y = 0.5,
    .width = 0.1,
    .height = 0.1,
    .rotation_z = 1,
    .coordinate_type = lens::NORMALIZED};

inline constexpr BoundingBoxStruct kTestBoundingBox2 = {
    .id = "1",
    .center_x = 0.2,
    .center_y = 0.2,
    .width = 0.2,
    .height = 0.2,
    .rotation_z = 0,
    .coordinate_type = lens::IMAGE};

inline const TextStruct kTestText = {
    .paragraph_geometry = kTestBoundingBox1,
    .lines = {{
        .geometry = kTestBoundingBox1,
        .words = {{
            .geometry = kTestBoundingBox1,
            .formula_metadata_latex = "latex",
            .plain_text = "plain",
            .text_separator = " ",
            .word_type = lens::TextLayout_Word_Type::TextLayout_Word_Type_TEXT,
        }},
    }},
    .writing_direction = lens::DEFAULT_WRITING_DIRECTION_LEFT_TO_RIGHT,
    .content_language = "en",
};

inline const TextStruct kTestTextMultiline = {
    .paragraph_geometry = kTestBoundingBox1,
    .lines =
        {{
             .geometry = kTestBoundingBox1,
             .words = {{
                 .geometry = kTestBoundingBox1,
                 .formula_metadata_latex = "latex",
                 .plain_text = "plain",
                 .text_separator = " ",
                 .word_type =
                     lens::TextLayout_Word_Type::TextLayout_Word_Type_FORMULA,
             }},
         },
         {
             .geometry = kTestBoundingBox2,
             .words = {{
                 .geometry = kTestBoundingBox2,
                 .formula_metadata_latex = "",
                 .plain_text = "text",
                 .text_separator = "",
                 .word_type =
                     lens::TextLayout_Word_Type::TextLayout_Word_Type_TEXT,

             }},
         }},
    .writing_direction = lens::WRITING_DIRECTION_RIGHT_TO_LEFT,
    .content_language = "fr",
};

inline const TranslationTextStruct kTestTranslationText = {
    .translation_text = "Hola, mundo!",
    .source_language = "fr",
    .target_language = "es",
    .lines = {{
        .start_index = 0,
        .end_index = 12,
        .background_color = 100,
        .background_image_bytes = "background",
        .background_image_height = 10,
        .background_image_width = 15,
        .background_image_vertical_padding = 25,
        .background_image_horizontal_padding = 20,
        .text_color = 200,
        .text_mask_bytes = "text-mask",
        .words = {{
                      .start_index = 0,
                      .end_index = 5,
                  },
                  {
                      .start_index = 6,
                      .end_index = 12,
                  }},
    }},
    .alignment = lens::Alignment::DEFAULT_LEFT_ALIGNED,
    .writing_direction = lens::DEFAULT_WRITING_DIRECTION_LEFT_TO_RIGHT,
    .status_code = lens::TranslationData::Status::SUCCESS,
};

inline const TranslationTextStruct kTestUnicodeTranslationText = {
    .translation_text = "美丽世界",
    .source_language = "fr",
    .target_language = "zh",
    .lines = {{
        .start_index = 0,
        .end_index = 4,
        .background_color = 100,
        .background_image_bytes = "background",
        .background_image_height = 10,
        .background_image_width = 15,
        .background_image_vertical_padding = 25,
        .background_image_horizontal_padding = 20,
        .text_color = 200,
        .text_mask_bytes = "text-mask",
        .words = {{
                      .start_index = 0,
                      .end_index = 2,
                      .expected_translation = "美丽",
                      .expected_separator = "",
                  },
                  {
                      .start_index = 2,
                      .end_index = 4,
                      .expected_translation = "世界",
                      .expected_separator = "",
                  }},
    }},
    .alignment = lens::Alignment::DEFAULT_LEFT_ALIGNED,
    .writing_direction = lens::DEFAULT_WRITING_DIRECTION_LEFT_TO_RIGHT,
    .status_code = lens::TranslationData::Status::SUCCESS,
};

inline const TranslationTextStruct kTestUnicodeTranslationTextMutliline = {
    .translation_text = "美丽世界。我在这里",
    .source_language = "fr",
    .target_language = "zh",
    .lines = {{
                  .start_index = 0,
                  .end_index = 5,
                  .background_color = 100,
                  .background_image_bytes = "background",
                  .background_image_height = 10,
                  .background_image_width = 15,
                  .background_image_vertical_padding = 25,
                  .background_image_horizontal_padding = 20,
                  .text_color = 200,
                  .text_mask_bytes = "text-mask",
                  .words = {{
                                .start_index = 0,
                                .end_index = 2,
                                .expected_translation = "美丽",
                                .expected_separator = "",

                            },
                            {
                                .start_index = 2,
                                .end_index = 4,
                                .expected_translation = "世界",
                                .expected_separator = "。",

                            }},
              },
              {
                  .start_index = 5,
                  .end_index = 9,
                  .background_color = 200,
                  .background_image_bytes = "bytes",
                  .background_image_height = 5,
                  .background_image_width = 9,
                  .background_image_vertical_padding = 10,
                  .background_image_horizontal_padding = 5,
                  .text_color = 300,
                  .text_mask_bytes = "mask-text",
                  .words = {{
                                .start_index = 5,
                                .end_index = 7,
                                .expected_translation = "我在",
                                .expected_separator = "",

                            },
                            {
                                .start_index = 7,
                                .end_index = 9,
                                .expected_translation = "这里",
                                .expected_separator = "",

                            }},
              }},
    .alignment = lens::Alignment::DEFAULT_LEFT_ALIGNED,
    .writing_direction = lens::DEFAULT_WRITING_DIRECTION_LEFT_TO_RIGHT,
    .status_code = lens::TranslationData::Status::SUCCESS,
};

class LensOverlayProtoConverterTest : public testing::Test {
 protected:
  lens::LensOverlayServerResponse CreateLensServerOverlayResponse(
      std::vector<lens::OverlayObject> server_objects) {
    lens::LensOverlayServerResponse server_response =
        lens::LensOverlayServerResponse::default_instance();
    for (auto server_object : server_objects) {
      auto* overlay_object =
          server_response.mutable_objects_response()->add_overlay_objects();
      overlay_object->CopyFrom(server_object);
    }
    return server_response;
  }

  lens::OverlayObject CreateServerOverlayObject(BoundingBoxStruct box) {
    lens::OverlayObject object;
    object.set_id(box.id);
    object.mutable_interaction_properties()->set_select_on_tap(true);
    CreateServerGeometry(box, object.mutable_geometry());
    return object;
  }

  lens::OverlayObject CreateServerOverlayObjectWithPolygon(
      BoundingBoxStruct box) {
    lens::OverlayObject object;
    object.set_id(box.id);
    object.mutable_interaction_properties()->set_select_on_tap(true);
    CreateServerGeometryWithPolygon(box, object.mutable_geometry());
    return object;
  }

  void CreateServerText(TextStruct text_struct, lens::Text* text) {
    text->set_content_language(text_struct.content_language);
    auto* paragraph = text->mutable_text_layout()->add_paragraphs();
    paragraph->set_content_language(text_struct.content_language);
    CreateServerGeometry(text_struct.paragraph_geometry,
                         paragraph->mutable_geometry());

    for (const auto& line_struct : text_struct.lines) {
      auto* line = paragraph->add_lines();
      CreateServerGeometry(line_struct.geometry, line->mutable_geometry());
      for (const auto& word_struct : line_struct.words) {
        auto* word = line->add_words();
        word->set_plain_text(word_struct.plain_text);
        word->set_text_separator(word_struct.text_separator);
        CreateServerGeometry(word_struct.geometry, word->mutable_geometry());
        word->mutable_formula_metadata()->set_latex(
            word_struct.formula_metadata_latex);
      }
    }
  }

  lens::DeepGleamData CreateServerTranslationText(
      TranslationTextStruct translation_struct) {
    lens::DeepGleamData deep_gleam_data;
    deep_gleam_data.mutable_translation()->set_translation(
        translation_struct.translation_text.data());
    deep_gleam_data.mutable_translation()->set_source_language(
        translation_struct.source_language.data());
    deep_gleam_data.mutable_translation()->set_target_language(
        translation_struct.target_language.data());
    deep_gleam_data.mutable_translation()->set_alignment(
        translation_struct.alignment);
    deep_gleam_data.mutable_translation()->set_writing_direction(
        translation_struct.writing_direction);
    deep_gleam_data.mutable_translation()->mutable_status()->set_code(
        translation_struct.status_code);

    for (const auto& lineStruct : translation_struct.lines) {
      auto* line = deep_gleam_data.mutable_translation()->add_line();
      line->set_start(lineStruct.start_index);
      line->set_end(lineStruct.end_index);
      line->mutable_style()->set_background_primary_color(
          lineStruct.background_color);
      line->mutable_style()->set_text_color(lineStruct.text_color);
      line->mutable_background_image_data()->set_image_height(
          lineStruct.background_image_height);
      line->mutable_background_image_data()->set_image_width(
          lineStruct.background_image_width);
      line->mutable_background_image_data()->set_vertical_padding(
          lineStruct.background_image_vertical_padding);
      line->mutable_background_image_data()->set_horizontal_padding(
          lineStruct.background_image_horizontal_padding);
      line->mutable_background_image_data()->set_background_image(
          lineStruct.background_image_bytes.data());
      line->mutable_background_image_data()->set_text_mask(
          lineStruct.text_mask_bytes.data());

      for (const auto& wordStruct : lineStruct.words) {
        auto* word = line->add_word();
        word->set_start(wordStruct.start_index);
        word->set_end(wordStruct.end_index);
      }
    }

    return deep_gleam_data;
  }

  void CreateServerGeometry(BoundingBoxStruct box, lens::Geometry* geometry) {
    geometry->mutable_bounding_box()->set_center_x(box.center_x);
    geometry->mutable_bounding_box()->set_center_y(box.center_y);
    geometry->mutable_bounding_box()->set_height(box.height);
    geometry->mutable_bounding_box()->set_width(box.width);
    geometry->mutable_bounding_box()->set_rotation_z(box.rotation_z);
    geometry->mutable_bounding_box()->set_coordinate_type(box.coordinate_type);
  }

  void CreateServerGeometryWithPolygon(
      BoundingBoxStruct box, lens::Geometry* geometry) {
    CreateServerGeometry(box, geometry);
    auto* polygon = geometry->add_segmentation_polygon();
    auto* vertex = polygon->add_vertex();
    vertex->set_x(0.19);
    vertex->set_y(0.21);
    polygon->set_vertex_ordering(lens::Polygon::CLOCKWISE);
    polygon->set_coordinate_type(lens::NORMALIZED);
  }

  void VerifyGeometriesAreEqual(
      lens::Geometry server_geometry,
      lens::mojom::GeometryPtr mojo_geometry) {
    EXPECT_EQ(gfx::PointF(server_geometry.bounding_box().center_x(),
                          server_geometry.bounding_box().center_y()),
              mojo_geometry->bounding_box->box.origin());
    EXPECT_EQ(server_geometry.bounding_box().rotation_z(),
              mojo_geometry->bounding_box->rotation);
    EXPECT_EQ(gfx::SizeF(server_geometry.bounding_box().width(),
                         server_geometry.bounding_box().height()),
              mojo_geometry->bounding_box->box.size());
    EXPECT_EQ(
        static_cast<int>(server_geometry.bounding_box().coordinate_type()),
        static_cast<int>(mojo_geometry->bounding_box->coordinate_type));

    EXPECT_EQ(static_cast<size_t>(
                  server_geometry.segmentation_polygon().size()),
              mojo_geometry->segmentation_polygon.size());
    for (int i = 0; i < server_geometry.segmentation_polygon().size(); i++) {
      lens::Polygon server_polygon =
          server_geometry.segmentation_polygon().at(i);
      lens::mojom::PolygonPtr mojo_polygon =
          mojo_geometry->segmentation_polygon.at(i)->Clone();
      EXPECT_EQ(static_cast<size_t>(server_polygon.vertex().size()),
                mojo_polygon->vertex.size());
      for (int j = 0; j < server_polygon.vertex().size(); j++) {
        lens::Polygon::Vertex server_vertex = server_polygon.vertex().at(j);
        lens::mojom::VertexPtr mojo_vertex =
            mojo_polygon->vertex.at(j)->Clone();
        EXPECT_EQ(server_vertex.x(), mojo_vertex->x);
        EXPECT_EQ(server_vertex.y(), mojo_vertex->y);
      }
      EXPECT_EQ(static_cast<int>(server_polygon.vertex_ordering()),
                static_cast<int>(mojo_polygon->vertex_ordering));
      EXPECT_EQ(static_cast<int>(server_polygon.coordinate_type()),
                static_cast<int>(mojo_polygon->coordinate_type));
    }
  }

  void VerifyOverlayObjectsAreEqual(
      std::vector<lens::OverlayObject> server_objects,
      std::vector<lens::mojom::OverlayObjectPtr> mojo_objects) {
    EXPECT_EQ(server_objects.size(), mojo_objects.size());
    for (size_t i = 0; i < server_objects.size() && i < mojo_objects.size();
         i++) {
      lens::OverlayObject server_object = server_objects.at(i);
      lens::mojom::OverlayObjectPtr mojo_object = mojo_objects.at(i)->Clone();
      EXPECT_EQ(server_object.id(), mojo_object->id);
      VerifyGeometriesAreEqual(server_object.geometry(),
                                        std::move(mojo_object->geometry));
    }
  }
};

TEST_F(LensOverlayProtoConverterTest,
       CreateObjectsMojomArrayFromServerResponse) {
  lens::OverlayObject no_tap_object =
      CreateServerOverlayObject(kTestBoundingBox1);
  no_tap_object.mutable_interaction_properties()->set_select_on_tap(false);
  std::vector<lens::OverlayObject> server_objects = {
      CreateServerOverlayObject(kTestBoundingBox1),
      CreateServerOverlayObjectWithPolygon(kTestBoundingBox2), no_tap_object};
  std::vector<lens::OverlayObject> server_objects_with_tap = {
      CreateServerOverlayObject(kTestBoundingBox1),
      CreateServerOverlayObjectWithPolygon(kTestBoundingBox2)};

  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse(server_objects);

  std::vector<lens::mojom::OverlayObjectPtr> mojo_objects =
      lens::CreateObjectsMojomArrayFromServerResponse(server_response);
  EXPECT_FALSE(mojo_objects.empty());
  VerifyOverlayObjectsAreEqual(std::move(server_objects_with_tap),
                               std::move(mojo_objects));
}

TEST_F(LensOverlayProtoConverterTest,
       CreateObjectsMojomArrayFromServerResponse_Empty) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});

  std::vector<lens::mojom::OverlayObjectPtr> mojo_objects =
      lens::CreateObjectsMojomArrayFromServerResponse(server_response);
  EXPECT_TRUE(mojo_objects.empty());
}

TEST_F(LensOverlayProtoConverterTest,
       CreateObjectsMojomArrayFromServerResponse_NoObjectsResponse) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  server_response.clear_objects_response();

  std::vector<lens::mojom::OverlayObjectPtr> mojo_objects =
      lens::CreateObjectsMojomArrayFromServerResponse(server_response);
  EXPECT_TRUE(mojo_objects.empty());
}

TEST_F(LensOverlayProtoConverterTest, CreateTextMojomFromServerResponse) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  CreateServerText(kTestText,
                   server_response.mutable_objects_response()->mutable_text());

  // Compare top level text object.
  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size());
  EXPECT_TRUE(mojo_text);
  EXPECT_EQ(mojo_text->content_language, kTestText.content_language);

  // Compare paragraphs.
  EXPECT_EQ(mojo_text->text_layout->paragraphs.size(),
            static_cast<unsigned long>(1));
  lens::TextLayout_Paragraph server_paragraph =
      server_response.objects_response().text().text_layout().paragraphs()[0];
  const lens::mojom::ParagraphPtr& mojo_paragraph =
      mojo_text->text_layout->paragraphs[0];
  EXPECT_EQ(mojo_paragraph->content_language, kTestText.content_language);
  EXPECT_TRUE(mojo_paragraph->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(mojo_paragraph->writing_direction.value()),
            static_cast<int>(kTestText.writing_direction));
  VerifyGeometriesAreEqual(server_paragraph.geometry(),
                           mojo_paragraph->geometry->Clone());

  // Compare line for a paragraph.
  EXPECT_EQ(mojo_paragraph->lines.size(), static_cast<unsigned long>(1));
  lens::TextLayout_Line server_line = server_paragraph.lines()[0];
  lens::mojom::LinePtr mojo_line = mojo_paragraph->lines[0]->Clone();
  VerifyGeometriesAreEqual(server_line.geometry(),
                           mojo_line->geometry->Clone());

  // Compare words in line.
  EXPECT_EQ(mojo_line->words.size(), static_cast<unsigned long>(1));
  lens::mojom::WordPtr mojo_word = mojo_line->words[0]->Clone();
  const auto& test_word_struct = kTestText.lines[0].words[0];
  EXPECT_EQ(mojo_word->plain_text, test_word_struct.plain_text);
  EXPECT_EQ(mojo_word->text_separator, test_word_struct.text_separator);
  lens::TextLayout_Word server_word = server_line.words()[0];
  VerifyGeometriesAreEqual(server_word.geometry(),
                           mojo_word->geometry->Clone());
  EXPECT_TRUE(mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(mojo_word->writing_direction.value()),
            static_cast<int>(kTestText.writing_direction));
  EXPECT_EQ(mojo_word->formula_metadata->latex,
            test_word_struct.formula_metadata_latex);
}

TEST_F(LensOverlayProtoConverterTest,
       CreateTextMojomFromServerResponse_WithTranslation) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  CreateServerText(kTestText,
                   server_response.mutable_objects_response()->mutable_text());
  server_response.mutable_objects_response()->mutable_deep_gleams()->Add(
      CreateServerTranslationText(kTestTranslationText));

  // Compare top level text object.
  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size(1, 1));
  EXPECT_TRUE(mojo_text);
  EXPECT_EQ(mojo_text->content_language, kTestText.content_language);

  // Compare paragraphs.
  lens::TextLayout_Paragraph server_paragraph =
      server_response.objects_response().text().text_layout().paragraphs()[0];
  const lens::mojom::ParagraphPtr& mojo_paragraph =
      mojo_text->text_layout->paragraphs[0];
  EXPECT_TRUE(mojo_paragraph->translation);
  EXPECT_EQ(mojo_text->text_layout->paragraphs.size(),
            static_cast<unsigned long>(1));
  const lens::mojom::TranslatedParagraphPtr& mojo_translate_paragraph =
      mojo_paragraph->translation;
  EXPECT_EQ(mojo_translate_paragraph->content_language,
            kTestTranslationText.target_language);
  EXPECT_TRUE(mojo_translate_paragraph->writing_direction.has_value());
  EXPECT_EQ(
      static_cast<int>(mojo_translate_paragraph->writing_direction.value()),
      static_cast<int>(kTestTranslationText.writing_direction));
  EXPECT_TRUE(mojo_translate_paragraph->alignment.has_value());
  EXPECT_EQ(static_cast<int>(mojo_translate_paragraph->alignment.value()),
            static_cast<int>(kTestTranslationText.alignment));
  VerifyGeometriesAreEqual(server_paragraph.geometry(),
                           mojo_paragraph->geometry->Clone());

  // Compare line for a paragraph.
  EXPECT_EQ(mojo_translate_paragraph->lines.size(),
            static_cast<unsigned long>(1));
  const lens::mojom::TranslatedLinePtr& mojo_line =
      mojo_translate_paragraph->lines[0];
  const auto& test_line_struct = kTestTranslationText.lines[0];
  lens::TextLayout_Line server_line = server_paragraph.lines()[0];
  VerifyGeometriesAreEqual(server_line.geometry(),
                           mojo_line->geometry->Clone());
  EXPECT_EQ(mojo_line->translation, kTestTranslationText.translation_text);
  EXPECT_EQ(mojo_line->text_color, test_line_struct.text_color);
  EXPECT_EQ(mojo_line->background_primary_color,
            test_line_struct.background_color);

  // Compare background image data of line.
  const lens::mojom::BackgroundImageDataPtr& image_data =
      mojo_line->background_image_data;
  EXPECT_EQ(image_data->image_size.height(),
            test_line_struct.background_image_height);
  EXPECT_EQ(image_data->image_size.width(),
            test_line_struct.background_image_width);
  EXPECT_EQ(image_data->vertical_padding,
            test_line_struct.background_image_vertical_padding);
  EXPECT_EQ(image_data->horizontal_padding,
            test_line_struct.background_image_horizontal_padding);
  std::string background_image = std::string(
      image_data->background_image.begin(), image_data->background_image.end());
  EXPECT_EQ(background_image, test_line_struct.background_image_bytes);
  std::string text_mask =
      std::string(image_data->text_mask.begin(), image_data->text_mask.end());
  EXPECT_EQ(text_mask, test_line_struct.text_mask_bytes);

  // Compare words in line.
  EXPECT_EQ(mojo_line->words.size(), static_cast<unsigned long>(2));
  const lens::mojom::WordPtr& first_mojo_word = mojo_line->words[0];
  EXPECT_EQ(first_mojo_word->plain_text, "Hola,");
  EXPECT_EQ(first_mojo_word->text_separator, " ");
  EXPECT_TRUE(first_mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(first_mojo_word->writing_direction.value()),
            static_cast<int>(kTestTranslationText.writing_direction));

  const lens::mojom::WordPtr& second_mojo_word = mojo_line->words[1];
  EXPECT_EQ(second_mojo_word->plain_text, "mundo!");
  EXPECT_EQ(second_mojo_word->text_separator, "");
  EXPECT_TRUE(second_mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(second_mojo_word->writing_direction.value()),
            static_cast<int>(kTestTranslationText.writing_direction));
}

TEST_F(LensOverlayProtoConverterTest,
       CreateTextMojomFromServerResponse_WithUnicodeTranslation) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  CreateServerText(kTestText,
                   server_response.mutable_objects_response()->mutable_text());
  server_response.mutable_objects_response()->mutable_deep_gleams()->Add(
      CreateServerTranslationText(kTestUnicodeTranslationText));

  // Compare top level text object.
  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size(1, 1));
  EXPECT_TRUE(mojo_text);
  EXPECT_EQ(mojo_text->content_language, kTestText.content_language);

  // Compare paragraphs.
  lens::TextLayout_Paragraph server_paragraph =
      server_response.objects_response().text().text_layout().paragraphs()[0];
  const lens::mojom::ParagraphPtr& mojo_paragraph =
      mojo_text->text_layout->paragraphs[0];
  EXPECT_TRUE(mojo_paragraph->translation);
  EXPECT_EQ(mojo_text->text_layout->paragraphs.size(),
            static_cast<unsigned long>(1));
  const lens::mojom::TranslatedParagraphPtr& mojo_translate_paragraph =
      mojo_paragraph->translation;
  EXPECT_EQ(mojo_translate_paragraph->content_language,
            kTestUnicodeTranslationText.target_language);
  EXPECT_TRUE(mojo_translate_paragraph->writing_direction.has_value());
  EXPECT_EQ(
      static_cast<int>(mojo_translate_paragraph->writing_direction.value()),
      static_cast<int>(kTestUnicodeTranslationText.writing_direction));
  EXPECT_TRUE(mojo_translate_paragraph->alignment.has_value());
  EXPECT_EQ(static_cast<int>(mojo_translate_paragraph->alignment.value()),
            static_cast<int>(kTestUnicodeTranslationText.alignment));
  VerifyGeometriesAreEqual(server_paragraph.geometry(),
                           mojo_paragraph->geometry->Clone());

  // Compare line for a paragraph.
  EXPECT_EQ(mojo_translate_paragraph->lines.size(),
            static_cast<unsigned long>(1));
  const lens::mojom::TranslatedLinePtr& mojo_line =
      mojo_translate_paragraph->lines[0];
  const auto& test_line_struct = kTestUnicodeTranslationText.lines[0];
  lens::TextLayout_Line server_line = server_paragraph.lines()[0];
  VerifyGeometriesAreEqual(server_line.geometry(),
                           mojo_line->geometry->Clone());
  EXPECT_EQ(mojo_line->translation,
            kTestUnicodeTranslationText.translation_text);
  EXPECT_EQ(mojo_line->text_color, test_line_struct.text_color);
  EXPECT_EQ(mojo_line->background_primary_color,
            test_line_struct.background_color);

  // Compare background image data of line.
  const lens::mojom::BackgroundImageDataPtr& image_data =
      mojo_line->background_image_data;
  EXPECT_EQ(image_data->image_size.height(),
            test_line_struct.background_image_height);
  EXPECT_EQ(image_data->image_size.width(),
            test_line_struct.background_image_width);
  EXPECT_EQ(image_data->vertical_padding,
            test_line_struct.background_image_vertical_padding);
  EXPECT_EQ(image_data->horizontal_padding,
            test_line_struct.background_image_horizontal_padding);
  std::string background_image = std::string(
      image_data->background_image.begin(), image_data->background_image.end());
  EXPECT_EQ(background_image, test_line_struct.background_image_bytes);
  std::string text_mask =
      std::string(image_data->text_mask.begin(), image_data->text_mask.end());
  EXPECT_EQ(text_mask, test_line_struct.text_mask_bytes);

  // Compare words in line.
  EXPECT_EQ(mojo_line->words.size(), static_cast<unsigned long>(2));
  const lens::mojom::WordPtr& first_mojo_word = mojo_line->words[0];
  EXPECT_EQ(first_mojo_word->plain_text, "美丽");
  EXPECT_EQ(first_mojo_word->text_separator, "");
  EXPECT_TRUE(first_mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(first_mojo_word->writing_direction.value()),
            static_cast<int>(kTestUnicodeTranslationText.writing_direction));

  const lens::mojom::WordPtr& second_mojo_word = mojo_line->words[1];
  EXPECT_EQ(second_mojo_word->plain_text, "世界");
  EXPECT_EQ(second_mojo_word->text_separator, "");
  EXPECT_TRUE(second_mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(second_mojo_word->writing_direction.value()),
            static_cast<int>(kTestUnicodeTranslationText.writing_direction));
}

TEST_F(LensOverlayProtoConverterTest,
       CreateTextMojomFromServerResponse_Multiline) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  CreateServerText(kTestTextMultiline,
                   server_response.mutable_objects_response()->mutable_text());
  server_response.mutable_objects_response()->mutable_deep_gleams()->Add(
      CreateServerTranslationText(kTestUnicodeTranslationTextMutliline));

  // Compare top level text object.
  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size(1, 1));
  EXPECT_TRUE(mojo_text);
  EXPECT_EQ(mojo_text->content_language, kTestTextMultiline.content_language);

  // Compare paragraphs.
  lens::TextLayout_Paragraph server_paragraph =
      server_response.objects_response().text().text_layout().paragraphs()[0];
  const lens::mojom::ParagraphPtr& mojo_paragraph =
      mojo_text->text_layout->paragraphs[0];
  EXPECT_TRUE(mojo_paragraph->translation);
  EXPECT_EQ(mojo_text->text_layout->paragraphs.size(),
            static_cast<unsigned long>(1));
  const lens::mojom::TranslatedParagraphPtr& mojo_translate_paragraph =
      mojo_paragraph->translation;
  EXPECT_EQ(mojo_translate_paragraph->content_language,
            kTestUnicodeTranslationTextMutliline.target_language);
  EXPECT_TRUE(mojo_translate_paragraph->writing_direction.has_value());
  EXPECT_EQ(
      static_cast<int>(mojo_translate_paragraph->writing_direction.value()),
      static_cast<int>(kTestUnicodeTranslationTextMutliline.writing_direction));
  EXPECT_TRUE(mojo_translate_paragraph->alignment.has_value());
  EXPECT_EQ(static_cast<int>(mojo_translate_paragraph->alignment.value()),
            static_cast<int>(kTestUnicodeTranslationTextMutliline.alignment));
  VerifyGeometriesAreEqual(server_paragraph.geometry(),
                           mojo_paragraph->geometry->Clone());

  // Verify lines.
  EXPECT_EQ(mojo_translate_paragraph->lines.size(),
            kTestUnicodeTranslationTextMutliline.lines.size());
  for (unsigned long i = 0; i < mojo_translate_paragraph->lines.size(); i++) {
    const auto& mojo_line = mojo_translate_paragraph->lines[i];
    const auto& test_line_struct =
        kTestUnicodeTranslationTextMutliline.lines[i];
    lens::TextLayout_Line server_line = server_paragraph.lines()[i];
    VerifyGeometriesAreEqual(server_line.geometry(),
                             mojo_line->geometry->Clone());
    EXPECT_EQ(mojo_line->translation,
              kTestUnicodeTranslationTextMutliline.translation_text);
    EXPECT_EQ(mojo_line->text_color, test_line_struct.text_color);
    EXPECT_EQ(mojo_line->background_primary_color,
              test_line_struct.background_color);

    // Compare background image data of line.
    const lens::mojom::BackgroundImageDataPtr& image_data =
        mojo_line->background_image_data;
    EXPECT_EQ(image_data->image_size.height(),
              test_line_struct.background_image_height);
    EXPECT_EQ(image_data->image_size.width(),
              test_line_struct.background_image_width);
    EXPECT_EQ(image_data->vertical_padding,
              test_line_struct.background_image_vertical_padding);
    EXPECT_EQ(image_data->horizontal_padding,
              test_line_struct.background_image_horizontal_padding);
    std::string background_image =
        std::string(image_data->background_image.begin(),
                    image_data->background_image.end());
    EXPECT_EQ(background_image, test_line_struct.background_image_bytes);
    std::string text_mask =
        std::string(image_data->text_mask.begin(), image_data->text_mask.end());
    EXPECT_EQ(text_mask, test_line_struct.text_mask_bytes);

    // Verify words in lines.
    EXPECT_EQ(mojo_line->words.size(), test_line_struct.words.size());
    for (unsigned long j = 0; j < mojo_line->words.size(); j++) {
      const auto& mojo_word = mojo_line->words[j];
      const auto& test_word_struct = test_line_struct.words[j];
      EXPECT_EQ(mojo_word->plain_text, test_word_struct.expected_translation);
      EXPECT_EQ(mojo_word->text_separator, test_word_struct.expected_separator);
      EXPECT_TRUE(mojo_word->writing_direction.has_value());
      EXPECT_EQ(
          static_cast<int>(mojo_word->writing_direction.value()),
          static_cast<int>(kTestUnicodeTranslationText.writing_direction));
    }
  }
}

TEST_F(LensOverlayProtoConverterTest, CreateTextMojomFromServerResponse_Empty) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size());
  EXPECT_FALSE(mojo_text);
}

TEST_F(LensOverlayProtoConverterTest,
       CreateTextMojomFromServerResponse_NoObjectsResponse) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  server_response.clear_objects_response();

  lens::mojom::TextPtr mojo_text = lens::CreateTextMojomFromServerResponse(
      server_response, /*resized_bitmap_size=*/gfx::Size());
  EXPECT_FALSE(mojo_text);
}

}  // namespace lens
