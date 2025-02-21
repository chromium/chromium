// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
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

namespace {

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

class BookmarkBarDragAndDropInteractiveTest : public InteractiveBrowserTest {
 public:
  using DragObserver =
      views::test::PollingViewObserver<bool, views::MenuItemView>;

  BookmarkBarDragAndDropInteractiveTest() = default;
  ~BookmarkBarDragAndDropInteractiveTest() override = default;
  BookmarkBarDragAndDropInteractiveTest(
      const BookmarkBarDragAndDropInteractiveTest&) = delete;
  void operator=(const BookmarkBarDragAndDropInteractiveTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
  }

  // Names the menu item at a given index of `target_view`'s submenu.
  // The usual `NameChildView` doesn't work for this case because submenus
  // have their own widgets and are not part of the parent menu item's view
  // hierarchy.
  auto NameSubmenuChild(ElementSpecifier target_view,
                        std::string name,
                        size_t index) {
    return NameViewRelative(
        target_view, name,
        base::BindLambdaForTesting([=](views::View* view) -> views::View* {
          auto* const menu = views::AsViewClass<views::MenuItemView>(view);
          CHECK(menu);
          CHECK(menu->HasSubmenu());
          return menu->GetSubmenu()->GetMenuItemAt(index);
        }));
  }

  // Names the child menu item of the currently open bookmark bar menu.
  auto NameBarMenuChild(std::string name, size_t index) {
    return NameView(
        name, base::BindLambdaForTesting([this, index]() -> views::View* {
          views::MenuItemView* const menu = BookmarkBarActiveMenu();
          CHECK(menu);
          CHECK(menu->HasSubmenu());
          CHECK_LE(index, menu->GetSubmenu()->GetMenuItems().size());
          return menu->GetSubmenu()->GetMenuItemAt(index);
        }));
  }

  // Names the bookmark bar button for the given bookmark folder.
  auto NameBookmarkButton(std::string name,
                          const bookmarks::BookmarkNode* folder) {
    return NameViewRelative(
        kBookmarkBarElementId, name,
        base::BindLambdaForTesting([=](views::View* view) -> views::View* {
          auto* const bookmark_bar = views::AsViewClass<BookmarkBarView>(view);
          CHECK(bookmark_bar);
          return bookmark_bar->GetMenuButtonForFolder(
              BookmarkParentFolder::FromFolderNode(folder));
        }));
  }

  // Waits for the menu to be shown after clicking a bookmark bar button.
  auto WaitForBookmarkBarMenuShown() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
        ui::test::PollingStateObserver<views::MenuItemView*>, kMenuShownId);
    return Steps(PollState(kMenuShownId, base::BindLambdaForTesting([this]() {
                             return BookmarkBarActiveMenu();
                           })),
                 WaitForState(kMenuShownId, testing::NotNull()));
  }

  // Returns a relative-position callback that gets the top-center point of
  // a view. The value is slightly adjusted to prevent rounding errors on
  // scaled devices.
  auto TopCenter() {
    return base::BindLambdaForTesting([](views::View* el) {
      return el->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 2);
    });
  }

  // Returns a relative-position callback that gets the center point of
  // a view.
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

                   gfx::Rect bounds = view->GetBoundsInScreen();
                   gfx::Point start_location(bounds.width() / 2,
                                             bounds.height() / 2);

                   // Send an initial mouse movement to start the drag.
                   gfx::Point target_location =
                       start_location + gfx::Vector2d(10, 10);
                   EXPECT_TRUE(ui_controls::SendMouseMove(target_location.x(),
                                                          target_location.y()));

                   // Send another mouse movement to the target desitnation.
                   target_location = std::move(pos).Run(view);
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
  views::MenuItemView* BookmarkBarActiveMenu() {
    return browser()->GetBrowserView().bookmark_bar()->GetMenu();
  }

  std::unique_ptr<DragWaiter> drag_waiter_;
};

// TODO(crbug.com/375959961): For X11, the menu is always closed on drag
// completion because the native widget's state is not properly updated.
// TODO(crbug.com/388531778): DND tests are fail on Windows and Wayland. This
// should be re-enabled once fix.
#if BUILDFLAG(IS_OZONE_X11) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_DISABLED(test_name) DISABLED_##test_name
#else
#define MAYBE_DISABLED(test_name) test_name
#endif

// TODO(crbug.com/391735476) Deflake on Mac11.
#if BUILDFLAG(IS_MAC)
#define SKIP_IF_MAC11()                                             \
  if (base::mac::MacOSMajorVersion() == 11) {                       \
    GTEST_SKIP() << "Test is flaky on Mac11 (crbug.com/391735476)"; \
  }
#else
#define SKIP_IF_MAC11()
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDrop)) {
  SKIP_IF_MAC11();

  // Add two bookmarks nodes to the bookmarks bar.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  model->AddFolder(folder, 0, u"a");
  model->AddFolder(folder, 1, u"b");

  static constexpr std::string kFolderButtonId = "button";
  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      WaitForBookmarkBarMenuShown(), NameBarMenuChild(kANodeMenuId, 0u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameBarMenuChild(kBNodeMenuId, 1u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "b" above bookmark "a".
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kBNodeMenuId, kANodeMenuId, TopCenter()),
      // The order of the bookmarks should now be swapped.
      NameBarMenuChild(kANodeMenuId, 1u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameBarMenuChild(kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDropToNestedFolder)) {
  SKIP_IF_MAC11();

  // Add two bookmarks nodes to the bookmarks bar.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  model->AddFolder(folder, 0, u"a");
  model->AddFolder(folder, 1, u"b");

  static constexpr std::string kFolderButtonId = "button";
  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      WaitForBookmarkBarMenuShown(), NameBarMenuChild(kANodeMenuId, 0u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameBarMenuChild(kBNodeMenuId, 1u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "a" onto bookmark "a".
      MoveMouseTo(kANodeMenuId),
      DragMouseTo(kANodeMenuId, kBNodeMenuId, CenterPoint()),
      // Bookmark "a" should now be a child of bookmark folder "b".
      NameBarMenuChild(kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameSubmenuChild(kBNodeMenuId, kANodeMenuId, 0u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       MAYBE_DISABLED(BookmarksDragAndDropFromNestedFolder)) {
  SKIP_IF_MAC11();

  // Add two bookmarks nodes to the bookmarks bar.
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  const bookmarks::BookmarkNode* const a_node =
      model->AddFolder(folder, 0, u"a");
  model->AddFolder(a_node, 0, u"b");

  static constexpr std::string kFolderButtonId = "button";
  static constexpr std::string kANodeMenuId = "a_node";
  static constexpr std::string kBNodeMenuId = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      WaitForBookmarkBarMenuShown(), NameBarMenuChild(kANodeMenuId, 0u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      SelectMenuItem(kANodeMenuId),
      NameSubmenuChild(kANodeMenuId, kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      // Drag bookmark "b" above bookmark "a".
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kBNodeMenuId, kANodeMenuId, TopCenter()),
      // The order of the bookmarks should now be swapped.
      NameBarMenuChild(kBNodeMenuId, 0u),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameBarMenuChild(kANodeMenuId, 1u),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"));
}
}  // namespace
