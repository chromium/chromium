// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

class AppMenuDragAndDropInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuDragAndDropInteractiveTest() = default;
  ~AppMenuDragAndDropInteractiveTest() override = default;
  AppMenuDragAndDropInteractiveTest(const AppMenuDragAndDropInteractiveTest&) =
      delete;
  void operator=(const AppMenuDragAndDropInteractiveTest&) = delete;

  // Names the menu item at a given index of `target_view`'s submenu.
  // The usual `NameChildView` doesn't work for this case because submenus
  // have their own widgets and are not part of the parent menu item's view
  // hierarchy.
  auto NameSubmenuChild(ElementSpecifier target_view,
                        std::string name,
                        size_t index) {
    return Steps(NameViewRelative(
        target_view, name,
        base::BindLambdaForTesting([=](views::View* view) -> views::View* {
          views::MenuItemView* const menu =
              views::AsViewClass<views::MenuItemView>(view);
          CHECK(menu);
          CHECK(menu->HasSubmenu());
          return menu->GetSubmenu()->GetMenuItemAt(index);
        })));
  }

  // Returns a relative-position callback that gets the top-center point of
  // a view. The value is slightly adjusted to prevent rounding errors on
  // scaled devices.
  auto TopCenter() {
    return base::BindLambdaForTesting([](ui::TrackedElement* el) {
      return el->GetScreenBounds().top_center() + gfx::Vector2d(0, 2);
    });
  }
};

// TODO(crbug.com/375959961): For X11, the menu is always closed on drag
// completion because the native widget's state is not properly updated.
// TODO(crbug.com/388531778): DND tests are flaky on Windows. This should be
// re-enabled once de-flaked.
// TODO(crbug.com/388531778): On Mac-ARM64, the "drop" portion of the drag is
// never invoked because the platform is registering the drag as "exited"
// from the view bounds.
#if BUILDFLAG(IS_OZONE_X11) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_MAC)
#define MAYBE_DISABLED(test_name) DISABLED_##test_name
#else
#define MAYBE_DISABLED(test_name) test_name
#endif

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDrop)) {
  // Add two bookmarks nodes to the bookmarks bar.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      // Open the bookmarks menu in the App menu.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      // The 9th and 10th menu items of the bookmarks menu should be the two
      // bookmarks added to the bookmarks bar.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 8u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 9u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "b" above bookmark "a".
      MoveMouseTo(kBNodeMenuId), DragMouseTo(kANodeMenuId, TopCenter()),
      // The order of the bookmarks should now be swapped.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 9u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 8u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"), );
}

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDropToNestedFolder)) {
  // Add two bookmarks nodes to the bookmarks bar.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      // Open the bookmarks menu in the App menu.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      // The 9th and 10th menu items of the bookmarks menu should be the two
      // bookmarks added to the bookmarks bar.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 8u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 9u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "a" onto bookmark folder "b".
      MoveMouseTo(kANodeMenuId), DragMouseTo(kBNodeMenuId, CenterPoint()),
      // Bookmark folder "b" should now be the 9th menu item, and bookmark "a"
      // should be in its submenu.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 8u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameSubmenuChild(kBNodeMenuId, kANodeMenuId, 0u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"));
}

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDropFromNestedFolder)) {
  // Add one bookmark folder to the bookmarks bar, and add a bookmark node to
  // the new folder.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node->children()[0].get(), 0, u"b");

  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      // Open the bookmarks menu in the App menu.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      // The 9th menu item of the bookmarks menu should be the bookmark "a".
      // The 1st menu item of bookmark folder "a" should be bookmark "b".
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 8u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      SelectMenuItem(kANodeMenuId),  // Open bookmark folder "a".
      NameSubmenuChild(kANodeMenuId, kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "b" out of bookmark folder "a".
      MoveMouseTo(kBNodeMenuId), DragMouseTo(kANodeMenuId, TopCenter()),
      // Bookmark "b" should now be the 9th menu item and bookmark folder "a"
      // should be the 10th.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 8u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 9u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"a"));
}
