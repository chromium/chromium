// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl_old.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// The number of pixels the constrained window should overlap the bottom
// of the omnibox.
const int kConstrainedWindowOverlap = 3;

bool ShouldUseBrowserContentMinimumSize(Browser* browser) {
  if (!browser) {
    CHECK_IS_TEST();
    return false;
  }
  if (browser->is_type_normal()) {
    return true;
  }
  bool is_web_app = browser->is_type_app() &&
                    web_app::AppBrowserController::IsWebApp(browser);
#if BUILDFLAG(IS_CHROMEOS)
  // app_controller() is only available if the BrowserView is a WebAppType.
  is_web_app = is_web_app && !browser->app_controller()->system_app();
#endif
  return is_web_app;
}

// The normal clipping created by `View::Paint()` may not cover the bottom of
// the TopContainerView at certain scale factor because both of the position and
// the height might be rounded down. This function sets the clip path that
// enlarges the height at 2 DPs to compensate this error (both origin and size)
// that the canvas can cover the entire TopContainerView.  See
// crbug.com/390669712 for more details.  TODO(crbug.com/41344902): Remove this
// hack once the pixel canvas is enabled on all aura platforms.  Note that macOS
// supports integer scale only, so this isn't necessary on macOS.
void SetClipPathWithBottomAllowance(views::View* view) {
  if (!features::IsPixelCanvasRecordingEnabled()) {
    constexpr int kBottomPaintAllowance = 2;
    const gfx::Rect local_bounds = view->GetLocalBounds();
    const int extended_height = local_bounds.height() + kBottomPaintAllowance;
    view->SetClipPath(
        SkPath::Rect(SkRect::MakeWH(local_bounds.width(), extended_height)));
  }
}

}  // namespace

struct BrowserViewLayoutImplOld::ContentsContainerLayoutResult {
  gfx::Rect contents_container_bounds;
  gfx::Rect side_panel_bounds;
  bool side_panel_visible;
  bool side_panel_right_aligned;
  bool contents_container_after_side_panel;
  gfx::Rect separator_bounds;
};

BrowserViewLayoutImplOld::BrowserViewLayoutImplOld(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayout(std::move(delegate), browser, std::move(views)),
      use_browser_content_minimum_size_(
          ShouldUseBrowserContentMinimumSize(browser)) {}

BrowserViewLayoutImplOld::~BrowserViewLayoutImplOld() = default;

void BrowserViewLayoutImplOld::Layout(views::View* browser_view) {
  TRACE_EVENT0("ui", "BrowserViewLayoutImplOld::Layout");
  gfx::Rect available_bounds = browser_view->GetLocalBounds();

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    views().window_scrim->SetBoundsRect(available_bounds);
  }

  if (delegate().ShouldDrawVerticalTabStrip()) {
    LayoutVerticalTabStrip(available_bounds);
  }

  gfx::Rect main_container_bounds = available_bounds;
  main_container_bounds.set_y(available_bounds.y() +
                              delegate().GetTopInsetInBrowserView());

  LayoutTitleBarForWebApp(main_container_bounds);

  if (delegate().ShouldLayoutTabStrip()) {
    LayoutTabStripRegion(main_container_bounds);
    LayoutWebUITabStrip(main_container_bounds);
  }
  LayoutToolbar(main_container_bounds);

  dialog_top_y_ = main_container_bounds.y() - kConstrainedWindowOverlap;

  LayoutBookmarkAndInfoBars(main_container_bounds);

  // Top container requires updated toolbar and bookmark bar to compute bounds.
  UpdateTopContainerBounds(main_container_bounds);

  // Layout the contents container in the remaining space.
  // Ensure `available_bounds` has the correct height.
  main_container_bounds.set_height(main_container_bounds.height() -
                                   main_container_bounds.y());
  LayoutContentsContainerView(main_container_bounds);

  UpdateBubbles();
}

gfx::Size BrowserViewLayoutImplOld::GetMinimumSize(
    const views::View* host) const {
  // Prevent having a 0x0 sized-contents as this can allow the window to be
  // resized down such that it's invisible and can no longer accept events.
  // Use a very small 1x1 size to allow the user and the web contents to be able
  // to resize the window as small as possible without introducing bugs.
  // https://crbug.com/847179.
  constexpr gfx::Size kContentsMinimumSize(1, 1);
  if (delegate().GetBorderlessModeEnabled()) {
    // The minimum size of a window is unrestricted for a borderless mode app.
    return kContentsMinimumSize;
  }

  // The minimum height for the normal (tabbed) browser window's contents area.
  constexpr int kMainBrowserContentsMinimumHeight = 1;

  const bool has_tabstrip = delegate().SupportsWindowFeature(
      Browser::WindowFeature::kFeatureTabStrip);
  const bool has_toolbar =
      delegate().SupportsWindowFeature(Browser::WindowFeature::kFeatureToolbar);
  const bool has_location_bar = delegate().SupportsWindowFeature(
      Browser::WindowFeature::kFeatureLocationBar);
  const bool has_bookmarks_bar =
      views().bookmark_bar && views().bookmark_bar->GetVisible() &&
      delegate().SupportsWindowFeature(
          Browser::WindowFeature::kFeatureBookmarkBar);

  // TODO(crbug.com/437917495): Verify all callers have the correct bounds in
  // vertical and horizontal tabstrip modes.
  gfx::Size tabstrip_size(has_tabstrip
                              ? views().tab_strip_region_view->GetMinimumSize()
                              : gfx::Size());
  gfx::Size toolbar_size((has_toolbar || has_location_bar)
                             ? views().toolbar->GetMinimumSize()
                             : gfx::Size());
  gfx::Size bookmark_bar_size;
  if (has_bookmarks_bar) {
    bookmark_bar_size = views().bookmark_bar->GetMinimumSize();
  }
  gfx::Size infobar_container_size(views().infobar_container->GetMinimumSize());
  // TODO(pkotwicz): Adjust the minimum height for the find bar.

  gfx::Size contents_size(views().contents_container->GetMinimumSize());
  contents_size.SetToMax(use_browser_content_minimum_size_
                             ? gfx::Size(kMainBrowserContentsMinimumWidth,
                                         kMainBrowserContentsMinimumHeight)
                             : kContentsMinimumSize);

  const int min_height =
      delegate().GetTopInsetInBrowserView() + tabstrip_size.height() +
      toolbar_size.height() + bookmark_bar_size.height() +
      infobar_container_size.height() + contents_size.height();

  const int min_width = std::max(
      {tabstrip_size.width(), toolbar_size.width(), bookmark_bar_size.width(),
       infobar_container_size.width(), contents_size.width()});

  return gfx::Size(min_width, min_height);
}

int BrowserViewLayoutImplOld::GetMinWebContentsWidthForTesting() const {
  return GetMinWebContentsWidth();
}

BrowserViewLayoutImplOld::ContentsContainerLayoutResult
BrowserViewLayoutImplOld::CalculateContentsContainerLayout(
    const gfx::Rect& available_bounds) const {
  gfx::Rect contents_container_bounds = available_bounds;
  int vertical_tab_offset = 0;
  if (delegate().ShouldDrawVerticalTabStrip()) {
    vertical_tab_offset = kMinVerticalTabStripWidth;
    contents_container_bounds.set_width(available_bounds.width() -
                                        vertical_tab_offset);
  }

  if (views().webui_tab_strip && views().webui_tab_strip->GetVisible()) {
    // The WebUI tab strip container should "push" the tab contents down without
    // resizing it.
    contents_container_bounds.Inset(
        gfx::Insets().set_bottom(-views().webui_tab_strip->size().height()));
  }

  const bool side_panel_visible =
      views().contents_height_side_panel &&
      views().contents_height_side_panel->GetVisible();
  if (!side_panel_visible) {
    // The contents container takes all available space, and we're done.
    return ContentsContainerLayoutResult{contents_container_bounds,
                                         gfx::Rect(),
                                         false,
                                         false,
                                         false,
                                         gfx::Rect()};
  }

  SidePanel* const side_panel = views().contents_height_side_panel;

  const bool side_panel_right_aligned = side_panel->IsRightAligned();
  views::View* side_panel_separator =
      side_panel_right_aligned
          ? views().right_aligned_side_panel_separator.get()
          : views().left_aligned_side_panel_separator.get();
  const int separator_width =
      !side_panel_separator ? 0
                            : side_panel_separator->GetPreferredSize().width();

  // Side panel occupies some of the container's space. The side panel should
  // never occupy more space than is available in the content window, and
  // should never force the web contents to be smaller than its intended
  // minimum.
  gfx::Rect side_panel_bounds = contents_container_bounds;

  // If necessary, cap the side panel width at 2/3rds of the contents container
  // width as long as the side panel remains at or above its minimum width.
  if (side_panel->ShouldRestrictMaxWidth()) {
    side_panel_bounds.set_width(
        std::max(std::min(side_panel->GetPreferredSize().width(),
                          contents_container_bounds.width() * 2 / 3),
                 side_panel->GetMinimumSize().width()));
  } else {
    side_panel_bounds.set_width(std::min(side_panel->GetPreferredSize().width(),
                                         contents_container_bounds.width() -
                                             GetMinWebContentsWidth() -
                                             separator_width));
  }

  double side_panel_visible_width =
      side_panel_bounds.width() *
      views::AsViewClass<SidePanel>(views().contents_height_side_panel)
          ->GetAnimationValue();

  // Shrink container bounds to fit the side panel.
  contents_container_bounds.set_width(contents_container_bounds.width() -
                                      side_panel_visible_width -
                                      separator_width);

  // In LTR, the point (0,0) represents the top left of the browser.
  // In RTL, the point (0,0) represents the top right of the browser.
  const bool contents_container_after_side_panel =
      (base::i18n::IsRTL() && side_panel_right_aligned) ||
      (!base::i18n::IsRTL() && !side_panel_right_aligned);

  if (contents_container_after_side_panel) {
    // When the side panel should appear before the main content area relative
    // to the ui direction, move `contents_container_bounds` after the side
    // panel. Also leave space for the separator.
    contents_container_bounds.set_x(side_panel_visible_width + separator_width +
                                    vertical_tab_offset);
    side_panel_bounds.set_x(side_panel_bounds.x() - (side_panel_bounds.width() -
                                                     side_panel_visible_width));
  } else {
    // When the side panel should appear after the main content area relative to
    // the ui direction, move `side_panel_bounds` after the main content area.
    // Also leave space for the separator.
    side_panel_bounds.set_x(contents_container_bounds.right() +
                            separator_width);
  }

  // Adjust the side panel separator bounds based on the side panel bounds
  // calculated above.
  gfx::Rect separator_bounds = side_panel_bounds;
  // TODO (https://crbug.com/389972209): Adding 1px to the width as a bandaid
  // fix. This covers a case with subpixeling where a thin line of the
  // background finds its way to the front.
  separator_bounds.set_width(separator_width + 1);
  // If the side panel appears before `contents_container_bounds`, place the
  // separator immediately after the side panel but before the container
  // bounds. If the side panel appears after `contents_container_bounds`,
  // place the separator immediately after the contents bounds but before the
  // side panel.
  separator_bounds.set_x(contents_container_after_side_panel
                             ? side_panel_bounds.right()
                             : contents_container_bounds.right());

  return ContentsContainerLayoutResult{contents_container_bounds,
                                       side_panel_bounds,
                                       side_panel_visible,
                                       side_panel_right_aligned,
                                       contents_container_after_side_panel,
                                       separator_bounds};
}

bool BrowserViewLayout::IsInfobarVisible() const {
  return !views().infobar_container->IsEmpty() &&
         (!views().browser_view->GetWidget()->IsFullscreen() ||
          !views().infobar_container->ShouldHideInFullscreen());
}

bool BrowserViewLayout::IsInfobarVisibleForTesting() const {
  return IsInfobarVisible();
}

void BrowserViewLayout::SetDelegateForTesting(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate) {
  delegate_ = std::move(delegate);
  views().browser_view->InvalidateLayout();
}

void BrowserViewLayoutImplOld::LayoutTitleBarForWebApp(
    gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTitleBarForWebApp");
  if (!views().web_app_frame_toolbar) {
    return;
  }

  if (delegate().GetBorderlessModeEnabled()) {
    views().web_app_frame_toolbar->SetVisible(false);
    if (views().web_app_window_title) {
      views().web_app_window_title->SetVisible(false);
    }
    return;
  }

  gfx::Rect toolbar_bounds(
      delegate().GetBoundsForWebAppFrameToolbarInBrowserView());

  views().web_app_frame_toolbar->SetVisible(!toolbar_bounds.IsEmpty());
  if (views().web_app_window_title) {
    views().web_app_window_title->SetVisible(!toolbar_bounds.IsEmpty());
  }
  if (toolbar_bounds.IsEmpty()) {
    return;
  }

  if (delegate().IsWindowControlsOverlayEnabled()) {
    views().web_app_frame_toolbar->LayoutForWindowControlsOverlay(
        toolbar_bounds);
    toolbar_bounds.Subtract(views().web_app_frame_toolbar->bounds());
    delegate().UpdateWindowControlsOverlay(toolbar_bounds);
    if (views().web_app_window_title) {
      views().web_app_window_title->SetVisible(false);
    }
    return;
  }

  gfx::Rect window_title_bounds =
      views().web_app_frame_toolbar->LayoutInContainer(toolbar_bounds);

  if (views().web_app_window_title) {
    if (delegate().ShouldDrawTabStrip()) {
      views().web_app_window_title->SetVisible(false);
    } else {
      delegate().LayoutWebAppWindowTitle(window_title_bounds,
                                         *views().web_app_window_title);
    }
  }

  available_bounds.set_y(toolbar_bounds.bottom());
}

void BrowserViewLayoutImplOld::LayoutVerticalTabStrip(
    gfx::Rect& available_bounds) {
  if (views().vertical_tab_strip_container &&
      views().vertical_tab_strip_container->GetVisible()) {
    views().vertical_tab_strip_container->SetBounds(
        available_bounds.x(), available_bounds.y(), kMinVerticalTabStripWidth,
        available_bounds.height());
    available_bounds.set_x(available_bounds.x() + kMinVerticalTabStripWidth);
  }
}

void BrowserViewLayoutImplOld::LayoutTabStripRegion(
    gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");
  if (!delegate().ShouldDrawTabStrip()) {
    SetViewVisibility(views().tab_strip_region_view, false);
    views().tab_strip_region_view->SetBounds(0, 0, 0, 0);
    return;
  }
  // This retrieves the bounds for the tab strip based on whether or not we show
  // anything to the left of it, like the incognito avatar.
  gfx::Rect tab_strip_region_bounds(
      delegate().GetBoundsForTabStripRegionInBrowserView());

  if (views().web_app_frame_toolbar) {
    tab_strip_region_bounds.Inset(gfx::Insets::TLBR(
        0, 0, 0, views().web_app_frame_toolbar->GetPreferredSize().width()));
  }

  if (delegate().ShouldDrawVerticalTabStrip()) {
    SetViewVisibility(views().tab_strip_region_view, false);
  } else {
    SetViewVisibility(views().tab_strip_region_view, true);
    views().tab_strip_region_view->SetBoundsRect(tab_strip_region_bounds);
    available_bounds.set_y(tab_strip_region_bounds.bottom() -
                           GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
  }
}

void BrowserViewLayoutImplOld::LayoutWebUITabStrip(
    gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutWebUITabStrip");
  if (!views().webui_tab_strip) {
    return;
  }
  if (!views().webui_tab_strip->GetVisible()) {
    views().webui_tab_strip->SetBoundsRect(gfx::Rect());
    return;
  }
  views().webui_tab_strip->SetBounds(
      available_bounds.x(), available_bounds.y(), available_bounds.width(),
      views().webui_tab_strip->GetHeightForWidth(available_bounds.width()));
  available_bounds.set_y(views().webui_tab_strip->bounds().bottom());
}

void BrowserViewLayoutImplOld::LayoutToolbar(gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutToolbar");
  bool toolbar_visible = delegate().IsToolbarVisible();
  SetViewVisibility(views().toolbar, toolbar_visible);

  if (delegate().ShouldDrawVerticalTabStrip()) {
    gfx::Rect toolbar_bounds(
        delegate().GetBoundsForToolbarInVerticalTabBrowserView());
    toolbar_bounds.set_x(available_bounds.x());
    toolbar_bounds.set_width(toolbar_bounds.width() -
                             kMinVerticalTabStripWidth);
    views().toolbar->SetBoundsRect(toolbar_bounds);
  } else {
    int height =
        toolbar_visible ? views().toolbar->GetPreferredSize().height() : 0;
    int width = toolbar_visible ? available_bounds.width() : 0;
    views().toolbar->SetBounds(available_bounds.x(), available_bounds.y(),
                               width, height);
  }

  SetClipPathWithBottomAllowance(views().toolbar);
  available_bounds.set_y(views().toolbar->bounds().bottom());
}

void BrowserViewLayoutImplOld::LayoutBookmarkAndInfoBars(
    gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutBookmarkAndInfoBars");

  if (views().bookmark_bar) {
    available_bounds.set_y(
        std::max(views().toolbar->bounds().bottom(), available_bounds.y()));
    LayoutBookmarkBar(available_bounds);
  }

  if (delegate().IsContentsSeparatorEnabled() &&
      (views().toolbar->GetVisible() || views().bookmark_bar) &&
      available_bounds.y() > 0) {
    int separator_height = 0;
    if (views().multi_contents_view) {
      // Show top container separator when infobar is visible and for immersive
      // full screen without always showing toolbar.
      SetViewVisibility(
          views().top_container_separator,
          IsInfobarVisible() || IsImmersiveModeEnabledWithoutToolbar());

      if (views().top_container_separator->GetVisible()) {
        separator_height =
            views().top_container_separator->GetPreferredSize().height();
        views().top_container_separator->SetBounds(
            available_bounds.x(), available_bounds.y(),
            available_bounds.width(), separator_height);
      }
      // If the loading bar will be shown, it's supposed to replace the
      // separator in the content area.
      views().multi_contents_view->SetShouldShowTopSeparator(
          !views().loading_bar &&
          !views().top_container_separator->GetVisible());
    } else {
      separator_height =
          views().top_container_separator->GetPreferredSize().height();
      SetViewVisibility(views().top_container_separator, true);
      views().top_container_separator->SetBounds(
          available_bounds.x(), available_bounds.y(), available_bounds.width(),
          separator_height);
    }

    if (views().loading_bar) {
      SetViewVisibility(views().loading_bar, true);
      views().loading_bar->SetBounds(
          available_bounds.x(), available_bounds.y() - 2,
          available_bounds.width(), separator_height + 2);
      views().top_container->ReorderChildView(
          views().loading_bar, views().top_container->children().size());
    }
    available_bounds.set_y(available_bounds.y() + separator_height);
  } else {
    SetViewVisibility(views().top_container_separator, false);
    if (views().multi_contents_view) {
      views().multi_contents_view->SetShouldShowTopSeparator(false);
    }
    if (views().loading_bar) {
      SetViewVisibility(views().loading_bar, false);
    }
  }

  LayoutInfoBar(available_bounds);
}

void BrowserViewLayoutImplOld::LayoutBookmarkBar(gfx::Rect& available_bounds) {
  if (!delegate().IsBookmarkBarVisible()) {
    SetViewVisibility(views().bookmark_bar, false);
    // TODO(jamescook): Don't change the bookmark bar height when it is
    // invisible, so we can use its height for layout even in that state.
    views().bookmark_bar->SetBounds(0, available_bounds.y(),
                                    views().browser_view->width(), 0);
    return;
  }

  views().bookmark_bar->SetInfoBarVisible(IsInfobarVisible());
  int bookmark_bar_height = views().bookmark_bar->GetPreferredSize().height();
  views().bookmark_bar->SetBounds(available_bounds.x(), available_bounds.y(),
                                  available_bounds.width(),
                                  bookmark_bar_height);
  SetClipPathWithBottomAllowance(views().bookmark_bar);

  // Set visibility after setting bounds, as the visibility update uses the
  // bounds to determine if the mouse is hovering over a button.
  SetViewVisibility(views().bookmark_bar, true);
  available_bounds.set_y(available_bounds.y() + bookmark_bar_height);
}

void BrowserViewLayoutImplOld::LayoutInfoBar(gfx::Rect& available_bounds) {
  // In immersive fullscreen or when top-chrome is fully hidden due to the page
  // gesture scroll slide behavior, the infobar always starts near the top of
  // the screen.
  const ImmersiveModeController* immersive_mode_controller =
      delegate().GetImmersiveModeController();
  int top = available_bounds.y();
  if (immersive_mode_controller->IsEnabled() ||
      (delegate().IsTopControlsSlideBehaviorEnabled() &&
       delegate().GetTopControlsSlideBehaviorShownRatio() == 0.f)) {
    // Can be null in tests.
    top = immersive_mode_controller->GetMinimumContentOffset();
  }
  // The content usually starts at the bottom of the infobar. When there is an
  // extra infobar offset the infobar is shifted down while the content stays.
  int infobar_top = top;
  int content_top = infobar_top + views().infobar_container->height();
  infobar_top += delegate().GetExtraInfobarOffset();
  SetViewVisibility(views().infobar_container, IsInfobarVisible());
  if (views().infobar_container->GetVisible()) {
    views().infobar_container->SetBounds(
        available_bounds.x(), infobar_top, available_bounds.width(),
        views().infobar_container->GetPreferredSize().height());
  } else {
    views().infobar_container->SetBounds(available_bounds.x(), infobar_top, 0,
                                         0);
  }
  available_bounds.set_y(content_top);
}

void BrowserViewLayoutImplOld::LayoutContentsContainerView(
    const gfx::Rect& available_bounds) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutContentsContainerView");
  // |main_contents_region_| contains web page contents, side panel and
  // devtools. See browser_view.h for details.

  ContentsContainerLayoutResult layout_result =
      CalculateContentsContainerLayout(available_bounds);
  views().contents_container->SetBoundsRect(
      layout_result.contents_container_bounds);

  if (views().contents_height_side_panel) {
    views().contents_height_side_panel->SetBoundsRect(
        layout_result.side_panel_bounds);
  }

  if (views().multi_contents_view) {
    views().multi_contents_view->SetShouldShowLeadingSeparator(
        layout_result.side_panel_visible &&
        (layout_result.side_panel_right_aligned == base::i18n::IsRTL()));

    views().multi_contents_view->SetShouldShowTrailingSeparator(
        layout_result.side_panel_visible &&
        (layout_result.side_panel_right_aligned != base::i18n::IsRTL()));
  } else {
    SetViewVisibility(views().right_aligned_side_panel_separator,
                      layout_result.side_panel_visible &&
                          layout_result.side_panel_right_aligned);
    views().right_aligned_side_panel_separator->SetBoundsRect(
        layout_result.separator_bounds);
    SetViewVisibility(views().left_aligned_side_panel_separator,
                      layout_result.side_panel_visible &&
                          !layout_result.side_panel_right_aligned);
    views().left_aligned_side_panel_separator->SetBoundsRect(
        layout_result.separator_bounds);

    SetViewVisibility(views().side_panel_rounded_corner,
                      layout_result.side_panel_visible);
    if (layout_result.side_panel_visible) {
      // Adjust the rounded corner bounds based on the side panel bounds.
      const int corner_size =
          views().side_panel_rounded_corner->GetPreferredSize().width();

      const int top_separator_height = views::Separator::kThickness;
      if (layout_result.contents_container_after_side_panel) {
        views().side_panel_rounded_corner->SetBounds(
            layout_result.side_panel_bounds.right(),
            layout_result.side_panel_bounds.y() - top_separator_height,
            corner_size, corner_size);
      } else {
        views().side_panel_rounded_corner->SetBounds(
            layout_result.side_panel_bounds.x() - corner_size,
            layout_result.side_panel_bounds.y() - top_separator_height,
            corner_size, corner_size);
      }
    }
  }
}

void BrowserViewLayoutImplOld::UpdateTopContainerBounds(
    const gfx::Rect& available_bounds) {
  // Set the bounds of the top container view such that it is tall enough to
  // fully show all of its children. In particular, the bottom of the bookmark
  // bar can be above the bottom of the toolbar while the bookmark bar is
  // animating. The top container view is positioned relative to the top of the
  // client view instead of relative to GetTopInsetInBrowserView() because the
  // top container view paints parts of the frame (title, window controls)
  // during an immersive fullscreen reveal.
  int height = 0;
  for (views::View* child : views().top_container->children()) {
    if (child->GetVisible()) {
      height = std::max(height, child->bounds().bottom());
    }
  }

  // Ensure that the top container view reaches the topmost view in the
  // ClientView because the bounds of the top container view are used in
  // layout and we assume that this is the case.
  height = std::max(height, delegate().GetTopInsetInBrowserView());

  gfx::Rect top_container_bounds(available_bounds.width(), height);

  if (delegate().IsTopControlsSlideBehaviorEnabled()) {
    // If the top controls are fully hidden, then it's positioned outside the
    // views' bounds.
    const float ratio = delegate().GetTopControlsSlideBehaviorShownRatio();
    top_container_bounds.set_y(ratio == 0 ? -height : 0);
  } else {
    // If the immersive mode controller is animating the top container, it may
    // be partly offscreen.
    top_container_bounds.set_y(
        delegate().GetImmersiveModeController()->GetTopContainerVerticalOffset(
            top_container_bounds.size()));
  }
  views().top_container->SetBoundsRect(top_container_bounds);
  SetClipPathWithBottomAllowance(views().top_container);
}

int BrowserViewLayoutImplOld::GetMinWebContentsWidth() const {
  int min_width =
      kMainBrowserContentsMinimumWidth -
      views().contents_height_side_panel->GetMinimumSize().width() -
      (views().right_aligned_side_panel_separator
           ? views()
                 .right_aligned_side_panel_separator->GetPreferredSize()
                 .width()
           : 0);

  // When in split view, the minimum width of the contents is higher.
  if (views().multi_contents_view) {
    min_width =
        std::max(min_width, 2 * views().multi_contents_view->GetMinViewWidth());
  }
  DCHECK_GE(min_width, 0);
  return min_width;
}

bool BrowserViewLayoutImplOld::IsImmersiveModeEnabledWithoutToolbar() const {
  return delegate().GetImmersiveModeController()->IsEnabled()
#if BUILDFLAG(IS_MAC)
         && (!fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()) ||
             fullscreen_utils::IsInContentFullscreen(browser()))
#endif
      ;
}

gfx::Point BrowserViewLayoutImplOld::GetDialogPosition(
    const gfx::Size& dialog_size) const {
  // Horizontally places the dialog at the center of the content.

  views::View* view = views().contents_container;
  // Recalculate bounds of `contents_container_`. It may be stale due to
  // pending layouts (from switching tabs, for example). The `top` and
  // `bottom` parameters should not be relevant to the result, since we only
  // care about the resulting width here.
  const auto* parent_view = view->parent();

  gfx::Rect view_bounds(parent_view->GetLocalBounds());
  view_bounds.set_y(view->bounds().y());
  view_bounds.set_height(view->bounds().bottom());

  ContentsContainerLayoutResult layout_result =
      CalculateContentsContainerLayout(view_bounds);

  int leading_x;
  if (base::i18n::IsRTL()) {
    // Dialog coordinates are not flipped for RTL, but the View's coordinates
    // are. Calculate the left edge of `contents_container_bounds`.
    if (layout_result.contents_container_after_side_panel) {
      leading_x = 0;
    } else {
      leading_x = parent_view->GetLocalBounds().width() -
                  layout_result.contents_container_bounds.width();
    }
  } else {
    leading_x = layout_result.contents_container_bounds.x();
  }
  const int middle_x =
      leading_x + layout_result.contents_container_bounds.width() / 2;
  return gfx::Point(middle_x - dialog_size.width() / 2, dialog_top_y_);
}

gfx::Size BrowserViewLayoutImplOld::GetMaximumDialogSize() const {
  // Modals use NativeWidget and cannot be rendered beyond the browser
  // window boundaries. Restricting them to the browser window bottom
  // boundary and let the dialog to figure out a good layout.
  // WARNING: previous attempts to allow dialog to extend beyond the browser
  // boundaries have caused regressions in a number of dialogs. See
  // crbug.com/364463378, crbug.com/369739216, crbug.com/363205507.
  // TODO(crbug.com/334413759, crbug.com/346974105): use desktop widgets
  // universally.
  views::View* view = views().contents_container;
  gfx::Rect content_area = view->ConvertRectToWidget(view->GetLocalBounds());
  const int top = dialog_top_y_;
  return gfx::Size(content_area.width(), content_area.bottom() - top);
}
