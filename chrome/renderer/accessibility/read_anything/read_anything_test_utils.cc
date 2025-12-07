// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_test_utils.h"

#include "base/strings/string_number_conversions.h"

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

ui::AXNodeData TextNode(ui::AXNodeID id, const std::u16string& text_content) {
  ui::AXNodeData node = TextNode(id);
  node.SetNameChecked(text_content);
  return node;
}

ui::AXNodeData TextNode(ui::AXNodeID id) {
  ui::AXNodeData node;
  node.id = id;
  node.role = ax::mojom::Role::kStaticText;
  return node;
}

ui::AXNodeData TextNodeWithTextFromId(ui::AXNodeID id) {
  return TextNode(id, base::NumberToString16(id));
}

ui::AXNodeData ExplicitlyEmptyTextNode(ui::AXNodeID id) {
  ui::AXNodeData node = TextNode(id);
  node.SetNameExplicitlyEmpty();
  return node;
}

ui::AXNodeData ImageNode(ui::AXNodeID id, const std::string& src) {
  ui::AXNodeData node;
  node.id = id;
  node.role = ax::mojom::Role::kImage;
  node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, src);
  return node;
}

ui::AXNodeData LinkNode(ui::AXNodeID id, const std::string& url) {
  ui::AXNodeData node;
  node.id = id;
  node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  return node;
}

ui::AXNodeData GenericContainerNode(ui::AXNodeID id) {
  ui::AXNodeData node;
  node.id = id;
  node.role = ax::mojom::Role::kGenericContainer;
  return node;
}

ui::AXNodeData SuperscriptNode(ui::AXNodeID id,
                               const std::u16string& text_content) {
  ui::AXNodeData node = TextNode(id, text_content);
  node.SetTextPosition(ax::mojom::TextPosition::kSuperscript);
  return node;
}

std::vector<ui::AXTreeUpdate> CreateSimpleUpdateList(
    std::vector<ui::AXNodeID> child_ids,
    ui::AXTreeID tree_id) {
  std::vector<ui::AXTreeUpdate> updates;
  for (ui::AXNodeID id = 5; id < 8; ++id) {
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

testing::Matcher<ReadAloudTextSegment> TextSegmentMatcher(TextRange range) {
  return testing::AllOf(
      ::testing::Field(&ReadAloudTextSegment::id, ::testing::Eq(range.id)),
      ::testing::Field(&ReadAloudTextSegment::text_start,
                       ::testing::Eq(range.start)),
      ::testing::Field(&ReadAloudTextSegment::text_end,
                       ::testing::Eq(range.end)));
}

base::File GetValidModelFile() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path = source_root_dir.AppendASCII("chrome")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("accessibility")
                                       .AppendASCII("phrase_segmentation")
                                       .AppendASCII("model.tflite");
  base::File file(model_file_path,
                  (base::File::FLAG_OPEN | base::File::FLAG_READ));
  return file;
}

base::File GetInvalidModelFile() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("model_file.tflite");
  base::File file(file_path, (base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE |
                              base::File::FLAG_CAN_DELETE_ON_CLOSE));
  EXPECT_EQ(5u, file.WriteAtCurrentPos(base::byte_span_from_cstring("12345")));
  return file;
}

}  // namespace test
