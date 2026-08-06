// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "pdf/accessibility_structs.h"
#include "services/screen_ai/buildflags/buildflags.h"

namespace chrome_pdf {
struct AccessibilityButtonInfo;
struct AccessibilityCharInfo;
struct AccessibilityChoiceFieldInfo;
struct AccessibilityHighlightInfo;
struct AccessibilityImageInfo;
struct AccessibilityLinkInfo;
struct AccessibilityTextFieldInfo;
struct AccessibilityTextRunInfo;
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

// Heuristic rules used to classify a text block as a heading.
enum class HeadingClassifier {
  // Not classified as a heading.
  kNone,
  // Classified based on font size.
  kFontSize,
  // Classified based on bold styling.
  kBoldStyle,
  // Classified because all characters are uppercase.
  kAllUppercase,
  // Classified due to semi-bold font weight (600).
  kSemiBoldWeight,
  // Classified by looking at the font name to determine styling.
  kFontName,
};

// Bundles raw page layout data (text runs, characters, start indices) used by
// the heuristic tree builder.
struct PageLayoutData {
  // All the accessibility text runs on the page.
  base::raw_span<const chrome_pdf::AccessibilityTextRunInfo> text_runs;

  // All of the character info for the page.
  base::raw_span<const chrome_pdf::AccessibilityCharInfo> chars;

  // The starting character index for each text run on the page.
  base::raw_span<const uint32_t> text_run_start_indices;
};

// Computed page-specific metrics and classification thresholds used as
// decision factors by the heuristic tree builder.
struct HeuristicThresholds {
  // The line spacing threshold above which a paragraph break is identified.
  float paragraph_spacing_threshold;

  // A mapping from a text run's font size to its heading level (ranges from 1
  // to 6).
  const raw_ref<const std::map<float, int>> heading_font_size_mapping;

  // The median font size on the page.
  float median_font_size;

  // The minimum font size threshold required for a run to be considered a
  // heading.
  float heading_font_size_threshold;
};

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
  ~PdfAccessibilityTreeBuilderHeuristic();

  // Main entry point for heuristic tree building. Processes all text runs
  // sequentially, applying heuristics to determine block structure and
  // inserting page objects (links, images, forms) based on index tracking.
  void BuildPageTree();

 private:
  ui::AXNodeData* CreateBlockLevelNode(float font_size,
                                       const HeuristicThresholds& thresholds);

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
