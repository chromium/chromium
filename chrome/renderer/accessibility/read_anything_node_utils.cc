// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_node_utils.h"

#include <cinttypes>

#include "chrome/common/accessibility/read_anything_constants.h"
#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"

namespace a11y {

bool IsSuperscript(ui::AXNode* ax_node) {
  return ax_node->data().GetTextPosition() ==
         ax::mojom::TextPosition::kSuperscript;
}

bool IsNodeIgnoredForReadAnything(ui::AXNode* ax_node, bool is_pdf) {
  // If the node is not in the active tree (this could happen when RM is still
  // loading), ignore it.
  if (!ax_node) {
    return true;
  }
  ax::mojom::Role role = ax_node->GetRole();

  // PDFs processed with OCR have additional nodes that mark the start and end
  // of a page. The start of a page is indicated with a kBanner node that has a
  // child static text node. Ignore both. The end of a page is indicated with a
  // kContentInfo node that has a child static text node. Ignore the static text
  // node but keep the kContentInfo so a line break can be inserted in between
  // pages in GetHtmlTagForPDF.
  if (is_pdf) {
    // The text content of the aforementioned kBanner or kContentInfo nodes is
    // the same as the text content of its child static text node.
    std::string text = ax_node->GetTextContentUTF8();
    ui::AXNode* parent = ax_node->GetParent();

    std::string pdf_begin_message =
        l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_BEGIN);
    std::string pdf_end_message =
        l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END);

    bool is_start_or_end_static_text_node =
        parent && ((parent->GetRole() == ax::mojom::Role::kBanner &&
                    text == pdf_begin_message) ||
                   (parent->GetRole() == ax::mojom::Role::kContentInfo &&
                    text == pdf_end_message));
    if ((role == ax::mojom::Role::kBanner && text == pdf_begin_message) ||
        is_start_or_end_static_text_node) {
      return true;
    }
  }

  // Ignore interactive elements, except for text fields.
  return (ui::IsControl(role) && !ui::IsTextField(role)) || ui::IsSelect(role);
}

bool IsTextForReadAnything(ui::AXNode* node, bool is_pdf, bool is_docs) {
  if (!node) {
    return false;
  }

  // ListMarkers will have an HTML tag of "::marker," so they won't be
  // considered text when checking for the length of the html tag. However, in
  // order to read out loud ordered bullets, nodes that have the kListMarker
  // role should be included.
  // Note: This technically will include unordered list markers like bullets,
  // but these won't be spoken because they will be filtered by the TTS engine.
  bool is_list_marker = node->GetRole() == ax::mojom::Role::kListMarker;

  // TODO(crbug.com/40927698): Can this be updated to IsText() instead of
  // checking the length of the html tag?
  return (GetHtmlTag(node, is_pdf, is_docs).length() == 0) || is_list_marker;
}

std::string GetHtmlTag(ui::AXNode* ax_node, bool is_pdf, bool is_docs) {
  std::string html_tag =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);

  if (is_pdf) {
    return GetHtmlTagForPDF(ax_node, html_tag);
  }

  if (ui::IsTextField(ax_node->GetRole())) {
    return "div";
  }

  if (ui::IsHeading(ax_node->GetRole())) {
    int32_t hierarchical_level =
        ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    if (hierarchical_level) {
      return base::StringPrintf("h%" PRId32, hierarchical_level);
    }
  }

  if (html_tag == ui::ToString(ax::mojom::Role::kMark)) {
    // Replace mark element with bold element for readability.
    html_tag = "b";
  } else if (is_docs) {
    // Change HTML tags for SVG elements to allow Reading Mode to render text
    // for the Annotated Canvas elements in a Google Doc.
    if (html_tag == "svg") {
      html_tag = "div";
    }
    if (html_tag == "g" && ax_node->GetRole() == ax::mojom::Role::kParagraph) {
      html_tag = "p";
    }
  }

  return html_tag;
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

std::u16string GetTextContent(ui::AXNode* ax_node, bool is_docs) {
  // For Google Docs, because the content is rendered in canvas, we distill
  // text from the "Annotated Canvas"
  // (https://sites.google.com/corp/google.com/docs-canvas-migration/home)
  // instead of the HTML.
  if (is_docs) {
    // With 'Annotated Canvas', text is stored within the aria-labels of SVG
    // elements. To retrieve this text, we need to access the 'name' attribute
    // of these elements.
    if ((ax_node->GetTextContentUTF16()).empty()) {
      std::u16string nodeText = GetNameAttributeText(ax_node);
      if (!nodeText.empty()) {
        // Add a space between the text of two annotated canvas elements.
        // Otherwise, there is no space separating two lines of text.
        return nodeText + u" ";
      }
    } else {
      // We ignore all text in the HTML. These text are either from comments or
      // from off-screen divs that contain hidden information information that
      // only is intended for screen readers and braille support. These are not
      // actual text in the doc.
      // TODO(b/324143642): Reading Mode handles Doc comments.
      if (ax_node->GetRole() == ax::mojom::Role::kStaticText) {
        return u"";
      }
    }
  }
  return ax_node->GetTextContentUTF16();
}

std::u16string GetNameAttributeText(ui::AXNode* ax_node) {
  DCHECK(ax_node);
  std::u16string node_text;
  if (ax_node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    node_text =
        ax_node->GetString16Attribute(ax::mojom::StringAttribute::kName);
  }

  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (node_text.empty()) {
      node_text = GetNameAttributeText(it.get());
    } else {
      node_text += u" " + GetNameAttributeText(it.get());
    }
  }
  return node_text;
}

}  // namespace a11y
