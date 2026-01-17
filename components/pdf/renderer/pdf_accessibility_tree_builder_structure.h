// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_STRUCTURE_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_STRUCTURE_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "pdf/accessibility_structs.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"

namespace chrome_pdf {
struct AccessibilityStructureElement;
}  // namespace chrome_pdf

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace pdf {

class PdfAccessibilityTreeBuilder;

// Structure tree-based accessibility tree building for tagged PDFs.
//
// This class implements accessibility tree building for tagged PDFs that have
// explicit semantic structure information via PDF structure trees. It uses:
//
// 1. EXPLICIT ASSOCIATIONS: Uses structure tree's direct associations between
//    semantic elements and content (text runs, images), eliminating the need
//    for index-based tracking or sequential processing assumptions.
//
// 2. SEMANTIC STRUCTURE: Leverages PDF structure tree tags (H1, P, Table,
//    Figure, etc.) to create properly structured accessibility trees with
//    correct ARIA roles and relationships.
class PdfAccessibilityTreeBuilderStructure {
 public:
  PdfAccessibilityTreeBuilderStructure(
      PdfAccessibilityTreeBuilder& builder,
      const chrome_pdf::AccessibilityStructureElement* structure_tree_root);

  PdfAccessibilityTreeBuilderStructure(
      const PdfAccessibilityTreeBuilderStructure&) = delete;
  PdfAccessibilityTreeBuilderStructure& operator=(
      const PdfAccessibilityTreeBuilderStructure&) = delete;

  ~PdfAccessibilityTreeBuilderStructure();

  void BuildPageTree();

  // Checks if a structure element or any of its descendants have content
  // (text runs or images). Used to skip empty structural containers.
  static bool StructureTreeHasContent(
      const chrome_pdf::AccessibilityStructureElement* pdf_struct_element);

 private:
  // Recursively walks the structure tree, creating accessibility nodes for
  // each structure element based on its type and content.
  void WalkStructureTree(
      const chrome_pdf::AccessibilityStructureElement* pdf_struct_element,
      ui::AXNodeData* parent_node);

  // Creates an accessibility node with text content from associated text runs.
  // Returns the created node or nullptr if no content could be added.
  ui::AXNodeData* CreateNodeWithTextContent(
      ui::AXNodeData* parent_node,
      ax::mojom::Role role,
      base::span<const raw_ptr<chrome_pdf::AccessibilityTextRunInfo,
                               VectorExperimental>> text_runs);

  // Creates an accessibility node with image content. Returns the created image
  // node.
  ui::AXNodeData* CreateNodeWithImageContent(
      ui::AXNodeData* parent_node,
      const chrome_pdf::AccessibilityImageInfo& image_info);

  // If there are any unassociated text runs (those not linked to any structure
  // element) at the beginning of the document (before any structured content),
  // create a single paragraph node containing the unassociated text runs.
  void InsertUnassociatedTextRunsAtStart();

  // Given an index into the `builder_->text_runs()` array, return an
  // unassociated text range that begins in that index, if it exists. This is
  // used to interleave unassociated text ranges with text ranges associated
  // with structure tree content.
  std::optional<chrome_pdf::UnassociatedTextRunRange>
  FindUnassociatedTextRunRangeAtIndex(size_t range_start);

  raw_ref<PdfAccessibilityTreeBuilder> builder_;
  const raw_ptr<const chrome_pdf::AccessibilityStructureElement>
      structure_tree_root_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_BUILDER_STRUCTURE_H_
