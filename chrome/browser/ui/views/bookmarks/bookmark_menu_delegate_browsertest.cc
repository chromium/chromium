// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const char kBasePath[] = "file:///c:/tmp/";

MATCHER_P(BookmarkVariantMatcher, node, "") {
  if (node->is_url()) {
    return std::holds_alternative<const BookmarkNode*>(arg) &&
           std::get<const BookmarkNode*>(arg) == node;
  } else {
    return std::get<BookmarkParentFolder>(arg) ==
           BookmarkParentFolder::FromFolderNode(node);
  }
}

// Returns number of menu items in |folder| assuming it is
// a root folder (no other bookmark folder contains it). This is
// because we add two new menu items to such folders, see
// BookmarkMenuDelegate::CreateMenu for more details.
int RootFolderSizeOffset() {
  if (base::FeatureList::IsEnabled(features::kTabGroupMenuImprovements)) {
    return 2;
  }
  return 0;
}

}  // namespace

class BookmarkMenuDelegateTest : public InProcessBrowserTest {
 public:
  BookmarkMenuDelegateTest() = default;
  BookmarkMenuDelegateTest(const BookmarkMenuDelegateTest&) = delete;
  BookmarkMenuDelegateTest& operator=(const BookmarkMenuDelegateTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set managed bookmarks.
    PrefService* prefs = browser()->profile()->GetPrefs();
    ASSERT_FALSE(prefs->HasPrefPath(bookmarks::prefs::kManagedBookmarks));
    prefs->SetList(bookmarks::prefs::kManagedBookmarks,
                   base::Value::List().Append(
                       base::Value::Dict()
                           .Set("name", "Google")
                           .Set("url", GURL("http://google.com/").spec())));

    WaitForBookmarkMergedSurfaceServiceToLoad(bookmark_service());
    model()->CreateAccountPermanentFolders();

    CHECK(managed_node());
    AddTestData();
  }

  void TearDownOnMainThread() override {
    DestroyDelegate();

    root_menu_.reset();
    bookmark_menu_delegate_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool ShouldCloseOnRemove(const bookmarks::BookmarkNode* node) const {
    return bookmark_menu_delegate_->ShouldCloseOnRemove(
        BookmarkMenuDelegate::BookmarkFolderOrURL(node));
  }

  // Destroys the delegate. Do this rather than directly deleting
  // |bookmark_menu_delegate_| as otherwise the menu is leaked.
  void DestroyDelegate() {
    if (!bookmark_menu_delegate_.get()) {
      return;
    }

    views::MenuItemView* menu = bookmark_menu_delegate_->menu();
    bookmark_menu_delegate_.reset();
    // Since we never show the menu we need to pass the MenuItemView to
    // MenuRunner so that the MenuItemView is destroyed.
    if (menu) {
      views::MenuRunner menu_runner(base::WrapUnique(menu), 0);
    }
  }

  void NewDelegate() {
    DestroyDelegate();

    bookmark_menu_delegate_ = std::make_unique<BookmarkMenuDelegate>(
        browser(), nullptr, &test_delegate_, BookmarkLaunchLocation::kNone);
  }

  void NewAndBuildFullMenu() {
    root_menu_ = std::make_unique<views::MenuItemView>();
    // Add a placeholder here because in practice the full menu is never
    // empty.
    root_menu_->AppendTitle(std::u16string());
    root_menu_->CreateSubmenu();
    NewDelegate();
    bookmark_menu_delegate_->BuildFullMenu(root_menu_.get());
  }

  void NewAndBuildFullMenuWithBookmarksTitle() {
    // Remove the managed bookmarks node.
    browser()->profile()->GetPrefs()->SetList(
        bookmarks::prefs::kManagedBookmarks, base::Value::List());
    root_menu_ = std::make_unique<views::MenuItemView>();
    root_menu_->CreateSubmenu();
    // Add a placeholder to ensure the bookmarks title is added.
    root_menu_->AppendTitle(std::u16string());
    NewDelegate();
    bookmark_menu_delegate_->BuildFullMenu(root_menu_.get());
  }

  std::variant<const BookmarkNode*, BookmarkParentFolder> GetNodeForMenuItem(
      views::MenuItemView* menu) {
    const auto& node_map = bookmark_menu_delegate_->menu_id_to_node_map_;
    auto iter = node_map.find(menu->GetCommand());
    if (iter == node_map.end()) {
      return nullptr;
    }

    if (const BookmarkParentFolder* folder = iter->second.GetIfBookmarkFolder();
        folder) {
      return *folder;
    }

    return iter->second.GetIfBookmarkURL();
  }

  int next_menu_id() { return bookmark_menu_delegate_->next_menu_id_; }

  // Forces all the menus to load by way of invoking WillShowMenu() on all menu
  // items of tyep SUBMENU.
  void LoadAllMenus(views::MenuItemView* menu) {
    EXPECT_EQ(views::MenuItemView::Type::kSubMenu, menu->GetType());

    for (views::MenuItemView* item : menu->GetSubmenu()->GetMenuItems()) {
      if (item->GetType() == views::MenuItemView::Type::kSubMenu) {
        bookmark_menu_delegate_->WillShowMenu(item);
        LoadAllMenus(item);
      }
    }
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  }

  BookmarkMergedSurfaceService* bookmark_service() {
    return BookmarkMergedSurfaceServiceFactory::GetForProfile(
        browser()->profile());
  }

  const BookmarkNode* managed_node() {
    return ManagedBookmarkServiceFactory::GetForProfile(browser()->profile())
        ->managed_node();
  }

  // Returns the menu being used for the test.
  views::MenuItemView* menu() {
    return root_menu_.get() ? root_menu_.get()
                            : bookmark_menu_delegate_->menu();
  }

  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  std::unique_ptr<views::MenuItemView> root_menu_;

 private:
  // Creates the following structure:
  // bookmark bar node
  //   a (local)
  //   F1 (account)
  //    f1a
  //    F11
  //     f11a
  //   F2 (local)
  //   b (account)
  // other node
  //   oa (account)
  //   OF1 (account)
  //     of1a
  //   OF2 (local)
  //    of2a
  //    OF21
  //     of21a
  // mobile node
  //   ma (local)
  //   mF1 (local)
  //     mf1a
  void AddTestData() {
    const BookmarkNode* local_bb_node = model()->bookmark_bar_node();
    const BookmarkNode* bb_node = model()->account_bookmark_bar_node();

    std::string test_base(kBasePath);
    model()->AddURL(local_bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model()->AddFolder(bb_node, 0, u"F1");
    model()->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"F11");
    model()->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model()->AddFolder(local_bb_node, 1, u"F2");
    model()->AddURL(bb_node, 1, u"b", GURL(test_base + "b"));
    // F1 (account), b (account), a (local),  F2 (local) -> a (local) , F1
    // (account), F2 (local), b (account).
    bookmark_service()->Move(local_bb_node->children()[0].get(),
                             BookmarkParentFolder::BookmarkBarFolder(), 0,
                             /*browser=*/nullptr);
    bookmark_service()->Move(bb_node->children()[1].get(),
                             BookmarkParentFolder::BookmarkBarFolder(), 4u,
                             /*browser=*/nullptr);

    // Children of the other node.
    model()->AddURL(model()->account_other_node(), 0, u"oa",
                    GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->account_other_node(), 1, u"OF1");
    model()->AddURL(of1, 0, u"of1a", GURL(test_base + "of1a"));
    const BookmarkNode* of2 =
        model()->AddFolder(model()->other_node(), 0, u"F1");
    model()->AddURL(of2, 0, u"f1a", GURL(test_base + "of2a"));
    const BookmarkNode* of21 = model()->AddFolder(of2, 1, u"OF21");
    model()->AddURL(of21, 0, u"f11a", GURL(test_base + "of21a"));

    // Children of the mobile node.
    model()->AddURL(model()->mobile_node(), 0, u"ma", GURL(test_base + "ma"));
    const BookmarkNode* mf1 =
        model()->AddFolder(model()->mobile_node(), 1, u"mF1");
    model()->AddURL(mf1, 0, u"mf1a", GURL(test_base + "mf1a"));
  }

  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  views::MenuDelegate test_delegate_;
};

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, VerifyLazyLoad) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  ASSERT_TRUE(root_item->HasSubmenu());
  EXPECT_EQ(9u, root_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(11u, root_item->GetSubmenu()->children().size());  // + separators
  views::MenuItemView* f1_item = root_item->GetSubmenu()->GetMenuItemAt(4);
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
  const BookmarkNode* f1_node = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  EXPECT_THAT(GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(0)),
              BookmarkVariantMatcher(f1_node->children()[0].get()));
  EXPECT_THAT(GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(1)),
              BookmarkVariantMatcher(f1_node->children()[1].get()));

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
  EXPECT_THAT(GetNodeForMenuItem(f11_item->GetSubmenu()->GetMenuItemAt(0)),
              BookmarkVariantMatcher(f11_node->children()[0].get()));
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, RemoveBookmarks) {
  const BookmarkNode* f1 = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  LoadAllMenus(menu());
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {
          f1->children()[1].get(),
      };
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  bookmark_menu_delegate_->DidRemoveBookmarks();
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, CloseOnRemove) {
  NewDelegate();
  EXPECT_FALSE(ShouldCloseOnRemove(model()->account_bookmark_bar_node()));

  BookmarkParentFolder bookmark_bar_folder(
      BookmarkParentFolder::BookmarkBarFolder());

  const BookmarkNode* f1 =
      bookmark_service()->GetNodeAtIndex(bookmark_bar_folder, 1u);
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(ShouldCloseOnRemove(
      bookmark_service()->GetNodeAtIndex(bookmark_bar_folder, 2u)));

  // Descendants of the bookmark should not close on remove.
  EXPECT_FALSE(ShouldCloseOnRemove(f1->children()[0].get()));

  BookmarkParentFolderChildren other_folder_children =
      bookmark_service()->GetChildren(BookmarkParentFolder::OtherFolder());
  EXPECT_FALSE(ShouldCloseOnRemove(other_folder_children[0]));

  // Make it so the other node only has one child.
  // Destroy the current delegate so that it doesn't have any references to
  // deleted nodes.
  DestroyDelegate();
  while (other_folder_children.size() > 1) {
    model()->Remove(other_folder_children[other_folder_children.size() - 1],
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(ShouldCloseOnRemove(other_folder_children[0]));
}

// Tests that the "Bookmarks" title and separator are removed from the parent
// menu when the children of the bookmark bar node are removed.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       UpdateBookmarksTitleAfterNodeRemoved) {
  NewAndBuildFullMenuWithBookmarksTitle();
  views::MenuItemView* root_menu = menu();

  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(8u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(10u, root_menu->GetSubmenu()->children().size());  // + separators

  // Remove all bookmark bar nodes.
  BookmarkParentFolderChildren bookmark_bar_has_children =
      bookmark_service()->GetChildren(
          BookmarkParentFolder::BookmarkBarFolder());
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {};
  for (const BookmarkNode* node : bookmark_bar_has_children) {
    nodes_to_remove.push_back(node);
  }
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  while (bookmark_bar_has_children.size()) {
    model()->Remove(bookmark_bar_has_children[0],
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }
  bookmark_menu_delegate_->DidRemoveBookmarks();

  // The placeholder, "other" and mobile bookmark folders, and their separator
  // remain.
  EXPECT_EQ(3u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(4u, root_menu->GetSubmenu()->children().size());  // separator
}

// Tests that the separator is removed from the "other" bookmarks menu item
// when its child bookmarks are removed.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       UpdateOtherNodeMenuAfterNodeRemoved) {
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(BookmarkParentFolder::OtherFolder(),
                                         0);
  views::MenuItemView* other_node_menu = menu();

  ASSERT_TRUE(other_node_menu->HasSubmenu());
  EXPECT_EQ(4u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(5u, other_node_menu->GetSubmenu()->children().size());  // separator

  // Remove all "other" node children.
  BookmarkParentFolderChildren other_folder_children =
      bookmark_service()->GetChildren(BookmarkParentFolder::OtherFolder());
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {};
  for (const BookmarkNode* node : other_folder_children) {
    nodes_to_remove.push_back(node);
  }
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  while (other_folder_children.size()) {
    model()->Remove(other_folder_children[0],
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }
  bookmark_menu_delegate_->DidRemoveBookmarks();

  EXPECT_EQ(1u, other_node_menu->GetSubmenu()->children().size());
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropAfterNode) {
  const BookmarkNode* f1 = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1a_item =
      root_item->GetSubmenu()->GetMenuItemAt(0 + RootFolderSizeOffset());
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1a_item, drop_data));
  EXPECT_EQ(f1->children().size(), 2u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f1a_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1a_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f1->children().size(), 3u);
  // New bookmark added at `f1a_item` index + 1.
  EXPECT_EQ(f1->children()[1]->GetTitle(), std::u16string(u"z"));
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropOnNode) {
  const BookmarkNode* f1 = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f11_item =
      root_item->GetSubmenu()->GetMenuItemAt(1 + RootFolderSizeOffset());
  const BookmarkNode* f11_node = f1->children()[1].get();
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f11_item, drop_data));
  EXPECT_EQ(f11_node->children().size(), 1u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kOn;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f11_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f11_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f11_node->children().size(), 2u);
  // New bookmark added at `f11_item` old index.
  EXPECT_EQ(f11_node->children()[1]->GetTitle(), std::u16string(u"z"));
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropBeforeNode) {
  const BookmarkNode* f1 = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f11_item =
      root_item->GetSubmenu()->GetMenuItemAt(1 + RootFolderSizeOffset());
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f11_item, drop_data));
  EXPECT_EQ(f1->children().size(), 2u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f11_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f11_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f1->children().size(), 3u);
  // New bookmark added at `f11_item` old index.
  EXPECT_EQ(f1->children()[1]->GetTitle(), std::u16string(u"z"));
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DropCallbackModelChanged) {
  const BookmarkNode* f1 = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1_item, drop_data));
  EXPECT_EQ(f1->children().size(), 2u);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1_item, views::MenuDelegate::DropPosition::kAfter, target_event);
  model()->AddURL(model()->bookmark_bar_node(), 2, u"z1",
                  GURL(std::string(kBasePath) + "z1"));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kNone);
  EXPECT_EQ(f1->children().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropInvalid) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_COPY);

  // Drop before managed node.

  auto* managed_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(managed_folder_menu->title(), managed_node()->GetTitle());
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(managed_folder_menu, drop_data));

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                managed_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop before mobile node.

  BookmarkParentFolder bookmark_bar_folder(
      BookmarkParentFolder::BookmarkBarFolder());

  size_t mobile_folder_menu_index =
      4u +  // placeholder + bookmarks title + managed + other node.
      bookmark_service()->GetChildrenCount(bookmark_bar_folder);
  auto* mobile_folder_menu =
      root_item->GetSubmenu()->GetMenuItemAt(mobile_folder_menu_index);
  ASSERT_EQ(mobile_folder_menu->title(), model()->mobile_node()->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(mobile_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                mobile_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop after mobile node.

  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(mobile_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                mobile_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop after other node.

  auto* other_folder_menu =
      root_item->GetSubmenu()->GetMenuItemAt(mobile_folder_menu_index - 1);
  ASSERT_EQ(other_folder_menu->title(), model()->other_node()->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(other_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                other_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop on url.

  auto* url_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  ASSERT_EQ(
      url_item->title(),
      bookmark_service()->GetNodeAtIndex(bookmark_bar_folder, 0)->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(url_item, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kOn;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(url_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kNone);
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropAfterManagedNode) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  auto* managed_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(managed_folder_menu->title(), managed_node()->GetTitle());

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_LINK);
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(managed_folder_menu, drop_data));
  BookmarkParentFolder bookmark_bar_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  size_t bookmark_bar_nodes_size =
      bookmark_service()->GetChildrenCount(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_nodes_size, 4u);

  // Drop after managed node.
  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                managed_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kLink);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      managed_folder_menu, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(bookmark_service()->GetChildrenCount(bookmark_bar_folder),
            bookmark_bar_nodes_size + 1);
  // New nodes are added to the account bookmark bar node.
  EXPECT_EQ(model()->account_bookmark_bar_node()->children()[0]->GetTitle(),
            std::u16string(u"z"));
  EXPECT_EQ(
      bookmark_service()->GetNodeAtIndex(bookmark_bar_folder, 0)->GetTitle(),
      std::u16string(u"z"));
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DragAndDropBeforeOtherNode) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  BookmarkParentFolder bookmark_bar_folder(
      BookmarkParentFolder::BookmarkBarFolder());
  size_t bookmark_bar_nodes_size =
      bookmark_service()->GetChildrenCount(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_nodes_size, 4u);

  auto* other_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(
      bookmark_bar_nodes_size + 3u);  // add managed folder + bookmarks title.
  ASSERT_EQ(other_folder_menu->title(),
            model()->account_other_node()->GetTitle());

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_LINK);
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(other_folder_menu, drop_data));

  // Drop before other node.
  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                other_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kLink);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      other_folder_menu, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(bookmark_service()->GetChildrenCount(bookmark_bar_folder),
            bookmark_bar_nodes_size + 1);
  // New node added at the end of the bookmark bar children.
  EXPECT_EQ(bookmark_service()
                ->GetNodeAtIndex(bookmark_bar_folder, bookmark_bar_nodes_size)
                ->GetTitle(),
            std::u16string(u"z"));
  auto& account_bb_children = model()->account_bookmark_bar_node()->children();
  EXPECT_EQ(account_bb_children[account_bb_children.size() - 1]->GetTitle(),
            std::u16string(u"z"));
}

// Tests moving a bookmark between two normal bookmark folders.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MovingBookmarksBetweenNormalFolders) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  views::MenuItemView* f1_item = root_item->GetSubmenu()->GetMenuItemAt(4);
  views::MenuItemView* f2_item = root_item->GetSubmenu()->GetMenuItemAt(5);

  // Folders haven't been loaded yet.
  ASSERT_TRUE(f1_item->HasSubmenu());
  ASSERT_TRUE(f2_item->HasSubmenu());
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  const BookmarkNode* f1_node = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1u);
  const BookmarkNode* f1a_node = f1_node->children()[0].get();
  const BookmarkNode* f11_node = f1_node->children()[1].get();
  const BookmarkNode* f2_node = bookmark_service()->GetNodeAtIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 2u);

  // Move to a folder that doesn't have a menu. There should be no visible
  // changed.
  model()->Move(f1a_node, f11_node, 0);
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  // Move to F2, which has a menu but hasn't been loaded yet.
  model()->Move(f1a_node, f2_node, 0);
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  // Load the two menus. The move should now be reflected.
  bookmark_menu_delegate_->WillShowMenu(f1_item);
  bookmark_menu_delegate_->WillShowMenu(f2_item);
  EXPECT_EQ(1u, f1_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(1u, f2_item->GetSubmenu()->GetMenuItems().size());

  // Move from F2 to F1.
  model()->Move(f1a_node, f1_node, 0);
  EXPECT_EQ(2u, f1_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(0u, f2_item->GetSubmenu()->GetMenuItems().size());
}

// Tests moving a bookmark whose menu doesn't have a parent.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MoveBookmarkWithoutParentMenu) {
  BookmarkParentFolderChildren bookamrk_bar_children =
      bookmark_service()->GetChildren(
          BookmarkParentFolder::BookmarkBarFolder());
  ASSERT_EQ(bookamrk_bar_children.size(), 4u);

  const BookmarkNode* const f1_node = bookamrk_bar_children[1];

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1_node), 0);
  // In practice, additional menus created by `SetActiveMenu` are registered as
  // siblings of the menu runner, which handles deletion.
  const std::unique_ptr<views::MenuItemView> f1_menu(menu());
  ASSERT_NE(f1_menu, nullptr);
  EXPECT_EQ(f1_menu->GetParentMenuItem(), nullptr);

  const BookmarkNode* const f2_node = bookamrk_bar_children[2];

  // Move f1_node, which doesn't have a parent menu, to f2_node.
  // f1_node's menu should be a child of f2_node.
  model()->Move(f1_node, f2_node, 0);

  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f2_node), 0);
  ASSERT_NE(nullptr, menu());
  ASSERT_TRUE(menu()->HasSubmenu());
  ASSERT_FALSE(menu()->GetSubmenu()->GetMenuItems().empty());
  EXPECT_EQ(f1_node->GetTitle(),
            menu()->GetSubmenu()->GetMenuItemAt(0)->title());
}

// Tests that the bookmarks title is appropriately added and removed when moving
// bookmarks into/out of the bookmarks bar for an embedded menu.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MovingBookmarkUpdatesBookmarksTitle) {
  NewAndBuildFullMenuWithBookmarksTitle();
  views::MenuItemView* root_menu = menu();
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 8u);
  EXPECT_EQ(root_menu->GetSubmenu()->children().size(), 10u);  // + separators

  const BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(
          BookmarkParentFolder::BookmarkBarFolder());
  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();

  bookmark_service()->Move(bookmark_bar_children[2], other_folder, 0,
                           /*browser=*/nullptr);
  bookmark_service()->Move(bookmark_bar_children[1], other_folder, 0,
                           /*browser=*/nullptr);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 6u);

  // Removing the last bookmark bar node should remove both the bookmark and
  // the bookmarks title from the menu.
  const BookmarkNode* a_node = bookmark_bar_children[0];
  bookmark_service()->Move(a_node, other_folder, 0, /*browser=*/nullptr);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 5u);
  EXPECT_EQ(root_menu->GetSubmenu()->children().size(), 7u);

  // Adding the bookmark back to the bookmark bar should add the title above
  // permanent nodes. The moved bookmark's menu should appear after the
  // title.
  bookmark_service()->Move(a_node, BookmarkParentFolder::BookmarkBarFolder(), 0,
                           /*browser=*/nullptr);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 6u);
  EXPECT_EQ(root_menu->GetSubmenu()->children().size(), 8u);

  views::MenuItemView* bookmarks_title =
      root_menu->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE),
            bookmarks_title->title());

  views::MenuItemView* a_node_item = root_menu->GetSubmenu()->GetMenuItemAt(2);
  EXPECT_EQ(a_node_item->title(), u"a");
}

// Tests that the separator in the "other" bookmarks menu is appropriately added
// and removed when moving bookmarks into/out of it.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MovingBookmarkUpdatesOtherNodeHeader) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  views::MenuItemView* other_node_menu =
      root_item->GetSubmenu()->GetMenuItemAt(7);
  bookmark_menu_delegate_->WillShowMenu(other_node_menu);

  EXPECT_EQ(other_node_menu->GetSubmenu()->GetMenuItems().size(), 4u);
  EXPECT_EQ(other_node_menu->GetSubmenu()->children().size(), 5u);

  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();
  BookmarkParentFolderChildren other_folder_children =
      bookmark_service()->GetChildren(other_folder);
  const BookmarkNode* oa_node = other_folder_children[0];
  const BookmarkNode* of1_node = other_folder_children[1];
  const BookmarkNode* of2_node = other_folder_children[2];
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();

  bookmark_service()->Move(oa_node, bookmark_bar_folder, 0,
                           /*browser=*/nullptr);
  EXPECT_EQ(other_node_menu->GetSubmenu()->GetMenuItems().size(), 3u);
  EXPECT_EQ(other_node_menu->GetSubmenu()->children().size(), 4u);

  // Moving the last node should remove the separator too.
  bookmark_service()->Move(of1_node, bookmark_bar_folder, 0,
                           /*browser=*/nullptr);
  bookmark_service()->Move(of2_node, bookmark_bar_folder, 0,
                           /*browser=*/nullptr);
  EXPECT_EQ(other_node_menu->GetSubmenu()->GetMenuItems().size(), 1u);
  EXPECT_EQ(other_node_menu->GetSubmenu()->children().size(), 1u);

  // Adding the bookmark back to the other folder should add the separator.
  bookmark_service()->Move(oa_node, other_folder, 0, /*browser=*/nullptr);
  EXPECT_EQ(other_node_menu->GetSubmenu()->GetMenuItems().size(), 2u);
  EXPECT_EQ(other_node_menu->GetSubmenu()->children().size(), 3u);

  EXPECT_TRUE(views::IsViewClass<views::MenuSeparator>(
      other_node_menu->GetSubmenu()->children()[1]));

  views::MenuItemView* oa_item =
      other_node_menu->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(oa_item->title(), u"oa");
}

// Tests that moving bookmarks into/out of a folder built with a "start index"
// respescts the initially provided start index.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MovingBookmarkRespectsStartIndex) {
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 1);

  views::MenuItemView* root_menu = menu();
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            3u + RootFolderSizeOffset());

  const BookmarkNode* f1_node = bookmark_bar_children[1];
  const BookmarkNode* f2_node = bookmark_bar_children[2];
  const BookmarkNode* b_node = bookmark_bar_children[3];

  bookmark_service()->Move(f1_node, BookmarkParentFolder::OtherFolder(), 0,
                           /*browser=*/nullptr);
  bookmark_service()->Move(f2_node, BookmarkParentFolder::OtherFolder(), 0,
                           /*browser=*/nullptr);
  bookmark_service()->Move(b_node, BookmarkParentFolder::OtherFolder(), 0,
                           /*browser=*/nullptr);
  EXPECT_TRUE(root_menu->GetSubmenu()->GetMenuItems().size() ==
              0 + RootFolderSizeOffset());

  bookmark_service()->Move(f1_node, bookmark_bar_folder, 1,
                           /*browser=*/nullptr);
  bookmark_service()->Move(f2_node, bookmark_bar_folder, 2,
                           /*browser=*/nullptr);
  bookmark_service()->Move(b_node, bookmark_bar_folder, 3,
                           /*browser=*/nullptr);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            3u + RootFolderSizeOffset());
}

// Tests that moving a bookmark into the hidden section of a menu does nothing.
IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       MovingBookmarkBeforeStartIndexDoesNothing) {
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(bookmark_bar_folder, 1);

  views::MenuItemView* root_menu = menu();
  // The menu has items for nodes F1, F2 and b.
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            3u + RootFolderSizeOffset());

  // Moving another node to the first index should do nothing.
  bookmark_service()->Move(model()->account_other_node()->children()[0].get(),
                           bookmark_bar_folder, 0,
                           /*browser=*/nullptr);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            3u + RootFolderSizeOffset());
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, IncreaseStartIndex) {
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(bookmark_bar_folder, 0);
  views::MenuItemView* root_menu = menu();
  // The menu has items for nodes, a, F1, F2 and b.
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            4u + RootFolderSizeOffset());

  // Increasing the start index should remove the first nodes.
  bookmark_menu_delegate_->SetMenuStartIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  ASSERT_TRUE(root_menu->HasSubmenu());
  ASSERT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            2u + RootFolderSizeOffset());
  EXPECT_EQ(root_menu->GetSubmenu()
                ->GetMenuItemAt(0 + RootFolderSizeOffset())
                ->title(),
            u"F2");
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, DecreaseStartIndex) {
  // TODO(crbug.com/460480077): Enable test with the feature flag turned on.
  if (base::FeatureList::IsEnabled(features::kTabGroupMenuImprovements)) {
    GTEST_SKIP();
  }
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(bookmark_bar_folder, 2);
  views::MenuItemView* root_menu = menu();
  ASSERT_TRUE(root_menu->HasSubmenu());
  ASSERT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 2u);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItemAt(0)->title(), u"F2");
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItemAt(1)->title(), u"b");

  // Decreasing the starting should add the missing nodes.
  bookmark_menu_delegate_->SetMenuStartIndex(bookmark_bar_folder, 1);
  ASSERT_TRUE(root_menu->HasSubmenu());
  ASSERT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(), 3u);
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItemAt(0)->title(), u"F1");
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItemAt(1)->title(), u"F2");
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItemAt(2)->title(), u"b");
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest, SetMenuStartIndexUnchanged) {
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(bookmark_bar_folder, 2);
  views::MenuItemView* root_menu = menu();
  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            2u + RootFolderSizeOffset());

  // Nothing should happen if the index is unchanged.
  bookmark_menu_delegate_->SetMenuStartIndex(bookmark_bar_folder, 2);
  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(root_menu->GetSubmenu()->GetMenuItems().size(),
            2u + RootFolderSizeOffset());
}

IN_PROC_BROWSER_TEST_F(BookmarkMenuDelegateTest,
                       SetMenuStartIndexForMissingMenu) {
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      bookmark_service()->GetChildren(bookmark_bar_folder);
  ASSERT_EQ(bookmark_bar_children.size(), 4u);

  NewDelegate();

  // Nothing should happen if the menu wasn't built yet.
  bookmark_menu_delegate_->SetMenuStartIndex(bookmark_bar_folder, 2u);
  EXPECT_EQ(menu(), nullptr);
}
