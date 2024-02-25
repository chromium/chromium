// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_ax_action_target.h"

#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "pdf/accessibility_structs.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace pdf {

namespace {

chrome_pdf::AccessibilityScrollAlignment ConvertAXScrollToPdfScrollAlignment(
    ax::mojom::ScrollAlignment scroll_alignment) {
  switch (scroll_alignment) {
    case ax::mojom::ScrollAlignment::kScrollAlignmentCenter:
      return chrome_pdf::AccessibilityScrollAlignment::kCenter;
    case ax::mojom::ScrollAlignment::kScrollAlignmentTop:
      return chrome_pdf::AccessibilityScrollAlignment::kTop;
    case ax::mojom::ScrollAlignment::kScrollAlignmentBottom:
      return chrome_pdf::AccessibilityScrollAlignment::kBottom;
    case ax::mojom::ScrollAlignment::kScrollAlignmentLeft:
      return chrome_pdf::AccessibilityScrollAlignment::kLeft;
    case ax::mojom::ScrollAlignment::kScrollAlignmentRight:
      return chrome_pdf::AccessibilityScrollAlignment::kRight;
    case ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge:
      return chrome_pdf::AccessibilityScrollAlignment::kClosestToEdge;
    case ax::mojom::ScrollAlignment::kNone:
      return chrome_pdf::AccessibilityScrollAlignment::kNone;
  }
}

}  // namespace

// static
const PdfAXActionTarget* PdfAXActionTarget::FromAXActionTarget(
    const ui::AXActionTarget* ax_action_target) {
  if (ax_action_target &&
      ax_action_target->GetType() == ui::AXActionTarget::Type::kPdf) {
    return static_cast<const PdfAXActionTarget*>(ax_action_target);
  }

  return nullptr;
}

PdfAXActionTarget::PdfAXActionTarget(const ui::AXNode& plugin_node,
                                     PdfAccessibilityTree* pdf_tree_source)
    : target_plugin_node_(plugin_node),
      pdf_accessibility_tree_source_(pdf_tree_source) {
  DCHECK(pdf_accessibility_tree_source_);
}

PdfAXActionTarget::~PdfAXActionTarget() = default;

ui::AXActionTarget::Type PdfAXActionTarget::GetType() const {
  return ui::AXActionTarget::Type::kPdf;
}

bool PdfAXActionTarget::PerformAction(
    const ui::AXActionData& action_data) const {
  switch (action_data.action) {
    case ax::mojom::Action::kDoDefault:
      return Click();
    case ax::mojom::Action::kShowContextMenu:
      return ShowContextMenu();
    case ax::mojom::Action::kScrollToPoint:
      return ScrollToGlobalPoint(action_data.target_point);
    case ax::mojom::Action::kStitchChildTree: {
      ui::AXTreeData pdf_tree_data;
      if (!pdf_accessibility_tree_source_->GetTreeData(&pdf_tree_data)) {
        return false;
      }
      CHECK_EQ(action_data.target_tree_id, pdf_tree_data.tree_id);
      CHECK_EQ(action_data.target_node_id, target_plugin_node_->id());
      CHECK_NE(action_data.child_tree_id, ui::AXTreeIDUnknown());
      return StitchChildTree(action_data.child_tree_id);
    }
    default:
      return false;
  }
}

bool PdfAXActionTarget::Click() const {
  if (target_plugin_node_->GetRole() != ax::mojom::Role::kLink) {
    return false;
  }

  std::optional<PdfAccessibilityTree::AnnotationInfo> annotation_info_result =
      pdf_accessibility_tree_source_->GetPdfAnnotationInfoFromAXNode(
          target_plugin_node_->data().id);
  if (!annotation_info_result.has_value())
    return false;
  const auto& annotation_info = annotation_info_result.value();

  chrome_pdf::AccessibilityActionData pdf_action_data;
  pdf_action_data.page_index = annotation_info.page_index;
  pdf_action_data.annotation_index = annotation_info.annotation_index;
  pdf_action_data.annotation_type =
      chrome_pdf::AccessibilityAnnotationType::kLink;
  pdf_action_data.action = chrome_pdf::AccessibilityAction::kDoDefaultAction;

  pdf_accessibility_tree_source_->HandleAction(pdf_action_data);
  return true;
}

gfx::Rect PdfAXActionTarget::GetRelativeBounds() const {
  return gfx::Rect();
}

gfx::Point PdfAXActionTarget::GetScrollOffset() const {
  return gfx::Point();
}

gfx::Point PdfAXActionTarget::MinimumScrollOffset() const {
  return gfx::Point();
}

gfx::Point PdfAXActionTarget::MaximumScrollOffset() const {
  return gfx::Point();
}

void PdfAXActionTarget::SetScrollOffset(const gfx::Point& point) const {}

bool PdfAXActionTarget::SetSelection(const ui::AXActionTarget* anchor_object,
                                     int anchor_offset,
                                     const ui::AXActionTarget* focus_object,
                                     int focus_offset) const {
  const PdfAXActionTarget* pdf_anchor_object =
      FromAXActionTarget(anchor_object);
  const PdfAXActionTarget* pdf_focus_object = FromAXActionTarget(focus_object);
  if (!pdf_anchor_object || !pdf_focus_object || anchor_offset < 0 ||
      focus_offset < 0) {
    return false;
  }
  chrome_pdf::AccessibilityActionData pdf_action_data;
  if (!pdf_accessibility_tree_source_->FindCharacterOffset(
          pdf_anchor_object->AXNode(), anchor_offset,
          pdf_action_data.selection_start_index) ||
      !pdf_accessibility_tree_source_->FindCharacterOffset(
          pdf_focus_object->AXNode(), focus_offset,
          pdf_action_data.selection_end_index)) {
    return false;
  }
  pdf_action_data.action = chrome_pdf::AccessibilityAction::kSetSelection;
  pdf_action_data.target_rect =
      gfx::ToEnclosingRect(target_plugin_node_->data().relative_bounds.bounds);

  pdf_accessibility_tree_source_->HandleAction(pdf_action_data);
  return true;
}

bool PdfAXActionTarget::ShowContextMenu() const {
  return pdf_accessibility_tree_source_->ShowContextMenu();
}

bool PdfAXActionTarget::ScrollToMakeVisible() const {
  return false;
}

bool PdfAXActionTarget::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& rect,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) const {
  chrome_pdf::AccessibilityActionData pdf_action_data;
  pdf_action_data.action =
      chrome_pdf::AccessibilityAction::kScrollToMakeVisible;
  pdf_action_data.horizontal_scroll_alignment =
      ConvertAXScrollToPdfScrollAlignment(horizontal_scroll_alignment);
  pdf_action_data.vertical_scroll_alignment =
      ConvertAXScrollToPdfScrollAlignment(vertical_scroll_alignment);
  pdf_action_data.target_rect =
      gfx::ToEnclosingRect(target_plugin_node_->data().relative_bounds.bounds);

  pdf_accessibility_tree_source_->HandleAction(pdf_action_data);
  return true;
}

bool PdfAXActionTarget::ScrollToGlobalPoint(const gfx::Point& point) const {
  chrome_pdf::AccessibilityActionData pdf_action_data;
  pdf_action_data.action =
      chrome_pdf::AccessibilityAction::kScrollToGlobalPoint;
  pdf_action_data.target_point = point;
  pdf_action_data.target_rect =
      gfx::ToEnclosingRect(target_plugin_node_->data().relative_bounds.bounds);

  pdf_accessibility_tree_source_->HandleAction(pdf_action_data);
  return true;
}

bool PdfAXActionTarget::StitchChildTree(
    const ui::AXTreeID& child_tree_id) const {
  if (!target_plugin_node_->IsDataValid()) {
    return false;
  }
  return pdf_accessibility_tree_source_->SetChildTree(target_plugin_node_->id(),
                                                      child_tree_id);
}

}  // namespace pdf
