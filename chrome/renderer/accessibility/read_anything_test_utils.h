// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_

#include <string>

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"

namespace test {

void SetUpdateTreeID(ui::AXTreeUpdate* update, ui::AXTreeID tree_id);
std::unique_ptr<ui::AXTreeUpdate> CreateInitialUpdate();

std::vector<ui::AXTreeUpdate> CreateSimpleUpdateList(std::vector<int> child_ids,
                                                     ui::AXTreeID tree_id);

// Helpers for creating AXNodeData objects to be used in testing.
ui::AXNodeData TextNode(int id, std::u16string text_content);
ui::AXNodeData TextNode(int id);
ui::AXNodeData TextNodeWithTextFromId(int id);
ui::AXNodeData ExplicitlyEmptyTextNode(int id);
ui::AXNodeData LinkNode(int id, std::string url);
ui::AXNodeData GenericContainerNode(int id);
ui::AXNodeData SuperscriptNode(int id, std::u16string text_content);

}  // namespace test

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_
