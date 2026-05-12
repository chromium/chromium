// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
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
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_specifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

class BrowserViewTabbedLayoutImplUiTest : public InteractiveBrowserTest {
 public:
  static constexpr char kBaselineCl[] = "7635098";

  explicit BrowserViewTabbedLayoutImplUiTest(bool disable_animations = true)
      : render_mode_resetter_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            disable_animations
                ? gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED
                : gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}

  ~BrowserViewTabbedLayoutImplUiTest() override = default;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSidePanelTestElement);

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {tabs::kVerticalTabs, features::kSidePanelFlyoverAnimation}, {});
    set_open_about_blank_on_browser_launch(true);
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

  auto ReplaceAndShowSidePanel(
      SidePanelType type,
      std::optional<int> force_preferred_width = std::nullopt) {
    return Steps(
        Do([this, type]() {
          // Note: there is a registry at browser level and one per tab; we want
          // the tab-specific one.
          auto* const registry =
              SidePanelRegistry::From(browser()->GetActiveTabInterface());
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
        If(
            [force_preferred_width] {
              return force_preferred_width.has_value();
            },
            Then(WithView(kSidePanelElementId,
                          [force_preferred_width](SidePanel* side_panel) {
                            side_panel->SetPanelWidth(*force_preferred_width);
                            side_panel->InvalidateLayout();
                          }),
                 Do([this] { RunScheduledLayouts(); }))),
        // Wait for the indicator to show.
        WaitForShow(kSidePanelTestElement),
        // Ensure that the browser gets a full layout.
        Do([this] { RunScheduledLayouts(); }));
  }

  auto SaveBounds(ElementSpecifier spec,
                  TemporaryIdentifier<gfx::Rect> temp_id) {
    return WithElement(spec, [this, temp_id](ui::TrackedElement* el) {
      SetTemporaryValue(temp_id, el->GetScreenBounds());
    });
  }

  auto CheckContains(TemporaryIdentifier<gfx::Rect> rect,
                     ElementSpecifier spec) {
    return CheckElement(
               spec,
               [=, this](ui::TrackedElement* el) {
                 return GetTemporaryValue(rect).Contains(el->GetScreenBounds());
               })
        .AddDescriptionPrefix(base::StringPrintf("CheckContains(%s)",
                                                 rect.identifier().GetName()));
  }

  auto VerifyLayout() {
    INTERACTIVE_TEST_TEMPORARY_VALUE(gfx::Rect, kBrowserBounds);
    INTERACTIVE_TEST_TEMPORARY_VALUE(gfx::Rect, kSidePanelBounds);
    return Steps(
        SaveBounds(kBrowserViewElementId, kBrowserBounds),
        SaveBounds(kSidePanelElementId, kSidePanelBounds),
        CheckContains(kBrowserBounds, kSidePanelElementId),
        CheckContains(kBrowserBounds, ToolbarView::kToolbarElementId),
        CheckContains(kBrowserBounds, kMultiContentsViewElementId),
        CheckElement(ToolbarView::kToolbarElementId,
                     [=, this](ui::TrackedElement* el) {
                       return !GetTemporaryValue(kSidePanelBounds)
                                   .Intersects(el->GetScreenBounds());
                     }),
        CheckView(
            ToolbarView::kToolbarElementId,
            [=](ToolbarView* toolbar) {
              LOG(INFO) << "Toolbar minimum size is "
                        << toolbar->GetMinimumSize().ToString();
              return toolbar->width() >= toolbar->GetMinimumSize().width();
            },
            "Toolbar is at least minimum size"));
  }

  auto CloseSidePanel() {
    return Steps(PressButton(kSidePanelCloseButtonElementId),
                 WaitForHide(kSidePanelElementId),
                 Do([this]() { RunScheduledLayouts(); }));
  }

  static int GetCornerScreenshotSize() {
    return GetLayoutConstant(LayoutConstant::kToolbarCornerRadius);
  }

  template <typename F>
  auto ScreenshotSubregion(ui::ElementSpecifier spec,
                           const std::string& name,
                           F&& func) {
    INTERACTIVE_TEST_TEMPORARY_VALUE(gfx::Rect, kSubregion);
    return Steps(
        WithElement(spec,
                    [=, this](ui::TrackedElement* el) {
                      SetTemporaryValue(kSubregion, el->GetScreenBounds());
                    })
            .AddDescriptionPrefix("Get element bounds"),
        WithView(
            kBrowserViewElementId,
            [=, this,
             func = std::forward<F>(func)](BrowserView* browser_view) mutable {
              gfx::Rect bounds = GetTemporaryValue(kSubregion);
              bounds = std::move(
                           ui::test::internal::MaybeBind(std::forward<F>(func)))
                           .Run(bounds);
              bounds.Intersect(browser_view->GetBoundsInScreen());
              SetTemporaryValue(kSubregion, views::View::ConvertRectFromScreen(
                                                browser_view, bounds));
            })
            .AddDescriptionPrefix("Apply clip function"),
        // Since the target area may go outside the bounds of the element,
        // target the BrowserView instead.
        Screenshot(kBrowserViewElementId, name, kBaselineCl,
                   [=, this]() { return GetTemporaryValue(kSubregion); }));
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

  auto ResizeToMinimumWidth() {
    auto steps = Steps(
        WithView(kBrowserViewElementId,
                 [=](BrowserView* browser_view) {
                   auto* const widget = browser_view->GetWidget();
                   widget->SetSize(gfx::Size(widget->GetMinimumSize().width(),
                                             widget->GetSize().height()));
                 }),
        Do([this]() { RunScheduledLayouts(); }));
    AddDescriptionPrefix(steps, "ResizeToMinimumWidth");
    return steps;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  const gfx::AnimationTestApi::RenderModeResetter render_mode_resetter_;
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
                       HorizontalTabsFirstTabActiveWithTabSearchUnpinned) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Test is screenshot-only."),
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(
            prefs::kTabSearchPinnedToTabstrip, false);
      }),
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
      ReplaceAndShowSidePanel(SidePanelType::kToolbar),
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
      ReplaceAndShowSidePanel(SidePanelType::kToolbar),
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

// Regression tests to ensure that when the side panel is open and the browser
// is at its minimum size, it doesn't go into content-height mode.
// Content-height mode is only for cases where the browser is already too small
// when the side panel is opened.
//
// See https://crbug.com/491484034 for rationale.

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       OpenSidePanelAtSmallWidthCollapsesLayout) {
  gfx::Rect toolbar_bounds_in_screen;
  RunTestSequence(ResizeToMinimumWidth(), SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar),
                  WithView(ToolbarView::kToolbarElementId,
                           [&](views::View* toolbar) {
                             toolbar_bounds_in_screen =
                                 toolbar->GetBoundsInScreen();
                           }),
                  CheckView(kSidePanelElementId, [&](views::View* side_panel) {
                    return side_panel->GetBoundsInScreen().y() >=
                           toolbar_bounds_in_screen.bottom();
                  }));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       TestMinimumWindowSizeWhenSidePanelIsOpen) {
  gfx::Rect toolbar_bounds_in_screen;
  RunTestSequence(SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar),
                  ResizeToMinimumWidth(),
                  WithView(ToolbarView::kToolbarElementId,
                           [&](views::View* toolbar) {
                             toolbar_bounds_in_screen =
                                 toolbar->GetBoundsInScreen();
                           }),
                  CheckView(kSidePanelElementId, [&](views::View* side_panel) {
                    return side_panel->GetBoundsInScreen().y() <
                           toolbar_bounds_in_screen.bottom();
                  }));
}

// Regression tests for cases where preferred width for side panel is
// unreasonably large or small.

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest, NormalSidePanelSize) {
  RunScheduledLayouts();
  RunTestSequence(SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar),
                  VerifyLayout());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest, SmallSidePanelSize) {
  RunScheduledLayouts();
  RunTestSequence(SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar, 100),
                  VerifyLayout());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       LargeSidePanelPreferredSize) {
  RunScheduledLayouts();
  const int large_width = browser()->GetBrowserView().width() - 20;
  RunTestSequence(SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar, large_width),
                  VerifyLayout());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplUiTest,
                       VeryLargeSidePanelPreferredSize) {
  RunScheduledLayouts();
  const int large_width = browser()->GetBrowserView().width() + 100;
  RunTestSequence(SelectTab(kBrowserViewElementId, 0),
                  ReplaceAndShowSidePanel(SidePanelType::kToolbar, large_width),
                  VerifyLayout());
}

// Regression test for limiting the amount of WebContents resizing when the
// "flyover" animation flag is enabled.
class BrowserViewTabbedLayoutImplContentLayoutUiTest
    : public BrowserViewTabbedLayoutImplUiTest,
      public views::ViewObserver {
 public:
  BrowserViewTabbedLayoutImplContentLayoutUiTest()
      : BrowserViewTabbedLayoutImplUiTest(/*disable_animations=*/false) {}
  ~BrowserViewTabbedLayoutImplContentLayoutUiTest() override = default;

  void SetUpOnMainThread() override {
    BrowserViewTabbedLayoutImplUiTest::SetUpOnMainThread();
    const auto& container_views = GetContentsContainers();
    for (ContentsContainerView* container : container_views) {
      auto* const contents = container->contents_view();
      resize_data_.emplace(contents, ResizeData());
      observation_.AddObservation(contents);
    }
  }

  void TearDownOnMainThread() override {
    observation_.RemoveAllObservations();
    resize_data_.clear();
    BrowserViewTabbedLayoutImplUiTest::TearDownOnMainThread();
  }

  auto OpenSidePanel() {
    return Steps(
        InParallel(
            RunSubsequence(ReplaceAndShowSidePanel(SidePanelType::kToolbar)),
            RunSubsequence(WaitForEvent(kSidePanelElementId,
                                        SidePanel::kOpenAnimationCompletedEvent)
                               .SetMustBeVisibleAtStart(false))),
        Do([this]() { RunScheduledLayouts(); }));
  }

  auto ClearResizeCounts() {
    return Do([this]() {
      const auto& container_views = GetContentsContainers();
      for (ContentsContainerView* container : container_views) {
        auto* contents = container->contents_view();
        auto& data = resize_data_[contents];
        data.last_size = contents->size();
        data.count = 0U;
      }
    });
  }

  template <typename... Args>
  auto CheckResizeCounts(Args&&... args) {
    return CheckResult(
        [this]() {
          const auto& container_views = GetContentsContainers();
          std::vector<size_t> counts;
          for (ContentsContainerView* container : container_views) {
            counts.push_back(resize_data_[container->contents_view()].count);
          }
          return counts;
        },
        testing::ElementsAre(
            testing::Matcher<size_t>(std::forward<Args>(args))...));
  }

  auto ToggleVerticalTabStripCollapsed(bool should_be_collapsed) {
    auto steps = Steps(
        PressButton(kVerticalTabStripCollapseButtonElementId),
        WaitForEvent(kTabStripRegionElementId,
                     VerticalTabStripRegionView::kAnimationCompletedEvent),
        Do([this]() { RunScheduledLayouts(); }),
        CheckView(
            kTabStripRegionElementId,
            [](VerticalTabStripRegionView* region) {
              return region->GetVerticalTabStripController()
                  ->GetStateController()
                  ->IsCollapsed();
            },
            should_be_collapsed));
    AddDescriptionPrefix(
        steps, base::StringPrintf("ToggleVerticalTabStripCollapsed(%s)",
                                  should_be_collapsed ? "true" : "false"));
    return steps;
  }

  auto EnterSplitView() {
    return Steps(
        Do([this]() {
          chrome::NewSplitTab(
              browser(), split_tabs::SplitTabLayout::kVertical,
              split_tabs::SplitTabCreatedSource::kToolbarButton);
        }),
        WaitForShow(MultiContentsResizeArea::kMultiContentsResizeAreaElementId),
        Do([this]() { RunScheduledLayouts(); }));
  }

 private:
  const std::vector<ContentsContainerView*>& GetContentsContainers() {
    return browser()
        ->GetBrowserView()
        .multi_contents_view()
        ->contents_container_views();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    if (auto* webview = views::AsViewClass<views::WebView>(observed_view)) {
      auto& data = resize_data_[webview];
      if (data.last_size != observed_view->size()) {
        data.last_size = observed_view->size();
        ++data.count;
      }
    }
  }

  struct ResizeData {
    gfx::Size last_size;
    size_t count = 0U;
  };

  std::map<raw_ptr<const views::WebView>, ResizeData> resize_data_;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      observation_{this};
};

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       HorizontalTabsSinglePaneOpenSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  RunScheduledLayouts();
  RunTestSequence(ClearResizeCounts(), OpenSidePanel(),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       HorizontalTabsSinglePaneCloseSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  RunScheduledLayouts();
  RunTestSequence(OpenSidePanel(), ClearResizeCounts(), CloseSidePanel(),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSinglePaneOpenSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(ClearResizeCounts(), OpenSidePanel(),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSinglePaneCloseSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(OpenSidePanel(), ClearResizeCounts(), CloseSidePanel(),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSinglePaneCollapse) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(ClearResizeCounts(), ToggleVerticalTabStripCollapsed(true),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSinglePaneExpand) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(ToggleVerticalTabStripCollapsed(true), ClearResizeCounts(),
                  ToggleVerticalTabStripCollapsed(false),
                  CheckResizeCounts(1U, 0U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       HorizontalTabsSplitViewOpenSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), ClearResizeCounts(), OpenSidePanel(),
                  CheckResizeCounts(1U, 1U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       HorizontalTabsSplitViewCloseSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), OpenSidePanel(), ClearResizeCounts(),
                  CloseSidePanel(), CheckResizeCounts(1U, 1U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSplitViewOpenSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), ClearResizeCounts(), OpenSidePanel(),
                  // There is a known issue where the elements can resize more
                  // than once. See https://crbug.com/485909751.
                  CheckResizeCounts(testing::Le(2U), testing::Le(2U)));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSplitViewCloseSidePanel) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), OpenSidePanel(), ClearResizeCounts(),
                  CloseSidePanel(),
                  // There is a known issue where the elements can resize more
                  // than once. See https://crbug.com/485909751.
                  CheckResizeCounts(testing::Le(2U), testing::Le(2U)));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSplitViewCollapse) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), ClearResizeCounts(),
                  ToggleVerticalTabStripCollapsed(true),
                  CheckResizeCounts(1U, 1U));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTabbedLayoutImplContentLayoutUiTest,
                       VerticalTabsSplitViewExpand) {
  if (!features::UseSidePanelFlyoverAnimation()) {
    GTEST_SKIP();
  }
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  RunTestSequence(EnterSplitView(), ToggleVerticalTabStripCollapsed(true),
                  ClearResizeCounts(), ToggleVerticalTabStripCollapsed(false),
                  CheckResizeCounts(1U, 1U));
}
