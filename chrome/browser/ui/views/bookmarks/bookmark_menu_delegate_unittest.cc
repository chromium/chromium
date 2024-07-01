// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const char kBasePath[] = "file:///c:/tmp/";
}  // namespace

class BookmarkMenuDelegateTest : public BrowserWithTestWindowTest {
 public:
  BookmarkMenuDelegateTest() = default;
  BookmarkMenuDelegateTest(const BookmarkMenuDelegateTest&) = delete;
  BookmarkMenuDelegateTest& operator=(const BookmarkMenuDelegateTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    bookmarks::test::WaitForBookmarkModelToLoad(model());

    AddTestData();
  }

  void TearDown() override {
    DestroyDelegate();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                ManagedBookmarkServiceFactory::GetInstance(),
                ManagedBookmarkServiceFactory::GetDefaultFactory()}};
  }

 protected:
  bool ShouldCloseOnRemove(const bookmarks::BookmarkNode* node) const {
    return bookmark_menu_delegate_->ShouldCloseOnRemove(node);
  }

  // Destroys the delegate. Do this rather than directly deleting
  // |bookmark_menu_delegate_| as otherwise the menu is leaked.
  void DestroyDelegate() {
    if (!bookmark_menu_delegate_.get())
      return;

    // Since we never show the menu we need to pass the MenuItemView to
    // MenuRunner so that the MenuItemView is destroyed.
    views::MenuRunner menu_runner(
        base::WrapUnique(bookmark_menu_delegate_->menu()), 0);
    bookmark_menu_delegate_.reset();
  }

  void NewDelegate() {
    DestroyDelegate();

    bookmark_menu_delegate_ =
        std::make_unique<BookmarkMenuDelegate>(browser(), nullptr);
  }

  void NewAndInitDelegateForPermanent() {
    const BookmarkNode* node = model()->bookmark_bar_node();
    NewDelegate();
    bookmark_menu_delegate_->Init(&test_delegate_, nullptr, node, 0,
                                  BookmarkMenuDelegate::SHOW_PERMANENT_FOLDERS,
                                  BookmarkLaunchLocation::kNone);
  }

  const BookmarkNode* GetNodeForMenuItem(views::MenuItemView* menu) {
    const auto& node_map = bookmark_menu_delegate_->menu_id_to_node_map_;
    auto iter = node_map.find(menu->GetCommand());
    return (iter == node_map.end()) ? nullptr : iter->second;
  }

  int next_menu_id() { return bookmark_menu_delegate_->next_menu_id_; }

  // Forces all the menus to load by way of invoking WillShowMenu() on all menu
  // items of tyep SUBMENU.
  void LoadAllMenus() { LoadAllMenus(bookmark_menu_delegate_->menu()); }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

 private:
  void LoadAllMenus(views::MenuItemView* menu) {
    EXPECT_EQ(views::MenuItemView::Type::kSubMenu, menu->GetType());

    for (views::MenuItemView* item : menu->GetSubmenu()->GetMenuItems()) {
      if (item->GetType() == views::MenuItemView::Type::kSubMenu) {
        bookmark_menu_delegate_->WillShowMenu(item);
        LoadAllMenus(item);
      }
    }
  }

  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  void AddTestData() {
    const BookmarkNode* bb_node = model()->bookmark_bar_node();
    std::string test_base(kBasePath);
    model()->AddURL(bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model()->AddFolder(bb_node, 1, u"F1");
    model()->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"F11");
    model()->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model()->AddFolder(bb_node, 2, u"F2");

    // Children of the other node.
    model()->AddURL(model()->other_node(), 0, u"oa", GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->other_node(), 1, u"OF1");
    model()->AddURL(of1, 0, u"of1a", GURL(test_base + "of1a"));
  }

  views::MenuDelegate test_delegate_;
};

TEST_F(BookmarkMenuDelegateTest, VerifyLazyLoad) {
  NewAndInitDelegateForPermanent();
  views::MenuItemView* root_item = bookmark_menu_delegate_->menu();
  ASSERT_TRUE(root_item->HasSubmenu());
  EXPECT_EQ(4u, root_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(5u, root_item->GetSubmenu()->children().size());  // + separator
  views::MenuItemView* f1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(f1_item->HasSubmenu());
  // f1 hasn't been loaded yet.
  EXPECT_EQ(0u, f1_item->GetSubmenu()->GetMenuItems().size());
  // Will show triggers a load.
  int next_id_before_load = next_menu_id();
  bookmark_menu_delegate_->WillShowMenu(f1_item);
  // f1 should have loaded its children.
  EXPECT_EQ(next_id_before_load + 2 * AppMenuModel::kNumUnboundedMenuTypes,
            next_menu_id());
  ASSERT_EQ(2u, f1_item->GetSubmenu()->GetMenuItems().size());
  const BookmarkNode* f1_node =
      model()->bookmark_bar_node()->children()[1].get();
  EXPECT_EQ(f1_node->children()[0].get(),
            GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(0)));
  EXPECT_EQ(f1_node->children()[1].get(),
            GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(1)));

  // F11 shouldn't have loaded yet.
  views::MenuItemView* f11_item = f1_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(f11_item->HasSubmenu());
  EXPECT_EQ(0u, f11_item->GetSubmenu()->GetMenuItems().size());

  next_id_before_load = next_menu_id();
  bookmark_menu_delegate_->WillShowMenu(f11_item);
  // Invoke WillShowMenu() twice to make sure the second call doesn't cause
  // problems.
  bookmark_menu_delegate_->WillShowMenu(f11_item);
  // F11 should have loaded its single child (f11a).
  EXPECT_EQ(next_id_before_load + AppMenuModel::kNumUnboundedMenuTypes,
            next_menu_id());

  ASSERT_EQ(1u, f11_item->GetSubmenu()->GetMenuItems().size());
  const BookmarkNode* f11_node = f1_node->children()[1].get();
  EXPECT_EQ(f11_node->children()[0].get(),
            GetNodeForMenuItem(f11_item->GetSubmenu()->GetMenuItemAt(0)));
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, RemoveBookmarks) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->Init(&test_delegate, nullptr, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
  LoadAllMenus();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {
          node->children()[1].get(),
      };
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  bookmark_menu_delegate_->DidRemoveBookmarks();
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, CloseOnRemove) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->Init(&test_delegate, nullptr, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(
      ShouldCloseOnRemove(model()->bookmark_bar_node()->children()[2].get()));

  // Descendants of the bookmark should not close on remove.
  EXPECT_FALSE(ShouldCloseOnRemove(
      model()->bookmark_bar_node()->children()[1]->children()[0].get()));

  EXPECT_FALSE(ShouldCloseOnRemove(model()->other_node()->children()[0].get()));

  // Make it so the other node only has one child.
  // Destroy the current delegate so that it doesn't have any references to
  // deleted nodes.
  DestroyDelegate();
  while (model()->other_node()->children().size() > 1) {
    model()->Remove(model()->other_node()->children()[1].get(),
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }

  NewDelegate();
  bookmark_menu_delegate_->Init(&test_delegate, nullptr, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(ShouldCloseOnRemove(model()->other_node()->children()[0].get()));
}

TEST_F(BookmarkMenuDelegateTest, DropCallback) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->Init(&test_delegate, nullptr, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
  LoadAllMenus();

  views::MenuItemView* root_item = bookmark_menu_delegate_->menu();
  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1_item, drop_data));
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 2u);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1_item, views::MenuDelegate::DropPosition::kAfter, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 3u);
}

TEST_F(BookmarkMenuDelegateTest, DropCallback_ModelChanged) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->Init(&test_delegate, nullptr, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
  LoadAllMenus();

  views::MenuItemView* root_item = bookmark_menu_delegate_->menu();
  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1_item, drop_data));
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 2u);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1_item, views::MenuDelegate::DropPosition::kAfter, target_event);
  model()->AddURL(model()->bookmark_bar_node(), 2, u"z1",
                  GURL(std::string(kBasePath) + "z1"));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kNone);
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 2u);
}
