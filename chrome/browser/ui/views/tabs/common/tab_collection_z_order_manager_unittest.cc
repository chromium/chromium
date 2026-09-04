// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_collection_z_order_manager.h"

#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace {

using ZOrderLevel = TabCollectionZOrderManager::ZOrderLevel;
using testing::ElementsAre;

class TestZOrderContainer : public TabCollectionZOrderManager {
  METADATA_HEADER(TestZOrderContainer, TabCollectionZOrderManager)
 public:
  using TabCollectionZOrderManager::InvalidateZOrder;
};

BEGIN_METADATA(TestZOrderContainer)
END_METADATA

class TabCollectionZOrderManagerTest : public views::ViewsTestBase {
 public:
  TabCollectionZOrderManagerTest() = default;
  ~TabCollectionZOrderManagerTest() override = default;

 protected:
  views::View* CreateChild(ZOrderLevel level = ZOrderLevel::kDefault) {
    auto child = std::make_unique<views::View>();
    child->SetProperty(kTabZOrderKey, level);
    return child.release();
  }
};

TEST_F(TabCollectionZOrderManagerTest, DefaultOrderPreserved) {
  TestZOrderContainer container;
  views::View* child1 = container.AddChildView(CreateChild());
  views::View* child2 = container.AddChildView(CreateChild());
  views::View* child3 = container.AddChildView(CreateChild());

  EXPECT_THAT(container.GetChildrenInZOrder(),
              ElementsAre(child1, child2, child3));
}

TEST_F(TabCollectionZOrderManagerTest, SortedByZOrderLevel) {
  TestZOrderContainer container;
  views::View* active_child =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));
  views::View* default_child =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* underline_child =
      container.AddChildView(CreateChild(ZOrderLevel::kGroupUnderline));
  views::View* hovered_child =
      container.AddChildView(CreateChild(ZOrderLevel::kHovered));
  views::View* selected_child =
      container.AddChildView(CreateChild(ZOrderLevel::kSelected));

  EXPECT_THAT(container.GetChildrenInZOrder(),
              ElementsAre(default_child, hovered_child, selected_child,
                          active_child, underline_child));
}

TEST_F(TabCollectionZOrderManagerTest, StableSortPreservesRelativeOrder) {
  TestZOrderContainer container;
  views::View* default1 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* hovered1 =
      container.AddChildView(CreateChild(ZOrderLevel::kHovered));
  views::View* default2 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* hovered2 =
      container.AddChildView(CreateChild(ZOrderLevel::kHovered));
  views::View* active1 =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));
  views::View* active2 =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));

  EXPECT_THAT(
      container.GetChildrenInZOrder(),
      ElementsAre(default1, default2, hovered1, hovered2, active1, active2));
}

TEST_F(TabCollectionZOrderManagerTest, ChildZOrderDynamicChange) {
  TestZOrderContainer container;
  views::View* child1 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* child2 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));

  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child1, child2));

  // Elevate child1 to hovered -> child1 should be painted after child2.
  child1->SetProperty(kTabZOrderKey, ZOrderLevel::kHovered);
  container.OnChildZOrderChanged(child1);
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child2, child1));

  // Elevate child2 to active -> child2 should be painted after child1.
  child2->SetProperty(kTabZOrderKey, ZOrderLevel::kActive);
  container.OnChildZOrderChanged(child2);
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child1, child2));

  // Demote child2 back to default -> child1 (hovered) is painted after child2.
  child2->SetProperty(kTabZOrderKey, ZOrderLevel::kDefault);
  container.OnChildZOrderChanged(child2);
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child2, child1));
}

TEST_F(TabCollectionZOrderManagerTest,
       ContainerOwnZOrderPropertyUpdatedOnChildLevelChange) {
  TestZOrderContainer container;
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);

  views::View* child1 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* child2 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);

  child1->SetProperty(kTabZOrderKey, ZOrderLevel::kHovered);
  container.OnChildZOrderChanged(child1);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);

  child2->SetProperty(kTabZOrderKey, ZOrderLevel::kActive);
  container.OnChildZOrderChanged(child2);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);

  // Demoting active child drops container level back to hovered (child1's
  // level).
  child2->SetProperty(kTabZOrderKey, ZOrderLevel::kDefault);
  container.OnChildZOrderChanged(child2);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);

  // Demoting hovered child drops container level back to default.
  child1->SetProperty(kTabZOrderKey, ZOrderLevel::kDefault);
  container.OnChildZOrderChanged(child1);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
}

TEST_F(TabCollectionZOrderManagerTest,
       NonPropagatingLevelsDoNotElevateContainer) {
  TestZOrderContainer container;
  EXPECT_TRUE(
      TabCollectionZOrderManager::ShouldPropagateZOrder(ZOrderLevel::kDefault));
  EXPECT_TRUE(
      TabCollectionZOrderManager::ShouldPropagateZOrder(ZOrderLevel::kHovered));
  EXPECT_TRUE(TabCollectionZOrderManager::ShouldPropagateZOrder(
      ZOrderLevel::kSelected));
  EXPECT_TRUE(
      TabCollectionZOrderManager::ShouldPropagateZOrder(ZOrderLevel::kActive));
  EXPECT_FALSE(TabCollectionZOrderManager::ShouldPropagateZOrder(
      ZOrderLevel::kGroupUnderline));

  views::View* underline =
      container.AddChildView(CreateChild(ZOrderLevel::kGroupUnderline));
  // Container itself should not elevate because kGroupUnderline does not
  // propagate.
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);

  views::View* selected_tab =
      container.AddChildView(CreateChild(ZOrderLevel::kSelected));
  // Container elevates to kSelected.
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kSelected);

  // But underline is ordered after selected_tab inside container's Z-order.
  EXPECT_THAT(container.GetChildrenInZOrder(),
              ElementsAre(selected_tab, underline));
}

TEST_F(TabCollectionZOrderManagerTest, NestedContainersPropagateZOrderUpward) {
  auto parent_container = std::make_unique<TestZOrderContainer>();
  TestZOrderContainer* child_container =
      parent_container->AddChildView(std::make_unique<TestZOrderContainer>());
  views::View* parent_default_child =
      parent_container->AddChildView(CreateChild(ZOrderLevel::kDefault));

  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey),
            ZOrderLevel::kDefault);
  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_THAT(parent_container->GetChildrenInZOrder(),
              ElementsAre(child_container, parent_default_child));

  // Add child to nested container and set to hovered -> bubbles up to parent.
  views::View* nested_tab1 =
      child_container->AddChildView(CreateChild(ZOrderLevel::kDefault));
  nested_tab1->SetProperty(kTabZOrderKey, ZOrderLevel::kHovered);
  child_container->OnChildZOrderChanged(nested_tab1);

  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey),
            ZOrderLevel::kHovered);
  EXPECT_THAT(parent_container->GetChildrenInZOrder(),
              ElementsAre(parent_default_child, child_container));

  // Add another nested child and set to active -> bubbles up active to both.
  views::View* nested_tab2 =
      child_container->AddChildView(CreateChild(ZOrderLevel::kDefault));
  nested_tab2->SetProperty(kTabZOrderKey, ZOrderLevel::kActive);
  child_container->OnChildZOrderChanged(nested_tab2);

  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);

  // Demote nested_tab2 to default -> child and parent drop to hovered.
  nested_tab2->SetProperty(kTabZOrderKey, ZOrderLevel::kDefault);
  child_container->OnChildZOrderChanged(nested_tab2);
  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey),
            ZOrderLevel::kHovered);

  // Demote nested_tab1 to default -> child and parent drop to default.
  nested_tab1->SetProperty(kTabZOrderKey, ZOrderLevel::kDefault);
  child_container->OnChildZOrderChanged(nested_tab1);
  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey),
            ZOrderLevel::kDefault);
}

TEST_F(TabCollectionZOrderManagerTest, InvalidateZOrderRefreshesCache) {
  TestZOrderContainer container;
  views::View* child1 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* child2 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));

  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child1, child2));
  EXPECT_FALSE(container.is_z_order_cache_empty_for_testing());

  // Modify property without notifying via OnChildZOrderChanged directly.
  child1->SetProperty(kTabZOrderKey, ZOrderLevel::kActive);
  // Calling InvalidateZOrder should dirty the cache, clear it, and re-sort.
  container.InvalidateZOrder();
  EXPECT_TRUE(container.is_z_order_cache_empty_for_testing());
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child2, child1));
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
}

TEST_F(TabCollectionZOrderManagerTest, CacheClearedOnChildRemoval) {
  TestZOrderContainer container;
  views::View* child1 =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* child2 =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));

  // Populate cache.
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child1, child2));
  EXPECT_FALSE(container.is_z_order_cache_empty_for_testing());

  // Removing a child should immediately clear the cache rather than leaving
  // dangling pointers in the cache until the next query.
  container.RemoveChildViewT(child2);
  EXPECT_TRUE(container.is_z_order_cache_empty_for_testing());

  // Subsequent call to GetChildrenInZOrder repopulates with the remaining
  // child.
  EXPECT_THAT(container.GetChildrenInZOrder(), ElementsAre(child1));
  EXPECT_FALSE(container.is_z_order_cache_empty_for_testing());
}

TEST_F(TabCollectionZOrderManagerTest,
       ContainerOwnZOrderPropertyUpdatedOnChildRemoval) {
  TestZOrderContainer container;
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);

  views::View* hovered_child =
      container.AddChildView(CreateChild(ZOrderLevel::kHovered));
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);

  views::View* active_child =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);

  // Removing active child drops container level back to hovered.
  container.RemoveChildViewT(active_child);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kHovered);

  // Removing hovered child drops container level back to default.
  container.RemoveChildViewT(hovered_child);
  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
}

TEST_F(TabCollectionZOrderManagerTest,
       NestedContainersPropagateZOrderUpwardOnChildRemoval) {
  auto parent_container = std::make_unique<TestZOrderContainer>();
  TestZOrderContainer* child_container =
      parent_container->AddChildView(std::make_unique<TestZOrderContainer>());

  views::View* nested_active_tab =
      child_container->AddChildView(CreateChild(ZOrderLevel::kActive));

  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);

  // Removing active child drops both child and parent containers to default.
  child_container->RemoveChildViewT(nested_active_tab);
  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey),
            ZOrderLevel::kDefault);
}

TEST_F(TabCollectionZOrderManagerTest, CrossContainerMoveTransfersZOrder) {
  TestZOrderContainer container1;
  TestZOrderContainer container2;

  views::View* tab1_default =
      container1.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* tab2_active =
      container1.AddChildView(CreateChild(ZOrderLevel::kActive));
  views::View* tab3_default =
      container2.AddChildView(CreateChild(ZOrderLevel::kDefault));

  EXPECT_EQ(container1.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_EQ(container2.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_THAT(container1.GetChildrenInZOrder(),
              ElementsAre(tab1_default, tab2_active));
  EXPECT_THAT(container2.GetChildrenInZOrder(), ElementsAre(tab3_default));

  // Move active tab from container1 to container2.
  std::unique_ptr<views::View> moved_tab =
      container1.RemoveChildViewT(tab2_active);
  container2.AddChildView(std::move(moved_tab));

  // container1 should drop to default, container2 should elevate to active.
  EXPECT_EQ(container1.GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_EQ(container2.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_THAT(container1.GetChildrenInZOrder(), ElementsAre(tab1_default));
  EXPECT_THAT(container2.GetChildrenInZOrder(),
              ElementsAre(tab3_default, tab2_active));
}

TEST_F(TabCollectionZOrderManagerTest,
       ReparentingFromNestedContainerToParentContainer) {
  auto parent_container = std::make_unique<TestZOrderContainer>();
  TestZOrderContainer* child_container =
      parent_container->AddChildView(std::make_unique<TestZOrderContainer>());

  views::View* active_tab =
      child_container->AddChildView(CreateChild(ZOrderLevel::kActive));

  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);

  // Move active_tab from nested child_container directly into parent_container.
  std::unique_ptr<views::View> moved_tab =
      child_container->RemoveChildViewT(active_tab);
  views::View* tab_in_parent =
      parent_container->AddChildView(std::move(moved_tab));

  // child_container drops to default, parent_container stays active.
  EXPECT_EQ(child_container->GetProperty(kTabZOrderKey), ZOrderLevel::kDefault);
  EXPECT_EQ(parent_container->GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_THAT(child_container->GetChildrenInZOrder(), testing::IsEmpty());
  EXPECT_THAT(parent_container->GetChildrenInZOrder(),
              ElementsAre(child_container, tab_in_parent));
}

TEST_F(TabCollectionZOrderManagerTest, MultiSelectionWithActiveTabHandoff) {
  TestZOrderContainer container;
  views::View* tab1_default =
      container.AddChildView(CreateChild(ZOrderLevel::kDefault));
  views::View* tab2_selected =
      container.AddChildView(CreateChild(ZOrderLevel::kSelected));
  views::View* tab3_selected =
      container.AddChildView(CreateChild(ZOrderLevel::kSelected));
  views::View* tab4_active =
      container.AddChildView(CreateChild(ZOrderLevel::kActive));

  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  EXPECT_THAT(
      container.GetChildrenInZOrder(),
      ElementsAre(tab1_default, tab2_selected, tab3_selected, tab4_active));

  // Active tab handoff: tab4 becomes selected, tab2 becomes active.
  tab4_active->SetProperty(kTabZOrderKey, ZOrderLevel::kSelected);
  container.OnChildZOrderChanged(tab4_active);
  tab2_selected->SetProperty(kTabZOrderKey, ZOrderLevel::kActive);
  container.OnChildZOrderChanged(tab2_selected);

  EXPECT_EQ(container.GetProperty(kTabZOrderKey), ZOrderLevel::kActive);
  // Tab 2 should now be at the top of the z-order.
  EXPECT_THAT(
      container.GetChildrenInZOrder(),
      ElementsAre(tab1_default, tab3_selected, tab4_active, tab2_selected));
}

}  // namespace
