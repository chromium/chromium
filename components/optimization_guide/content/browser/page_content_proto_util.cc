// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/mojom/ai_page_content_metadata.mojom.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "url/gurl.h"

namespace optimization_guide {

class SecurityOriginSerializer {
 public:
  static void Serialize(
      const url::Origin& origin,
      optimization_guide::proto::SecurityOrigin* proto_origin) {
    proto_origin->set_opaque(origin.opaque());

    if (origin.opaque()) {
      proto_origin->set_value(origin.GetNonceForSerialization()->ToString());
    } else {
      proto_origin->set_value(origin.Serialize());
    }
  }
};

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
    case blink::mojom::AIPageContentAttributeType::kSVG:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_SVG;
    case blink::mojom::AIPageContentAttributeType::kCanvas:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_CANVAS;
    case blink::mojom::AIPageContentAttributeType::kForm:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_FORM;
    case blink::mojom::AIPageContentAttributeType::kFormControl:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL;
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
    case blink::mojom::AIPageContentAnnotatedRole::kContentHidden:
      return optimization_guide::proto::ANNOTATED_ROLE_CONTENT_HIDDEN;
    case blink::mojom::AIPageContentAnnotatedRole::kPaidContent:
      return optimization_guide::proto::ANNOTATED_ROLE_PAID_CONTENT;
  }
  NOTREACHED();
}

void AddDocumentIdentifier(content::GlobalRenderFrameHostToken frame_token,
                           FrameTokenSet& frame_token_set,
                           std::string serialized_server_token,
                           optimization_guide::proto::FrameData* frame_data) {
  frame_token_set.insert(frame_token);
  frame_data->mutable_document_identifier()->set_serialized_token(
      serialized_server_token);
}

void ConvertSize(const gfx::Size& mojom_size,
                 optimization_guide::proto::BoundingSize* proto_size) {
  proto_size->set_width(mojom_size.width());
  proto_size->set_height(mojom_size.height());
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
}

void ConvertScrollerInfo(
    const blink::mojom::AIPageContentScrollerInfo& mojom_scroller_info,
    optimization_guide::proto::ScrollerInfo* proto_scroller_info) {
  ConvertSize(mojom_scroller_info.scrolling_bounds,
              proto_scroller_info->mutable_scrolling_bounds());
  ConvertRect(mojom_scroller_info.visible_area,
              proto_scroller_info->mutable_visible_area());
  proto_scroller_info->set_user_scrollable_horizontal(
      mojom_scroller_info.user_scrollable_horizontal);
  proto_scroller_info->set_user_scrollable_vertical(
      mojom_scroller_info.user_scrollable_vertical);
}

void ConvertNodeInteractionInfo(
    const blink::mojom::AIPageContentNodeInteractionInfo&
        mojom_node_interaction_info,
    optimization_guide::proto::InteractionInfo* proto_interaction_info) {
  if (mojom_node_interaction_info.scroller_info) {
    ConvertScrollerInfo(*mojom_node_interaction_info.scroller_info,
                        proto_interaction_info->mutable_scroller_info());
  }
  proto_interaction_info->set_is_selectable(
      mojom_node_interaction_info.is_selectable);
  proto_interaction_info->set_is_editable(
      mojom_node_interaction_info.is_editable);
  proto_interaction_info->set_can_resize_horizontal(
      mojom_node_interaction_info.can_resize_horizontal);
  proto_interaction_info->set_can_resize_vertical(
      mojom_node_interaction_info.can_resize_vertical);
  proto_interaction_info->set_is_focusable(
      mojom_node_interaction_info.is_focusable);
  proto_interaction_info->set_is_draggable(
      mojom_node_interaction_info.is_draggable);
  proto_interaction_info->set_is_clickable(
      mojom_node_interaction_info.is_clickable);

  if (mojom_node_interaction_info.document_scoped_z_order) {
    proto_interaction_info->set_document_scoped_z_order(
        *mojom_node_interaction_info.document_scoped_z_order);
  }
}

void ConvertPoint(const gfx::Point& mojom_point,
                  optimization_guide::proto::Point* proto_point) {
  proto_point->set_x(mojom_point.x());
  proto_point->set_y(mojom_point.y());
}

void ConvertSelection(
    const blink::mojom::AIPageContentSelection& mojom_selection,
    optimization_guide::proto::Selection* proto_selection) {
  proto_selection->set_selected_text(mojom_selection.selected_text);
  proto_selection->set_start_node_id(mojom_selection.start_dom_node_id);
  proto_selection->set_end_node_id(mojom_selection.end_dom_node_id);
  proto_selection->set_start_offset(mojom_selection.start_offset);
  proto_selection->set_end_offset(mojom_selection.end_offset);
}

void ConvertFrameInteractionInfo(
    const blink::mojom::AIPageContentFrameInteractionInfo&
        mojom_frame_interaction_info,
    optimization_guide::proto::FrameInteractionInfo*
        proto_frame_interaction_info) {
  if (mojom_frame_interaction_info.selection) {
    ConvertSelection(*mojom_frame_interaction_info.selection,
                     proto_frame_interaction_info->mutable_selection());
  }
}

void ConvertPageInteractionInfo(
    const blink::mojom::AIPageContentPageInteractionInfo&
        mojom_page_interaction_info,
    optimization_guide::proto::PageInteractionInfo*
        proto_page_interaction_info) {
  if (mojom_page_interaction_info.focused_dom_node_id) {
    proto_page_interaction_info->set_focused_node_id(
        *mojom_page_interaction_info.focused_dom_node_id);
  }
  if (mojom_page_interaction_info.accessibility_focused_dom_node_id) {
    proto_page_interaction_info->set_accessibility_focused_node_id(
        *mojom_page_interaction_info.accessibility_focused_dom_node_id);
  }
  if (mojom_page_interaction_info.mouse_position) {
    ConvertPoint(*mojom_page_interaction_info.mouse_position,
                 proto_page_interaction_info->mutable_mouse_position());
  }
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
  text_style->set_color(mojom_text_info.text_style->color);
}

void ConvertImageInfo(
    const blink::mojom::AIPageContentImageInfo& mojom_image_info,
    optimization_guide::proto::ImageInfo* proto_image_info) {
  if (mojom_image_info.image_caption) {
    proto_image_info->set_image_caption(*mojom_image_info.image_caption);
  }
  if (mojom_image_info.source_origin) {
    SecurityOriginSerializer::Serialize(
        *mojom_image_info.source_origin,
        proto_image_info->mutable_security_origin());
  }
}

void ConvertSVGData(const blink::mojom::AIPageContentSVGData& mojom_svg_data,
                    optimization_guide::proto::SVGData* proto_svg_data) {
  if (mojom_svg_data.inner_text) {
    proto_svg_data->set_inner_text(*mojom_svg_data.inner_text);
  }
}

void ConvertCanvasData(
    const blink::mojom::AIPageContentCanvasData& mojom_canvas_data,
    optimization_guide::proto::CanvasData* proto_canvas_data) {
  proto_canvas_data->set_layout_width(mojom_canvas_data.layout_size.width());
  proto_canvas_data->set_layout_height(mojom_canvas_data.layout_size.height());
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
                     optimization_guide::proto::FormInfo* proto_form_data) {
  if (mojom_form_data.form_name) {
    proto_form_data->set_form_name(*mojom_form_data.form_name);
  }
}

optimization_guide::proto::FormControlType ConvertFormControlType(
    blink::mojom::FormControlType form_control_type) {
  switch (form_control_type) {
    case blink::mojom::FormControlType::kButtonButton:
      return optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_BUTTON;
    case blink::mojom::FormControlType::kButtonSubmit:
      return optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_SUBMIT;
    case blink::mojom::FormControlType::kButtonReset:
      return optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_RESET;
    case blink::mojom::FormControlType::kButtonPopover:
      return optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_POPOVER;
    case blink::mojom::FormControlType::kFieldset:
      return optimization_guide::proto::FORM_CONTROL_TYPE_FIELDSET;
    case blink::mojom::FormControlType::kInputButton:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_BUTTON;
    case blink::mojom::FormControlType::kInputCheckbox:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_CHECKBOX;
    case blink::mojom::FormControlType::kInputColor:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_COLOR;
    case blink::mojom::FormControlType::kInputDate:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_DATE;
    case blink::mojom::FormControlType::kInputDatetimeLocal:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_DATETIME_LOCAL;
    case blink::mojom::FormControlType::kInputEmail:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_EMAIL;
    case blink::mojom::FormControlType::kInputFile:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_FILE;
    case blink::mojom::FormControlType::kInputHidden:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_HIDDEN;
    case blink::mojom::FormControlType::kInputImage:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_IMAGE;
    case blink::mojom::FormControlType::kInputMonth:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_MONTH;
    case blink::mojom::FormControlType::kInputNumber:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_NUMBER;
    case blink::mojom::FormControlType::kInputPassword:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD;
    case blink::mojom::FormControlType::kInputRadio:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RADIO;
    case blink::mojom::FormControlType::kInputRange:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RANGE;
    case blink::mojom::FormControlType::kInputReset:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RESET;
    case blink::mojom::FormControlType::kInputSearch:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH;
    case blink::mojom::FormControlType::kInputSubmit:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SUBMIT;
    case blink::mojom::FormControlType::kInputTelephone:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TELEPHONE;
    case blink::mojom::FormControlType::kInputText:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT;
    case blink::mojom::FormControlType::kInputTime:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TIME;
    case blink::mojom::FormControlType::kInputUrl:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_URL;
    case blink::mojom::FormControlType::kInputWeek:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_WEEK;
    case blink::mojom::FormControlType::kOutput:
      return optimization_guide::proto::FORM_CONTROL_TYPE_OUTPUT;
    case blink::mojom::FormControlType::kSelectOne:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE;
    case blink::mojom::FormControlType::kSelectMultiple:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_MULTIPLE;
    case blink::mojom::FormControlType::kTextArea:
      return optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA;
  }
  NOTREACHED();
}

void ConvertFormControlData(
    const blink::mojom::AIPageContentFormControlData& mojom_form_control_data,
    optimization_guide::proto::FormControlData* proto_form_control_data) {
  proto_form_control_data->set_form_control_type(
      ConvertFormControlType(mojom_form_control_data.form_control_type));
  proto_form_control_data->set_is_checked(mojom_form_control_data.is_checked);
  proto_form_control_data->set_is_required(mojom_form_control_data.is_required);
  if (mojom_form_control_data.field_name) {
    proto_form_control_data->set_field_name(
        *mojom_form_control_data.field_name);
  }
  if (mojom_form_control_data.field_value) {
    proto_form_control_data->set_field_value(
        *mojom_form_control_data.field_value);
  }
  if (mojom_form_control_data.placeholder) {
    proto_form_control_data->set_placeholder(
        *mojom_form_control_data.placeholder);
  }
  for (const auto& select_option : mojom_form_control_data.select_options) {
    auto* proto_select_option = proto_form_control_data->add_select_options();
    if (select_option->value) {
      proto_select_option->set_value(*select_option->value);
    }
    if (select_option->text) {
      proto_select_option->set_text(*select_option->text);
    }
    proto_select_option->set_is_selected(select_option->is_selected);
  }
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
  if (mojom_attributes.dom_node_id.has_value()) {
    proto_attributes->set_common_ancestor_dom_node_id(
        mojom_attributes.dom_node_id.value());
  }

  proto_attributes->set_attribute_type(
      ConvertAttributeType(mojom_attributes.attribute_type));

  if (mojom_attributes.geometry) {
    ConvertGeometry(*mojom_attributes.geometry,
                    proto_attributes->mutable_geometry());
  }

  if (mojom_attributes.node_interaction_info) {
    ConvertNodeInteractionInfo(*mojom_attributes.node_interaction_info,
                               proto_attributes->mutable_interaction_info());
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
  } else if (mojom_attributes.svg_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kSVG) {
      return false;
    }
    ConvertSVGData(*mojom_attributes.svg_data,
                   proto_attributes->mutable_svg_data());
  } else if (mojom_attributes.canvas_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kCanvas) {
      return false;
    }
    ConvertCanvasData(*mojom_attributes.canvas_data,
                      proto_attributes->mutable_canvas_data());
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
  } else if (mojom_attributes.form_control_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kFormControl) {
      return false;
    }
    ConvertFormControlData(*mojom_attributes.form_control_data,
                           proto_attributes->mutable_form_control_data());
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
  if (mojom_attributes.label) {
    proto_attributes->set_label(*mojom_attributes.label);
  }

  for (const auto& annotated_role : mojom_attributes.annotated_roles) {
    proto_attributes->add_annotated_roles(ConvertAnnotatedRole(annotated_role));
  }

  if (mojom_attributes.aria_role) {
    proto_attributes->set_aria_role(AXRoleToProto(*mojom_attributes.aria_role));
  }

  if (mojom_attributes.label_for_dom_node_id) {
    proto_attributes->set_label_for_dom_node_id(
        *mojom_attributes.label_for_dom_node_id);
  }

  return true;
}

void ConvertFrameMetadata(
    GURL url,
    const blink::mojom::AIPageContentFrameData& mojom_frame_data,
    optimization_guide::mojom::PageMetadata& metadata) {

  auto frame_metadata = optimization_guide::mojom::FrameMetadata::New();
  frame_metadata->url = url;

  for (const auto& mojom_meta_tag : mojom_frame_data.meta_data) {
    auto meta_tag = optimization_guide::mojom::MetaTag::New();
    meta_tag->name = mojom_meta_tag->name;
    meta_tag->content = mojom_meta_tag->content;
    frame_metadata->meta_tags.push_back(std::move(meta_tag));
  }
  metadata.frame_metadata.push_back(std::move(frame_metadata));
}

void ConvertFrameData(
    const RenderFrameInfo& render_frame_info,
    const blink::mojom::AIPageContentFrameData& mojom_frame_data,
    optimization_guide::proto::FrameData* proto_frame_data,
    optimization_guide::mojom::PageMetadata& metadata,
    FrameTokenSet& frame_token_set) {
  ConvertFrameMetadata(render_frame_info.url, mojom_frame_data, metadata);
  SecurityOriginSerializer::Serialize(
      render_frame_info.source_origin,
      proto_frame_data->mutable_security_origin());
  ConvertFrameInteractionInfo(
      *mojom_frame_data.frame_interaction_info,
      proto_frame_data->mutable_frame_interaction_info());
  if (render_frame_info.url.SchemeIsHTTPOrHTTPS()) {
    proto_frame_data->set_url(render_frame_info.url.spec());
  }
  if (mojom_frame_data.title) {
    proto_frame_data->set_title(mojom_frame_data.title.value());
  }
  AddDocumentIdentifier(render_frame_info.global_frame_token, frame_token_set,
                        render_frame_info.serialized_server_token,
                        proto_frame_data);

  if (mojom_frame_data.contains_paid_content) {
    auto* paid_content_metadata =
        proto_frame_data->mutable_paid_content_metadata();
    paid_content_metadata->set_contains_paid_content(
        mojom_frame_data.contains_paid_content.value());
  }
}

// `mojom_iframe_data` holds information about the iframe provided by the
// embedder. It comes from the iframe node in the ContentNode tree pulled from
// the embedder's process.
//
// `mojom_local_frame_data` holds information about the embedded Document. This
// is inlined into the ContentNode tree pulled from the embedder for local
// frames as an optimization.
void ConvertIframeData(
    const RenderFrameInfo& render_frame_info,
    const blink::mojom::AIPageContentIframeData& mojom_iframe_data,
    const blink::mojom::AIPageContentFrameData& mojom_local_frame_data,
    optimization_guide::mojom::PageMetadata& metadata,
    FrameTokenSet& frame_token_set,
    optimization_guide::proto::IframeData* proto_iframe_data) {
  proto_iframe_data->set_likely_ad_frame(mojom_iframe_data.likely_ad_frame);

  ConvertFrameData(render_frame_info, mojom_local_frame_data,
                   proto_iframe_data->mutable_frame_data(), metadata,
                   frame_token_set);
}

bool ConvertNode(content::GlobalRenderFrameHostToken source_frame_token,
                 const blink::mojom::AIPageContentNode& mojom_node,
                 const AIPageContentMap& page_content_map,
                 FrameTokenSet& frame_token_set,
                 GetRenderFrameInfo get_render_frame_info,
                 optimization_guide::mojom::PageMetadata& metadata,
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

    const blink::mojom::AIPageContentFrameData* frame_data = nullptr;
    if (frame_token.Is<blink::RemoteFrameToken>()) {
      // RemoteFrame should have no child nodes since the content is out of
      // process.
      if (!mojom_node.children_nodes.empty()) {
        return false;
      }

      // The embedder shouldn't be providing LocalFrameData for remote frames.
      if (iframe_data.local_frame_data) {
        return false;
      }

      auto it = page_content_map.find(render_frame_info->global_frame_token);
      if (it == page_content_map.end()) {
        return true;
      }

      const auto& frame_page_content = *it->second;
      frame_data = frame_page_content.frame_data.get();
      auto* proto_child_frame_node = proto_node->add_children_nodes();
      if (!ConvertNode(render_frame_info->global_frame_token,
                       *frame_page_content.root_node, page_content_map,
                       frame_token_set, get_render_frame_info, metadata,

                       proto_child_frame_node)) {
        return false;
      }
    } else {
      if (!iframe_data.local_frame_data) {
        return false;
      }

      frame_data = iframe_data.local_frame_data.get();
    }

    auto* proto_iframe_data =
        proto_node->mutable_content_attributes()->mutable_iframe_data();
    ConvertIframeData(*render_frame_info, iframe_data, *frame_data, metadata,
                      frame_token_set, proto_iframe_data);
  }

  const auto source_frame_for_children =
      render_frame_info ? render_frame_info->global_frame_token
                        : source_frame_token;
  for (const auto& mojom_child : mojom_node.children_nodes) {
    auto* proto_child = proto_node->add_children_nodes();
    if (!ConvertNode(source_frame_for_children, *mojom_child, page_content_map,
                     frame_token_set, get_render_frame_info, metadata,
                     proto_child)) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool ConvertAIPageContentToProto(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    FrameTokenSet& frame_token_set,
    optimization_guide::AIPageContentResult& page_content_result) {
  auto it = page_content_map.find(main_frame_token);
  if (it == page_content_map.end()) {
    return false;
  }

  const auto& main_frame_page_content = *it->second;

  auto render_frame_info = get_render_frame_info.Run(
      main_frame_token.child_id, main_frame_token.frame_token);
  // The frame may have been torn down or crashed before we got a response.
  if (!render_frame_info) {
    return false;
  }

  ConvertFrameData(*render_frame_info, *main_frame_page_content.frame_data,
                   page_content_result.proto.mutable_main_frame_data(),
                   *page_content_result.metadata, frame_token_set);
  if (!ConvertNode(main_frame_token, *main_frame_page_content.root_node,
                   page_content_map, frame_token_set, get_render_frame_info,
                   *page_content_result.metadata,
                   page_content_result.proto.mutable_root_node())) {
    return false;
  }

  if (main_frame_page_content.page_interaction_info) {
    ConvertPageInteractionInfo(
        *main_frame_page_content.page_interaction_info,
        page_content_result.proto.mutable_page_interaction_info());
  }

  auto version = optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0;
  if (main_frame_options->enable_experimental_actionable_data) {
    version = optimization_guide::proto::
        ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0;
  }
  page_content_result.proto.set_version(version);

  return true;
}

RenderFrameInfo::RenderFrameInfo() = default;
RenderFrameInfo::RenderFrameInfo(const RenderFrameInfo& other) = default;

}  // namespace optimization_guide
