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
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

// Observes a widget to track when a drag is complete.
class DragWaiter : public views::WidgetObserver {
 public:
  explicit DragWaiter(views::Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }
  ~DragWaiter() override {
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }
  DragWaiter(const DragWaiter&) = delete;
  void operator=(const DragWaiter&) = delete;

  void OnWidgetDestroying(views::Widget* widget) override {
    if (drag_loop_ && drag_loop_->running()) {
      drag_loop_->Quit();
    }
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

  void OnWidgetDragComplete(views::Widget* widget) override {
    drag_complete_ = true;
    if (drag_loop_ && drag_loop_->running()) {
      drag_loop_->Quit();
    }
  }

  void Wait() {
    if (!drag_complete_) {
      drag_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      drag_loop_->Run();
    }
  }

 private:
  raw_ptr<views::Widget> widget_ = nullptr;
  bool drag_complete_ = false;
  std::unique_ptr<base::RunLoop> drag_loop_;
};

class AppMenuDragAndDropInteractiveTest : public InteractiveBrowserTest {
 public:
  using DragObserver =
      views::test::PollingViewObserver<bool, views::MenuItemView>;

  AppMenuDragAndDropInteractiveTest() {
    // Disabled to hide the comparison tables submenu.
    scoped_feature_list_.InitAndDisableFeature(
        commerce::kProductSpecifications);
  }

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
    return base::BindLambdaForTesting([](views::View* el) {
      return el->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 2);
    });
  }
  auto CenterPoint() {
    return base::BindLambdaForTesting([](views::View* view) {
      return view->GetBoundsInScreen().CenterPoint();
    });
  }

  auto RegisterDragWaiter(ElementSpecifier target_view) {
    return WithView(target_view, [&](views::View* view) {
      auto* menu = static_cast<views::MenuItemView*>(view);
      drag_waiter_ = std::make_unique<DragWaiter>(menu->GetWidget());
    });
  }

  auto RemoveDragWaiter() {
    return Do([&]() { drag_waiter_.reset(); });
  }

  // The original "DragMouseTo" method has issues on various platforms. This
  // method uses custom logic to perform the drag.
  auto DragMouseTo(ElementSpecifier dragged_view,
                   ElementSpecifier target_view,
                   base::RepeatingCallback<gfx::Point(views::View*)> pos) {
    return Steps(
        RegisterDragWaiter(dragged_view),
        WithView(target_view,
                 [this, pos = std::move(pos)](views::View* view) {
                   base::RunLoop press_loop(
                       base::RunLoop::Type::kNestableTasksAllowed);
                   EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
                       ui_controls::MouseButton::LEFT,
                       ui_controls::MouseButtonState::DOWN,
                       press_loop.QuitClosure()));
                   press_loop.Run();

// On Mac, no initial mouse movement is needed in order to test drag events. The
// test element can be moved directly to the final target location.
#if !BUILDFLAG(IS_MAC)
                   gfx::Rect bounds = view->GetBoundsInScreen();
                   gfx::Point start_location(bounds.width() / 2,
                                             bounds.height() / 2);
                   gfx::Point initial_target_location =
                       start_location + gfx::Vector2d(10, 10);

                   // Send an initial mouse movement to start the drag.
                   EXPECT_TRUE(
                       ui_controls::SendMouseMove(initial_target_location.x(),
                                                  initial_target_location.y()));
#endif  //! BUILDFLAG(IS_MAC)

                   // Send another mouse movement to the target desitnation.
                   gfx::Point target_location = std::move(pos).Run(view);
                   EXPECT_TRUE(ui_controls::SendMouseMove(target_location.x(),
                                                          target_location.y()));

                   // Release the mouse movement.
                   EXPECT_TRUE(ui_controls::SendMouseEvents(
                       ui_controls::MouseButton::LEFT,
                       ui_controls::MouseButtonState::UP));
                   // Await drag completion.
                   drag_waiter_->Wait();
                 }),
        RemoveDragWaiter());
  }

 private:
  std::unique_ptr<DragWaiter> drag_waiter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/375959961): For X11, the menu is always closed on drag
// completion because the native widget's state is not properly updated.
// TODO(crbug.com/388531778): DND tests are flaky on Windows. This should be
// re-enabled once de-flaked.
#if BUILDFLAG(IS_OZONE_X11) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE_WAYLAND)
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
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kBNodeMenuId, kANodeMenuId, TopCenter()),
      // The order of the bookmarks should now be swapped.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 9u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 8u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"));
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
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kBNodeMenuId, kANodeMenuId, CenterPoint()),
      // Bookmark folder "b" should now be the 9th menu item, and bookmark "a"
      // should be in its submenu.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kANodeMenuId, 8u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameSubmenuChild(kANodeMenuId, kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"));
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
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kBNodeMenuId, kANodeMenuId, TopCenter()),
      // Bookmark "b" should now be the 9th menu item and bookmark folder "a"
      // should be the 10th.
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 8u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameSubmenuChild(AppMenuModel::kBookmarksMenuItem, kBNodeMenuId, 9u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"a"));
}

using AppMenuInteractiveTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(AppMenuInteractiveTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(
      // Open the App menu.
      PressButton(kToolbarAppMenuButtonElementId),
      // Close all browsers, ensure the browser process does not crash.
      Do([]() {
        chrome::CloseAllBrowsers();
        ui_test_utils::WaitForBrowserToClose();
      }));
}
