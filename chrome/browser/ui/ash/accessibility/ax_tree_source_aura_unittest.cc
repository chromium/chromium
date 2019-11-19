// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::AXAuraObjCache;
using views::AXAuraObjWrapper;
using views::AXTreeSourceViews;
using views::Textfield;
using views::View;
using views::Widget;

using AuraAXTreeSerializer = ui::
    AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>;

// Helper to count the number of nodes in a tree.
size_t GetSize(AXAuraObjWrapper* tree) {
  size_t count = 1;

  std::vector<AXAuraObjWrapper*> out_children;
  tree->GetChildren(&out_children);

  for (size_t i = 0; i < out_children.size(); ++i)
    count += GetSize(out_children[i]);

  return count;
}

// Tests integration of AXTreeSourceViews with AXRootObjWrapper.
// TODO(jamescook): Move into //ui/views/accessibility and combine with
// AXTreeSourceViewsTest.
class AXTreeSourceAuraTest : public ChromeViewsTestBase {
 public:
  AXTreeSourceAuraTest() {}
  ~AXTreeSourceAuraTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = new Widget();
    Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = GetContext();
    widget_->Init(std::move(init_params));

    content_ = new View();
    widget_->SetContentsView(content_);

    textfield_ = new Textfield();
    textfield_->SetText(base::ASCIIToUTF16("Value"));
    content_->AddChildView(textfield_);
    widget_->Show();
  }

  void TearDown() override {
    // ViewsTestBase requires all Widgets to be closed before shutdown.
    widget_->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  Widget* widget_;
  View* content_;
  Textfield* textfield_;
  AXAuraObjCache cache_;
  // A simulated desktop root with no delegate.
  AXRootObjWrapper root_wrapper_{nullptr, &cache_};

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceAuraTest);
};

TEST_F(AXTreeSourceAuraTest, Accessors) {
  // Focus the textfield so the cursor does not disappear.
  textfield_->RequestFocus();

  AXTreeSourceViews ax_tree(&root_wrapper_, ui::AXTreeID::CreateNewAXTreeID(),
                            &cache_);
  ASSERT_TRUE(ax_tree.GetRoot());

  // ID's should be > 0.
  ASSERT_GE(ax_tree.GetRoot()->GetUniqueId(), 1);

  // Grab the content view directly from cache to avoid walking down the tree.
  AXAuraObjWrapper* content = cache_.GetOrCreate(content_);
  std::vector<AXAuraObjWrapper*> content_children;
  ax_tree.GetChildren(content, &content_children);
  ASSERT_EQ(1U, content_children.size());

  // Walk down to the text field and assert it is what we expect.
  AXAuraObjWrapper* textfield = content_children[0];
  AXAuraObjWrapper* cached_textfield = cache_.GetOrCreate(textfield_);
  ASSERT_EQ(cached_textfield, textfield);
  std::vector<AXAuraObjWrapper*> textfield_children;
  ax_tree.GetChildren(textfield, &textfield_children);
  // The textfield has an extra child in Harmony, the focus ring.
  const size_t expected_children = 2;
  ASSERT_EQ(expected_children, textfield_children.size());

  ASSERT_EQ(content, textfield->GetParent());

  ASSERT_NE(textfield->GetUniqueId(), ax_tree.GetRoot()->GetUniqueId());

  // Try walking up the tree to the root.
  AXAuraObjWrapper* test_root = NULL;
  for (AXAuraObjWrapper* root_finder = ax_tree.GetParent(content); root_finder;
       root_finder = ax_tree.GetParent(root_finder))
    test_root = root_finder;
  ASSERT_EQ(ax_tree.GetRoot(), test_root);
}

TEST_F(AXTreeSourceAuraTest, DoDefault) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::AXTreeID::CreateNewAXTreeID(),
                            &cache_);

  // Grab a wrapper to |DoDefault| (click).
  AXAuraObjWrapper* textfield_wrapper = cache_.GetOrCreate(textfield_);

  // Click and verify focus.
  ASSERT_FALSE(textfield_->HasFocus());
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = textfield_wrapper->GetUniqueId();
  textfield_wrapper->HandleAccessibleAction(action_data);
  ASSERT_TRUE(textfield_->HasFocus());
}

TEST_F(AXTreeSourceAuraTest, Focus) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::AXTreeID::CreateNewAXTreeID(),
                            &cache_);

  // Grab a wrapper to focus.
  AXAuraObjWrapper* textfield_wrapper = cache_.GetOrCreate(textfield_);

  // Focus and verify.
  ASSERT_FALSE(textfield_->HasFocus());
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = textfield_wrapper->GetUniqueId();
  textfield_wrapper->HandleAccessibleAction(action_data);
  ASSERT_TRUE(textfield_->HasFocus());
}

TEST_F(AXTreeSourceAuraTest, Serialize) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::AXTreeID::CreateNewAXTreeID(),
                            &cache_);
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  ui::AXTreeUpdate out_update;

  // This is the initial serialization.
  ax_serializer.SerializeChanges(ax_tree.GetRoot(), &out_update);

  // The update should just be the desktop node.
  ASSERT_EQ(1U, out_update.nodes.size());

  // Try removing some child views and re-adding which should fire some events.
  content_->RemoveAllChildViews(false /* delete_children */);
  content_->AddChildView(textfield_);

  // Grab the textfield since serialization only walks up the tree (not down
  // from root).
  AXAuraObjWrapper* textfield_wrapper = cache_.GetOrCreate(textfield_);

  // Now, re-serialize.
  ui::AXTreeUpdate out_update2;
  ax_serializer.SerializeChanges(textfield_wrapper, &out_update2);

  size_t node_count = out_update2.nodes.size();

  // We should have far more updates this time around.
  EXPECT_GE(node_count, 7U);

  int text_field_update_index = -1;
  for (size_t i = 0; i < node_count; ++i) {
    if (textfield_wrapper->GetUniqueId() == out_update2.nodes[i].id)
      text_field_update_index = i;
  }

  ASSERT_NE(-1, text_field_update_index);
  ASSERT_EQ(ax::mojom::Role::kTextField,
            out_update2.nodes[text_field_update_index].role);
}

TEST_F(AXTreeSourceAuraTest, SerializeWindowSetsClipsChildren) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::AXTreeID::CreateNewAXTreeID(),
                            &cache_);
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  AXAuraObjWrapper* widget_wrapper = cache_.GetOrCreate(widget_);
  ui::AXNodeData node_data;
  ax_tree.SerializeNode(widget_wrapper, &node_data);
  EXPECT_EQ(ax::mojom::Role::kWindow, node_data.role);
  bool clips_children = false;
  EXPECT_TRUE(node_data.GetBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, &clips_children));
  EXPECT_TRUE(clips_children);
}
