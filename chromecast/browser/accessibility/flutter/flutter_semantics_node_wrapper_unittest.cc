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
using ::gallium::castos::Rect;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Return;
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
  void SetUp() override { Reset(); }

  SemanticsNode* CreateNewSemanticsNode(int32_t node_id = kNodeId) {
    SemanticsNode* node = event_.add_node_data();
    node->set_node_id(node_id);
    return node;
  }

  SemanticsNode* CreateNewRootChildSemanticsNode(int32_t child_node_id = 1) {
    SemanticsNode* child_semantics_node;
    FlutterSemanticsNodeWrapper* child_node;
    CreateChildSemanticsNode(semantics_node_, node_.get(), child_semantics_node,
                             child_node, child_node_id);
    return child_semantics_node;
  }

  void CreateChildSemanticsNode(SemanticsNode* parent_semantics_node,
                                FlutterSemanticsNodeWrapper* parent_node,
                                SemanticsNode*& child_semantics_node,
                                FlutterSemanticsNodeWrapper*& child_node,
                                int32_t child_node_id,
                                const std::string& node_name = {}) {
    child_semantics_node = CreateNewSemanticsNode(child_node_id);
    if (!node_name.empty()) {
      child_semantics_node->set_label(node_name);
    }
    parent_semantics_node->add_child_node_ids(child_node_id);
    other_nodes_[child_node_id] = std::make_unique<FlutterSemanticsNodeWrapper>(
        &ax_tree_source_, child_semantics_node);
    child_node = other_nodes_[child_node_id].get();

    EXPECT_CALL(ax_tree_source_, GetFromId(child_node_id))
        .WillRepeatedly(Return(other_nodes_[child_node_id].get()));
    EXPECT_CALL(ax_tree_source_, GetParent(other_nodes_[child_node_id].get()))
        .WillRepeatedly(Return(parent_node));
  }

  void Reset() {
    event_.Clear();
    other_nodes_.clear();
    semantics_node_ = CreateNewSemanticsNode();
    node_ = std::make_unique<FlutterSemanticsNodeWrapper>(&ax_tree_source_,
                                                          semantics_node_);
    ax_node_data_ = ui::AXNodeData();
    testing::Mock::VerifyAndClearExpectations(&ax_tree_source_);
    EXPECT_CALL(ax_tree_source_, GetFromId(kNodeId))
        .WillRepeatedly(Return(node_.get()));
    EXPECT_CALL(ax_tree_source_, GetParent(node_.get()))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(ax_tree_source_, GetRoot()).WillRepeatedly(Return(node_.get()));
  }

  ui::MockAXTreeSource ax_tree_source_;
  SemanticsNode* semantics_node_;
  static const int32_t kNodeId = 0;
  std::unique_ptr<FlutterSemanticsNodeWrapper> node_;
  std::unordered_map<int, std::unique_ptr<FlutterSemanticsNodeWrapper>>
      other_nodes_;
  ui::AXNodeData ax_node_data_;

 private:
  gallium::castos::OnAccessibilityEventRequest event_;
};

TEST_F(FlutterSemanticsNodeWrapperTest, GetId) {
  semantics_node_->set_node_id(1);
  EXPECT_EQ(node_->GetId(), 1);
}

TEST_F(FlutterSemanticsNodeWrapperTest, GetBounds) {
  EXPECT_EQ(node_->GetBounds(), gfx::Rect(0, 0, 0, 0));

  Rect* bounds = semantics_node_->mutable_bounds_in_screen();
  bounds->set_left(100);
  bounds->set_top(200);
  bounds->set_right(300);
  bounds->set_bottom(400);
  EXPECT_EQ(node_->GetBounds(), gfx::Rect(100, 200, 200, 200));
}

TEST_F(FlutterSemanticsNodeWrapperTest, IsVisibleToUser) {
  EXPECT_EQ(node_->IsVisibleToUser(), true);
  semantics_node_->mutable_boolean_properties()->set_is_hidden(true);
  EXPECT_EQ(node_->IsVisibleToUser(), false);

  Reset();
  semantics_node_->mutable_boolean_properties()->set_is_hidden(false);
  EXPECT_EQ(node_->IsVisibleToUser(), true);
}

TEST_F(FlutterSemanticsNodeWrapperTest, IsFocused) {
  EXPECT_EQ(node_->IsFocused(), false);
  semantics_node_->mutable_boolean_properties()->set_is_focused(true);
  EXPECT_EQ(node_->IsFocused(), true);

  Reset();
  semantics_node_->mutable_boolean_properties()->set_is_focused(false);
  EXPECT_EQ(node_->IsFocused(), false);
}

TEST_F(FlutterSemanticsNodeWrapperTest, IsLiveRegion) {
  EXPECT_EQ(node_->IsLiveRegion(), false);
  semantics_node_->mutable_boolean_properties()->set_is_live_region(true);
  EXPECT_EQ(node_->IsLiveRegion(), true);

  Reset();
  semantics_node_->mutable_boolean_properties()->set_is_live_region(false);
  EXPECT_EQ(node_->IsLiveRegion(), false);
}

TEST_F(FlutterSemanticsNodeWrapperTest, HasScopesRoute) {
  EXPECT_EQ(node_->HasScopesRoute(), false);
  semantics_node_->mutable_boolean_properties()->set_scopes_route(true);
  EXPECT_EQ(node_->HasScopesRoute(), true);

  Reset();
  semantics_node_->mutable_boolean_properties()->set_scopes_route(false);
  EXPECT_EQ(node_->HasScopesRoute(), false);
}

TEST_F(FlutterSemanticsNodeWrapperTest, HasNamesRoute) {
  EXPECT_EQ(node_->HasNamesRoute(), false);
  semantics_node_->mutable_boolean_properties()->set_names_route(true);
  EXPECT_EQ(node_->HasNamesRoute(), true);

  Reset();
  semantics_node_->mutable_boolean_properties()->set_names_route(false);
  EXPECT_EQ(node_->HasNamesRoute(), false);
}

TEST_F(FlutterSemanticsNodeWrapperTest, IsRapidChangingSlider) {
  EXPECT_EQ(node_->IsRapidChangingSlider(), false);

  semantics_node_->mutable_action_properties()->set_scroll_up(true);
  semantics_node_->set_scroll_extent_min(0);
  semantics_node_->set_scroll_extent_max(10);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);

  Reset();
  semantics_node_->mutable_action_properties()->set_scroll_down(true);
  semantics_node_->set_scroll_extent_min(0);
  semantics_node_->set_scroll_extent_max(10);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);

  Reset();
  semantics_node_->mutable_action_properties()->set_scroll_left(true);
  semantics_node_->set_scroll_extent_min(0);
  semantics_node_->set_scroll_extent_max(10);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);

  Reset();
  semantics_node_->mutable_action_properties()->set_scroll_right(true);
  semantics_node_->set_scroll_extent_min(0);
  semantics_node_->set_scroll_extent_max(10);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);

  Reset();
  semantics_node_->mutable_action_properties()->set_scroll_down(true);
  semantics_node_->set_scroll_extent_min(10);
  semantics_node_->set_scroll_extent_max(10);
  // scroll_extent_min is not less than scroll_extent_max
  EXPECT_EQ(node_->IsRapidChangingSlider(), false);

  Reset();
  semantics_node_->mutable_action_properties()->set_increase(true);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);

  Reset();
  semantics_node_->mutable_action_properties()->set_decrease(true);
  EXPECT_EQ(node_->IsRapidChangingSlider(), true);
}

TEST_F(FlutterSemanticsNodeWrapperTest, LabelHintAndValue) {
  EXPECT_EQ(node_->HasLabelHint(), false);
  EXPECT_EQ(node_->GetLabelHint(), "");
  EXPECT_EQ(node_->HasValue(), false);
  EXPECT_EQ(node_->GetValue(), "");

  const std::string name = "dummy";
  semantics_node_->set_label(name);
  EXPECT_EQ(node_->HasLabelHint(), true);
  EXPECT_EQ(node_->GetLabelHint(), name);

  Reset();
  semantics_node_->set_hint(name);
  EXPECT_EQ(node_->HasLabelHint(), true);
  EXPECT_EQ(node_->GetLabelHint(), name);

  Reset();
  semantics_node_->set_value(name);
  EXPECT_EQ(node_->HasValue(), true);
  EXPECT_EQ(node_->GetValue(), name);
}

TEST_F(FlutterSemanticsNodeWrapperTest, CanBeAccessibilityFocused) {
  // A node with a non-generic role and:
  // actionable nodes or top level scrollables with a name.
  EXPECT_EQ(node_->CanBeAccessibilityFocused(), false);

  // Not a generic container.
  semantics_node_->mutable_boolean_properties()->set_is_image(true);
  // Actionable.
  semantics_node_->mutable_action_properties()->set_tap(true);
  EXPECT_EQ(node_->CanBeAccessibilityFocused(), true);

  Reset();
  // Not a generic container.
  semantics_node_->mutable_boolean_properties()->set_is_image(true);
  // scrollable.
  semantics_node_->mutable_action_properties()->set_scroll_up(true);
  EXPECT_EQ(node_->CanBeAccessibilityFocused(), false);
  // Set a name.
  semantics_node_->set_label("dummy");
  EXPECT_EQ(node_->CanBeAccessibilityFocused(), true);
}

TEST_F(FlutterSemanticsNodeWrapperTest, GetChildren) {
  // For following structure:
  //     0
  //    / \
  //   1   2
  //  /
  // 3
  // GetChildren() of node 0 should returns node 1 and 2.
  SemanticsNode *child_semantics_node1, *child_semantics_node2,
      *child_semantics_node3;
  FlutterSemanticsNodeWrapper *child_node1, *child_node2, *child_node3;

  CreateChildSemanticsNode(semantics_node_, node_.get(), child_semantics_node1,
                           child_node1, 1, "node1");
  CreateChildSemanticsNode(semantics_node_, node_.get(), child_semantics_node2,
                           child_node2, 2, "node2");
  CreateChildSemanticsNode(child_semantics_node1, child_node1,
                           child_semantics_node3, child_node3, 3, "node3");

  std::vector<FlutterSemanticsNode*> children1;
  node_->GetChildren(&children1);
  EXPECT_THAT(children1, Contains(child_node1));
  EXPECT_THAT(children1, Contains(child_node2));
  EXPECT_THAT(children1, Not(Contains(child_node3)));
  EXPECT_THAT(children1, Not(Contains(node_.get())));

  std::vector<FlutterSemanticsNode*> children2;
  child_node1->GetChildren(&children2);
  EXPECT_THAT(children2, Contains(child_node3));
  EXPECT_THAT(children2, Not(Contains(child_node1)));
  EXPECT_THAT(children2, Not(Contains(child_node2)));
  EXPECT_THAT(children2, Not(Contains(node_.get())));

  std::vector<FlutterSemanticsNode*> children3;
  child_node2->GetChildren(&children3);
  EXPECT_EQ(children3.empty(), true);
}

TEST_F(FlutterSemanticsNodeWrapperTest, PopulateAXRole) {
  {
    semantics_node_->mutable_boolean_properties()->set_is_text_field(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kTextField);
  }

  {
    Reset();
    semantics_node_->mutable_boolean_properties()->set_is_header(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kHeading);
  }

  {
    Reset();
    semantics_node_->mutable_boolean_properties()->set_is_image(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kImage);

    // Node with children should not be marked as image.
    CreateNewRootChildSemanticsNode();
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kImage);
  }

  {
    Reset();
    semantics_node_->mutable_action_properties()->set_increase(1);
    semantics_node_->mutable_action_properties()->set_decrease(1);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kSlider);
  }

  {
    // Nodes to be considered buttons:
    // 1. Have labels, have taps and no children.
    Reset();
    semantics_node_->mutable_action_properties()->set_tap(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kButton);
    semantics_node_->set_label("dummy");
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kButton);

    CreateNewRootChildSemanticsNode();
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kButton);

    // 2. Have labels, is_button == true and no actionable children.
    Reset();
    semantics_node_->mutable_boolean_properties()->set_is_button(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kButton);
    semantics_node_->set_label("dummy");
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kButton);

    // Create child node.
    SemanticsNode* child_semantics_node = CreateNewRootChildSemanticsNode();

    // child node is not actionable.
    child_semantics_node->mutable_action_properties()->set_tap(false);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kButton);

    // child node is actionable.
    child_semantics_node->mutable_action_properties()->set_tap(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kButton);
  }

  {
    Reset();
    semantics_node_->mutable_boolean_properties()
        ->set_is_in_mutually_exclusive_group(true);
    semantics_node_->mutable_boolean_properties()->set_has_checked_state(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kRadioButton);

    semantics_node_->mutable_boolean_properties()
        ->set_is_in_mutually_exclusive_group(false);
    semantics_node_->mutable_boolean_properties()->set_has_checked_state(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kCheckBox);
  }

  {
    Reset();
    semantics_node_->mutable_boolean_properties()->set_has_toggled_state(true);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kSwitch);
  }

  {
    Reset();
    semantics_node_->set_label("dummy");
    semantics_node_->mutable_action_properties()->set_tap(false);
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kStaticText);
    // kStaticText should not contain children.
    CreateNewRootChildSemanticsNode();
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_NE(ax_node_data_.role, ax::mojom::Role::kStaticText);
  }

  {
    Reset();
    semantics_node_->set_label("dummy");
    CreateNewRootChildSemanticsNode();
    node_->PopulateAXRole(&ax_node_data_);
    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kHeader);
  }

  {
    // For following structure:
    //     0
    //    / \
    //   1   2
    //  /     \
    // 3       4
    // If node 0 has scroll_children, node 1 and 2 are not actionable, one of
    // node 3 is 4 are actionable.
    // then node0 is consider a kList, node2 and node3 are consider
    // kListBoxOption.
    Reset();

    SemanticsNode *child_semantics_node1, *child_semantics_node2,
        *child_semantics_node3, *child_semantics_node4;
    FlutterSemanticsNodeWrapper *child_node1, *child_node2, *child_node3,
        *child_node4;
    ui::AXNodeData node_data1, node_data2, node_data3, node_data4;

    CreateChildSemanticsNode(semantics_node_, node_.get(),
                             child_semantics_node1, child_node1, 1, "node1");
    CreateChildSemanticsNode(semantics_node_, node_.get(),
                             child_semantics_node2, child_node2, 2, "node2");
    CreateChildSemanticsNode(child_semantics_node1, child_node1,
                             child_semantics_node3, child_node3, 3, "node3");
    CreateChildSemanticsNode(child_semantics_node2, child_node2,
                             child_semantics_node4, child_node4, 4, "node4");

    semantics_node_->set_scroll_children(2);
    child_semantics_node1->mutable_action_properties()->set_tap(false);
    child_semantics_node2->mutable_action_properties()->set_tap(false);
    child_semantics_node3->mutable_action_properties()->set_tap(true);
    child_semantics_node4->mutable_action_properties()->set_tap(false);
    node_->PopulateAXRole(&ax_node_data_);
    child_node1->PopulateAXRole(&node_data1);
    child_node2->PopulateAXRole(&node_data2);
    child_node3->PopulateAXRole(&node_data3);
    child_node4->PopulateAXRole(&node_data4);

    EXPECT_EQ(ax_node_data_.role, ax::mojom::Role::kList);
    EXPECT_EQ(node_data1.role, ax::mojom::Role::kListBoxOption);
    EXPECT_EQ(node_data2.role, ax::mojom::Role::kListBoxOption);
    EXPECT_NE(node_data3.role, ax::mojom::Role::kListBoxOption);
    EXPECT_NE(node_data4.role, ax::mojom::Role::kListBoxOption);
  }
}

TEST_F(FlutterSemanticsNodeWrapperTest, DisabledNode) {
  BooleanProperties* boolean_properties =
      semantics_node_->mutable_boolean_properties();

  // Test has_enabled_state = false, is_enabled = false
  boolean_properties->set_has_enabled_state(false);
  boolean_properties->set_is_enabled(false);
  node_->PopulateAXState(&ax_node_data_);
  EXPECT_EQ(ax_node_data_.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = false, is_enabled = true
  boolean_properties->set_has_enabled_state(false);
  boolean_properties->set_is_enabled(true);
  node_->PopulateAXState(&ax_node_data_);
  EXPECT_EQ(ax_node_data_.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = true, is_enabled = true
  boolean_properties->set_has_enabled_state(true);
  boolean_properties->set_is_enabled(true);
  node_->PopulateAXState(&ax_node_data_);
  EXPECT_EQ(ax_node_data_.GetRestriction(), ax::mojom::Restriction::kNone);

  // Test has_enabled_state = true, is_enabled = false
  boolean_properties->set_has_enabled_state(true);
  boolean_properties->set_is_enabled(false);
  node_->PopulateAXState(&ax_node_data_);
  EXPECT_EQ(ax_node_data_.GetRestriction(), ax::mojom::Restriction::kDisabled);
}

}  // namespace accessibility
}  // namespace chromecast
