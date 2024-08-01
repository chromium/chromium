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

ui::AXTreeUpdate& CreateInitialUpdate() {
  ui::AXTreeUpdate* snapshot = new ui::AXTreeUpdate();
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

  return *snapshot;
}

}  // namespace test
