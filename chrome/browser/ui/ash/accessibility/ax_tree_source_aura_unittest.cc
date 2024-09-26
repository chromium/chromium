// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
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

using AuraAXTreeSerializer =
    ui::AXTreeSerializer<views::AXAuraObjWrapper*,
                         std::vector<views::AXAuraObjWrapper*>,
                         ui::AXTreeUpdate*,
                         ui::AXTreeData*,
                         ui::AXNodeData>;

// Helper to count the number of nodes in a tree.
size_t GetSize(AXAuraObjWrapper* tree) {
  size_t count = 1;

  std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>> out_children;
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

  AXTreeSourceAuraTest(const AXTreeSourceAuraTest&) = delete;
  AXTreeSourceAuraTest& operator=(const AXTreeSourceAuraTest&) = delete;

  ~AXTreeSourceAuraTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // A simulated desktop root with no delegate owned by the cache.
    auto root_wrapper = std::make_unique<AXRootObjWrapper>(nullptr, &cache_);
    root_wrapper_ = root_wrapper.get();
    cache_.CreateOrReplace(std::move(root_wrapper));

    widget_ = new Widget();
    Widget::InitParams init_params(
        Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = GetContext();
    widget_->Init(std::move(init_params));

    content_ = widget_->SetContentsView(std::make_unique<View>());

    textfield_ = new Textfield();
    textfield_->SetText(u"Value");
    content_->AddChildView(textfield_.get());
    widget_->Show();
  }

  void TearDown() override {
    // ViewsTestBase requires all Widgets to be closed before shutdown.
    textfield_ = nullptr;
    content_ = nullptr;
    widget_.ExtractAsDangling()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  raw_ptr<Widget> widget_ = nullptr;
  raw_ptr<View> content_ = nullptr;
  raw_ptr<Textfield> textfield_ = nullptr;
  AXAuraObjCache cache_;
  raw_ptr<AXRootObjWrapper> root_wrapper_ = nullptr;
};

TEST_F(AXTreeSourceAuraTest, Accessors) {
  // Focus the textfield so the cursor does not disappear.
  textfield_->RequestFocus();

  AXTreeSourceViews ax_tree(root_wrapper_->GetUniqueId(),
                            ui::AXTreeID::CreateNewAXTreeID(), &cache_);
  ASSERT_TRUE(ax_tree.GetRoot());

  // ID's should be > 0.
  ASSERT_GE(ax_tree.GetRoot()->GetUniqueId(), 1);

  // Grab the content view directly from cache to avoid walking down the tree.
  AXAuraObjWrapper* content = cache_.GetOrCreate(content_);
  ax_tree.CacheChildrenIfNeeded(content);
  ASSERT_EQ(1U, ax_tree.GetChildCount(content));

  // Walk down to the text field and assert it is what we expect.
  AXAuraObjWrapper* textfield = ax_tree.ChildAt(content, 0);
  AXAuraObjWrapper* cached_textfield = cache_.GetOrCreate(textfield_);
  ASSERT_EQ(cached_textfield, textfield);
  ax_tree.CacheChildrenIfNeeded(textfield);
  ASSERT_EQ(0u, ax_tree.GetChildCount(textfield));
  ax_tree.ClearChildCache(textfield);

  ax_tree.ClearChildCache(content);

  ASSERT_EQ(content, textfield->GetParent());

  ASSERT_NE(textfield->GetUniqueId(), ax_tree.GetRoot()->GetUniqueId());

  // Try walking up the tree to the root.
  AXAuraObjWrapper* test_root = nullptr;
  for (AXAuraObjWrapper* root_finder = ax_tree.GetParent(content); root_finder;
       root_finder = ax_tree.GetParent(root_finder))
    test_root = root_finder;
  ASSERT_EQ(ax_tree.GetRoot(), test_root);
}

TEST_F(AXTreeSourceAuraTest, DoDefault) {
  AXTreeSourceViews ax_tree(root_wrapper_->GetUniqueId(),
                            ui::AXTreeID::CreateNewAXTreeID(), &cache_);

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
  AXTreeSourceViews ax_tree(root_wrapper_->GetUniqueId(),
                            ui::AXTreeID::CreateNewAXTreeID(), &cache_);

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
  AXTreeSourceViews ax_tree(root_wrapper_->GetUniqueId(),
                            ui::AXTreeID::CreateNewAXTreeID(), &cache_);
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  ui::AXTreeUpdate out_update;

  // This is the initial serialization.
  ax_serializer.SerializeChanges(ax_tree.GetRoot(), &out_update);

  // The update should be the desktop node.
  ASSERT_EQ(1U, out_update.nodes.size());

  // Try removing some child views and re-adding which should fire some events.
  content_->RemoveAllChildViewsWithoutDeleting();
  content_->AddChildView(textfield_.get());

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
  AXTreeSourceViews ax_tree(root_wrapper_->GetUniqueId(),
                            ui::AXTreeID::CreateNewAXTreeID(), &cache_);
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  AXAuraObjWrapper* widget_wrapper = cache_.GetOrCreate(widget_);
  ui::AXNodeData node_data;
  ax_tree.SerializeNode(widget_wrapper, &node_data);
  EXPECT_EQ(ax::mojom::Role::kWindow, node_data.role);
  EXPECT_TRUE(
      node_data.HasBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren));
  bool clips_children =
      node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren);
  EXPECT_TRUE(clips_children);
}
