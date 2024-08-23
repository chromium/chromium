// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "third_party/lens_server_proto/lens_overlay_deep_gleam_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_geometry.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_text.pb.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"

namespace lens {

namespace {

lens::mojom::Polygon_VertexOrdering ProtoToMojo(
    lens::Polygon::VertexOrdering vertex_ordering) {
  switch (vertex_ordering) {
    case lens::Polygon::VERTEX_ORDERING_UNSPECIFIED:
      return lens::mojom::Polygon_VertexOrdering::kUnspecified;
    case lens::Polygon::CLOCKWISE:
      return lens::mojom::Polygon_VertexOrdering::kClockwise;
    case lens::Polygon::COUNTER_CLOCKWISE:
      return lens::mojom::Polygon_VertexOrdering::kCounterClockwise;
  }
  return lens::mojom::Polygon_VertexOrdering::kUnspecified;
}

lens::mojom::Polygon_CoordinateType ProtoToMojo(
    lens::CoordinateType coordinate_type) {
  switch (coordinate_type) {
    case lens::COORDINATE_TYPE_UNSPECIFIED:
      return lens::mojom::Polygon_CoordinateType::kUnspecified;
    case lens::NORMALIZED:
      return lens::mojom::Polygon_CoordinateType::kNormalized;
    case lens::IMAGE:
      return lens::mojom::Polygon_CoordinateType::kImage;
  }
  return lens::mojom::Polygon_CoordinateType::kUnspecified;
}

lens::mojom::Alignment ProtoToMojo(lens::Alignment text_alignment) {
  switch (text_alignment) {
    case lens::Alignment::DEFAULT_LEFT_ALIGNED:
      return lens::mojom::Alignment::kDefaultLeftAlgined;
    case lens::Alignment::RIGHT_ALIGNED:
      return lens::mojom::Alignment::kRightAligned;
    case lens::Alignment::CENTER_ALIGNED:
      return lens::mojom::Alignment::kCenterAligned;
    default:
      // This default case is needed because two dummy enums that should not be
      // used are created by the proto compiler.
      break;
  }
  return lens::mojom::Alignment::kDefaultLeftAlgined;
}

lens::mojom::PolygonPtr CreatePolygonMojomFromProto(
    const lens::Polygon& proto_polygon) {
  lens::mojom::PolygonPtr polygon = lens::mojom::Polygon::New();

  std::vector<lens::mojom::VertexPtr> vertices;
  for (auto vertex : proto_polygon.vertex()) {
    vertices.push_back(lens::mojom::Vertex::New(vertex.x(), vertex.y()));
  }
  polygon->vertex = std::move(vertices);
  polygon->vertex_ordering = ProtoToMojo(proto_polygon.vertex_ordering());
  polygon->coordinate_type = ProtoToMojo(proto_polygon.coordinate_type());

  return polygon;
}

lens::mojom::GeometryPtr CreateGeometryMojomFromProto(
    const lens::Geometry& response_geometry) {
  lens::mojom::GeometryPtr geometry = lens::mojom::Geometry::New();
  if (!response_geometry.has_bounding_box()) {
    return geometry;
  }

  auto bounding_box_response = response_geometry.bounding_box();
  lens::mojom::CenterRotatedBoxPtr center_rotated_box =
      lens::mojom::CenterRotatedBox::New();
  gfx::SizeF box_size(bounding_box_response.width(),
                      bounding_box_response.height());
  // TODO(b/333562179): Replace this setting of the origin with just a point and
  // size that is passed to the WebUI.
  gfx::PointF center_point = gfx::PointF(bounding_box_response.center_x(),
                                         bounding_box_response.center_y());
  center_rotated_box->box.set_origin(center_point);
  center_rotated_box->box.set_size(box_size);
  center_rotated_box->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType(
          bounding_box_response.coordinate_type());
  center_rotated_box->rotation = bounding_box_response.rotation_z();

  geometry->bounding_box = std::move(center_rotated_box);

  std::vector<lens::mojom::PolygonPtr> polygons;
  for (auto polygon : response_geometry.segmentation_polygon()) {
    polygons.push_back(CreatePolygonMojomFromProto(polygon));
  }
  geometry->segmentation_polygon = std::move(polygons);

  return geometry;
}

lens::mojom::WordPtr CreateWordMojomFromProto(
    const lens::TextLayout_Word& proto_word,
    lens::WritingDirection writing_direction) {
  lens::mojom::WordPtr word = lens::mojom::Word::New();
  word->plain_text = proto_word.plain_text();
  if (proto_word.has_text_separator()) {
    word->text_separator = proto_word.text_separator();
  }
  if (proto_word.has_geometry()) {
    word->geometry = CreateGeometryMojomFromProto(proto_word.geometry());
  }
  if (proto_word.has_formula_metadata()) {
    lens::mojom::FormulaMetadataPtr metadata =
        lens::mojom::FormulaMetadata::New();
    metadata->latex = proto_word.formula_metadata().latex();
    word->formula_metadata = std::move(metadata);
  }
  word->writing_direction = lens::mojom::WritingDirection(writing_direction);
  return word;
}

lens::mojom::LinePtr CreateLineMojomFromProto(
    const lens::TextLayout_Line& proto_line,
    lens::WritingDirection writing_direction) {
  lens::mojom::LinePtr line = lens::mojom::Line::New();
  std::vector<lens::mojom::WordPtr> words;
  for (auto word : proto_line.words()) {
    words.push_back(CreateWordMojomFromProto(word, writing_direction));
  }
  line->words = std::move(words);
  if (proto_line.has_geometry()) {
    line->geometry = CreateGeometryMojomFromProto(proto_line.geometry());
  }
  return line;
}

lens::mojom::BackgroundImageDataPtr CreateBackgroundImageDataMojomFromProto(
    lens::TranslationData_BackgroundImageData background_image_data) {
  lens::mojom::BackgroundImageDataPtr image_data =
      lens::mojom::BackgroundImageData::New();
  gfx::Size image_size = gfx::Size(background_image_data.image_width(),
                                   background_image_data.image_height());
  image_data->image_size = std::move(image_size);
  image_data->vertical_padding = background_image_data.vertical_padding();
  image_data->horizontal_padding = background_image_data.horizontal_padding();

  // Create vector for `background_image_data`.
  const std::string& image_bytes = background_image_data.background_image();
  std::vector<unsigned char> background_pixel_data(image_bytes.begin(),
                                                   image_bytes.end());
  image_data->background_image = std::move(background_pixel_data);

  // Create vector for `text_mask`.
  const std::string& text_mask = background_image_data.text_mask();
  std::vector<unsigned char> mask_pixel_data(text_mask.begin(),
                                             text_mask.end());
  image_data->text_mask = std::move(mask_pixel_data);

  return image_data;
}

lens::mojom::WordPtr CreateTranslatedWordMojomFromProto(
    const std::string& translated_text,
    const std::string& text_separator,
    const gfx::SizeF& word_size,
    const gfx::PointF& word_center,
    float rotation,
    lens::WritingDirection writing_direction) {
  lens::mojom::WordPtr word = lens::mojom::Word::New();

  // Calculate the new geometry of the word.
  lens::mojom::GeometryPtr geometry = lens::mojom::Geometry::New();
  lens::mojom::CenterRotatedBoxPtr center_rotated_box =
      lens::mojom::CenterRotatedBox::New();
  center_rotated_box->box.set_size(word_size);
  // TODO(b/333562179): Replace this setting of the origin with just a point
  // and size that is passed to the WebUI.
  center_rotated_box->box.set_origin(word_center);
  center_rotated_box->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  center_rotated_box->rotation = rotation;
  geometry->bounding_box = std::move(center_rotated_box);

  word->geometry = std::move(geometry);
  word->plain_text = translated_text;
  word->text_separator = text_separator;
  word->writing_direction = lens::mojom::WritingDirection(writing_direction);
  return word;
}

lens::mojom::TranslatedLinePtr CreateTranslatedLineMojomFromProto(
    const lens::TextLayout_Line& proto_line,
    const lens::TranslationData_Line& translated_line,
    const std::string& line_translation,
    const gfx::Size& resized_bitmap_size,
    lens::WritingDirection writing_direction) {
  lens::mojom::TranslatedLinePtr line = lens::mojom::TranslatedLine::New();

  // We can have a different amount of words in the detected text line and
  // translated text line. The translated words can also be different sizes than
  // the detected words. Because of this, we need to recalculate the geometry
  // of the translated words (it is not provided in server response). If there
  // is no line geometry, we cannot translate this line.
  if (!proto_line.has_geometry()) {
    return line;
  }
  line->geometry = CreateGeometryMojomFromProto(proto_line.geometry());

  // First, we need to calculate the pixel size of the line using
  // |resized_bitmap_size|, which is the size of the bitmap that should have
  // been sent to the server. We do not need to calculate the line height, since
  // the word's height should be equal to the line height.
  float original_line_width = proto_line.geometry().bounding_box().width() *
                              resized_bitmap_size.width();

  // We then need a total width of all the new translated words. This does not
  // have to be exact as we will be scaling and normalizing these values when
  // calculating the final word geometries.
  float total_translated_words_width = 0;

  // Define some arrays for values that we need to get to calculate the
  // `total_translated_words_width`. Do this so we do not have to do this work
  // again.
  std::vector<std::string> word_translations;
  std::vector<std::string> word_text_separators;
  std::vector<float> word_widths;
  for (const auto& translated_proto_word : translated_line.word()) {
    int substring_length =
        translated_proto_word.end() - translated_proto_word.start();
    const std::string translation = line_translation.substr(
        translated_proto_word.start(), substring_length);
    const std::string separator =
        line_translation.substr(translated_proto_word.end(), 1);
    float word_width =
        gfx::GetStringWidthF(base::UTF8ToUTF16(translation), gfx::FontList());

    total_translated_words_width += word_width;
    word_widths.push_back(word_width);
    word_text_separators.push_back(std::move(separator));
    word_translations.push_back(std::move(translation));
  }

  // Get the scale to multiply the new word width by so it remains within the
  // line geometry.
  float scale = total_translated_words_width > 0
                    ? original_line_width / total_translated_words_width
                    : 0;

  // Now that we have all of our needed values, we need to loop through the
  // words again to actually create the mojo objects.
  std::vector<mojom::WordPtr> words;
  float offset_x = proto_line.geometry().bounding_box().center_x() -
                   proto_line.geometry().bounding_box().width() / 2;
  for (int i = 0; i < translated_line.word_size(); i++) {
    // Calculate the word size and center point.
    gfx::SizeF word_size(word_widths[i] * scale / resized_bitmap_size.width(),
                         proto_line.geometry().bounding_box().height());
    gfx::PointF word_center(offset_x + word_size.width() / 2,
                            proto_line.geometry().bounding_box().center_y());
    offset_x += word_size.width();

    words.push_back(CreateTranslatedWordMojomFromProto(
        word_translations[i], word_text_separators[i], word_size, word_center,
        proto_line.geometry().bounding_box().rotation_z(), writing_direction));
  }

  if (translated_line.has_background_image_data()) {
    line->background_image_data = CreateBackgroundImageDataMojomFromProto(
        translated_line.background_image_data());
  }
  line->translation = line_translation;
  line->background_primary_color =
      translated_line.style().background_primary_color();
  line->text_color = translated_line.style().text_color();
  line->words = std::move(words);
  return line;
}

lens::mojom::TranslatedParagraphPtr CreateTranslatedParagraphMojomFromProto(
    const lens::TextLayout_Paragraph& proto_paragraph,
    const lens::DeepGleamData& deep_gleam,
    const gfx::Size& resized_bitmap_size) {
  lens::mojom::TranslatedParagraphPtr paragraph =
      lens::mojom::TranslatedParagraph::New();
  // If there is no deep gleam translation for this paragraph, just return
  // empty.
  if (!deep_gleam.has_translation()) {
    return paragraph;
  }

  auto translation_data = deep_gleam.translation();
  // We need a status code so we can know if the translation was successful.
  if (!translation_data.has_status() &&
      translation_data.status().code() !=
          lens::TranslationData::Status::SUCCESS) {
    return paragraph;
  }

  // We should have the same amount of translated lines in the detected text
  // and the translated text data.
  if (proto_paragraph.lines_size() != translation_data.line_size()) {
    return paragraph;
  }

  std::vector<lens::mojom::TranslatedLinePtr> lines;
  for (int line_index = 0; line_index < translation_data.line_size();
       line_index++) {
    auto proto_line = proto_paragraph.lines()[line_index];
    auto translated_line = translation_data.line()[line_index];
    lines.push_back(CreateTranslatedLineMojomFromProto(
        proto_line, translated_line, translation_data.translation(),
        resized_bitmap_size, translation_data.writing_direction()));
  }

  paragraph->lines = std::move(lines);
  paragraph->content_language = translation_data.target_language();
  paragraph->alignment = ProtoToMojo(translation_data.alignment());
  paragraph->writing_direction =
      lens::mojom::WritingDirection(translation_data.writing_direction());
  return paragraph;
}

lens::mojom::ParagraphPtr CreateParagraphMojomFromProto(
    const lens::TextLayout_Paragraph& proto_paragraph,
    base::optional_ref<const lens::DeepGleamData> deep_gleam,
    const gfx::Size& resized_bitmap_size) {
  lens::mojom::ParagraphPtr paragraph = lens::mojom::Paragraph::New();
  paragraph->content_language = proto_paragraph.content_language();
  std::vector<lens::mojom::LinePtr> lines;
  for (auto line : proto_paragraph.lines()) {
    lines.push_back(
        CreateLineMojomFromProto(line, proto_paragraph.writing_direction()));
  }
  paragraph->lines = std::move(lines);

  if (proto_paragraph.has_geometry()) {
    paragraph->geometry =
        CreateGeometryMojomFromProto(proto_paragraph.geometry());
  }
  paragraph->writing_direction =
      lens::mojom::WritingDirection(proto_paragraph.writing_direction());

  if (deep_gleam.has_value()) {
    paragraph->translation = CreateTranslatedParagraphMojomFromProto(
        proto_paragraph, deep_gleam.value(), resized_bitmap_size);
  }

  return paragraph;
}

}  // namespace

std::vector<lens::mojom::OverlayObjectPtr>
CreateObjectsMojomArrayFromServerResponse(
    const lens::LensOverlayServerResponse& response) {
  std::vector<lens::mojom::OverlayObjectPtr> object_array;
  if (!response.has_objects_response() ||
      response.objects_response().overlay_objects().empty()) {
    return object_array;
  }

  auto response_objects = response.objects_response().overlay_objects();
  for (auto response_object : response_objects) {
    if (!response_object.has_interaction_properties() ||
        !response_object.interaction_properties().select_on_tap()) {
      continue;
    }
    lens::mojom::OverlayObjectPtr overlay_object =
        lens::mojom::OverlayObject::New();
    overlay_object->id = std::string(response_object.id());
    if (response_object.has_geometry()) {
      overlay_object->geometry =
          CreateGeometryMojomFromProto(response_object.geometry());
    }
    object_array.push_back(std::move(overlay_object));
  }

  return object_array;
}

lens::mojom::TextPtr CreateTextMojomFromServerResponse(
    const lens::LensOverlayServerResponse& response,
    const gfx::Size resized_bitmap_size) {
  lens::mojom::TextPtr text;
  // If the server response lacks text, then return an empty vector.
  if (!response.has_objects_response() ||
      !response.objects_response().has_text()) {
    return text;
  }

  text = lens::mojom::Text::New();
  const lens::Text response_text = response.objects_response().text();
  text->content_language = response_text.content_language();
  if (response_text.has_text_layout()) {
    const lens::TextLayout response_layout = response_text.text_layout();
    lens::mojom::TextLayoutPtr text_layout = lens::mojom::TextLayout::New();
    std::vector<lens::mojom::ParagraphPtr> paragraphs;

    for (int i = 0; i < response_text.text_layout().paragraphs_size(); i++) {
      const auto& response_paragraph =
          response_text.text_layout().paragraphs()[i];
      lens::DeepGleamData deep_gleam_data;
      // The translated paragraphs should correspond to each paragraph of
      // detected text and deep gleam data. That is, there should be the same
      // amount of deep gleam data as paragraphs.
      if (i < response.objects_response().deep_gleams_size()) {
        deep_gleam_data = response.objects_response().deep_gleams()[i];
      }
      paragraphs.push_back(CreateParagraphMojomFromProto(
          response_paragraph, deep_gleam_data, resized_bitmap_size));
    }
    text_layout->paragraphs = std::move(paragraphs);
    text->text_layout = std::move(text_layout);
  }

  return text;
}
}  // namespace lens
