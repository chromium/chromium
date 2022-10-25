// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai {

TEST(ScreenAIAXTreeSerializerTest, Serialize) {
  ui::AXNodeData root_data;
  ui::AXNodeData page_1_data;
  ui::AXNodeData page_1_text_data;
  ui::AXNodeData page_2_data;
  ui::AXNodeData page_2_text_data;

  root_data.id = 1;
  root_data.role = ax::mojom::Role::kPdfRoot;

  page_1_data.id = 2;
  page_1_data.role = ax::mojom::Role::kRegion;
  page_1_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);

  page_1_text_data.id = 3;
  page_1_text_data.role = ax::mojom::Role::kStaticText;
  page_1_text_data.SetName("some text on page 1");
  page_1_text_data.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  page_1_data.child_ids = {page_1_text_data.id};

  page_2_data.id = 4;
  page_2_data.role = ax::mojom::Role::kRegion;
  page_2_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);

  page_2_text_data.id = 5;
  page_2_text_data.role = ax::mojom::Role::kStaticText;
  page_2_text_data.SetName("some text on page 2");
  page_2_text_data.AddIntAttribute(
      ax::mojom::IntAttribute::kTextStyle,
      static_cast<int32_t>(ax::mojom::TextStyle::kBold));
  page_2_data.child_ids = {page_2_text_data.id};

  root_data.child_ids = {page_1_data.id, page_2_data.id};

  ScreenAIAXTreeSerializer serializer(
      /* parent_tree_id */ ui::AXTreeID::CreateNewAXTreeID(),
      {root_data, page_1_data, page_1_text_data, page_2_data,
       page_2_text_data});
  ui::AXTreeUpdate update = serializer.Serialize();

  EXPECT_NE(update.tree_data.tree_id, ui::AXTreeIDUnknown());
  EXPECT_NE(update.tree_data.parent_tree_id, ui::AXTreeIDUnknown());
  // After checking the tree IDs, reset them to allow testing using a constant
  // expectation string, since tree IDs are dynamically generated.
  update.tree_data.tree_id = ui::AXTreeIDUnknown();
  update.tree_data.parent_tree_id = ui::AXTreeIDUnknown();

  const std::string expected_update(
      "AXTreeUpdate tree data: title=Screen AI\n"
      "AXTreeUpdate: root id 1\nid=1 pdfRoot (0, 0)-(0, 0) child_ids=2,4\n  "
      "id=2 region (0, 0)-(0, 0) is_page_breaking_object=true child_ids=3\n    "
      "id=3 staticText (0, 0)-(0, 0) name_from=contents name=some text on page "
      "1 is_line_breaking_object=true\n  id=4 region (0, 0)-(0, 0) "
      "is_page_breaking_object=true child_ids=5\n    id=5 staticText (0, "
      "0)-(0, 0) name_from=contents name=some text on page 2\n");
  EXPECT_EQ(expected_update, update.ToString());
}

}  // namespace screen_ai
