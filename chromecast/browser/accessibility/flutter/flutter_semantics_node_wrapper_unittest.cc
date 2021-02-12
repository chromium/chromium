// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/flutter/flutter_semantics_node_wrapper.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

using chromecast::accessibility::FlutterSemanticsNode;
using gallium::castos::BooleanProperties;
using ::testing::StrictMock;

namespace ui {

class MockAXTreeSource : public AXTreeSource<FlutterSemanticsNode*> {
 public:
  MOCK_METHOD(bool, GetTreeData, (AXTreeData * data), (const, override));
  MOCK_METHOD(FlutterSemanticsNode*, GetRoot, (), (const, override));
  MOCK_METHOD(FlutterSemanticsNode*,
              GetFromId,
              (int32_t id),
              (const, override));
  MOCK_METHOD(int32_t, GetId, (FlutterSemanticsNode * node), (const, override));
  MOCK_METHOD(void,
              GetChildren,
              (FlutterSemanticsNode * node,
               std::vector<FlutterSemanticsNode*>* out_children),
              (const, override));
  MOCK_METHOD(FlutterSemanticsNode*,
              GetParent,
              (FlutterSemanticsNode * node),
              (const, override));
  MOCK_METHOD(bool, IsValid, (FlutterSemanticsNode * node), (const, override));
  MOCK_METHOD(bool,
              IsIgnored,
              (FlutterSemanticsNode * node),
              (const, override));
  MOCK_METHOD(bool,
              IsEqual,
              (FlutterSemanticsNode * node1, FlutterSemanticsNode* node2),
              (const, override));
  MOCK_METHOD(FlutterSemanticsNode*, GetNull, (), (const, override));
  MOCK_METHOD(void,
              SerializeNode,
              (FlutterSemanticsNode * node, AXNodeData* out_data),
              (const, override));
  MOCK_METHOD(std::string,
              GetDebugString,
              (FlutterSemanticsNode * node),
              (const, override));
  MOCK_METHOD(void, SerializerClearedNode, (int32_t node_id), (override));
};

}  // namespace ui

namespace chromecast {
namespace accessibility {

class FlutterSemanticsNodeWrapperTest : public testing::Test {
 public:
  FlutterSemanticsNodeWrapperTest() = default;
  FlutterSemanticsNodeWrapperTest(const FlutterSemanticsNodeWrapperTest&) =
      delete;
  ~FlutterSemanticsNodeWrapperTest() override {}
  FlutterSemanticsNodeWrapperTest& operator=(
      const FlutterSemanticsNodeWrapperTest&) = delete;

 protected:
  SemanticsNode* CreateNewSemanticsNode() { return event_.add_node_data(); }
  ui::MockAXTreeSource ax_tree_source_;

 private:
  gallium::castos::OnAccessibilityEventRequest event_;
};

TEST_F(FlutterSemanticsNodeWrapperTest, GetId) {
  SemanticsNode* semantics_node = CreateNewSemanticsNode();
  semantics_node->set_node_id(0);
  FlutterSemanticsNodeWrapper node(&ax_tree_source_, semantics_node);
  EXPECT_EQ(node.GetId(), 0);
}

TEST_F(FlutterSemanticsNodeWrapperTest, DisabledNode) {
  SemanticsNode* semantics_node = CreateNewSemanticsNode();
  semantics_node->set_node_id(0);

  BooleanProperties* boolean_properties =
      semantics_node->mutable_boolean_properties();
  boolean_properties->set_has_enabled_state(false);
  boolean_properties->set_is_enabled(false);

  // Test has_enabled_state = false, is_enabled = false
  FlutterSemanticsNodeWrapper node(&ax_tree_source_, semantics_node);
  ui::AXNodeData out_data;
  node.PopulateAXState(&out_data);
  EXPECT_EQ(out_data.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = false, is_enabled = true
  boolean_properties->set_has_enabled_state(false);
  boolean_properties->set_is_enabled(true);
  node.PopulateAXState(&out_data);
  EXPECT_EQ(out_data.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = true, is_enabled = false
  boolean_properties->set_has_enabled_state(true);
  boolean_properties->set_is_enabled(true);
  node.PopulateAXState(&out_data);
  EXPECT_EQ(out_data.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = true, is_enabled = true
  boolean_properties->set_has_enabled_state(true);
  boolean_properties->set_is_enabled(false);
  node.PopulateAXState(&out_data);
  EXPECT_EQ(out_data.GetRestriction(), ax::mojom::Restriction::kDisabled);
}

}  // namespace accessibility
}  // namespace chromecast
