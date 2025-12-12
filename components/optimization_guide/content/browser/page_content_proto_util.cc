// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/optimization_guide/content/browser/autofill_annotations_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/page_content_proto_serializer.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-data-view.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace features {
// Killswitch to adding autofill information to form controls.
BASE_FEATURE(kAnnotatedPageContentWithAutofillAnnotations,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether or not Autofill-suggested redactions of credit card fields
// are applied to the page content.
BASE_FEATURE(kAnnotatedPageContentAutofillCreditCardRedactions,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace {

std::optional<AutofillFieldMetadata> GetAutofillFieldData(
    std::optional<content::GlobalRenderFrameHostToken> source_frame_token,
    ConvertAIPageContentToProtoSession& session,
    const optimization_guide::proto::ContentAttributes& proto_attributes) {
  if (source_frame_token.has_value() &&
      base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentWithAutofillAnnotations)) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromFrameToken(*source_frame_token);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    if (auto* autofill_annotations_provider =
            AutofillAnnotationsProvider::GetFor(web_contents)) {
      return autofill_annotations_provider->GetAutofillFieldData(
          *render_frame_host, proto_attributes.common_ancestor_dom_node_id(),
          session);
    }
  }

  return std::nullopt;
}

proto::RedactionDecision ConvertAutofillFieldRedactionReason(
    const optimization_guide::proto::FormControlData& form_control_data,
    AutofillFieldRedactionReason redaction_reason) {
  switch (redaction_reason) {
    case AutofillFieldRedactionReason::kNoRedactionNeeded:
      return proto::REDACTION_DECISION_NO_REDACTION_NECESSARY;
    case AutofillFieldRedactionReason::kShouldRedactForPayments:
      return form_control_data.field_value().empty()
                 ? proto::REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD
                 : proto::
                       REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD;
  }
}

bool ShouldRedactContent(proto::RedactionDecision redaction_decision) {
  switch (redaction_decision) {
    case proto::REDACTION_DECISION_NO_REDACTION_NECESSARY:
    case proto::REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD:
    case proto::REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD:
      return false;

    case proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD:
      return true;

    case proto::REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD:
      return base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentAutofillCreditCardRedactions);

    default:
      // We cannot exhaustively switch nor static_assert on the proto values, as
      // otherwise automatic syncing of new enum values will break compilation.
      // Instead, we default to not redacting and just best-effort log.
      LOG(ERROR) << "Missing case statement in ShouldRedactContent";
      return false;
  }
}

// Returns whether or not a given form control node should have its content
// redacted (irrespective of the reason).
bool ShouldRedactContent(
    const optimization_guide::proto::FormControlData& form_control_data) {
  return form_control_data.has_redaction_decision() &&
         ShouldRedactContent(form_control_data.redaction_decision());
}

optimization_guide::proto::ClickabilityReason ConvertClickabilityReason(
    blink::mojom::AIPageContentClickabilityReason reason) {
  switch (reason) {
    case blink::mojom::AIPageContentClickabilityReason::kClickableControl:
      return optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL;
    case blink::mojom::AIPageContentClickabilityReason::kClickEvents:
      return optimization_guide::proto::CLICKABILITY_REASON_CLICK_HANDLER;
    case blink::mojom::AIPageContentClickabilityReason::kMouseEvents:
      return optimization_guide::proto::CLICKABILITY_REASON_MOUSE_EVENTS;
    case blink::mojom::AIPageContentClickabilityReason::kKeyEvents:
      return optimization_guide::proto::CLICKABILITY_REASON_KEY_EVENTS;
    case blink::mojom::AIPageContentClickabilityReason::kEditable:
      return optimization_guide::proto::CLICKABILITY_REASON_EDITABLE;
    case blink::mojom::AIPageContentClickabilityReason::kCursorPointer:
      return optimization_guide::proto::CLICKABILITY_REASON_CURSOR_POINTER;
    case blink::mojom::AIPageContentClickabilityReason::kAriaRole:
      return optimization_guide::proto::CLICKABILITY_REASON_ARIA_ROLE;
    case blink::mojom::AIPageContentClickabilityReason::kAriaHasPopup:
      return optimization_guide::proto::CLICKABILITY_REASON_ARIA_HAS_POPUP;
    case blink::mojom::AIPageContentClickabilityReason::kAriaExpandedTrue:
      return optimization_guide::proto::CLICKABILITY_REASON_ARIA_EXPANDED_TRUE;
    case blink::mojom::AIPageContentClickabilityReason::kAriaExpandedFalse:
      return optimization_guide::proto::CLICKABILITY_REASON_ARIA_EXPANDED_FALSE;
    case blink::mojom::AIPageContentClickabilityReason::kTabIndex:
      return optimization_guide::proto::CLICKABILITY_REASON_TAB_INDEX;
    case blink::mojom::AIPageContentClickabilityReason::kAutocomplete:
      return optimization_guide::proto::CLICKABILITY_REASON_AUTOCOMPLETE;
    case blink::mojom::AIPageContentClickabilityReason::kMouseClick:
      return optimization_guide::proto::CLICKABILITY_REASON_MOUSE_CLICK;
    case blink::mojom::AIPageContentClickabilityReason::kMouseHover:
      return optimization_guide::proto::CLICKABILITY_REASON_MOUSE_HOVER;
    case blink::mojom::AIPageContentClickabilityReason::kHoverPseudoClass:
      return optimization_guide::proto::CLICKABILITY_REASON_HOVER_PSEUDO_CLASS;
  }
  NOTREACHED();
}

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
    case blink::mojom::AIPageContentAttributeType::kVideo:
      return optimization_guide::proto::CONTENT_ATTRIBUTE_VIDEO;
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
  for (const gfx::Rect& rect : mojom_geometry.fragment_visible_bounding_boxes) {
    ConvertRect(rect, proto_geometry->add_fragment_visible_bounding_boxes());
  }
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
  proto_interaction_info->set_is_clickable(
      mojom_node_interaction_info.is_clickable);
  proto_interaction_info->set_is_focusable(
      mojom_node_interaction_info.is_focusable);

  if (mojom_node_interaction_info.document_scoped_z_order) {
    proto_interaction_info->set_document_scoped_z_order(
        *mojom_node_interaction_info.document_scoped_z_order);
  }

  for (const auto& reason : mojom_node_interaction_info.clickability_reasons) {
    // TODO(khushalsagar): Remove this once consumers move to the new field.
    proto_interaction_info->add_debug_clickability_reasons(
        ConvertClickabilityReason(reason));

    proto_interaction_info->add_clickability_reasons(
        ConvertClickabilityReason(reason));
  }

  proto_interaction_info->set_is_disabled(
      mojom_node_interaction_info.is_disabled);
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

void ConvertVideoData(
    const blink::mojom::AIPageContentVideoData& mojom_video_data,
    optimization_guide::proto::VideoData* proto_video_data) {
  proto_video_data->set_url(mojom_video_data.url.spec());
  if (mojom_video_data.source_origin) {
    SecurityOriginSerializer::Serialize(
        *mojom_video_data.source_origin,
        proto_video_data->mutable_security_origin());
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
                     optimization_guide::proto::FormInfo* proto_form_data) {
  if (mojom_form_data.form_name) {
    proto_form_data->set_form_name(*mojom_form_data.form_name);
  }
  if (mojom_form_data.action_url) {
    // The Blink agent passes a fully resolved action URL so downstream
    // consumers can understand the destination of a form submission.
    proto_form_data->set_action_url(mojom_form_data.action_url->spec());
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

optimization_guide::proto::RedactionDecision ConvertRedactionDecision(
    blink::mojom::AIPageContentRedactionDecision redaction_decision) {
  switch (redaction_decision) {
    case blink::mojom::AIPageContentRedactionDecision::kNoRedactionNecessary:
      return optimization_guide::proto::
          REDACTION_DECISION_NO_REDACTION_NECESSARY;
    case blink::mojom::AIPageContentRedactionDecision::
        kUnredacted_EmptyPassword:
      return optimization_guide::proto::
          REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD;
    case blink::mojom::AIPageContentRedactionDecision::
        kRedacted_HasBeenPassword:
      return optimization_guide::proto::
          REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD;
  }
  NOTREACHED();
}

void ConvertFormControlData(
    const blink::mojom::AIPageContentFormControlData& mojom_form_control_data,
    const std::optional<AutofillFieldMetadata>& autofill_metadata,
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
  proto_form_control_data->set_redaction_decision(
      ConvertRedactionDecision(mojom_form_control_data.redaction_decision));

  // Incorporate any information received from Autofill.
  if (autofill_metadata) {
    proto_form_control_data->set_autofill_section_id(
        autofill_metadata->section_id);
    proto_form_control_data->add_coarse_autofill_field_type(
        autofill_metadata->coarse_field_type);

    // If we do not current have a redaction decision and Autofill does, use the
    // one that Autofill suggests.
    //
    // TODO(b/454611037): Handle <select> related data as well.
    if (base::FeatureList::IsEnabled(
            features::kAnnotatedPageContentAutofillCreditCardRedactions)) {
      proto::RedactionDecision autofill_redaction_decision =
          ConvertAutofillFieldRedactionReason(
              *proto_form_control_data, autofill_metadata->redaction_reason);
      if (proto_form_control_data->redaction_decision() ==
              proto::REDACTION_DECISION_NO_REDACTION_NECESSARY &&
          autofill_redaction_decision !=
              proto::REDACTION_DECISION_NO_REDACTION_NECESSARY) {
        proto_form_control_data->set_redaction_decision(
            autofill_redaction_decision);

        if (ShouldRedactContent(
                proto_form_control_data->redaction_decision())) {
          proto_form_control_data->clear_field_value();
        }
      }
    }
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

// `source_frame_token` is std::nullopt for documents inside popup windows since
// they're not associated with a `RenderFrameHost`.
base::expected<void, std::string> ConvertAttributes(
    std::optional<content::GlobalRenderFrameHostToken> source_frame_token,
    ConvertAIPageContentToProtoSession& session,
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
      return base::unexpected("text_info present, but node isn't kText");
    }
    ConvertTextInfo(*mojom_attributes.text_info,
                    proto_attributes->mutable_text_data());
  } else if (mojom_attributes.image_info) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kImage) {
      return base::unexpected("image_info present, but node isn't kImage");
    }
    ConvertImageInfo(*mojom_attributes.image_info,
                     proto_attributes->mutable_image_data());
  } else if (mojom_attributes.svg_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kSVG) {
      return base::unexpected("svg_data present, but node isn't kSvg");
    }
    ConvertSVGData(*mojom_attributes.svg_data,
                   proto_attributes->mutable_svg_data());
  } else if (mojom_attributes.canvas_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kCanvas) {
      return base::unexpected("canvas_data present, but node isn't kCanvas");
    }
    ConvertCanvasData(*mojom_attributes.canvas_data,
                      proto_attributes->mutable_canvas_data());
  } else if (mojom_attributes.video_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kVideo) {
      return base::unexpected("video_data present, but node isn't kVideo");
    }
    ConvertVideoData(*mojom_attributes.video_data,
                     proto_attributes->mutable_video_data());
  } else if (mojom_attributes.anchor_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kAnchor) {
      return base::unexpected("anchor_data present, but node isn't kAnchor");
    }
    ConvertAnchorData(*mojom_attributes.anchor_data,
                      proto_attributes->mutable_anchor_data());
  } else if (mojom_attributes.form_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kForm) {
      return base::unexpected("form_data present, but node isn't kForm");
    }
    ConvertFormData(*mojom_attributes.form_data,
                    proto_attributes->mutable_form_data());
  } else if (mojom_attributes.form_control_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kFormControl) {
      return base::unexpected(
          "form_control_data present, but node isn't kFormControl");
    }
    ConvertFormControlData(
        *mojom_attributes.form_control_data,
        GetAutofillFieldData(source_frame_token, session, *proto_attributes),
        proto_attributes->mutable_form_control_data());
  } else if (mojom_attributes.table_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kTable) {
      return base::unexpected("table_data present, but node isn't kTable");
    }
    ConvertTableData(*mojom_attributes.table_data,
                     proto_attributes->mutable_table_data());
  } else if (mojom_attributes.table_row_data) {
    if (mojom_attributes.attribute_type !=
        blink::mojom::AIPageContentAttributeType::kTableRow) {
      return base::unexpected(
          "table_row_data present, but node isn't kTableRow");
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

  proto_attributes->set_is_ad_related(mojom_attributes.is_ad_related);

  return base::ok();
}

void ConvertFrameMetadata(
    GURL url,
    const blink::mojom::AIPageContentFrameData& mojom_frame_data,
    blink::mojom::PageMetadata& metadata) {
  auto frame_metadata = blink::mojom::FrameMetadata::New();
  frame_metadata->url = url;

  for (const auto& mojom_meta_tag : mojom_frame_data.meta_data) {
    auto meta_tag = blink::mojom::MetaTag::New();
    meta_tag->name = mojom_meta_tag->name;
    meta_tag->content = mojom_meta_tag->content;
    frame_metadata->meta_tags.push_back(std::move(meta_tag));
  }
  metadata.frame_metadata.push_back(std::move(frame_metadata));
}

void ConvertScriptTool(
    const blink::mojom::ScriptTool& tool,
    optimization_guide::proto::ScriptTool* proto_script_tool) {
  proto_script_tool->set_name(tool.name);
  proto_script_tool->set_description(tool.description);

  if (tool.input_schema) {
    proto_script_tool->set_input_schema(*tool.input_schema);
  }

  if (tool.annotations) {
    proto_script_tool->mutable_annotations()->set_read_only(
        tool.annotations->read_only);
  }
}

void ConvertFrameData(
    const RenderFrameInfo& render_frame_info,
    const blink::mojom::AIPageContentFrameData& mojom_frame_data,
    optimization_guide::proto::FrameData* proto_frame_data,
    blink::mojom::PageMetadata& metadata,
    FrameTokenSet& frame_token_set) {
  ConvertFrameMetadata(GetURLForFrameMetadata(render_frame_info.url,
                                              render_frame_info.source_origin),
                       mojom_frame_data, metadata);
  SecurityOriginSerializer::Serialize(
      render_frame_info.source_origin,
      proto_frame_data->mutable_security_origin());
  ConvertFrameInteractionInfo(
      *mojom_frame_data.frame_interaction_info,
      proto_frame_data->mutable_frame_interaction_info());
  if (render_frame_info.url.SchemeIs(url::kDataScheme)) {
    // For data URLs the information is already in the content.
    proto_frame_data->set_url("data:");
  } else {
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

  if (render_frame_info.media_data) {
    *proto_frame_data->mutable_media_data() = *render_frame_info.media_data;
  }

  for (const auto& tool : mojom_frame_data.script_tools) {
    ConvertScriptTool(*tool, proto_frame_data->add_script_tools());
  }
}

void ConvertRedactionReason(
    const blink::mojom::RedactedFrameMetadata_Reason& mojom_reason,
    optimization_guide::proto::IframeData::RedactedFrameMetadata*
        proto_redacted_frame_metadata) {
  switch (mojom_reason) {
    case blink::mojom::RedactedFrameMetadata_Reason::kCrossSite:
      proto_redacted_frame_metadata->set_reason(
          optimization_guide::proto::IframeData::RedactedFrameMetadata::Reason::
              IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);
      break;
    case blink::mojom::RedactedFrameMetadata_Reason::kCrossOrigin:
      proto_redacted_frame_metadata->set_reason(
          optimization_guide::proto::IframeData::RedactedFrameMetadata::Reason::
              IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_ORIGIN);
      break;
  };
}

void ConvertRedactedIframeData(
    const RenderFrameInfo& render_frame_info,
    const blink::mojom::AIPageContentIframeData& mojom_iframe_data,
    const blink::mojom::RedactedFrameMetadata& mojom_redacted_frame_metadata,
    optimization_guide::proto::IframeData* proto_iframe_data) {
  ConvertRedactionReason(mojom_redacted_frame_metadata.reason,
                         proto_iframe_data->mutable_redacted_frame_metadata());
}

// Contains the information that remains the same throughout the tree
// recursion for ConvertAIPageContentToProto.
class Converter {
 public:
  Converter(blink::mojom::AIPageContentOptionsPtr options,
            const AIPageContentMap& page_content_map,
            const GetRenderFrameInfo get_render_frame_info,
            FrameTokenSet& frame_token_set,
            blink::mojom::PageMetadata& page_metadata,
            optimization_guide::proto::AnnotatedPageContent& page_content_proto)
      : options_(std::move(options)),
        page_content_map_(page_content_map),
        get_render_frame_info_(get_render_frame_info),
        frame_token_set_(frame_token_set),
        page_metadata_(page_metadata),
        page_content_proto_(page_content_proto) {}
  ~Converter() = default;

  base::expected<void, std::string> ConvertNode(
      content::GlobalRenderFrameHostToken source_frame_token,
      const blink::mojom::AIPageContentNode& mojom_node,
      optimization_guide::proto::ContentNode* proto_node) {
    const auto& mojom_attributes = *mojom_node.content_attributes;
    RETURN_IF_ERROR(
        ConvertAttributes(source_frame_token, session_, mojom_attributes,
                          proto_node->mutable_content_attributes()));

    std::optional<RenderFrameInfo> render_frame_info;
    if (mojom_attributes.attribute_type ==
        blink::mojom::AIPageContentAttributeType::kIframe) {
      if (!mojom_attributes.iframe_data) {
        return base::unexpected("iframe missing iframe_data");
      }

      const auto& iframe_data = *mojom_attributes.iframe_data;
      const auto frame_token = iframe_data.frame_token;

      // The frame may have been torn down or crashed before we got a response.
      render_frame_info =
          get_render_frame_info_.Run(source_frame_token.child_id, frame_token);
      if (!render_frame_info) {
        if (base::FeatureList::IsEnabled(
                blink::features::kAIPageContentMissingSubframesFailSilently)) {
          // If the frame was removed ignore its subtree but don't fail APC
          // generation for the whole tree.
          return base::ok();
        }
        return base::unexpected("could not find render_frame_info for iframe");
      }

      auto* proto_iframe_data =
          proto_node->mutable_content_attributes()->mutable_iframe_data();
      if (frame_token.Is<blink::RemoteFrameToken>()) {
        // RemoteFrame should have no child nodes since the content is out of
        // process.
        if (!mojom_node.children_nodes.empty()) {
          return base::unexpected("remote frame contains child nodes");
        }

        // The embedder shouldn't be providing LocalFrameData for remote frames.
        if (iframe_data.content) {
          return base::unexpected(
              "embedder incorrectly provided content for this iframe");
        }

        auto it =
            page_content_map_->find(render_frame_info->global_frame_token);
        if (it == page_content_map_->end()) {
          // This may happen either because the remote renderer responsible for
          // this frame was destroyed before we were able to query it, because
          // the renderer did not return before a timeout, or because the
          // supplied frame token was manipulated by a compromised renderer.
          return base::ok();
        }

        return std::visit(
            absl::Overload{
                [&](const blink::mojom::AIPageContentPtr& page_content) mutable
                    -> base::expected<void, std::string> {
                  auto* proto_child_frame_node =
                      proto_node->add_children_nodes();

                  if (page_content->frame_data &&
                      page_content->frame_data->popup) {
                    RETURN_IF_ERROR(ConvertPopup(
                        *page_content->frame_data->popup, *render_frame_info));
                  }

                  RETURN_IF_ERROR(ConvertNode(
                      render_frame_info->global_frame_token,
                      *page_content->root_node, proto_child_frame_node));

                  ConvertIframeData(*render_frame_info, iframe_data,
                                    /*mojom_local_frame_data=*/
                                    *page_content->frame_data.get(),
                                    proto_iframe_data);
                  return base::ok();
                },
                [&](const blink::mojom::RedactedFrameMetadataPtr& r) mutable
                    -> base::expected<void, std::string> {
                  ConvertRedactedIframeData(
                      *render_frame_info, iframe_data,
                      /*mojom_redacted_frame_metadata*/ *r.get(),
                      proto_iframe_data);
                  return base::ok();
                }},
            it->second);
      } else /* this is a local frame */ {
        if (!iframe_data.content) {
          return base::unexpected(
              "local frame missing local_frame_data or "
              "redacted_frame_metadata");
        }

        switch (iframe_data.content->which()) {
          case blink::mojom::AIPageContentIframeContent::Tag::kLocalFrameData:
            if (iframe_data.content->get_local_frame_data() &&
                iframe_data.content->get_local_frame_data()->popup) {
              RETURN_IF_ERROR(ConvertPopup(
                  *iframe_data.content->get_local_frame_data()->popup,
                  *render_frame_info));
            }
            ConvertIframeData(*render_frame_info, iframe_data,
                              /*mojom_local_frame_data=*/
                              *iframe_data.content->get_local_frame_data(),
                              proto_iframe_data);
            // Breaking instead of returning so we get to copy the child nodes.
            break;
          case blink::mojom::AIPageContentIframeContent::Tag::
              kRedactedFrameMetadata:
            ConvertRedactedIframeData(
                *render_frame_info, iframe_data,
                *iframe_data.content->get_redacted_frame_metadata().get(),
                proto_iframe_data);
            return base::ok();
        }
      }
    }

    // If we have discovered new redaction reasons during this conversion (e.g.,
    // from Autofill provided data), then we should not include child nodes as
    // child nodes can include content from redacted fields.
    //
    // Note that this logic can come after the iframe code above, because
    // a single node can never be both an iframe and also be redacted due to
    // form control data.
    const optimization_guide::proto::ContentAttributes& proto_attributes =
        proto_node->content_attributes();
    if (proto_attributes.has_form_control_data() &&
        ShouldRedactContent(proto_attributes.form_control_data())) {
      return base::ok();
    }

    // We should only get here if this is either a non-redacted local frame or a
    // regular node.
    const auto source_frame_for_children =
        render_frame_info ? render_frame_info->global_frame_token
                          : source_frame_token;
    for (const auto& mojom_child : mojom_node.children_nodes) {
      auto* proto_child = proto_node->add_children_nodes();
      RETURN_IF_ERROR(
          ConvertNode(source_frame_for_children, *mojom_child, proto_child));
    }

    return base::ok();
  }

  // Popup windows (annoyingly) do not have an RFH and cannot contain iframes,
  // so their traversal can be greatly simplified.
  base::expected<void, std::string> ConvertPopupNode(
      const blink::mojom::AIPageContentNode& mojom_node,
      optimization_guide::proto::ContentNode* proto_node) {
    const auto& mojom_attributes = *mojom_node.content_attributes;
    if (mojom_attributes.attribute_type ==
        blink::mojom::AIPageContentAttributeType::kIframe) {
      return base::unexpected("iframe is unexpected in popup");
    }

    RETURN_IF_ERROR(
        ConvertAttributes(std::nullopt, session_, mojom_attributes,
                          proto_node->mutable_content_attributes()));

    for (const auto& mojom_child : mojom_node.children_nodes) {
      auto* proto_child = proto_node->add_children_nodes();
      RETURN_IF_ERROR(ConvertPopupNode(*mojom_child, proto_child));
    }

    return base::ok();
  }

  base::expected<void, std::string> ConvertPopup(
      const blink::mojom::AIPageContentPopup& mojom_popup,
      const RenderFrameInfo& opener_frame_info) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kAIPageContentIncludePopupWindows)) {
      return base::ok();
    }

    optimization_guide::proto::PopupWindow* popup_window =
        page_content_proto_->mutable_popup_window();

    // First, walk the popup's DOM tree to create proto::ContentNodes.
    RETURN_IF_ERROR(ConvertPopupNode(*mojom_popup.root_node,
                                     popup_window->mutable_root_node()));

    // Set the document ID to the frame which opened the popup (might be wrong,
    // because we treat a main page and its same-site iframes as the same
    // document id). Also we don't need browser-side security check as the data
    // all come from the same renderer.
    popup_window->mutable_opener_document_id()->set_serialized_token(
        opener_frame_info.serialized_server_token);

    popup_window->set_opener_common_ancestor_dom_node_id(
        mojom_popup.opener_dom_node_id);

    ConvertRect(mojom_popup.visible_bounding_box,
                popup_window->mutable_visible_bounding_box());

    return base::ok();
  }

  const blink::mojom::AIPageContentOptionsPtr& options() const LIFETIME_BOUND {
    return options_;
  }

 private:
  // `mojom_iframe_data` holds information about the iframe provided by the
  // embedder. It comes from the iframe node in the ContentNode tree pulled from
  // the embedder's process.
  //
  // `mojom_local_frame_data` holds information about the embedded Document.
  // This is inlined into the ContentNode tree pulled from the embedder for
  // local frames as an optimization.
  void ConvertIframeData(
      const RenderFrameInfo& render_frame_info,
      const blink::mojom::AIPageContentIframeData& mojom_iframe_data,
      const blink::mojom::AIPageContentFrameData& mojom_local_frame_data,
      optimization_guide::proto::IframeData* proto_iframe_data) {
    ConvertFrameData(render_frame_info, mojom_local_frame_data,
                     proto_iframe_data->mutable_frame_data(), *page_metadata_,
                     *frame_token_set_);
  }

  blink::mojom::AIPageContentOptionsPtr options_;
  raw_ref<const AIPageContentMap> page_content_map_;
  GetRenderFrameInfo get_render_frame_info_;
  raw_ref<FrameTokenSet> frame_token_set_;
  raw_ref<blink::mojom::PageMetadata> page_metadata_;
  ConvertAIPageContentToProtoSession session_;
  raw_ref<optimization_guide::proto::AnnotatedPageContent> page_content_proto_;
};

// Private helper template to handle both mutable and const traversals for
// VisitContentNodes().
template <typename ContentNodeType, typename VisitorType>
  requires std::is_same_v<std::remove_const_t<ContentNodeType>,
                          optimization_guide::proto::ContentNode>
void VisitContentNodesImpl(ContentNodeType& node,
                           std::string_view document_identifier,
                           VisitorType visitor) {
  visitor(node, document_identifier);

  // In case of an iframe, replace the document_identifier for the traversal
  // of children.
  if (node.content_attributes().has_iframe_data()) {
    const optimization_guide::proto::IframeData& iframe_data =
        node.content_attributes().iframe_data();
    if (iframe_data.has_frame_data()) {
      const optimization_guide::proto::FrameData& frame_data =
          iframe_data.frame_data();
      document_identifier = frame_data.document_identifier().serialized_token();
    }
  }

  if constexpr (std::is_const_v<std::remove_reference_t<ContentNodeType>>) {
    for (const auto& child : node.children_nodes()) {
      VisitContentNodesImpl(child, document_identifier, visitor);
    }
  } else {
    for (auto& child : *node.mutable_children_nodes()) {
      VisitContentNodesImpl(child, document_identifier, visitor);
    }
  }
}

}  // namespace

ConvertAIPageContentToProtoSession::ConvertAIPageContentToProtoSession() =
    default;
ConvertAIPageContentToProtoSession::~ConvertAIPageContentToProtoSession() =
    default;

base::expected<void, std::string> ConvertAIPageContentToProto(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    FrameTokenSet& frame_token_set,
    optimization_guide::AIPageContentResult& page_content_result) {
  auto it = page_content_map.find(main_frame_token);
  if (it == page_content_map.end()) {
    return base::unexpected(
        "could not find AIPageContent or RedactedFrameMetadata for main frame");
  }

  const blink::mojom::AIPageContent* main_frame_page_content = nullptr;
  std::visit(absl::Overload{
                 [&main_frame_page_content](
                     const blink::mojom::AIPageContentPtr& p) mutable {
                   main_frame_page_content = p.get();
                 },
                 [](const blink::mojom::RedactedFrameMetadataPtr& r) mutable {
                   return;
                 }},
             it->second);

  if (!main_frame_page_content) {
    return base::unexpected(
        "Main content frame was redacted; this should not happen.");
  }

  auto render_frame_info = get_render_frame_info.Run(
      main_frame_token.child_id, main_frame_token.frame_token);
  // The frame may have been torn down or crashed before we got a response.
  if (!render_frame_info) {
    return base::unexpected("could not find RenderFrameInfo for main frame");
  }

  ConvertFrameData(*render_frame_info, *main_frame_page_content->frame_data,
                   page_content_result.proto.mutable_main_frame_data(),
                   *page_content_result.metadata, frame_token_set);

  Converter converter(std::move(main_frame_options), page_content_map,
                      get_render_frame_info, frame_token_set,
                      *page_content_result.metadata, page_content_result.proto);

  RETURN_IF_ERROR(converter.ConvertNode(
      main_frame_token, *main_frame_page_content->root_node,
      page_content_result.proto.mutable_root_node()));

  if (main_frame_page_content->page_interaction_info) {
    ConvertPageInteractionInfo(
        *main_frame_page_content->page_interaction_info,
        page_content_result.proto.mutable_page_interaction_info());
  }

  auto version = optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0;
  auto mode = optimization_guide::proto::ANNOTATED_PAGE_CONTENT_MODE_DEFAULT;
  if (converter.options()->mode ==
      blink::mojom::AIPageContentMode::kActionableElements) {
    version = optimization_guide::proto::
        ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0;
    mode = optimization_guide::proto::
        ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS;
  }
  page_content_result.proto.set_version(version);
  page_content_result.proto.set_mode(mode);

  // If the page had a popup open, provide that popup to APC as well.
  if (main_frame_page_content->frame_data->popup) {
    RETURN_IF_ERROR(converter.ConvertPopup(
        *main_frame_page_content->frame_data->popup, *render_frame_info));
  }

  return base::ok();
}

bool IsCoordinateInNode(
    const gfx::Point& coordinate,
    const optimization_guide::proto::ContentAttributes& node_attributes) {
  if (!node_attributes.geometry().has_visible_bounding_box()) {
    return false;
  }
  const auto& bounds = node_attributes.geometry().visible_bounding_box();
  return coordinate.x() >= bounds.x() && coordinate.y() >= bounds.y() &&
         coordinate.x() < bounds.x() + bounds.width() &&
         coordinate.y() < bounds.y() + bounds.height();
}

// Recursive helper function to find the document identifier and the topmost
// node for a coordinate. Performs a depth first search on current root node
// and if the hit node is an iframe recurse into it.
std::optional<TargetNodeInfo> FindNodeAtPointRecursive(
    const optimization_guide::proto::DocumentIdentifier&
        current_document_identifier,
    const optimization_guide::proto::ContentNode* current_root_node,
    const gfx::Point& coordinate,
    std::optional<TargetNodeInfo> prev_target_node_info) {
  std::vector<const optimization_guide::proto::ContentNode*> nodes_for_walk;
  int highest_z_order = std::numeric_limits<int>::min();
  const optimization_guide::proto::ContentNode*
      highest_z_order_node_in_document = nullptr;

  nodes_for_walk.push_back(current_root_node);
  while (!nodes_for_walk.empty()) {
    const optimization_guide::proto::ContentNode* node = nodes_for_walk.back();
    nodes_for_walk.pop_back();

    if (IsCoordinateInNode(coordinate, node->content_attributes()) &&
        node->content_attributes()
            .interaction_info()
            .has_document_scoped_z_order() &&
        highest_z_order < node->content_attributes()
                              .interaction_info()
                              .document_scoped_z_order()) {
      // If current node's z-order is higher, it becomes the new candidate.
      highest_z_order = node->content_attributes()
                            .interaction_info()
                            .document_scoped_z_order();
      highest_z_order_node_in_document = node;
    }

    // APC proto includes iframe contents as nodes under the iframe node. We
    // will first complete search within current document before recursing into
    // child frames.
    if (node->content_attributes().has_iframe_data()) {
      continue;
    }

    for (const optimization_guide::proto::ContentNode& child :
         node->children_nodes()) {
      nodes_for_walk.push_back(&child);
    }
  }

  // If no node in the current document context matches, return the target found
  // in the last recursive step.
  if (!highest_z_order_node_in_document) {
    return prev_target_node_info;
  }

  // The highest z-order node is not an iframe, so it's the target within the
  // current document.
  if (!highest_z_order_node_in_document->content_attributes()
           .has_iframe_data()) {
    return {{current_document_identifier, highest_z_order_node_in_document}};
  }

  // An iframe content node should have exactly 1 child node, i.e. the iframe's
  // root node. Otherwise fail silently since the data is coming from an
  // untrusted renderer.
  if (highest_z_order_node_in_document->children_nodes_size() != 1) {
    return std::nullopt;
  }
  return FindNodeAtPointRecursive(
      highest_z_order_node_in_document->content_attributes()
          .iframe_data()
          .frame_data()
          .document_identifier(),
      // Pass the root of iframe's content.
      &highest_z_order_node_in_document->children_nodes(0), coordinate,
      // This is the iframe node target in case no nodes in the iframe matches
      // the coordinate.
      {{current_document_identifier, highest_z_order_node_in_document}});
}

std::optional<optimization_guide::TargetNodeInfo> FindNodeAtPoint(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    const gfx::Point& coordinate) {
  // If we have a popup, search it first. Popups are always on top.
  if (base::FeatureList::IsEnabled(
          blink::features::kAIPageContentIncludePopupWindows) &&
      annotated_page_content.has_popup_window()) {
    std::optional<optimization_guide::TargetNodeInfo> target_node =
        FindNodeAtPointRecursive(
            annotated_page_content.popup_window().opener_document_id(),
            &annotated_page_content.popup_window().root_node(), coordinate,
            std::nullopt);
    if (target_node.has_value()) {
      return target_node;
    }
  }
  return FindNodeAtPointRecursive(
      annotated_page_content.main_frame_data().document_identifier(),
      &annotated_page_content.root_node(), coordinate, std::nullopt);
}

// Recursively searches the tree for a node with target_node_id in a frame with
// matching document identifier. Since the node id is unique per document, the
// search stops and returns as soon as the node is found in that document.
std::optional<TargetNodeInfo> FindNodeWithIDRecursive(
    const proto::ContentNode& current_node,
    const proto::DocumentIdentifier& current_doc_id,
    const std::string_view target_document_identifier,
    const int target_node_id) {
  if (current_node.has_content_attributes() &&
      current_node.content_attributes().has_common_ancestor_dom_node_id() &&
      current_node.content_attributes().common_ancestor_dom_node_id() ==
          target_node_id &&
      current_doc_id.serialized_token() == target_document_identifier) {
    return TargetNodeInfo{current_doc_id, &current_node};
  }

  // If this node is an iframe, its children has the iframe's document
  // identifier.
  const proto::DocumentIdentifier* child_context_doc_id = &current_doc_id;
  if (current_node.has_content_attributes() &&
      current_node.content_attributes().has_iframe_data() &&
      current_node.content_attributes().iframe_data().has_frame_data() &&
      current_node.content_attributes()
          .iframe_data()
          .frame_data()
          .has_document_identifier()) {
    child_context_doc_id = &current_node.content_attributes()
                                .iframe_data()
                                .frame_data()
                                .document_identifier();
  }

  for (const auto& child_node : current_node.children_nodes()) {
    std::optional<TargetNodeInfo> result =
        FindNodeWithIDRecursive(child_node, *child_context_doc_id,
                                target_document_identifier, target_node_id);
    if (result) {
      return result;
    }
  }

  return std::nullopt;
}

std::optional<TargetNodeInfo> FindNodeWithID(
    const proto::AnnotatedPageContent& annotated_page_content,
    const std::string_view document_identifier,
    const int content_node_id) {
  // If we have a popup, check it first.
  if (base::FeatureList::IsEnabled(
          blink::features::kAIPageContentIncludePopupWindows) &&
      annotated_page_content.has_popup_window() &&
      annotated_page_content.popup_window().has_opener_document_id()) {
    std::optional<TargetNodeInfo> target = FindNodeWithIDRecursive(
        annotated_page_content.popup_window().root_node(),
        annotated_page_content.popup_window().opener_document_id(),
        annotated_page_content.popup_window()
            .opener_document_id()
            .serialized_token(),
        content_node_id);
    if (target) {
      return target;
    }
  }

  // Validate the apc first.
  if (!annotated_page_content.has_root_node() ||
      !annotated_page_content.has_main_frame_data() ||
      !annotated_page_content.main_frame_data().has_document_identifier()) {
    return std::nullopt;
  }

  const proto::DocumentIdentifier& main_frame_doc_id =
      annotated_page_content.main_frame_data().document_identifier();

  return FindNodeWithIDRecursive(annotated_page_content.root_node(),
                                 main_frame_doc_id, document_identifier,
                                 content_node_id);
}

content::RenderFrameHost* GetRenderFrameForDocumentIdentifier(
    content::WebContents& web_contents,
    std::string_view target_document_token) {
  content::RenderFrameHost* render_frame = nullptr;
  web_contents.ForEachRenderFrameHostWithAction(
      [&target_document_token, &render_frame](content::RenderFrameHost* rfh) {
        // Skip inactive frame and its children.
        if (!rfh->IsActive()) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        auto* user_data =
            DocumentIdentifierUserData::GetForCurrentDocument(rfh);
        if (user_data &&
            user_data->serialized_token() == target_document_token) {
          render_frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return render_frame;
}

RenderFrameInfo::RenderFrameInfo() = default;
RenderFrameInfo::RenderFrameInfo(const RenderFrameInfo& other) = default;
RenderFrameInfo::~RenderFrameInfo() = default;

GURL GetURLForFrameMetadata(const GURL& committed_url,
                            const url::Origin& committed_origin) {
  // We could always rely on the origin but the full path of the URL is
  // important if it's not an opaque origin.
  if (committed_origin.opaque()) {
    return committed_origin.GetTupleOrPrecursorTupleIfOpaque().GetURL();
  }
  return committed_url;
}

void VisitContentNodes(
    optimization_guide::proto::ContentNode& node,
    std::string_view document_identifier,
    base::FunctionRef<void(optimization_guide::proto::ContentNode& node,
                           std::string_view document_identifier)> visitor) {
  VisitContentNodesImpl(node, document_identifier, visitor);
}

void VisitContentNodes(
    const optimization_guide::proto::ContentNode& node,
    std::string_view document_identifier,
    base::FunctionRef<void(const optimization_guide::proto::ContentNode& node,
                           std::string_view document_identifier)> visitor) {
  VisitContentNodesImpl(node, document_identifier, visitor);
}

}  // namespace optimization_guide
