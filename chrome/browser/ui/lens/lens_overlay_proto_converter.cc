// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"

#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "third_party/lens_server_proto/lens_overlay_geometry.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_text.pb.h"

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

lens::mojom::PolygonPtr CreatePolygonMojomFromProto(
    lens::Polygon proto_polygon) {
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
    lens::Geometry response_geometry) {
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
    lens::TextLayout_Word proto_word,
    lens::WritingDirection writing_direction) {
  lens::mojom::WordPtr word = lens::mojom::Word::New();
  word->plain_text = proto_word.plain_text();
  word->text_separator = proto_word.text_separator();
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
    lens::TextLayout_Line proto_line,
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

lens::mojom::ParagraphPtr CreateParagraphMojomFromProto(
    lens::TextLayout_Paragraph proto_paragraph) {
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
    const lens::LensOverlayServerResponse& response) {
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

    for (auto response_paragraph : response_layout.paragraphs()) {
      paragraphs.push_back(CreateParagraphMojomFromProto(response_paragraph));
    }
    text_layout->paragraphs = std::move(paragraphs);
    text->text_layout = std::move(text_layout);
  }
  return text;
}
}  // namespace lens
