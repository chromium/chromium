// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_node_utils.h"

#include <cinttypes>

#include "chrome/common/accessibility/read_anything_constants.h"
#include "services/strings/grit/services_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace a11y {

bool IsSuperscript(ui::AXNode* ax_node) {
  return ax_node->data().GetTextPosition() ==
         ax::mojom::TextPosition::kSuperscript;
}

std::string GetHtmlTagForPDF(ui::AXNode* ax_node, const std::string& html_tag) {
  ax::mojom::Role role = ax_node->GetRole();

  // Some nodes in PDFs don't have an HTML tag so use role instead.
  switch (role) {
    case ax::mojom::Role::kEmbeddedObject:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kRootWebArea:
      return "span";
    case ax::mojom::Role::kParagraph:
      return "p";
    case ax::mojom::Role::kLink:
      return "a";
    case ax::mojom::Role::kStaticText:
      return "";
    case ax::mojom::Role::kHeading:
      return GetHeadingHtmlTagForPDF(ax_node, html_tag);
    // Add a line break after each page of an inaccessible PDF for readability
    // since there is no other formatting included in the OCR output.
    case ax::mojom::Role::kContentInfo:
      if (ax_node->GetTextContentUTF8() ==
          l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END)) {
        return "br";
      }
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return html_tag.empty() ? "span" : html_tag;
  }
}

std::string GetHeadingHtmlTagForPDF(ui::AXNode* ax_node,
                                    const std::string& html_tag) {
  // Sometimes whole paragraphs can be formatted as a heading. If the text is
  // longer than 2 lines, assume it was meant to be a paragragh,
  if (ax_node->GetTextContentUTF8().length() > (2 * kMaxLineWidth)) {
    return "p";
  }

  // A single block of text could be incorrectly formatted with multiple heading
  // nodes (one for each line of text) instead of a single paragraph node. This
  // case should be detected to improve readability. If there are multiple
  // consecutive nodes with the same heading level, assume that they are all a
  // part of one paragraph.
  ui::AXNode* next = ax_node->GetNextUnignoredSibling();
  ui::AXNode* prev = ax_node->GetPreviousUnignoredSibling();

  if ((next && next->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag) ||
      (prev && prev->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag)) {
    return "span";
  }

  int32_t hierarchical_level =
      ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  if (hierarchical_level) {
    return base::StringPrintf("h%" PRId32, hierarchical_level);
  }
  return html_tag;
}

ui::AXNode* GetParentForSelection(ui::AXNode* ax_node) {
  ui::AXNode* parent = ax_node->GetUnignoredParentCrossingTreeBoundary();
  // For most nodes, the parent is the same as the most direct parent. However,
  // to handle special types of text formatting such as links and custom spans,
  // another parent may be needed. e.g. when a link is highlighted, the start
  // node has an "inline" display but the parent we want would have a "block"
  // display role, so in order to get the common parent of
  // all sibling nodes, the grandparent should be used.
  // Displays of type "list-item" is an exception to the "inline" display rule
  // so that all siblings in a list can be shown correctly to avoid
  // misnumbering.
  while (parent && parent->GetUnignoredParentCrossingTreeBoundary() &&
         parent->HasStringAttribute(ax::mojom::StringAttribute::kDisplay) &&
         ((parent->GetStringAttribute(ax::mojom::StringAttribute::kDisplay)
               .find("inline") != std::string::npos) ||
          (parent->GetStringAttribute(ax::mojom::StringAttribute::kDisplay)
               .find("list-item") != std::string::npos))) {
    parent = parent->GetUnignoredParentCrossingTreeBoundary();
  }

  return parent;
}

std::string GetAltText(ui::AXNode* ax_node) {
  std::string alt_text =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  return alt_text;
}

std::string GetImageDataUrl(ui::AXNode* ax_node) {
  std::string url =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kImageDataUrl);
  return url;
}

}  // namespace a11y
