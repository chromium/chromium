// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include <vector>

#include "base/notreached.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

namespace {
optimization_guide::proto::ContentAttributeType ConvertAttributeType(
    blink::mojom::AIPageContentAttributeType type) {
  switch (type) {
    case blink::mojom::AIPageContentAttributeType::kRoot:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT;
    case blink::mojom::AIPageContentAttributeType::kIframe:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME;
    case blink::mojom::AIPageContentAttributeType::kParagraph:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH;
    case blink::mojom::AIPageContentAttributeType::kHeading:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_HEADING;
    case blink::mojom::AIPageContentAttributeType::kList:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_LIST;
    case blink::mojom::AIPageContentAttributeType::kForm:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_FORM;
    case blink::mojom::AIPageContentAttributeType::kFigure:
      // TODO(khushalsagar): Add this type to the proto.
      return optimization_guide::proto::CONTENT_ATTRIBUTE_UNKNOWN;
  }

  NOTREACHED();
}

void ConvertRect(const gfx::Rect& mojom_rect,
                 optimization_guide::proto::BoundingRect* proto_rect) {
  proto_rect->set_x(mojom_rect.x());
  proto_rect->set_y(mojom_rect.y());
  proto_rect->set_width(mojom_rect.width());
  proto_rect->set_height(mojom_rect.height());
}

void ConvertGeometry(const blink::mojom::AIPageContentGeometry& mojom_geometry,
                     optimization_guide::proto::Geometry* proto_geometry) {
  ConvertRect(mojom_geometry.outer_bounding_box,
              proto_geometry->mutable_outer_bounding_box());
  ConvertRect(mojom_geometry.visible_bounding_box,
              proto_geometry->mutable_visible_bounding_box());
}

void ConvertTextInfo(
    const std::vector<blink::mojom::AIPageContentTextInfoPtr>& mojom_text_info,
    google::protobuf::RepeatedPtrField<optimization_guide::proto::TextInfo>*
        proto_text_info) {
  for (const auto& mojom_text : mojom_text_info) {
    auto* proto_text = proto_text_info->Add();
    proto_text->set_text_content(mojom_text->text_content);

    auto* bounding_box = proto_text->mutable_text_bounding_box();
    bounding_box->set_x(mojom_text->text_bounding_box.x());
    bounding_box->set_y(mojom_text->text_bounding_box.y());
    bounding_box->set_width(mojom_text->text_bounding_box.width());
    bounding_box->set_height(mojom_text->text_bounding_box.height());
  }
}

void ConvertImageInfo(
    const std::vector<blink::mojom::AIPageContentImageInfoPtr>&
        mojom_image_info,
    google::protobuf::RepeatedPtrField<optimization_guide::proto::ImageInfo>*
        proto_image_info) {
  for (const auto& mojom_image : mojom_image_info) {
    auto* proto_image = proto_image_info->Add();
    if (mojom_image->image_caption) {
      proto_image->set_image_caption(*mojom_image->image_caption);
    }

    auto* bounding_box = proto_image->mutable_image_bounding_box();
    bounding_box->set_x(mojom_image->image_bounding_box.x());
    bounding_box->set_y(mojom_image->image_bounding_box.y());
    bounding_box->set_width(mojom_image->image_bounding_box.width());
    bounding_box->set_height(mojom_image->image_bounding_box.height());
  }
}

void ConvertAttributes(
    const blink::mojom::AIPageContentAttributes& mojom_attributes,
    optimization_guide::proto::ContentAttributes* proto_attributes) {
  for (const auto& dom_node_id : mojom_attributes.dom_node_ids) {
    proto_attributes->add_dom_node_ids(dom_node_id);
  }

  if (mojom_attributes.common_ancestor_dom_node_id.has_value()) {
    proto_attributes->set_common_ancestor_dom_node_id(
        mojom_attributes.common_ancestor_dom_node_id.value());
  }

  proto_attributes->set_attribute_type(
      ConvertAttributeType(mojom_attributes.attribute_type));

  if (mojom_attributes.geometry) {
    ConvertGeometry(*mojom_attributes.geometry,
                    proto_attributes->mutable_geometry());
  }

  ConvertTextInfo(mojom_attributes.text_info,
                  proto_attributes->mutable_text_info());
  ConvertImageInfo(mojom_attributes.image_info,
                   proto_attributes->mutable_image_info());
}

void ConvertNode(const blink::mojom::AIPageContentNode& mojom_node,
                 optimization_guide::proto::ContentNode* proto_node) {
  ConvertAttributes(*mojom_node.content_attributes,
                    proto_node->mutable_content_attributes());

  for (const auto& mojom_child : mojom_node.children_nodes) {
    auto* proto_child = proto_node->add_children_nodes();
    ConvertNode(*mojom_child, proto_child);
  }
}

}  // namespace

void ConvertAIPageContentToProto(
    const blink::mojom::AIPageContent& mojo,
    optimization_guide::proto::AnnotatedPageContent* proto) {
  ConvertNode(*mojo.root_node, proto->mutable_root_node());
  proto->set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
}

}  // namespace optimization_guide
