// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <set>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_test.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

namespace {

views::MenuItemView* FindMenuItemByTitle(views::MenuItemView* parent,
                                         const std::u16string& title) {
  if (!parent || !parent->HasSubmenu()) {
    return nullptr;
  }
  for (views::MenuItemView* item : parent->GetSubmenu()->GetMenuItems()) {
    if (item->title() == title) {
      return item;
    }
  }
  return nullptr;
}

void SimulateBookmarkDragAndDrop(
    ActionAppMenu* menu,
    views::MenuItemView* source_item,
    views::MenuItemView* target_item,
    views::MenuDelegate::DropPosition drop_position) {
  ASSERT_TRUE(menu->CanDrag(source_item));
  EXPECT_NE(menu->GetDragOperations(source_item), ui::DragDropTypes::DRAG_NONE);

  ui::OSExchangeData drag_data;
  menu->WriteDragData(source_item, &drag_data);

  views::MenuItemView* target_parent = target_item->GetParentMenuItem();
  ASSERT_TRUE(target_parent);
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(menu->GetDropFormats(target_parent, &formats, &format_types));
  EXPECT_TRUE(menu->AreDropTypesRequired(target_parent));
  ASSERT_TRUE(menu->CanDrop(target_parent, drag_data));

  ui::DropTargetEvent target_event(drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(menu->GetDropOperation(target_item, target_event, &drop_position),
            ui::mojom::DragOperation::kMove);

  views::View::DropCallback drop_cb =
      menu->GetDropCallback(target_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kMove);
}

}  // namespace

class ActionAppMenuBrowserTest : public InProcessBrowserTest {
 public:
  ActionAppMenuBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppMenuGlowUp);
  }
  ~ActionAppMenuBrowserTest() override = default;

  BrowserAppMenuButton* GetMenuButton() {
    return views::AsViewClass<BrowserAppMenuButton>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kToolbarAppMenuButtonElementId,
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetElementContext()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenu) {
  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  EXPECT_FALSE(menu_button->IsMenuShowing());
  EXPECT_FALSE(menu_button->action_app_menu());

  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);
  EXPECT_FALSE(menu_button->app_menu());

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Verify that the Action items have been converted into visual menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(block_item);

  // Check if the menu items have background styling
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenuDarkMode) {
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);

  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());
  views::SubmenuView* submenu = root->GetSubmenu();
  const ui::ColorProvider* color_provider = submenu->GetColorProvider();
  ASSERT_TRUE(color_provider);

  for (views::MenuItemView* child : submenu->GetMenuItems()) {
    if (child->GetMenuItemBackground().has_value()) {
      ui::ColorId bg_id = child->GetMenuItemBackground()->background_color_id;
      SkColor bg_color = color_provider->GetColor(bg_id);
      EXPECT_NE(bg_color, gfx::kPlaceholderColor)
          << "Item with id " << child->GetCommand()
          << " has placeholder color red background!";
    }
  }

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, BookmarksDragAndDropReorder) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node_a =
      model->AddURL(bb_node, 0, u"Bookmark A", GURL("https://a.example.com"));
  const bookmarks::BookmarkNode* node_b =
      model->AddURL(bb_node, 1, u"Bookmark B", GURL("https://b.example.com"));

  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);
  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  views::MenuItemView* bookmarks_item =
      root->GetMenuItemByID(kActionBookmarksSubmenu);
  ASSERT_TRUE(bookmarks_item);
  action_menu->WillShowMenu(bookmarks_item);

  // Non-bookmark items cannot be dragged.
  views::MenuItemView* bookmark_this_tab =
      root->GetMenuItemByID(kActionBookmarkThisTab);
  ASSERT_TRUE(bookmark_this_tab);
  EXPECT_FALSE(action_menu->CanDrag(bookmark_this_tab));

  views::MenuItemView* item_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  views::MenuItemView* item_b =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark B");
  ASSERT_TRUE(item_a);
  ASSERT_TRUE(item_b);

  // Drag Bookmark B before Bookmark A.
  SimulateBookmarkDragAndDrop(action_menu, item_b, item_a,
                              views::MenuDelegate::DropPosition::kBefore);

  EXPECT_EQ(bb_node->children()[0].get(), node_b);
  EXPECT_EQ(bb_node->children()[1].get(), node_a);

  // Verify the live menu updated in place with Bookmark B before Bookmark A.
  views::MenuItemView* updated_b =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark B");
  views::MenuItemView* updated_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  ASSERT_TRUE(updated_b);
  ASSERT_TRUE(updated_a);
  EXPECT_LT(bookmarks_item->GetSubmenu()->GetIndexOf(updated_b),
            bookmarks_item->GetSubmenu()->GetIndexOf(updated_a));

  menu_button->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest,
                       BookmarksDragAndDropToAndFromNestedFolder) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node_a =
      model->AddURL(bb_node, 0, u"Bookmark A", GURL("https://a.example.com"));
  const bookmarks::BookmarkNode* folder_b =
      model->AddFolder(bb_node, 1, u"Folder B");

  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);
  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  views::MenuItemView* bookmarks_item =
      root->GetMenuItemByID(kActionBookmarksSubmenu);
  ASSERT_TRUE(bookmarks_item);
  action_menu->WillShowMenu(bookmarks_item);

  views::MenuItemView* item_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  views::MenuItemView* item_b =
      FindMenuItemByTitle(bookmarks_item, u"Folder B");
  ASSERT_TRUE(item_a);
  ASSERT_TRUE(item_b);

  // Drop Bookmark A onto empty Folder B.
  SimulateBookmarkDragAndDrop(action_menu, item_a, item_b,
                              views::MenuDelegate::DropPosition::kOn);
  ASSERT_EQ(folder_b->children().size(), 1u);
  EXPECT_EQ(folder_b->children()[0].get(), node_a);

  // Verify Bookmark A now appears inside Folder B's submenu in the live menu.
  views::MenuItemView* updated_b =
      FindMenuItemByTitle(bookmarks_item, u"Folder B");
  ASSERT_TRUE(updated_b);
  views::MenuItemView* a_in_b = FindMenuItemByTitle(updated_b, u"Bookmark A");
  ASSERT_TRUE(a_in_b);

  // Drag Bookmark A back out of Folder B before Folder B.
  SimulateBookmarkDragAndDrop(action_menu, a_in_b, updated_b,
                              views::MenuDelegate::DropPosition::kBefore);
  EXPECT_TRUE(folder_b->children().empty());
  ASSERT_EQ(bb_node->children().size(), 2u);
  EXPECT_EQ(bb_node->children()[0].get(), node_a);
  EXPECT_EQ(bb_node->children()[1].get(), folder_b);

  menu_button->CloseMenu();
}

class ActionAppMenuWithSearchBrowserTest : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithSearchBrowserTest() {
    search_feature_list_.InitAndEnableFeature(features::kChroMenuSearch);
  }
  ~ActionAppMenuWithSearchBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList search_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithSearchBrowserTest,
                       ShowActionAppMenuWithSearch) {
  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  EXPECT_FALSE(menu_button->IsMenuShowing());
  EXPECT_FALSE(menu_button->action_app_menu());

  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  AppMenuSearchBarView* search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_TRUE(search_bar->search_icon_for_testing()->GetVisible());

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}

class ActionAppMenuWithSearchQueryBrowserTest
    : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithSearchQueryBrowserTest() {
    search_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChroMenuSearch, {{"query", "extensions"}});
  }
  ~ActionAppMenuWithSearchQueryBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList search_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithSearchQueryBrowserTest,
                       ShowActionAppMenuWithExtensionsSearchTwice) {
  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  EXPECT_FALSE(menu_button->IsMenuShowing());
  EXPECT_FALSE(menu_button->action_app_menu());

  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  AppMenuSearchBarView* search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_EQ(search_bar->GetText(), u"extensions");

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());

  menu_button->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_button->IsMenuShowing());
  action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_EQ(search_bar->GetText(), u"extensions");

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}
