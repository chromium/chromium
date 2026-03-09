// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_specifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class BrowserViewTabbedLayoutImplUiTest : public InteractiveBrowserTest {
 public:
  static constexpr char kBaselineCl[] = "7635098";

  BrowserViewTabbedLayoutImplUiTest() = default;

  ~BrowserViewTabbedLayoutImplUiTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({tabs::kVerticalTabs}, {});
    set_open_about_blank_on_browser_launch(true);
    gfx::Animation::SetPrefersReducedMotionForTesting(true);
    InteractiveBrowserTest::SetUp();
  }

  void TearDown() override {
    gfx::Animation::SetPrefersReducedMotionForTesting(false);
    InteractiveBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  }

  static int GetCornerScreenshotSize() {
    return GetLayoutConstant(LayoutConstant::kToolbarCornerRadius);
  }

  using Bounds = base::RefCountedData<gfx::Rect>;

  template <typename F>
  auto ScreenshotSubregion(ui::ElementSpecifier spec,
                           const std::string& name,
                           F&& func) {
    const auto bounds = base::MakeRefCounted<Bounds>();
    return Steps(
        WithElement(spec,
                    [bounds](ui::TrackedElement* el) {
                      bounds->data = el->GetScreenBounds();
                    })
            .AddDescriptionPrefix("Get element bounds"),
        WithView(kBrowserViewElementId,
                 [bounds, func = std::forward<F>(func)](
                     BrowserView* browser_view) mutable {
                   bounds->data = std::move(ui::test::internal::MaybeBind(
                                                std::forward<F>(func)))
                                      .Run(bounds->data);
                   bounds->data.Intersect(browser_view->GetBoundsInScreen());
                   bounds->data = views::View::ConvertRectFromScreen(
                       browser_view, bounds->data);
                 })
            .AddDescriptionPrefix("Apply clip function"),
        // Since the target area may go outside the bounds of the element,
        // target the BrowserView instead.
        Screenshot(kBrowserViewElementId, name, kBaselineCl,
                   [bounds]() mutable { return bounds->data; }));
  }

  auto ScreenshotLeft(ui::ElementSpecifier spec,
                      const std::string& name,
                      int outset) {
    return ScreenshotSubregion(spec, name, [outset](gfx::Rect rect) {
      rect.set_width(GetCornerScreenshotSize());
      rect.Outset(outset);
      return rect;
    });
  }

  auto ScreenshotRight(ui::ElementSpecifier spec,
                       const std::string& name,
                       int outset) {
    return ScreenshotSubregion(spec, name, [outset](gfx::Rect rect) {
      rect.set_x(rect.right() - GetCornerScreenshotSize());
      rect.set_width(GetCornerScreenshotSize());
      rect.Outset(outset);
      return rect;
    });
  }

  auto ScreenshotTop(ui::ElementSpecifier spec,
                     const std::string& name,
                     int outset) {
    return ScreenshotSubregion(spec, name, [outset](gfx::Rect rect) {
      rect.set_height(GetCornerScreenshotSize());
      rect.Outset(outset);
      return rect;
    });
  }

  auto ScreenshotBottom(ui::ElementSpecifier spec,
                        const std::string& name,
                        int outset) {
    return ScreenshotSubregion(spec, name, [outset](gfx::Rect rect) {
      rect.set_y(rect.bottom() - GetCornerScreenshotSize());
      rect.set_height(GetCornerScreenshotSize());
      rect.Outset(outset);
      return rect;
    });
  }

  auto ScreenshotAround(ui::ElementSpecifier spec,
                        const std::string& name,
                        int overlap) {
    return ScreenshotSubregion(spec, name, [overlap](gfx::Rect rect) {
      rect.Outset(overlap);
      return rect;
    });
  }

  auto ScreenshotUpperLeft(ui::ElementSpecifier spec,
                           const std::string& name,
                           int overlap) {
    return ScreenshotSubregion(spec, name, [overlap](gfx::Rect rect) {
      rect.Offset(-overlap, -overlap);
      const int size = GetCornerScreenshotSize() + overlap;
      rect.set_size(gfx::Size(size, size));
      return rect;
    });
  }

  auto ScreenshotUpperRight(ui::ElementSpecifier spec,
                            const std::string& name,
                            int overlap) {
    return ScreenshotSubregion(spec, name, [overlap](gfx::Rect rect) {
      rect.set_x(rect.right() - GetCornerScreenshotSize());
      rect.set_y(rect.y() - overlap);
      const int size = GetCornerScreenshotSize() + overlap;
      rect.set_size(gfx::Size(size, size));
      return rect;
    });
  }

  auto ScreenshotLowerRight(ui::ElementSpecifier spec,
                            const std::string& name,
                            int overlap) {
    return ScreenshotSubregion(spec, name, [overlap](gfx::Rect rect) {
      rect.set_x(rect.right() - GetCornerScreenshotSize());
      rect.set_y(rect.bottom() - GetCornerScreenshotSize());
      const int size = GetCornerScreenshotSize() + overlap;
      rect.set_size(gfx::Size(size, size));
      return rect;
    });
  }

  auto ScreenshotLowerLeft(ui::ElementSpecifier spec,
                           const std::string& name,
                           int overlap) {
    return ScreenshotSubregion(spec, name, [overlap](gfx::Rect rect) {
      rect.set_x(rect.x() - overlap);
      rect.set_y(rect.bottom() - GetCornerScreenshotSize());
      const int size = GetCornerScreenshotSize() + overlap;
      rect.set_size(gfx::Size(size, size));
      return rect;
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       HorizontalTabsFirstTabActive) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      SelectTab(kBrowserViewElementId, 0),
      ScreenshotLeft(kTabStripRegionElementId, "tabstrip_leading", 3),
      ScreenshotRight(kTabStripRegionElementId, "tabstrip_trailing", 3),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       HorizontalTabsSecondTabActive) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      SelectTab(kBrowserViewElementId, 1),
      ScreenshotLeft(kTabStripRegionElementId, "tabstrip_leading", 3),
      ScreenshotRight(kTabStripRegionElementId, "tabstrip_trailing", 3),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       VerticalTabsWithoutBookmarks) {
  PrefService* const prefs = user_prefs::UserPrefs::Get(GetProfile());
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(
      WaitForShow(kVerticalTabStripRegionElementId),
      WaitForHide(kBookmarkBarElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kVerticalTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kVerticalTabStripRegionElementId, "tabstrip_bottom", 3),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripTopCornerElementId,
          "top_corner", 5),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripBottomCornerElementId,
          "bottom_corner", 5),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       VerticalTabsWithBookmarks) {
  PrefService* const prefs = user_prefs::UserPrefs::Get(GetProfile());
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(
      WaitForShow(kVerticalTabStripRegionElementId),
      WaitForShow(kBookmarkBarElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kVerticalTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kVerticalTabStripRegionElementId, "tabstrip_bottom", 3),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripTopCornerElementId,
          "top_corner", 5),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripBottomCornerElementId,
          "bottom_corner", 5),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3),
      ScreenshotLeft(kBookmarkBarElementId, "bookmarks_leading", 3),
      ScreenshotRight(kBookmarkBarElementId, "bookmarks_trailing", 3));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       VerticalTabsCollapsed) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(
      PressButton(kVerticalTabStripCollapseButtonElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kVerticalTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kVerticalTabStripRegionElementId, "tabstrip_bottom", 3),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripTopCornerElementId,
          "top_corner", 5),
      ScreenshotAround(
          BrowserViewLayoutViews::kVerticalTabStripBottomCornerElementId,
          "bottom_corner", 5),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3));
}
