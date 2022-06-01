// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

const int kMaxChildInTemplate = 3;

// A dummy tree node definition.
typedef struct {
  int node_id;
  int child_count;
  int child_ids[kMaxChildInTemplate];
} NodeTemplate;

ui::AXTreeUpdate CreateAXTreeUpdateFromTemplate(int root_id,
                                                NodeTemplate* nodes_template,
                                                int nodes_count) {
  ui::AXTreeUpdate update;
  update.root_id = root_id;

  for (int i = 0; i < nodes_count; i++) {
    ui::AXNodeData node;
    node.id = nodes_template[i].node_id;
    for (int j = 0; j < nodes_template[i].child_count; j++)
      node.child_ids.push_back(nodes_template[i].child_ids[j]);
    update.nodes.push_back(node);
  }
  return update;
}

int GetAxNodeID(const ::screenai::UiElement& ui_element) {
  for (const auto& attribute : ui_element.attributes()) {
    if (attribute.name() == "/axnode/node_id")
      return attribute.int_value();
  }
  return static_cast<int>(ui::kInvalidAXNodeID);
}

}  // namespace

namespace screen_ai {

using ProtoConvertorTest = testing::Test;

// Tests if the given tree is properly traversed and new ids are assigned.
TEST_F(ProtoConvertorTest, PreOrderTreeGeneration) {
  // Input Tree:
  // +-- 1
  //     +-- 2
  //         +-- 7
  //         +-- 8
  //             +-- 3
  //     +-- 4
  //         +-- 5
  //         +-- 6
  //         +-- 9
  //     +-- -20

  // Input tree is added in shuffled order to avoid order assumption.
  NodeTemplate input_tree[] = {
      {4, 3, {5, 6, 9}}, {1, 3, {2, 4, -20}}, {7, 0, {}}, {8, 1, {3}},
      {6, 0, {}},        {5, 0, {}},          {3, 0, {}}, {2, 2, {7, 8}},
      {9, 0, {}},        {-20, 0, {}}};
  const int nodes_count = sizeof(input_tree) / sizeof(NodeTemplate);

  // Expected order of nodes in the output.
  int expected_order[] = {1, 2, 7, 8, 3, 4, 5, 6, 9, -20};

  // Create the tree, convert it, and decode from proto.
  ui::AXTreeUpdate tree_update =
      CreateAXTreeUpdateFromTemplate(1, input_tree, nodes_count);
  std::string serialized_proto = Screen2xSnapshotToViewHierarchy(tree_update);
  screenai::ViewHierarchy view_hierarchy;
  EXPECT_TRUE(view_hierarchy.ParseFromString(serialized_proto));

  // Verify.
  EXPECT_EQ(view_hierarchy.ui_elements().size(), nodes_count);
  for (int i = 0; i < nodes_count; i++) {
    const screenai::UiElement& ui_element = view_hierarchy.ui_elements(i);

    // Expect node to be correctly re-ordered.
    EXPECT_EQ(expected_order[i], GetAxNodeID(ui_element));

    // Expect node at index 'i' has id 'i'
    EXPECT_EQ(ui_element.id(), i);
  }
}

}  // namespace screen_ai
