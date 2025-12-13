// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_

#include <cstddef>
#include <cstdint>

#include "base/memory/raw_ref.h"
#include "services/screen_ai/buildflags/buildflags.h"

namespace chrome_pdf {
struct AccessibilityButtonInfo;
struct AccessibilityChoiceFieldInfo;
struct AccessibilityHighlightInfo;
struct AccessibilityImageInfo;
struct AccessibilityLinkInfo;
struct AccessibilityTextFieldInfo;
}  // namespace chrome_pdf

namespace ui {
struct AXNodeData;
}

namespace pdf {

class PdfAccessibilityTreeBuilder;

// Heuristic-based accessibility tree building for untagged PDFs.
//
// This file contains functions used to build accessibility trees from untagged
// PDFs that lack semantic structure information. These functions use:
//
// 1. Index-based tracking: Matches page objects (links, images, highlights,
//    form fields) to text runs using sequential index tracking.
//
// 2. Heuristic analysis: Infers semantic structure (paragraphs, headings,
//    lines) by analyzing visual layout properties like font sizes, line
//    spacing, and spatial relationships.
//

// This class implements the complete heuristic accessibility tree building
// algorithm for untagged PDFs.
class PdfAccessibilityTreeBuilderHeuristic {
 public:
  explicit PdfAccessibilityTreeBuilderHeuristic(
      PdfAccessibilityTreeBuilder& builder);

  PdfAccessibilityTreeBuilderHeuristic(
      const PdfAccessibilityTreeBuilderHeuristic&) = delete;
  PdfAccessibilityTreeBuilderHeuristic& operator=(
      const PdfAccessibilityTreeBuilderHeuristic&) = delete;

  // Main entry point for heuristic tree building. Processes all text runs
  // sequentially, applying heuristics to determine block structure and
  // inserting page objects (links, images, forms) based on index tracking.
  void BuildPageTree();

 private:
  ui::AXNodeData* CreateBlockLevelNode(const std::string& text_run_type,
                                       float font_size);

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

  void AddRemainingAnnotations(ui::AXNodeData* para_node
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
                               ,
                               bool ocr_applied
#endif
  );

  raw_ref<PdfAccessibilityTreeBuilder> builder_;

  // Heuristic-specific state for sequential processing and analysis.
  float heading_font_size_threshold_ = 0;
  float paragraph_spacing_threshold_ = 0;

  // Sequential index tracking for page objects.
  uint32_t current_link_index_ = 0;
  uint32_t current_image_index_ = 0;
  uint32_t current_highlight_index_ = 0;
  uint32_t current_text_field_index_ = 0;
  uint32_t current_button_index_ = 0;
  uint32_t current_choice_field_index_ = 0;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_
