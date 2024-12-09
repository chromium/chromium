// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include <vector>

#include "base/notreached.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
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
    case blink::mojom::AIPageContentAttributeType::kOrderedList:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_ORDERED_LIST;
    case blink::mojom::AIPageContentAttributeType::kUnorderedList:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_UNORDERED_LIST;
    case blink::mojom::AIPageContentAttributeType::kForm:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_FORM;
    case blink::mojom::AIPageContentAttributeType::kFigure:
    case blink::mojom::AIPageContentAttributeType::kHeader:
    case blink::mojom::AIPageContentAttributeType::kNav:
    case blink::mojom::AIPageContentAttributeType::kSearch:
    case blink::mojom::AIPageContentAttributeType::kMain:
    case blink::mojom::AIPageContentAttributeType::kArticle:
    case blink::mojom::AIPageContentAttributeType::kSection:
    case blink::mojom::AIPageContentAttributeType::kAside:
    case blink::mojom::AIPageContentAttributeType::kFooter:
      // TODO(crbug.com/382083796): Add this type to the proto.
      return optimization_guide::proto::CONTENT_ATTRIBUTE_UNKNOWN;
    case blink::mojom::AIPageContentAttributeType::kTable:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE;
    case blink::mojom::AIPageContentAttributeType::kTableCell:
      // TODO(crbug.com/382083796): Add this type to the proto.
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

    if (mojom_image->source_origin) {
      proto_image->set_source_url(mojom_image->source_origin->GetURL().spec());
    }
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

void ConvertIframeData(
    const content::RenderFrameHost& render_frame_host,
    const blink::mojom::AIPageContentIframeData& iframe_data,
    optimization_guide::proto::IframeData* proto_iframe_data) {
  // We use the origin instead of last committed URL here to ensure the security
  // origin for the iframe's content is accurately tracked.
  // For example, for data URLs we need the source origin for the URL instead of
  // the raw URL itself.
  proto_iframe_data->set_url(render_frame_host.GetLastCommittedOrigin()
                                 .GetTupleOrPrecursorTupleIfOpaque()
                                 .Serialize());
  proto_iframe_data->set_likely_ad_frame(iframe_data.likely_ad_frame);
}

bool ConvertNode(content::GlobalRenderFrameHostToken source_frame_token,
                 const blink::mojom::AIPageContentNode& mojom_node,
                 const AIPageContentMap& page_content_map,
                 optimization_guide::proto::ContentNode* proto_node) {
  const auto& mojom_attributes = *mojom_node.content_attributes;
  ConvertAttributes(mojom_attributes, proto_node->mutable_content_attributes());

  content::RenderFrameHost* render_frame_host = nullptr;
  if (mojom_attributes.attribute_type ==
      blink::mojom::AIPageContentAttributeType::kIframe) {
    if (!mojom_attributes.iframe_data) {
      return false;
    }

    const auto& iframe_data = *mojom_attributes.iframe_data;
    const auto frame_token = iframe_data.frame_token;
    if (frame_token.Is<blink::RemoteFrameToken>()) {
      // RemoteFrame should have no child nodes since the content is out of
      // process.
      if (!mojom_node.children_nodes.empty()) {
        return false;
      }

      render_frame_host = content::RenderFrameHost::FromPlaceholderToken(
          source_frame_token.child_id,
          frame_token.GetAs<blink::RemoteFrameToken>());

      // The OOPIF may have been torn down or crashed before we got a response.
      if (!render_frame_host) {
        return true;
      }

      auto it = page_content_map.find(render_frame_host->GetGlobalFrameToken());
      if (it == page_content_map.end()) {
        return true;
      }

      const auto& frame_page_content = *it->second;
      auto* proto_child_frame_node = proto_node->add_children_nodes();
      if (!ConvertNode(render_frame_host->GetGlobalFrameToken(),
                       *frame_page_content.root_node, page_content_map,
                       proto_child_frame_node)) {
        return false;
      }
    } else {
      render_frame_host = content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              source_frame_token.child_id,
              frame_token.GetAs<blink::LocalFrameToken>()));

      if (!render_frame_host) {
        return true;
      }
    }

    auto* proto_iframe_data =
        proto_node->mutable_content_attributes()->mutable_iframe_data();
    ConvertIframeData(*render_frame_host, iframe_data, proto_iframe_data);
  }

  const auto source_frame_for_children =
      render_frame_host ? render_frame_host->GetGlobalFrameToken()
                        : source_frame_token;
  for (const auto& mojom_child : mojom_node.children_nodes) {
    auto* proto_child = proto_node->add_children_nodes();
    if (!ConvertNode(source_frame_for_children, *mojom_child, page_content_map,
                     proto_child)) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool ConvertAIPageContentToProto(
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    optimization_guide::proto::AnnotatedPageContent* proto) {
  auto it = page_content_map.find(main_frame_token);
  if (it == page_content_map.end()) {
    return false;
  }

  const auto& main_frame_page_content = *it->second;
  if (!ConvertNode(main_frame_token, *main_frame_page_content.root_node,
                   page_content_map, proto->mutable_root_node())) {
    return false;
  }

  proto->set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  return true;
}

}  // namespace optimization_guide
