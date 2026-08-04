// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

// TODO(crbug.com/448993919): Re-enable this test on Mac and ChromeOS.
// TODO(crbug.com/388531778): DND tests fail on Windows.
#if BUILDFLAG(IS_LINUX)

namespace {

class BookmarkBarDragAndDropInteractiveTest : public InteractiveBrowserTest {
 public:
  BookmarkBarDragAndDropInteractiveTest() = default;
  ~BookmarkBarDragAndDropInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    if (base::FeatureList::IsEnabled(
            ntp_features::kNtpSimplificationBookmarkBar)) {
      browser()->GetProfile()->GetPrefs()->SetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState,
          static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
    } else {
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          bookmarks::prefs::kShowBookmarkBar, true);
    }
  }

  auto NameBookmarkButton(std::string name,
                          const bookmarks::BookmarkNode* folder) {
    return NameViewRelative(
        kBookmarkBarElementId, name,
        base::BindLambdaForTesting([=](views::View* view) -> views::View* {
          auto* const bookmark_bar = views::AsViewClass<BookmarkBarView>(view);
          if (!bookmark_bar) {
            return nullptr;
          }
          return bookmark_bar->GetMenuButtonForFolder(
              BookmarkParentFolder::FromFolderNode(folder));
        }));
  }

  auto TopCenter() {
    return base::BindOnce([](ui::TrackedElement* el) {
      auto* const view =
          views::test::InteractiveViewsTestApi::AsView<views::View>(el);
      return view->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 5);
    });
  }

  static views::MenuItemView* FindMenuItemInSubmenu(views::MenuItemView* menu,
                                                    std::u16string title) {
    if (!menu->HasSubmenu()) {
      return nullptr;
    }
    for (auto* item : menu->GetSubmenu()->GetMenuItems()) {
      if (item->title() == title) {
        return item;
      }
      if (auto* result = FindMenuItemInSubmenu(item, title)) {
        return result;
      }
    }
    return nullptr;
  }

  auto NameBarMenuChildByTitle(std::string name, std::u16string title) {
    return NameView(
        name, base::BindLambdaForTesting([this, title]() -> views::View* {
          auto* const bar =
              BrowserView::GetBrowserViewForBrowser(browser())->bookmark_bar();
          if (!bar || !bar->GetMenu()) {
            return nullptr;
          }
          return FindMenuItemInSubmenu(bar->GetMenu(), title);
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

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       BookmarksDragAndDrop) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  model->AddFolder(folder, 0, u"a");
  model->AddFolder(folder, 1, u"b");

  static constexpr char kFolderButtonId[] = "button";
  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      NameBarMenuChildByTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      PressButton(kFolderButtonId), NameBarMenuChildByTitle(kANodeMenuId, u"a"),
      NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       BookmarksDragAndDropToNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  model->AddFolder(folder, 0, u"a");
  model->AddFolder(folder, 1, u"b");

  static constexpr char kFolderButtonId[] = "button";
  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      NameBarMenuChildByTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kANodeMenuId),
      DragMouseTo(kBNodeMenuId, CenterPoint()).SetMustRemainVisible(false),
      PressButton(kFolderButtonId), NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      SelectMenuItem(kBNodeMenuId), NameBarMenuChildByTitle("a_in_b", u"a"),
      CheckViewProperty("a_in_b", &views::MenuItemView::title, u"a"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBarDragAndDropInteractiveTest,
                       BookmarksDragAndDropFromNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* const folder =
      model->AddFolder(bb_node, 0, u"folder");
  const bookmarks::BookmarkNode* const a_node =
      model->AddFolder(folder, 0, u"a");
  model->AddFolder(a_node, 0, u"b");

  static constexpr char kFolderButtonId[] = "button";
  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      NameBookmarkButton(kFolderButtonId, folder), PressButton(kFolderButtonId),
      NameBarMenuChildByTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      SelectMenuItem(kANodeMenuId), NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      PressButton(kFolderButtonId), NameBarMenuChildByTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameBarMenuChildByTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

}  // namespace

#endif  // BUILDFLAG(IS_LINUX)

namespace {

class BookmarkBarSimplifiedIPHInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  BookmarkBarSimplifiedIPHInteractiveTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHBookmarkBarSimplifiedFeature})) {}
  ~BookmarkBarSimplifiedIPHInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    // Simulate the state where the bookmark bar has been auto-hidden
    // due to inactivity.
    browser()->GetProfile()->GetPrefs()->SetInteger(
        bookmarks::prefs::kBookmarkBarVisibilityState,
        static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysHide));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BookmarkBarSimplifiedIPHInteractiveTest,
                       UndoChangesVisibilityStateToOnlyShowOnNtp) {
  RunTestSequence(
      MaybeShowPromo(feature_engagement::kIPHBookmarkBarSimplifiedFeature),
      PressNonDefaultPromoButton(),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetInteger(
                bookmarks::prefs::kBookmarkBarVisibilityState);
          },
          static_cast<int>(
              bookmarks::BookmarkBarVisibilityState::kOnlyShowOnNtp)));
}
