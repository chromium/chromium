// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include <vector>

#include "base/notreached.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {
optimization_guide::proto::ContentAttributeType ConvertAttributeType(
    blink::mojom::AIPageContentAttributeType type) {
  switch (type) {
    case blink::mojom::AIPageContentAttributeType::kRoot:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT;
    case blink::mojom::AIPageContentAttributeType::kIframe:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME;
    case blink::mojom::AIPageContentAttributeType::kContainer:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER;
    case blink::mojom::AIPageContentAttributeType::kText:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT;
    case blink::mojom::AIPageContentAttributeType::kAnchor:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR;
    case blink::mojom::AIPageContentAttributeType::kImage:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE;
    case blink::mojom::AIPageContentAttributeType::kForm:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_FORM;
    case blink::mojom::AIPageContentAttributeType::kTable:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE;
    case blink::mojom::AIPageContentAttributeType::kTableRow:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_ROW;
    case blink::mojom::AIPageContentAttributeType::kParagraph:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH;
    case blink::mojom::AIPageContentAttributeType::kHeading:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_HEADING;
    case blink::mojom::AIPageContentAttributeType::kOrderedList:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_ORDERED_LIST;
    case blink::mojom::AIPageContentAttributeType::kUnorderedList:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_UNORDERED_LIST;
    case blink::mojom::AIPageContentAttributeType::kTableCell:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_CELL;
    case blink::mojom::AIPageContentAttributeType::kListItem:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_LIST_ITEM;
  }
  NOTREACHED();
}

optimization_guide::proto::AnnotatedRole ConvertAnnotatedRole(
    blink::mojom::AIPageContentAnnotatedRole role) {
  switch (role) {
    case blink::mojom::AIPageContentAnnotatedRole::kHeader:
      return optimization_guide::proto::ANNOTATED_ROLE_HEADER;
    case blink::mojom::AIPageContentAnnotatedRole::kNav:
      return optimization_guide::proto::ANNOTATED_ROLE_NAV;
    case blink::mojom::AIPageContentAnnotatedRole::kSearch:
      return optimization_guide::proto::ANNOTATED_ROLE_SEARCH;
    case blink::mojom::AIPageContentAnnotatedRole::kMain:
      return optimization_guide::proto::ANNOTATED_ROLE_MAIN;
    case blink::mojom::AIPageContentAnnotatedRole::kArticle:
      return optimization_guide::proto::ANNOTATED_ROLE_ARTICLE;
    case blink::mojom::AIPageContentAnnotatedRole::kSection:
      return optimization_guide::proto::ANNOTATED_ROLE_SECTION;
    case blink::mojom::AIPageContentAnnotatedRole::kAside:
      return optimization_guide::proto::ANNOTATED_ROLE_ASIDE;
    case blink::mojom::AIPageContentAnnotatedRole::kFooter:
      return optimization_guide::proto::ANNOTATED_ROLE_FOOTER;
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
  proto_geometry->set_is_fixed_or_sticky_position(
      mojom_geometry.is_fixed_or_sticky_position);
  proto_geometry->set_scrolls_overflow_x(mojom_geometry.scrolls_overflow_x);
  proto_geometry->set_scrolls_overflow_y(mojom_geometry.scrolls_overflow_y);
}

optimization_guide::proto::TextSize ConvertTextSize(
    blink::mojom::AIPageContentTextSize text_size) {
  switch (text_size) {
    case blink::mojom::AIPageContentTextSize::kXS:
      return optimization_guide::proto::TextSize::TEXT_SIZE_XS;
    case blink::mojom::AIPageContentTextSize::kS:
      return optimization_guide::proto::TextSize::TEXT_SIZE_S;
    case blink::mojom::AIPageContentTextSize::kM:
      return optimization_guide::proto::TextSize::TEXT_SIZE_M_DEFAULT;
    case blink::mojom::AIPageContentTextSize::kL:
      return optimization_guide::proto::TextSize::TEXT_SIZE_L;
    case blink::mojom::AIPageContentTextSize::kXL:
      return optimization_guide::proto::TextSize::TEXT_SIZE_XL;
  }
  NOTREACHED();
}

void ConvertTextInfo(const blink::mojom::AIPageContentTextInfo& mojom_text_info,
                     optimization_guide::proto::TextInfo* proto_text_info) {
  proto_text_info->set_text_content(mojom_text_info.text_content);

  auto* text_style = proto_text_info->mutable_text_style();
  text_style->set_text_size(
      ConvertTextSize(mojom_text_info.text_style->text_size));
  text_style->set_has_emphasis(mojom_text_info.text_style->has_emphasis);
}

void ConvertImageInfo(
    const blink::mojom::AIPageContentImageInfo& mojom_image_info,
    optimization_guide::proto::ImageInfo* proto_image_info) {
  if (mojom_image_info.image_caption) {
    proto_image_info->set_image_caption(*mojom_image_info.image_caption);
  }
  if (mojom_image_info.source_origin) {
    proto_image_info->set_source_url(
        mojom_image_info.source_origin->GetURL().spec());
  }
}

optimization_guide::proto::AnchorRel ConvertAnchorRel(
    blink::mojom::AIPageContentAnchorRel rel) {
  switch (rel) {
    case blink::mojom::AIPageContentAnchorRel::kRelationUnknown:
      return optimization_guide::proto::ANCHOR_REL_UNKNOWN;
    case blink::mojom::AIPageContentAnchorRel::kRelationNoReferrer:
      return optimization_guide::proto::ANCHOR_REL_NO_REFERRER;
    case blink::mojom::AIPageContentAnchorRel::kRelationNoOpener:
      return optimization_guide::proto::ANCHOR_REL_NO_OPENER;
    case blink::mojom::AIPageContentAnchorRel::kRelationOpener:
      return optimization_guide::proto::ANCHOR_REL_OPENER;
    case blink::mojom::AIPageContentAnchorRel::kRelationPrivacyPolicy:
      return optimization_guide::proto::ANCHOR_REL_PRIVACY_POLICY;
    case blink::mojom::AIPageContentAnchorRel::kRelationTermsOfService:
      return optimization_guide::proto::ANCHOR_REL_TERMS_OF_SERVICE;
  }
  NOTREACHED();
}

void ConvertAnchorData(
    const blink::mojom::AIPageContentAnchorData& mojom_anchor_data,
    optimization_guide::proto::AnchorData* proto_anchor_data) {
  proto_anchor_data->set_url(mojom_anchor_data.url.spec());
  for (const auto& rel : mojom_anchor_data.rel) {
    proto_anchor_data->add_rel(ConvertAnchorRel(rel));
  }
}

void ConvertFormData(const blink::mojom::AIPageContentFormData& mojom_form_data,
                     optimization_guide::proto::FormData* proto_form_data) {
  // TODO(crbug.com/381879263): Add fields for form data.
}

void ConvertTableData(
    const blink::mojom::AIPageContentTableData& mojom_table_data,
    optimization_guide::proto::TableData* proto_table_data) {
  if (mojom_table_data.table_name) {
    proto_table_data->set_table_name(*mojom_table_data.table_name);
  }
}

void ConvertTableRowData(
    const blink::mojom::AIPageContentTableRowData mojom_table_row_data,
    optimization_guide::proto::TableRowData* proto_table_row_data) {
  switch (mojom_table_row_data.row_type) {
    case blink::mojom::AIPageContentTableRowType::kHeader:
      proto_table_row_data->set_type(
          optimization_guide::proto::TableRowType::TABLE_ROW_TYPE_HEADER);
      break;
    case blink::mojom::AIPageContentTableRowType::kBody:
      proto_table_row_data->set_type(
          optimization_guide::proto::TableRowType::TABLE_ROW_TYPE_BODY);
      break;
    case blink::mojom::AIPageContentTableRowType::kFooter:
      proto_table_row_data->set_type(
          optimization_guide::proto::TableRowType::TABLE_ROW_TYPE_FOOTER);
      break;
  }
}

bool ConvertAttributes(
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

  if (mojom_attributes.text_info) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kText) {
      return false;
    }
    ConvertTextInfo(*mojom_attributes.text_info,
                    proto_attributes->mutable_text_data());
  } else if (mojom_attributes.image_info) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kImage) {
      return false;
    }
    ConvertImageInfo(*mojom_attributes.image_info,
                     proto_attributes->mutable_image_data());
  } else if (mojom_attributes.anchor_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kAnchor) {
      return false;
    }
    ConvertAnchorData(*mojom_attributes.anchor_data,
                      proto_attributes->mutable_anchor_data());
  } else if (mojom_attributes.form_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kForm) {
      return false;
    }
    ConvertFormData(*mojom_attributes.form_data,
                    proto_attributes->mutable_form_data());
  } else if (mojom_attributes.table_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kTable) {
      return false;
    }
    ConvertTableData(*mojom_attributes.table_data,
                     proto_attributes->mutable_table_data());
  } else if (mojom_attributes.table_row_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kTableRow) {
      return false;
    }
    ConvertTableRowData(*mojom_attributes.table_row_data,
                        proto_attributes->mutable_table_row_data());
  }

  for (const auto& annotated_role : mojom_attributes.annotated_roles) {
    proto_attributes->add_annotated_roles(ConvertAnnotatedRole(annotated_role));
  }
  return true;
}

void ConvertIframeData(
    const RenderFrameInfo& render_frame_info,
    const blink::mojom::AIPageContentIframeData& iframe_data,
    optimization_guide::proto::IframeData* proto_iframe_data) {
  proto_iframe_data->set_url(render_frame_info.source_origin.Serialize());
  proto_iframe_data->set_likely_ad_frame(iframe_data.likely_ad_frame);
}

bool ConvertNode(content::GlobalRenderFrameHostToken source_frame_token,
                 const blink::mojom::AIPageContentNode& mojom_node,
                 const AIPageContentMap& page_content_map,
                 GetRenderFrameInfo get_render_frame_info,
                 optimization_guide::proto::ContentNode* proto_node) {
  const auto& mojom_attributes = *mojom_node.content_attributes;
  if (!ConvertAttributes(mojom_attributes,
                         proto_node->mutable_content_attributes())) {
    return false;
  }

  std::optional<RenderFrameInfo> render_frame_info;
  if (mojom_attributes.attribute_type ==
      blink::mojom::AIPageContentAttributeType::kIframe) {
    if (!mojom_attributes.iframe_data) {
      return false;
    }

    const auto& iframe_data = *mojom_attributes.iframe_data;
    const auto frame_token = iframe_data.frame_token;

    // The frame may have been torn down or crashed before we got a response.
    render_frame_info =
        get_render_frame_info.Run(source_frame_token.child_id, frame_token);
    if (!render_frame_info) {
      return false;
    }

    if (frame_token.Is<blink::RemoteFrameToken>()) {
      // RemoteFrame should have no child nodes since the content is out of
      // process.
      if (!mojom_node.children_nodes.empty()) {
        return false;
      }

      auto it = page_content_map.find(render_frame_info->global_frame_token);
      if (it == page_content_map.end()) {
        return true;
      }

      const auto& frame_page_content = *it->second;
      auto* proto_child_frame_node = proto_node->add_children_nodes();
      if (!ConvertNode(render_frame_info->global_frame_token,
                       *frame_page_content.root_node, page_content_map,
                       get_render_frame_info, proto_child_frame_node)) {
        return false;
      }
    }

    auto* proto_iframe_data =
        proto_node->mutable_content_attributes()->mutable_iframe_data();
    ConvertIframeData(*render_frame_info, iframe_data, proto_iframe_data);
  }

  const auto source_frame_for_children =
      render_frame_info ? render_frame_info->global_frame_token
                        : source_frame_token;
  for (const auto& mojom_child : mojom_node.children_nodes) {
    auto* proto_child = proto_node->add_children_nodes();
    if (!ConvertNode(source_frame_for_children, *mojom_child, page_content_map,
                     get_render_frame_info, proto_child)) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool ConvertAIPageContentToProto(
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    optimization_guide::proto::AnnotatedPageContent* proto) {
  auto it = page_content_map.find(main_frame_token);
  if (it == page_content_map.end()) {
    return false;
  }

  const auto& main_frame_page_content = *it->second;
  if (!ConvertNode(main_frame_token, *main_frame_page_content.root_node,
                   page_content_map, get_render_frame_info,
                   proto->mutable_root_node())) {
    return false;
  }

  proto->set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  return true;
}

}  // namespace optimization_guide
