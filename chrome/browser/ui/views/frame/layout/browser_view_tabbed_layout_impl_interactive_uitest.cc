// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_specifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

class BrowserViewTabbedLayoutImplUiTest : public InteractiveBrowserTest {
 public:
  static constexpr char kBaselineCl[] = "7635098";

  BrowserViewTabbedLayoutImplUiTest() = default;

  ~BrowserViewTabbedLayoutImplUiTest() override = default;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSidePanelTestElement);

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

  auto ReplaceAndShowSidePanel(SidePanelEntry::PanelType type) {
    return Steps(
        Do([this, type]() {
          // Note: there is a registry at browser level and one per tab; we want
          // the tab-specific one.
          auto* const registry = browser()
                                     ->GetActiveTabInterface()
                                     ->GetTabFeatures()
                                     ->side_panel_registry();
          const auto key =
              SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome);
          CHECK(!registry->GetEntryForKey(key) || registry->Deregister(key));
          CHECK(registry->Register(std::make_unique<SidePanelEntry>(
              type, key, base::BindRepeating([](SidePanelEntryScope&) {
                auto content_view = std::make_unique<views::View>();
                content_view->SetLayoutManager(
                    std::make_unique<views::FillLayout>());
                auto label =
                    std::make_unique<views::Label>(u"Side Panel Ready");
                label->SetProperty(views::kElementIdentifierKey,
                                   kSidePanelTestElement);
                content_view->AddChildView(std::move(label));
                return content_view;
              }),
              /*default_content_width_callback=*/base::NullCallback())));
          browser()->browser_window_features()->side_panel_ui()->Show(
              SidePanelEntry::Id::kCustomizeChrome);
        }),
        WaitForShow(kSidePanelElementId),
        // Wait for the indicator to show.
        WaitForShow(kSidePanelTestElement));
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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BrowserViewTabbedLayoutImplUiTest,
                                      kSidePanelTestElement);

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
      WaitForShow(kTabStripRegionElementId), WaitForHide(kBookmarkBarElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kTabStripRegionElementId, "tabstrip_bottom", 3),
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
      WaitForShow(kTabStripRegionElementId), WaitForShow(kBookmarkBarElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kTabStripRegionElementId, "tabstrip_bottom", 3),
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
      ScreenshotTop(kTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kTabStripRegionElementId, "tabstrip_bottom", 3),
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
                       HorizontalTabsWithToolbarHeightSidePanel) {
  RunTestSequence(
      SelectTab(kBrowserViewElementId, 0),
      ReplaceAndShowSidePanel(SidePanelEntry::PanelType::kToolbar),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotLeft(kTabStripRegionElementId, "tabstrip_leading", 3),
      ScreenshotRight(kTabStripRegionElementId, "tabstrip_trailing", 3),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3),
      ScreenshotUpperLeft(BrowserViewLayoutViews::kShadowOverlayElementId,
                          "shadow_upper_leading", 3),
      ScreenshotUpperRight(BrowserViewLayoutViews::kShadowOverlayElementId,
                           "shadow_upper_trailing", 3),
      ScreenshotLowerRight(BrowserViewLayoutViews::kShadowOverlayElementId,
                           "shadow_lower_trailing", 3),
      ScreenshotLowerLeft(BrowserViewLayoutViews::kShadowOverlayElementId,
                          "shadow_lower_leading", 3),
      ScreenshotUpperLeft(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_upper_leading", 3),
      ScreenshotUpperRight(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_upper_trailing", 3),
      ScreenshotLowerRight(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_lower_trailing", 3),
      ScreenshotLowerLeft(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_lower_leading", 3),
      ScreenshotTop(kSidePanelElementId, "side_panel_top", 8),
      ScreenshotBottom(kSidePanelElementId, "side_panel_bottom", 8));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       VerticalTabsWithToolbarHeightSidePanel) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(
      WaitForShow(kTabStripRegionElementId),
      SelectTab(kBrowserViewElementId, 0),
      ReplaceAndShowSidePanel(SidePanelEntry::PanelType::kToolbar),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      ScreenshotTop(kTabStripRegionElementId, "tabstrip_top", 3),
      ScreenshotBottom(kTabStripRegionElementId, "tabstrip_bottom", 3),
      ScreenshotLeft(ToolbarView::kToolbarElementId, "toolbar_leading", 3),
      ScreenshotRight(ToolbarView::kToolbarElementId, "toolbar_trailing", 3),
      ScreenshotUpperLeft(BrowserViewLayoutViews::kShadowOverlayElementId,
                          "shadow_upper_leading", 3),
      ScreenshotUpperRight(BrowserViewLayoutViews::kShadowOverlayElementId,
                           "shadow_upper_trailing", 3),
      ScreenshotLowerRight(BrowserViewLayoutViews::kShadowOverlayElementId,
                           "shadow_lower_trailing", 3),
      ScreenshotLowerLeft(BrowserViewLayoutViews::kShadowOverlayElementId,
                          "shadow_lower_leading", 3),
      ScreenshotUpperLeft(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_upper_leading", 3),
      ScreenshotUpperRight(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_upper_trailing", 3),
      ScreenshotLowerRight(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_lower_trailing", 3),
      ScreenshotLowerLeft(
          BrowserViewLayoutViews::kMainBackgroundRegionElementId,
          "main_lower_leading", 3),
      ScreenshotTop(kSidePanelElementId, "side_panel_top", 8),
      ScreenshotBottom(kSidePanelElementId, "side_panel_bottom", 8));
}
