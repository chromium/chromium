// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

// TODO(crbug.com/498258602): Drag and drop tests are flaky on MacOS and
// ChromeOS.
// TODO(crbug.com/388531778): DND tests fail on Windows.
#if BUILDFLAG(IS_LINUX)

namespace {

class AppMenuDragAndDropInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuDragAndDropInteractiveTest() = default;
  ~AppMenuDragAndDropInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
  }

  auto TopCenter() {
    return base::BindOnce([](ui::TrackedElement* el) {
      auto* const view =
          views::test::InteractiveViewsTestApi::AsView<views::View>(el);
      return view->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 5);
    });
  }

  static views::MenuItemView* FindMenuItemWithTitle(views::View* root,
                                                    std::u16string title) {
    if (auto* const menu_item = views::AsViewClass<views::MenuItemView>(root)) {
      if (menu_item->title() == title) {
        return menu_item;
      }
    }
    for (views::View* child : root->children()) {
      if (auto* const result = FindMenuItemWithTitle(child, title)) {
        return result;
      }
    }
    return nullptr;
  }

  auto NameMenuItemWithTitle(std::string_view name, std::u16string title) {
    return NameView(name, base::BindLambdaForTesting([=]() -> views::View* {
                      for (views::Widget* widget :
                           views::test::WidgetTest::GetAllWidgets()) {
                        if (auto* const result = FindMenuItemWithTitle(
                                widget->GetRootView(), title)) {
                          return result;
                        }
                      }
                      return nullptr;
                    }));
  }

  auto CheckMenuItemBefore(ElementSpecifier item_after_spec,
                           std::u16string item_before_title) {
    return CheckView(item_after_spec, [item_before_title](
                                          views::MenuItemView* a) {
      views::View* parent = a->parent();
      for (views::View* child : parent->children()) {
        if (auto* menu_item = views::AsViewClass<views::MenuItemView>(child)) {
          if (menu_item->title() == item_before_title) {
            return true;
          }
          if (menu_item == a) {
            return false;
          }
        }
      }
      return false;
    });
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDrop) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDropToNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kANodeMenuId),
      DragMouseTo(kBNodeMenuId, CenterPoint()).SetMustRemainVisible(false),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      SelectMenuItem(kBNodeMenuId), NameMenuItemWithTitle("a_in_b", u"a"),
      CheckViewProperty("a_in_b", &views::MenuItemView::title, u"a"));
}

IN_PROC_BROWSER_TEST_F(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDropFromNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node->children()[0].get(), 0, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      SelectMenuItem(kANodeMenuId), NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

}  // namespace

#endif  // BUILDFLAG(IS_LINUX)

using AppMenuInteractiveTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(AppMenuInteractiveTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      Do([this]() { browser()->GetWindow()->Close(); }),
      WaitForHide(kBrowserViewElementId));
}
