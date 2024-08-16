// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_NODE_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_NODE_UTILS_H_

#include "ui/accessibility/ax_node.h"

// Utilities for getting html or text info about ax nodes.  These are used by
// ReadAnythingAppModel and ReadAloudAppModel.
namespace a11y {

// Returns whether the given node represents a superscript.
bool IsSuperscript(ui::AXNode* ax_node);

// Returns whether the given node is a text node displayed by read anything.
bool IsTextForReadAnything(ui::AXNode* ax_node, bool is_pdf, bool is_docs);

// Returns whether the given node is ignored when distilling content for read
// anything.
bool IsNodeIgnoredForReadAnything(ui::AXNode* ax_node, bool is_pdf);

// Returns the html tag for the given node.
std::string GetHtmlTag(ui::AXNode* ax_node, bool is_pdf, bool is_docs);

// Returns the html tag for the given node which is inside a pdf.
std::string GetHtmlTagForPDF(ui::AXNode* ax_node, const std::string& html_tag);

// Returns the heading html tag for the given node which is inside a pdf.
std::string GetHeadingHtmlTagForPDF(ui::AXNode* ax_node,
                                    const std::string& html_tag);

// Returns the node that was used for selection.
ui::AXNode* GetParentForSelection(ui::AXNode* ax_node);

// Returns the alt text for the given node.
std::string GetAltText(ui::AXNode* ax_node);

// Returns the image data url for the given node.
std::string GetImageDataUrl(ui::AXNode* ax_node);

// Returns the text content for the given node. This needs to be a wrapper
// instead of getting text from the node directly because the text content
// is different if in Google Docs
std::u16string GetTextContent(ui::AXNode* ax_node, bool is_docs);

std::u16string GetNameAttributeText(ui::AXNode* ax_node);
}  // namespace a11y

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_NODE_UTILS_H_
