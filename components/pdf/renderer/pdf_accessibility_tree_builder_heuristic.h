// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "pdf/accessibility_structs.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace chrome_pdf {
struct AccessibilityCharInfo;
struct AccessibilityHighlightInfo;
struct AccessibilityImageInfo;
struct AccessibilityLinkInfo;
struct AccessibilityTextRunInfo;
}  // namespace chrome_pdf

namespace ui {
struct AXNodeData;
}

namespace pdf {

class PdfAccessibilityTreeBuilder;
struct ListMarkerKind;

// One nesting level of the list being built: the top-level list, or a sublist
// inside an item of the level above. A level stays active, accepting new items,
// until a marker outdents past it or a non-list block ends the list.
//
// Levels are determined only by marker indentation, not by glyph, so a marker
// whose glyph changes (e.g. "•" to "‣") at the same indent is a sibling item.
struct ListLevel {
  raw_ptr<ui::AXNodeData> list;
  // The x-coordinate, in page pixels, of this level's first marker. Only
  // markers that start a new line are compared against it; mid-line bullets
  // stay in the current item's text.
  float marker_x = 0.0f;
  // The most recent `kListItem` at this level. A deeper marker opens a
  // nested list inside it.
  raw_ptr<ui::AXNodeData> last_item;
  // The x-coordinate, in page pixels, where `last_item`'s text starts.
  float item_text_x = 0.0f;
  // The value of the most recent item if this is an ordered list; unset for an
  // unordered list.
  std::optional<int> last_ordered_value;
};

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

// Bundles a single text run with its character span and the following run on
// the page.
struct TextRunContext {
  // The text run being evaluated.
  const raw_ref<const chrome_pdf::AccessibilityTextRunInfo> run;

  // The immediately following text run on the page, or nullptr if `run` is the
  // last run.
  raw_ptr<const chrome_pdf::AccessibilityTextRunInfo> next_run = nullptr;

  // The characters belonging to `run`.
  base::raw_span<const chrome_pdf::AccessibilityCharInfo> chars;

  // The characters belonging to `next_run`, or empty if there is none.
  base::raw_span<const chrome_pdf::AccessibilityCharInfo> next_chars;
};

// Tracks the in-progress static text node being accumulated across consecutive
// text runs with matching style and text position.
struct StaticTextState {
  StaticTextState();
  ~StaticTextState();

  // The static text node currently being built, or nullptr if none is active.
  raw_ptr<ui::AXNodeData> node = nullptr;

  // The accumulated UTF-8 text of all inline text boxes added to `node` so far.
  std::string text;

  // The text style of `node` when style tracking is active.
  std::optional<chrome_pdf::AccessibilityTextStyleInfo> style;

  // Whether `node` is superscript or subscript, when style tracking is active.
  std::optional<ax::mojom::TextPosition> text_position;

  // The language of `node`.
  std::string language;
};

// Computed page-specific metrics, styling properties, and classification
// thresholds used as decision factors by the heuristic tree builder.
//
// Every length, coordinate, and font size below uses the same units as
// `chrome_pdf::AccessibilityTextRunInfo::bounds`: page-relative pixels, with
// the origin at the top-left corner of the page and y increasing downward.
struct HeuristicPageProperties {
  // The line spacing threshold above which a paragraph break is identified.
  float paragraph_spacing_threshold = 0.0f;

  // The median font size on the page.
  float median_font_size = 0.0f;

  // The minimum font size threshold required for a run to be considered a
  // heading.
  float heading_font_size_threshold = 0.0f;

  // The height of the page.
  float page_height = 0.0f;

  // The vertical offset of the page in document coordinates.
  float page_offset_y = 0.0f;

  // The maximum width threshold for page numbers. Runs exceeding this width are
  // disqualified from being considered page numbers.
  float max_page_number_width = 0.0f;

  // The Y-coordinate threshold for the header margin (top margin). Runs ending
  // at or above this Y-coordinate are within the top margin.
  float top_margin = 0.0f;

  // The Y-coordinate threshold for page-number footers (bottom margin). Runs
  // starting at or below this Y-coordinate are within the page number bottom
  // margin.
  float bottom_page_number_margin = 0.0f;

  // The Y-coordinate threshold for non-page-number footers (bottom margin).
  // Runs starting at or below this Y-coordinate are within the non-page-number
  // bottom margin.
  float bottom_non_page_number_margin = 0.0f;

  // X-coordinate thresholds for the left and right side margins.
  float left_margin = 0.0f;
  float right_margin = 0.0f;

  // The dominant body text color on the page (in ARGB format), if multiple
  // colors exist.
  std::optional<uint32_t> body_text_color;

  // A mapping from a text run's font size to its heading level (ranges from 1
  // to 6).
  std::map<float, int> heading_font_size_mapping;
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
  // Result of `CreateBlockLevelNode()`.
  struct BlockLevelNodeResult;

  BlockLevelNodeResult CreateBlockLevelNode(
      const PageLayoutData& layout,
      size_t text_run_index,
      const HeuristicPageProperties& page_properties,
      const chrome_pdf::AccessibilityFormGraphicInfo* graphic);

  // Creates a `kListItem` node for `run_context` with `marker_kind`, updating
  // `list_levels_` to open a new list, nest inside the previous item, or pop
  // outdented levels.
  ui::AXNodeData* CreateListItemNode(const TextRunContext& run_context,
                                     const ListMarkerKind& marker_kind,
                                     std::vector<ListLevel> levels);

  ui::AXNodeData* CreateLineBreakingNode(ax::mojom::Role role,
                                         const std::string& language);

  // Appends a list marker node with `name` and `bounds` to `item_node` and
  // updates the bounds of `item_node` and the innermost list level.
  void AddListMarkerToNode(const std::string& name,
                           const gfx::RectF& bounds,
                           ui::AXNodeData* item_node);

  // Appends inline text box and static text nodes for the text run at
  // `text_run_index` to `parent_node`. For combined marker+text runs,
  // `marker_prefix_char_count` is the number of leading characters to skip:
  // the marker glyph and any whitespace surrounding it.
  ui::AXNodeData* AddTextRunToNode(size_t text_run_index,
                                   ui::AXNodeData* parent_node,
                                   StaticTextState* static_text_state,
                                   size_t marker_prefix_char_count = 0);

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

  void AddRemainingAnnotations(
      base::span<const chrome_pdf::AccessibilityImageInfo> images,
      ui::AXNodeData* para_node
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

  // The block node currently being built and the static text nodes created
  // while the block still has a uniform language.
  raw_ptr<ui::AXNodeData> current_block_node_ = nullptr;
  std::vector<raw_ptr<ui::AXNodeData>> current_block_static_text_nodes_;

  // List levels from outermost to innermost. Empty if the current block is not
  // in a list.
  std::vector<ListLevel> list_levels_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_HEURISTIC_H_
