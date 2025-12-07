// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "pdf/accessibility_structs.h"
#include "pdf/page_character_index.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"

namespace blink {
class WebAXObject;
}

namespace ui {
struct AXNodeData;
}

namespace pdf {

class PdfAccessibilityTreeBuilder {
 public:
  PdfAccessibilityTreeBuilder(
      bool mark_headings_using_heuristic,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects,
      const chrome_pdf::AccessibilityPageInfo& page_info,
      uint32_t page_index,
      ui::AXNodeData* root_node,
      blink::WebAXObject* container_obj,
      std::vector<std::unique_ptr<ui::AXNodeData>>* nodes,
      std::map<int32_t, chrome_pdf::PageCharacterIndex>*
          node_id_to_page_char_index,
      std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>*
          node_id_to_annotation_info
  );

  PdfAccessibilityTreeBuilder(const PdfAccessibilityTreeBuilder&) = delete;
  PdfAccessibilityTreeBuilder& operator=(const PdfAccessibilityTreeBuilder&) =
      delete;
  ~PdfAccessibilityTreeBuilder();

  void BuildPageTree();

 private:
  friend class PdfAccessibilityTreeBuilderHeuristic;

  void AddWordStartsAndEnds(ui::AXNodeData* inline_text_box);
  ui::AXNodeData* CreateAndAppendNode(ax::mojom::Role role,
                                      ax::mojom::Restriction restriction);
  ui::AXNodeData* CreateStaticTextNode();
  ui::AXNodeData* CreateStaticTextNode(
      const chrome_pdf::PageCharacterIndex& page_char_index);
  ui::AXNodeData* CreateInlineTextBoxNode(
      const chrome_pdf::AccessibilityTextRunInfo& text_run,
      const chrome_pdf::PageCharacterIndex& page_char_index);
  ui::AXNodeData* CreateLinkNode(const chrome_pdf::AccessibilityLinkInfo& link);
  ui::AXNodeData* CreateImageNode(
      const chrome_pdf::AccessibilityImageInfo& image);
  ui::AXNodeData* CreateHighlightNode(
      const chrome_pdf::AccessibilityHighlightInfo& highlight);
  ui::AXNodeData* CreatePopupNoteNode(
      const chrome_pdf::AccessibilityHighlightInfo& highlight);
  ui::AXNodeData* CreateTextFieldNode(
      const chrome_pdf::AccessibilityTextFieldInfo& text_field);
  ui::AXNodeData* CreateButtonNode(
      const chrome_pdf::AccessibilityButtonInfo& button);
  ui::AXNodeData* CreateListboxOptionNode(
      const chrome_pdf::AccessibilityChoiceFieldOptionInfo& choice_field_option,
      ax::mojom::Restriction restriction);
  ui::AXNodeData* CreateListboxNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
      ui::AXNodeData* control_node);
  ui::AXNodeData* CreateComboboxInputNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
      ax::mojom::Restriction restriction);
  ui::AXNodeData* CreateComboboxNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field);
  ui::AXNodeData* CreateChoiceFieldNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  ui::AXNodeData* CreateOcrWrapperNode(const gfx::PointF& position, bool start);
#endif

  const bool mark_headings_using_heuristic_;
  std::vector<uint32_t> text_run_start_indices_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityTextRunInfo>>
      text_runs_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityCharInfo>> chars_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityLinkInfo>> links_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityImageInfo>> images_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityHighlightInfo>>
      highlights_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityTextFieldInfo>>
      text_fields_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityButtonInfo>>
      buttons_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityChoiceFieldInfo>>
      choice_fields_;

  uint32_t page_index_;
  raw_ptr<ui::AXNodeData> root_node_;
  raw_ptr<ui::AXNodeData> page_node_;
  raw_ptr<blink::WebAXObject> container_obj_;
  raw_ptr<std::vector<std::unique_ptr<ui::AXNodeData>>> nodes_;
  raw_ptr<std::map<int32_t, chrome_pdf::PageCharacterIndex>>
      node_id_to_page_char_index_;
  raw_ptr<std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>>
      node_id_to_annotation_info_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_
