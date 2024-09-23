// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_test_utils.h"

namespace test {

void SetUpdateTreeID(ui::AXTreeUpdate* update, ui::AXTreeID tree_id) {
  ui::AXTreeData tree_data;
  tree_data.tree_id = tree_id;
  update->has_tree_data = true;
  update->tree_data = tree_data;
}

std::unique_ptr<ui::AXTreeUpdate> CreateInitialUpdate() {
  std::unique_ptr<ui::AXTreeUpdate> snapshot =
      std::make_unique<ui::AXTreeUpdate>();
  ui::AXNodeData node1;
  node1.id = 2;

  ui::AXNodeData node2;
  node2.id = 3;

  ui::AXNodeData node3;
  node3.id = 4;

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id, node3.id};
  snapshot->root_id = root.id;
  snapshot->nodes = {root, node1, node2, node3};

  return snapshot;
}

ui::AXNodeData TextNode(int id, std::u16string text_content) {
  ui::AXNodeData node = TextNode(id);
  node.SetNameChecked(text_content);
  return node;
}

ui::AXNodeData TextNode(int id) {
  ui::AXNodeData node;
  node.id = id;
  node.role = ax::mojom::Role::kStaticText;
  return node;
}

ui::AXNodeData TextNodeWithTextFromId(int id) {
  return TextNode(id, base::NumberToString16(id));
}

ui::AXNodeData ExplicitlyEmptyTextNode(int id) {
  ui::AXNodeData node = TextNode(id);
  node.SetNameExplicitlyEmpty();
  return node;
}

ui::AXNodeData LinkNode(int id, std::string url) {
  ui::AXNodeData node;
  node.id = id;
  node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  return node;
}

ui::AXNodeData GenericContainerNode(int id) {
  ui::AXNodeData node;
  node.id = id;
  node.role = ax::mojom::Role::kGenericContainer;
  return node;
}

ui::AXNodeData SuperscriptNode(int id, std::u16string text_content) {
  ui::AXNodeData node = TextNode(id, text_content);
  node.SetTextPosition(ax::mojom::TextPosition::kSuperscript);
  return node;
}

std::vector<ui::AXTreeUpdate> CreateSimpleUpdateList(std::vector<int> child_ids,
                                                     ui::AXTreeID tree_id) {
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update, tree_id);
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node = test::TextNodeWithTextFromId(id);
    update.root_id = root.id;
    update.nodes = {root, node};
    updates.push_back(update);
  }
  return updates;
}

}  // namespace test
