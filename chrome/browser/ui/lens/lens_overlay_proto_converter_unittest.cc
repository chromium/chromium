// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_geometry.pb.h"
#include "third_party/lens_server_proto/lens_overlay_overlay_object.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_text.pb.h"

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

// Struct for creating fake text data for one word.
struct TextStruct {
  BoundingBoxStruct paragraph_geometry;
  BoundingBoxStruct line_geometry;
  std::string word_plain_text;
  std::string word_text_seperator;
  BoundingBoxStruct word_geometry;
  lens::TextLayout_Word_Type word_type;
  std::string formula_metadata_latex;
  lens::WritingDirection writing_direction;
  std::string content_language;
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

inline constexpr TextStruct kTestText = {
    .paragraph_geometry = kTestBoundingBox1,
    .line_geometry = kTestBoundingBox1,
    .word_plain_text = "plain",
    .word_text_seperator = " ",
    .word_geometry = kTestBoundingBox1,
    .word_type = lens::TextLayout_Word_Type::TextLayout_Word_Type_TEXT,
    .formula_metadata_latex = "latex",
    .writing_direction = lens::DEFAULT_WRITING_DIRECTION_LEFT_TO_RIGHT,
    .content_language = "en",
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
    paragraph->set_content_language(kTestText.content_language);
    CreateServerGeometry(text_struct.paragraph_geometry,
                         paragraph->mutable_geometry());

    auto* line = paragraph->add_lines();
    CreateServerGeometry(text_struct.line_geometry, line->mutable_geometry());

    auto* word = line->add_words();
    word->set_plain_text(kTestText.word_plain_text);
    word->set_text_separator(kTestText.word_text_seperator);
    CreateServerGeometry(text_struct.word_geometry, word->mutable_geometry());
    word->mutable_formula_metadata()->set_latex(
        kTestText.formula_metadata_latex);
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
  lens::mojom::TextPtr mojo_text =
      lens::CreateTextMojomFromServerResponse(server_response);
  EXPECT_TRUE(mojo_text);
  EXPECT_EQ(mojo_text->content_language, kTestText.content_language);

  // Compare paragraphs.
  EXPECT_EQ(mojo_text->text_layout->paragraphs.size(),
            static_cast<unsigned long>(1));
  lens::TextLayout_Paragraph server_paragraph =
      server_response.objects_response().text().text_layout().paragraphs()[0];
  lens::mojom::ParagraphPtr mojo_paragraph =
      mojo_text->text_layout->paragraphs[0]->Clone();
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
  EXPECT_EQ(mojo_word->plain_text, kTestText.word_plain_text);
  EXPECT_EQ(mojo_word->text_separator, kTestText.word_text_seperator);
  lens::TextLayout_Word server_word = server_line.words()[0];
  VerifyGeometriesAreEqual(server_word.geometry(),
                                    mojo_word->geometry->Clone());
  EXPECT_TRUE(mojo_word->writing_direction.has_value());
  EXPECT_EQ(static_cast<int>(mojo_word->writing_direction.value()),
            static_cast<int>(kTestText.writing_direction));
  EXPECT_EQ(mojo_word->formula_metadata->latex,
            kTestText.formula_metadata_latex);
}

TEST_F(LensOverlayProtoConverterTest, CreateTextMojomFromServerResponse_Empty) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  lens::mojom::TextPtr mojo_text =
      lens::CreateTextMojomFromServerResponse(server_response);
  EXPECT_FALSE(mojo_text);
}

TEST_F(LensOverlayProtoConverterTest,
       CreateTextMojomFromServerResponse_NoObjectsResponse) {
  lens::LensOverlayServerResponse server_response =
      CreateLensServerOverlayResponse({});
  server_response.clear_objects_response();

  lens::mojom::TextPtr mojo_text =
      lens::CreateTextMojomFromServerResponse(server_response);
  EXPECT_FALSE(mojo_text);
}

}  // namespace lens
