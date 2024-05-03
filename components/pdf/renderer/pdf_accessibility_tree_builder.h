// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "pdf/accessibility_structs.h"
#include "services/screen_ai/buildflags/buildflags.h"
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
      base::WeakPtr<PdfAccessibilityTree> pdf_accessibility_tree,
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
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      ,
      PdfOcrHelper* ocr_helper,
      bool has_accessible_text
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  );

  PdfAccessibilityTreeBuilder(const PdfAccessibilityTreeBuilder&) = delete;
  PdfAccessibilityTreeBuilder& operator=(const PdfAccessibilityTreeBuilder&) =
      delete;
  ~PdfAccessibilityTreeBuilder();

  void BuildPageTree();

 private:
  void AddWordStartsAndEnds(ui::AXNodeData* inline_text_box);
  ui::AXNodeData* CreateAndAppendNode(ax::mojom::Role role,
                                      ax::mojom::Restriction restriction);
  ui::AXNodeData* CreateParagraphNode(float font_size);
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
  void AddTextToAXNode(size_t start_text_run_index,
                       uint32_t end_text_run_index,
                       ui::AXNodeData* ax_node,
                       ui::AXNodeData** previous_on_line_node);
  void AddTextToObjectNode(size_t object_text_run_index,
                           uint32_t object_text_run_count,
                           ui::AXNodeData* object_node,
                           ui::AXNodeData* para_node,
                           ui::AXNodeData** previous_on_line_node,
                           size_t* text_run_index);
  void AddLinkToParaNode(const chrome_pdf::AccessibilityLinkInfo& link,
                         ui::AXNodeData* para_node,
                         ui::AXNodeData** previous_on_line_node,
                         size_t* text_run_index);
  void AddImageToParaNode(const chrome_pdf::AccessibilityImageInfo& image,
                          ui::AXNodeData* para_node,
                          size_t* text_run_index);
  void AddHighlightToParaNode(
      const chrome_pdf::AccessibilityHighlightInfo& highlight,
      ui::AXNodeData* para_node,
      ui::AXNodeData** previous_on_line_node,
      size_t* text_run_index);
  void AddTextFieldToParaNode(
      const chrome_pdf::AccessibilityTextFieldInfo& text_field,
      ui::AXNodeData* para_node,
      size_t* text_run_index);
  void AddButtonToParaNode(const chrome_pdf::AccessibilityButtonInfo& button,
                           ui::AXNodeData* para_node,
                           size_t* text_run_index);
  void AddChoiceFieldToParaNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
      ui::AXNodeData* para_node,
      size_t* text_run_index);
  void AddRemainingAnnotations(ui::AXNodeData* para_node);

  base::WeakPtr<PdfAccessibilityTree> pdf_accessibility_tree_;
  std::vector<uint32_t> text_run_start_indices_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityTextRunInfo>>
      text_runs_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityCharInfo>> chars_;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityLinkInfo>> links_;
  uint32_t current_link_index_ = 0;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityImageInfo>> images_;
  uint32_t current_image_index_ = 0;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityHighlightInfo>>
      highlights_;
  uint32_t current_highlight_index_ = 0;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityTextFieldInfo>>
      text_fields_;
  uint32_t current_text_field_index_ = 0;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityButtonInfo>>
      buttons_;
  uint32_t current_button_index_ = 0;
  const raw_ref<const std::vector<chrome_pdf::AccessibilityChoiceFieldInfo>>
      choice_fields_;
  uint32_t current_choice_field_index_ = 0;
  uint32_t page_index_;
  raw_ptr<ui::AXNodeData> root_node_;
  raw_ptr<ui::AXNodeData> page_node_;
  raw_ptr<blink::WebAXObject> container_obj_;
  raw_ptr<std::vector<std::unique_ptr<ui::AXNodeData>>> nodes_;
  raw_ptr<std::map<int32_t, chrome_pdf::PageCharacterIndex>>
      node_id_to_page_char_index_;
  raw_ptr<std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>>
      node_id_to_annotation_info_;
  float heading_font_size_threshold_ = 0;
  float paragraph_spacing_threshold_ = 0;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  raw_ptr<PdfOcrHelper> ocr_helper_ = nullptr;
  const bool has_accessible_text_;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_H_
