// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include <map>
#include <optional>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"

namespace {

// Shorthand for validating both `child` and `parent` and checking that one is
// parented to the other. Ignores child visibility.
bool IsParentedTo(const views::View* child, const views::View* parent) {
  return child && parent && child->parent() == parent;
}

// Shorthand for validating both `child` and `parent` and checking that one is
// parented to the other. If `child` is not visible, returns false.
bool IsParentedToAndVisible(const views::View* child,
                            const views::View* parent) {
  return IsParentedTo(child, parent) && child->GetVisible();
}

// Insets `span` by `amount` on either the `leading` or (if false) trailing
// edge, to a minimum of zero width.
void Inset(views::Span& span, int amount, bool leading) {
  if (leading) {
    span.set_start(std::min(span.end(), span.start() + amount));
  } else {
    span.set_end(std::max(span.start(), span.end() - amount));
  }
}

// Gets the bounds for a `view`, placed between the exclusion zones in `params`
// if they are present.
gfx::Rect GetBoundsWithExclusion(const BrowserLayoutParams& params,
                                 const views::View* view) {
  const auto leading = params.leading_exclusion.ContentWithPadding();
  const auto trailing = params.trailing_exclusion.ContentWithPadding();
  int height = base::ClampCeil(std::max(leading.height(), trailing.height()));
  if (!height) {
    height = view->GetPreferredSize().height();
  }
  return gfx::Rect(
      /*x=*/params.visual_client_area.x() + leading.width(),
      /*y=*/params.visual_client_area.y(),
      /*width=*/params.visual_client_area.width() -
          (leading.width() + trailing.width()),
      /*height=*/height);
}

}  // namespace

struct BrowserViewLayoutImpl::ProposedLayout {
  ProposedLayout(const gfx::Rect& bounds_, std::optional<bool> visibility_)
      : bounds(bounds_), visibility(visibility_) {}
  ProposedLayout() = default;
  ~ProposedLayout() = default;

  // Current view's bounds relative to its parent.
  gfx::Rect bounds;

  // If visibility is to be set during layout, set this flag.
  std::optional<bool> visibility;

  // Layouts of children of the current view.
  //
  // It is very important that this object not be stored, but only exist on the
  // stack during calls, as the `raw_ptr` may otherwise dangle.
  std::map<raw_ptr<views::View, CtnExperimental>, ProposedLayout> children;

  // Adds a child layout for `child` and returns the layout. Fails if `child` is
  // already present.
  ProposedLayout& AddChild(views::View* child,
                           const gfx::Rect& bounds_,
                           std::optional<bool> visibility_ = std::nullopt) {
    const auto emplace_result =
        children.emplace(child, ProposedLayout(bounds_, visibility_));
    CHECK(emplace_result.second)
        << "Already added layout for " << child->GetClassName();
    return emplace_result.first->second;
  }

  // Searches the tree for `descendant` and returns its layout, otherwise,
  // returns null if not found.
  const ProposedLayout* GetLayoutFor(const views::View* descendant) const {
    for (const auto& child : children) {
      if (child.first == descendant) {
        return &child.second;
      }
      if (auto* const result = child.second.GetLayoutFor(descendant)) {
        return result;
      }
    }
    return nullptr;
  }

  // Finds `descendant`'s layout in the tree and returns its bounds relative to
  // `relative_to`.
  std::optional<gfx::Rect> GetBoundsFor(const views::View* descendant,
                                        const views::View* relative_to) const {
    const ProposedLayout* layout = GetLayoutFor(descendant);
    if (!layout) {
      return std::nullopt;
    }
    // Since layout bounds are relative to the parent, do the conversion from
    // there.
    return views::View::ConvertRectToTarget(descendant->parent(), relative_to,
                                            layout->bounds);
  }

  using SetViewVisibility =
      base::FunctionRef<void(views::View* view, bool visible)>;

  // Applies this layout to `root`. In order to ensure that all child layouts
  // are applied, this is an inherently destructive operation; each child layout
  // is removed as it is applied and if there are any orphan layouts a stack
  // dump is triggered (this will be a CHECK() in the future).
  void ApplyLayout(views::View* root,
                   SetViewVisibility set_view_visibility) && {
    for (auto& child : root->children()) {
      if (const auto it = children.find(child); it != children.end()) {
        if (it->second.visibility) {
          set_view_visibility(child, *it->second.visibility);
        }
        child->SetBoundsRect(it->second.bounds);
        std::move(it->second).ApplyLayout(child, set_view_visibility);
        children.erase(it);
      }
    }
    if (!children.empty()) {
      const views::View* const leftover = children.begin()->first;
      DUMP_WILL_BE_NOTREACHED()
          << "Unapplied layout remains for " << leftover->GetClassName()
          << " in " << root->GetClassName();
    }
  }
};

BrowserViewLayoutImpl::BrowserViewLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayout(std::move(delegate), browser, std::move(views)) {}

BrowserViewLayoutImpl::~BrowserViewLayoutImpl() = default;

void BrowserViewLayoutImpl::Layout(views::View* host) {
  CalculateProposedLayout().ApplyLayout(
      host, [this](views::View* view, bool visible) {
        SetViewVisibility(view, visible);
      });

  MaybeLayOutTopContainerOverlay();
}

void BrowserViewLayoutImpl::MaybeLayOutTopContainerOverlay() {
  if (!views().top_container ||
      views().top_container->parent() == views().main_container) {
    return;
  }

  // In slide/immersive mode, animating the top container is handled by someone
  // else, but there are adjustments that are needed to be made.
  ProposedLayout top_container_layout;

  // There are probably cases where these require some translation, but for
  // now, just use them as-is. Also determine which platforms require
  // exclusions and which do not.
  const auto params = delegate().GetBrowserLayoutParams();

  // The computation for the top container components does not change.
  gfx::Rect top_container_bounds =
      CalculateTopContainerLayout(top_container_layout, params, true);

  // In certain circumstances, the bounds require adjustment.
  if (delegate().IsTopControlsSlideBehaviorEnabled() &&
      delegate().GetTopControlsSlideBehaviorShownRatio() == 0.0) {
    // In slide mode, if the top container is hidden completely, it is placed
    // outside the window bounds.
    top_container_bounds.set_y(-top_container_bounds.height());
  } else if (auto* const controller = delegate().GetImmersiveModeController();
             controller && controller->IsEnabled()) {
    // If the immersive mode controller is animating the top container overlay,
    // it may be partly offscreen. The controller knows where the container
    // needs to be.
    top_container_bounds.set_y(
        delegate().GetImmersiveModeController()->GetTopContainerVerticalOffset(
            top_container_bounds.size()));
  }

  // Position the top container in its parent, whatever that is.
  views().top_container->SetBoundsRect(top_container_bounds);

  // Apply the child layouts for the top container.
  std::move(top_container_layout)
      .ApplyLayout(views().top_container,
                   [this](views::View* view, bool visible) {
                     SetViewVisibility(view, visible);
                   });
}

gfx::Size BrowserViewLayoutImpl::GetMinimumSize(const views::View* host) const {
  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const gfx::Size tabstrip_size =
      views().tab_strip_region_view->GetMinimumSize();
  const gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
  const gfx::Size bookmark_bar_size =
      (views().bookmark_bar && views().bookmark_bar->GetVisible())
          ? views().bookmark_bar->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size = views().contents_container->GetMinimumSize();
  const gfx::Size contents_height_side_panel_size =
      views().contents_height_side_panel &&
              views().contents_height_side_panel->GetVisible()
          ? views().contents_height_side_panel->GetMinimumSize()
          : gfx::Size();

  const int min_height =
      tabstrip_size.height() + toolbar_size.height() +
      bookmark_bar_size.height() + infobar_container_size.height() +
      std::max({contents_size.height(),
                contents_height_side_panel_size.height(), 1});

  // TODO(https://crbug.com/454583671): This probably needs to be more
  // sophisticated to handle separators, etc. but it's unwieldy to do it without
  // better decomposition of the layout.
  const int min_width = std::max(
      {tabstrip_size.width(), toolbar_size.width(), bookmark_bar_size.width(),
       infobar_container_size.width(),
       (contents_size.width() + contents_height_side_panel_size.width()),
       kMainBrowserContentsMinimumWidth});

  return gfx::Size(min_width, min_height);
}

int BrowserViewLayoutImpl::GetMinWebContentsWidthForTesting() const {
  int min_width = kMainBrowserContentsMinimumWidth;
  if (views().multi_contents_view &&
      views().multi_contents_view->IsInSplitView()) {
    // TODO(https://crbug.com/454583671): Eliminate
    // `MultiContentsView::GetMinViewWidth()` in favor of a functional
    // implementation of `GetMinimumSize()`.
    min_width =
        std::max(min_width, 2 * views().multi_contents_view->GetMinViewWidth());
  }
  return min_width;
}

BrowserViewLayoutImpl::ProposedLayout
BrowserViewLayoutImpl::CalculateProposedLayout() const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  TRACE_EVENT0("ui", "BrowserViewLayoutImpl::CalculateProposedLayout");
  ProposedLayout layout;
  const BrowserLayoutParams params = delegate().GetBrowserLayoutParams();

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, params.visual_client_area);
  }

  // TODO(https://crbug.com/453717426): Handle vertical tabstrip here.

  int y = params.visual_client_area.y();
  bool used_exclusion = false;

  // Lay out tab strip region.
  if (IsParentedToAndVisible(views().tab_strip_region_view,
                             views().browser_view)) {
    const gfx::Rect tabstrip_bounds =
        GetBoundsWithExclusion(params, views().tab_strip_region_view);
    y = tabstrip_bounds.bottom() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
    used_exclusion = true;
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds);
  }

  int x = params.visual_client_area.x();

  // Lay out toolbar-height side panel.
  if (IsParentedToAndVisible(views().toolbar_height_side_panel,
                             views().browser_view)) {
    if (const int width =
            views().toolbar_height_side_panel->GetPreferredSize().width();
        width > 0) {
      const int top = std::max(
          y, params.visual_client_area.y() +
                 base::ClampCeil(
                     params.leading_exclusion.ContentWithPadding().height()));
      const gfx::Rect toolbar_height_bounds(
          x, top, width, params.visual_client_area.bottom() - top);
      x = toolbar_height_bounds.right();
      layout.AddChild(views().toolbar_height_side_panel, toolbar_height_bounds);
    }
  }

  // Lay out the main container of the browser.
  const gfx::Rect main_bounds(x, y, params.visual_client_area.width() - x,
                              params.visual_client_area.height() - y);
  const BrowserLayoutParams main_params =
      params.InLocalCoordinates(main_bounds);
  ProposedLayout& main_layout =
      layout.AddChild(views().main_container, main_bounds);
  CalculateMainContainerLayout(main_layout, main_params, !used_exclusion);

  return layout;
}

void BrowserViewLayoutImpl::CalculateMainContainerLayout(
    ProposedLayout& layout,
    const BrowserLayoutParams& params,
    bool needs_exclusion) const {
  int y = params.visual_client_area.y();

  // Lay out top container.
  if (IsParentedToAndVisible(views().top_container, views().main_container)) {
    // Take advantage of the fact that the top container takes up the entire top
    // area of the main container.
    ProposedLayout& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    top_container_layout.bounds = CalculateTopContainerLayout(
        top_container_layout, params, needs_exclusion);
    y = top_container_layout.bounds.bottom();
  }

  // Lay out infobar container.
  if (IsParentedTo(views().infobar_container, views().main_container)) {
    const gfx::Rect infobar_bounds = gfx::Rect(
        params.visual_client_area.x(),
        // Infobar needs to get down out of the way of immersive mode elements
        // in some cases.
        y + delegate().GetImmersiveModeController()->GetExtraInfobarOffset(),
        params.visual_client_area.width(),
        // This returns zero for empty infobar.
        views().infobar_container->GetPreferredSize().height());
    layout.AddChild(views().infobar_container, infobar_bounds,
                    !infobar_bounds.IsEmpty());
    y = infobar_bounds.bottom();
  }

  // Lay out contents-height side panel.
  views::Span horizontal_space(params.visual_client_area.x(),
                               params.visual_client_area.width());
  bool show_left_separator = false;
  bool show_right_separator = false;
  bool side_panel_leading = false;
  if (IsParentedTo(views().contents_height_side_panel,
                   views().main_container)) {
    SidePanel* const side_panel = views().contents_height_side_panel;
    int side_panel_width = 0;
    const bool is_right_aligned = side_panel->IsRightAligned();
    side_panel_leading = is_right_aligned == base::i18n::IsRTL();
    if (side_panel->GetVisible()) {
      show_left_separator = !is_right_aligned;
      show_right_separator = is_right_aligned;
      const int min_width = side_panel->GetMinimumSize().width();
      const int preferred_width = side_panel->GetPreferredSize().width();
      // TODO(https://crbug.com/453717426): This is logic carried over from the
      // old layout and feels extremely wrong. Also the unrestricted size
      // computation is simplified because the limits are arbitrary anyway.
      const int max_width =
          side_panel->ShouldRestrictMaxWidth()
              ? horizontal_space.length() * 2 / 3
              : horizontal_space.length() - kMainBrowserContentsMinimumWidth;
      side_panel_width =
          std::max(min_width, std::min(preferred_width, max_width));
    }

    // Original layout sets side panel bounds regardless of visibility because
    // when the side panel animates open we want it to start at zero size.
    const gfx::Rect side_panel_bounds(
        side_panel_leading ? horizontal_space.start()
                           : horizontal_space.end() - side_panel_width,
        y, side_panel_width, params.visual_client_area.bottom() - y);
    layout.AddChild(side_panel, side_panel_bounds);
    Inset(horizontal_space, side_panel_width, side_panel_leading);
  }

  // Lay out the left side panel separator.
  if (IsParentedTo(views().left_aligned_side_panel_separator,
                   views().main_container)) {
    gfx::Rect separator_bounds;
    if (show_left_separator) {
      const int separator_width =
          views().left_aligned_side_panel_separator->GetPreferredSize().width();
      separator_bounds = gfx::Rect(
          side_panel_leading ? horizontal_space.start()
                             : horizontal_space.end() - separator_width,
          y, separator_width, params.visual_client_area.bottom() - y);
      Inset(horizontal_space, separator_width, side_panel_leading);
    }
    layout.AddChild(views().left_aligned_side_panel_separator, separator_bounds,
                    show_left_separator);
  }

  // Lay out the right side panel separator.
  if (IsParentedTo(views().right_aligned_side_panel_separator,
                   views().main_container)) {
    gfx::Rect separator_bounds;
    if (show_right_separator) {
      const int separator_width =
          views()
              .right_aligned_side_panel_separator->GetPreferredSize()
              .width();
      separator_bounds = gfx::Rect(
          side_panel_leading ? horizontal_space.start()
                             : horizontal_space.end() - separator_width,
          y, separator_width, params.visual_client_area.bottom() - y);
      Inset(horizontal_space, separator_width, side_panel_leading);
    }
    layout.AddChild(views().right_aligned_side_panel_separator,
                    separator_bounds, show_right_separator);
  }

  // Lay out contents container.
  CHECK(IsParentedToAndVisible(views().contents_container,
                               views().main_container));
  layout.AddChild(
      views().contents_container,
      gfx::Rect(horizontal_space.start(), y, horizontal_space.length(),
                params.visual_client_area.bottom() - y));
}

gfx::Rect BrowserViewLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    const BrowserLayoutParams& params,
    bool needs_exclusion) const {
  int y = params.visual_client_area.y();

  // If the tabstrip is in the top container (which can happen in immersive
  // mode), ensure it is laid out here.
  if (IsParentedToAndVisible(views().tab_strip_region_view,
                             views().top_container)) {
    const gfx::Rect tabstrip_bounds =
        needs_exclusion
            ? GetBoundsWithExclusion(params, views().tab_strip_region_view)
            : gfx::Rect(
                  params.visual_client_area.x(), y,
                  params.visual_client_area.width(),
                  views().tab_strip_region_view->GetPreferredSize().height());
    y = tabstrip_bounds.bottom();
    needs_exclusion = false;
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds);
  }

  // Lay out toolbar. If tabstrip is completely absent (or vertical), this can
  // go in the top exclusion area.
  CHECK(IsParentedToAndVisible(views().toolbar, views().top_container))
      << "Top container was in the browser but missing toolbar.";
  const gfx::Rect toolbar_bounds =
      needs_exclusion ? GetBoundsWithExclusion(params, views().toolbar)
                      : gfx::Rect(params.visual_client_area.x(), y,
                                  params.visual_client_area.width(),
                                  views().toolbar->GetPreferredSize().height());
  y = toolbar_bounds.bottom();
  needs_exclusion = false;
  layout.AddChild(views().toolbar, toolbar_bounds);

  // Lay out the bookmarks bar if one is present.
  if (IsParentedToAndVisible(views().bookmark_bar, views().top_container)) {
    const gfx::Rect bookmark_bounds(
        params.visual_client_area.x(), y, params.visual_client_area.width(),
        views().bookmark_bar->GetPreferredSize().height());
    layout.AddChild(views().bookmark_bar, bookmark_bounds);
    y = bookmark_bounds.bottom();
  }

  // Lay out the separator.
  if (delegate().IsContentsSeparatorEnabled()) {
    if (IsParentedTo(views().top_container_separator, views().top_container)) {
      // TODO(https://crbug.com/453717426): Implement. For now, we won't show
      // the separator.
      layout.AddChild(views().top_container_separator, gfx::Rect(), false);
    }
  }

  // These are the bounds for the top container.
  return gfx::Rect(params.visual_client_area.x(), params.visual_client_area.y(),
                   params.visual_client_area.width(),
                   y - params.visual_client_area.y());
}

// Dialog positioning.

int BrowserViewLayoutImpl::GetDialogTop(const ProposedLayout& layout) const {
  const int kConstrainedWindowOverlap = 3;
  const auto* const browser_view = views().browser_view.get();
  if (const auto toolbar_rect =
          layout.GetBoundsFor(views().toolbar, browser_view)) {
    return toolbar_rect->bottom() - kConstrainedWindowOverlap;
  }
  if (const auto webapp_toolbar_rect =
          layout.GetBoundsFor(views().web_app_frame_toolbar, browser_view)) {
    return webapp_toolbar_rect->bottom() - kConstrainedWindowOverlap;
  }
  return kConstrainedWindowOverlap;
}

int BrowserViewLayoutImpl::GetDialogBottom(const ProposedLayout& layout) const {
  const auto* const browser_view = views().browser_view.get();
  if (const auto contents_rect =
          layout.GetBoundsFor(views().contents_container, browser_view)) {
    return contents_rect->bottom();
  }
  return browser_view->height();
}

views::Span BrowserViewLayoutImpl::GetDialogHorizontalTarget(
    const ProposedLayout& layout) const {
  const auto* const browser_view = views().browser_view.get();
  views::Span horizontal;
  if (const auto contents_rect =
          layout.GetBoundsFor(views().contents_container, browser_view)) {
    horizontal.set_start(contents_rect->x());
    horizontal.set_length(contents_rect->width());
  } else {
    horizontal.set_end(browser_view->width());
  }
  return horizontal;
}

gfx::Point BrowserViewLayoutImpl::GetDialogPosition(
    const gfx::Size& dialog_size) const {
  const ProposedLayout layout = CalculateProposedLayout();
  const auto horizontal = GetDialogHorizontalTarget(layout);
  return gfx::Point(horizontal.start() + horizontal.length() / 2,
                    GetDialogTop(layout));
}

gfx::Size BrowserViewLayoutImpl::GetMaximumDialogSize() const {
  const ProposedLayout layout = CalculateProposedLayout();
  const auto horizontal = GetDialogHorizontalTarget(layout);
  const int top = GetDialogTop(layout);
  const int bottom = GetDialogBottom(layout);
  return gfx::Size(horizontal.length(), bottom - top);
}
