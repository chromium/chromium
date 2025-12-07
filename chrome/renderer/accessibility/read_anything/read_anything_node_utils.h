// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_NODE_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_NODE_UTILS_H_

#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/ax_node.h"

// Utilities for getting html or text info about ax nodes.  These are used by
// ReadAnythingAppModel and ReadAloudAppModel.
namespace a11y {

// Max number of characters to display in a reading mode line of text.
static constexpr int kMaxLineWidth = 60;

// Min number of characters needed to return the text preceding a given node.
// Used for text matching when the user selects text in the main panel in order
// to select the corresponding text in the reading mode panel. Determined as a
// balance between having enough characters for uniqueness vs risking not
// finding any node of that length nearby the given node. Should be updated if
// a better value is found.
static constexpr int kMinPrefixLength = 40;

// Returns whether the given node represents a superscript.
bool IsSuperscript(const ui::AXNode* ax_node);

// Returns whether the given node is a text node displayed by read anything.
bool IsTextForReadAnything(const ui::AXNode* ax_node,
                           bool is_pdf,
                           bool is_docs);

// Returns whether the given node should be ignored by reading mode.
bool IsIgnored(const ui::AXNode* const ax_node, bool is_pdf);

// Returns the html tag for the given node.
std::string GetHtmlTag(const ui::AXNode* ax_node, bool is_pdf, bool is_docs);

// Returns the html tag for the given node which is inside a pdf.
std::string GetHtmlTagForPDF(const ui::AXNode* ax_node,
                             const std::string& html_tag);

// Returns the heading html tag for the given node which is inside a pdf.
std::string GetHeadingHtmlTagForPDF(const ui::AXNode* ax_node,
                                    const std::string& html_tag);

// Returns the alt text for the given node.
std::string GetAltText(const ui::AXNode* ax_node);

// Returns the text content for the given node. This needs to be a wrapper
// instead of getting text from the node directly because the text content
// is different if in Google Docs or pdfs.
std::u16string GetTextContent(const ui::AXNode* ax_node,
                              bool is_pdf,
                              bool is_docs);

// Returns the text content that precedes the given node and is at least length
// kMinPrefixLength. This could return the text of the parent node, which would
// include the text of the given node. Used for text matching when the user
// selects text in the main panel in order to select the corresponding text in
// the reading mode panel.
std::u16string GetPrefixText(const ui::AXNode* ax_node,
                             bool is_pdf,
                             bool is_docs);

std::u16string GetNameAttributeText(const ui::AXNode* ax_node);
}  // namespace a11y

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_NODE_UTILS_H_
