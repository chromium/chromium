// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_structure.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_constants_helper.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"

namespace pdf {

namespace {

// Adds text runs as inline text box children of static_text_node. Returns the
// accumulated text string.
std::string AddTextRunsToStaticText(
    PdfAccessibilityTreeBuilder& builder,
    ui::AXNodeData* static_text_node,
    const chrome_pdf::UnassociatedTextRunRange& range) {
  const auto& text_runs = builder.text_runs();
  std::string accumulated_text;

  for (size_t run_idx = range.start; run_idx <= range.end; ++run_idx) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run = text_runs[run_idx];
    chrome_pdf::PageCharacterIndex page_char_index = {
        builder.page_index(), builder.text_run_start_indices()[run_idx]};

    ui::AXNodeData* inline_text_box =
        builder.CreateInlineTextBoxNode(text_run, page_char_index);
    static_text_node->child_ids.push_back(inline_text_box->id);

    static_text_node->relative_bounds.bounds.Union(
        inline_text_box->relative_bounds.bounds);
    base::StrAppend(&accumulated_text, {inline_text_box->GetStringAttribute(
                                           ax::mojom::StringAttribute::kName)});
  }

  return accumulated_text;
}

// Creates a paragraph node containing the text runs in the range.
ui::AXNodeData* CreateParagraphFromTextRunRange(
    PdfAccessibilityTreeBuilder& builder,
    const chrome_pdf::UnassociatedTextRunRange& range) {
  ui::AXNodeData* container_node = builder.CreateAndAppendNode(
      ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);

  chrome_pdf::PageCharacterIndex page_char_index = {
      builder.page_index(), builder.text_run_start_indices()[range.start]};
  ui::AXNodeData* static_text_node =
      builder.CreateStaticTextNode(page_char_index);
  container_node->child_ids.push_back(static_text_node->id);

  std::string accumulated_text =
      AddTextRunsToStaticText(builder, static_text_node, range);

  static_text_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                       accumulated_text);
  container_node->relative_bounds.bounds =
      static_text_node->relative_bounds.bounds;

  return container_node;
}

}  // namespace

PdfAccessibilityTreeBuilderStructure::PdfAccessibilityTreeBuilderStructure(
    PdfAccessibilityTreeBuilder& builder,
    const chrome_pdf::AccessibilityStructureElement* structure_tree_root)
    : builder_(builder), structure_tree_root_(structure_tree_root) {}

PdfAccessibilityTreeBuilderStructure::~PdfAccessibilityTreeBuilderStructure() =
    default;

void PdfAccessibilityTreeBuilderStructure::BuildPageTree() {
  InsertUnassociatedTextRunsAtStart();

  WalkStructureTree(structure_tree_root_, builder_->page_node());
}

// static
bool PdfAccessibilityTreeBuilderStructure::StructureTreeHasContent(
    const chrome_pdf::AccessibilityStructureElement* pdf_struct_element) {
  if (!pdf_struct_element) {
    return false;
  }

  // Check if this element has direct content.
  if (!pdf_struct_element->associated_text_runs_if_available.empty() ||
      pdf_struct_element->associated_image_if_available) {
    return true;
  }

  for (const auto& child : pdf_struct_element->children) {
    if (StructureTreeHasContent(child.get())) {
      return true;
    }
  }

  return false;
}

ui::AXNodeData* PdfAccessibilityTreeBuilderStructure::CreateNodeWithTextContent(
    ui::AXNodeData* parent_node,
    ax::mojom::Role role,
    base::span<const raw_ptr<chrome_pdf::AccessibilityTextRunInfo,
                             VectorExperimental>> text_runs) {
  // Create container node with the specified role (e.g., kParagraph, kHeading).
  ui::AXNodeData* container_node =
      builder_->CreateAndAppendNode(role, ax::mojom::Restriction::kReadOnly);
  parent_node->child_ids.push_back(container_node->id);

  // Find indices of the associated text runs in the page's text run list.
  std::vector<size_t> text_run_indices;
  for (const auto& text_run_ptr : text_runs) {
    auto it = std::ranges::find_if(
        builder_->text_runs(), [&text_run_ptr](const auto& tr) {
          return tr.start_index == text_run_ptr->start_index;
        });
    if (it != builder_->text_runs().end()) {
      size_t idx = std::distance(builder_->text_runs().begin(), it);
      text_run_indices.push_back(idx);
    }
  }

  if (text_run_indices.empty()) {
    return container_node;
  }

  // Create static text node as child of container.
  chrome_pdf::PageCharacterIndex page_char_index = {
      builder_->page_index(),
      builder_->text_run_start_indices()[text_run_indices[0]]};
  ui::AXNodeData* static_text_node =
      builder_->CreateStaticTextNode(page_char_index);
  container_node->child_ids.push_back(static_text_node->id);

  // Create inline text box nodes for each associated text run.
  std::string accumulated_text;
  for (size_t text_run_index : text_run_indices) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (builder_->text_runs())[text_run_index];
    page_char_index.char_index =
        builder_->text_run_start_indices()[text_run_index];

    ui::AXNodeData* inline_text_box =
        builder_->CreateInlineTextBoxNode(text_run, page_char_index);
    static_text_node->child_ids.push_back(inline_text_box->id);

    static_text_node->relative_bounds.bounds.Union(
        inline_text_box->relative_bounds.bounds);
    accumulated_text +=
        inline_text_box->GetStringAttribute(ax::mojom::StringAttribute::kName);
  }

  // If there are any unassociated text runs immediately after the associated
  // text runs, add them each as sibling text box nodes.
  // TODO(crbug.com/40707542): Consider using heuristics to determine whether
  // unassociated text should be siblings or in a separate container.
  auto range = FindUnassociatedTextRunRangeAtIndex(text_run_indices.back() + 1);
  if (range) {
    std::string text =
        AddTextRunsToStaticText(*builder_, static_text_node, *range);
    base::StrAppend(&accumulated_text, {text});
  }

  static_text_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                       accumulated_text);
  container_node->relative_bounds.bounds =
      static_text_node->relative_bounds.bounds;

  return container_node;
}

ui::AXNodeData*
PdfAccessibilityTreeBuilderStructure::CreateNodeWithImageContent(
    ui::AXNodeData* parent_node,
    const chrome_pdf::AccessibilityImageInfo& image_info) {
  ui::AXNodeData* image_node = builder_->CreateImageNode(image_info);
  parent_node->child_ids.push_back(image_node->id);
  return image_node;
}

void PdfAccessibilityTreeBuilderStructure::WalkStructureTree(
    const chrome_pdf::AccessibilityStructureElement* pdf_struct_element,
    ui::AXNodeData* parent_node) {
  if (!pdf_struct_element) {
    return;
  }

  // For purely structural container types (Part, Div), skip content check
  // and recurse directly to children. This avoids O(n²) redundant checks
  // for deeply nested Part/Div structures.
  if (pdf_struct_element->type == chrome_pdf::PdfTagType::kPart ||
      pdf_struct_element->type == chrome_pdf::PdfTagType::kDiv) {
    for (const auto& child : pdf_struct_element->children) {
      WalkStructureTree(child.get(), parent_node);
    }
    return;
  }

  // Skip empty elements entirely.
  if (!StructureTreeHasContent(pdf_struct_element)) {
    return;
  }

  // Check if this element has direct content.
  bool has_text =
      !pdf_struct_element->associated_text_runs_if_available.empty();
  bool has_image = !!pdf_struct_element->associated_image_if_available;

  // Map PDF tag to accessibility role, except kDocument which is mapped to
  // kGenericContainer to avoid introducing a redundant Document node in the
  // accessibility tree.
  const ax::mojom::Role role =
      pdf_struct_element->type == chrome_pdf::PdfTagType::kDocument
          ? ax::mojom::Role::kGenericContainer
          : chrome_pdf::AXRoleFromPdfTagType(pdf_struct_element->type);

  // Handle elements with both text and image content (from different MCIDs).
  if (has_text && has_image) {
    // Create container node with semantic role (e.g., Paragraph)
    ui::AXNodeData* container_node = CreateNodeWithTextContent(
        parent_node, role,
        pdf_struct_element->associated_text_runs_if_available);

    if (!pdf_struct_element->language.empty()) {
      container_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                         pdf_struct_element->language);
    }

    // Add image as additional child of the container.
    chrome_pdf::AccessibilityImageInfo modified_image =
        *pdf_struct_element->associated_image_if_available;
    if (!pdf_struct_element->alt_text.empty()) {
      modified_image.alt_text = pdf_struct_element->alt_text;
    }
    CreateNodeWithImageContent(container_node, modified_image);

    // TODO(crbug.com/40707542): Handle pdf_struct_element->actual_text as text
    // override.
    // TODO(crbug.com/40707542): Handle
    // pdf_struct_element->abbreviation_expansion.

    for (const auto& child : pdf_struct_element->children) {
      WalkStructureTree(child.get(), container_node);
    }
    return;
  }

  // Handle elements with text content only.
  if (has_text) {
    ui::AXNodeData* node_data = CreateNodeWithTextContent(
        parent_node, role,
        pdf_struct_element->associated_text_runs_if_available);

    if (!pdf_struct_element->alt_text.empty()) {
      node_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                    pdf_struct_element->alt_text);
    }
    if (!pdf_struct_element->language.empty()) {
      node_data->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                    pdf_struct_element->language);
    }

    // TODO(crbug.com/40707542): Handle pdf_struct_element->actual_text as text
    // override.
    // TODO(crbug.com/40707542): Handle
    // pdf_struct_element->abbreviation_expansion.

    for (const auto& child : pdf_struct_element->children) {
      WalkStructureTree(child.get(), node_data);
    }
    return;
  }

  // Handle elements with image content.
  if (has_image) {
    // Use alt_text from structure element (authoritative per PDF spec).
    chrome_pdf::AccessibilityImageInfo modified_image =
        *pdf_struct_element->associated_image_if_available;
    modified_image.alt_text = pdf_struct_element->alt_text;

    // PDF images can appear in two contexts:
    // (i) Figure elements: These are standalone images that should have a
    // semantic Figure container (role="figure") with an Image child
    // (role="img"). E.g. photos, diagrams. This matches HTML
    // <figure><img></figure> semantics. The Figure container holds the alt text
    // and can have additional children like captions.
    //
    // (ii) Non-Figure images: Images that are part of other structural elements
    // and should be created as direct Image nodes (role="img") within their
    // parent container. E.g. link elements (clickable images/buttons with
    // role="link" with img child), formula elements (equations with role="math"
    // with img child).

    if (pdf_struct_element->type == chrome_pdf::PdfTagType::kFigure) {
      // Create a Figure container node with role="figure".
      ui::AXNodeData* figure_node = builder_->CreateAndAppendNode(
          ax::mojom::Role::kFigure, ax::mojom::Restriction::kReadOnly);
      parent_node->child_ids.push_back(figure_node->id);

      // Add alt text and language to the Figure container (not the image
      // child). The Figure is the semantic element, like HTML <figure>.
      if (!pdf_struct_element->alt_text.empty()) {
        figure_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                        pdf_struct_element->alt_text);
      }
      if (!pdf_struct_element->language.empty()) {
        figure_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                        pdf_struct_element->language);
      }

      ui::AXNodeData* image_node =
          CreateNodeWithImageContent(figure_node, modified_image);

      figure_node->relative_bounds.bounds = image_node->relative_bounds.bounds;

      // Process any children of the Figure (e.g., captions).
      for (const auto& child : pdf_struct_element->children) {
        WalkStructureTree(child.get(), figure_node);
      }
    } else {
      // Non-Figure image: Create the image directly within the parent element.
      // The parent's role (Link, Formula, etc.) provides semantic context.
      ui::AXNodeData* image_node =
          CreateNodeWithImageContent(parent_node, modified_image);

      if (!pdf_struct_element->language.empty()) {
        image_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                       pdf_struct_element->language);
      }

      for (const auto& child : pdf_struct_element->children) {
        WalkStructureTree(child.get(), image_node);
      }
    }
    return;
  }

  // Handle empty semantic containers (create container and recurse).
  ui::AXNodeData* container =
      builder_->CreateAndAppendNode(role, ax::mojom::Restriction::kReadOnly);
  parent_node->child_ids.push_back(container->id);

  if (!pdf_struct_element->alt_text.empty()) {
    container->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                  pdf_struct_element->alt_text);
  }
  if (!pdf_struct_element->language.empty()) {
    container->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                  pdf_struct_element->language);
  }

  for (const auto& child : pdf_struct_element->children) {
    WalkStructureTree(child.get(), container);
  }
}

std::optional<chrome_pdf::UnassociatedTextRunRange>
PdfAccessibilityTreeBuilderStructure::FindUnassociatedTextRunRangeAtIndex(
    size_t range_start) {
  auto ranges = structure_tree_root_->unassociated_text_run_ranges_for_page;
  if (ranges.empty()) {
    return std::nullopt;
  }

  auto range = std::ranges::lower_bound(
      ranges, range_start, {}, &chrome_pdf::UnassociatedTextRunRange::start);

  if (range != ranges.end() && range->start == range_start) {
    return *range;
  }

  return std::nullopt;
}

void PdfAccessibilityTreeBuilderStructure::InsertUnassociatedTextRunsAtStart() {
  auto range = FindUnassociatedTextRunRangeAtIndex(0);
  if (!range) {
    return;
  }

  ui::AXNodeData* container =
      CreateParagraphFromTextRunRange(*builder_, *range);

  builder_->page_node()->child_ids.insert(
      builder_->page_node()->child_ids.begin(), container->id);
}

}  // namespace pdf
